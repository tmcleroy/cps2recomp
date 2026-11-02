// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026. Local Input Lab extension to cps2recomp.
#include "raw_keyboards.h"
#include <algorithm>
#include <cstddef>
#include <cstdio>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <cfgmgr32.h>
#include <devpropdef.h>
#endif
namespace port {
struct RawKeyboards::Impl {
    KeyboardState state;
    std::string error;
#ifdef _WIN32
    HWND sink = nullptr, owner = nullptr;
    std::map<HANDLE, std::string> handles;
    bool Focused() const { return owner && GetForegroundWindow() == owner; }
    std::string PhysicalDevice(const wchar_t *path,const std::string &fallback) {
        // SDK device properties: map all HID keyboard collections to the same
        // physical PnP container, never merely to matching VID/PID/model names.
        static const DEVPROPKEY instanceKey={{0x78c34fc8,0x104a,0x4aca,{0x9e,0xa4,0x52,0x4d,0x52,0x99,0x6e,0x57}},256};
        static const DEVPROPKEY containerKey={{0x8c7ed206,0x3f8a,0x4827,{0xb3,0xab,0xae,0x9e,0x1f,0xae,0xfc,0x6c}},2};
        wchar_t instance[4096]{};ULONG size=sizeof(instance);DEVPROPTYPE type=0;DEVINST node=0;GUID container{};
        if(CM_Get_Device_Interface_PropertyW(path,&instanceKey,&type,reinterpret_cast<PBYTE>(instance),&size,0)!=CR_SUCCESS ||
           type!=DEVPROP_TYPE_STRING || CM_Locate_DevNodeW(&node,instance,CM_LOCATE_DEVNODE_NORMAL)!=CR_SUCCESS)return fallback;
        size=sizeof(container);
        if(CM_Get_DevNode_PropertyW(node,&containerKey,&type,reinterpret_cast<PBYTE>(&container),&size,0)!=CR_SUCCESS ||
           type!=DEVPROP_TYPE_GUID || size!=sizeof(container))return fallback;
        // Null/system containers are not evidence of one physical keyboard.
        if(container.Data1==0 && container.Data2==0 && container.Data3==0)return fallback;
        char text[96];std::snprintf(text,sizeof(text),"wincontainer:%08lx-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x",
            container.Data1,container.Data2,container.Data3,container.Data4[0],container.Data4[1],container.Data4[2],container.Data4[3],
            container.Data4[4],container.Data4[5],container.Data4[6],container.Data4[7]);
        return text;
    }
    void UpdateFocus() {
        const bool current = Focused();
        state.Focus(current);
    }
    std::string Identify(HANDLE handle) {
        if (!handle) return {};
        const auto known = handles.find(handle);
        if (known != handles.end()) return known->second;
        UINT count = 0;
        if (GetRawInputDeviceInfoW(handle, RIDI_DEVICENAME, nullptr, &count) == UINT(-1) || !count) return {};
        std::vector<wchar_t> name(count + 1);
        if (GetRawInputDeviceInfoW(handle, RIDI_DEVICENAME, name.data(), &count) == UINT(-1)) return {};
        const int bytes = WideCharToMultiByte(CP_UTF8, 0, name.data(), -1, nullptr, 0, nullptr, nullptr);
        if (bytes <= 1) return {};
        std::string id(bytes, '\0');
        WideCharToMultiByte(CP_UTF8, 0, name.data(), -1, id.data(), bytes, nullptr, nullptr);
        id.pop_back();
        std::transform(id.begin(), id.end(), id.begin(), [](unsigned char c) { return c >= 'A' && c <= 'Z' ? c + 32 : c; });
        id = "winraw:" + id;
        handles[handle] = id;
        // SDL's Windows backend uses the raw handle as its display ID. This is
        // only a friendly label; routing/persistence always use the Windows path.
        const char *friendly = SDL_GetKeyboardNameForID(static_cast<SDL_KeyboardID>(reinterpret_cast<uintptr_t>(handle)));
        state.Connect(id, friendly && *friendly ? friendly : "Keyboard",PhysicalDevice(name.data(),id));
        return id;
    }
    void Enumerate() {
        UINT count = 0;
        if (GetRawInputDeviceList(nullptr, &count, sizeof(RAWINPUTDEVICELIST)) == UINT(-1)) return;
        std::vector<RAWINPUTDEVICELIST> devices(count);
        const UINT found = GetRawInputDeviceList(devices.data(), &count, sizeof(RAWINPUTDEVICELIST));
        if (found == UINT(-1)) return;
        for (UINT i = 0; i < found; ++i) if (devices[i].dwType == RIM_TYPEKEYBOARD) Identify(devices[i].hDevice);
    }
    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
        auto *self = reinterpret_cast<Impl *>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
        if (msg == WM_NCCREATE) {
            self = static_cast<Impl *>(reinterpret_cast<CREATESTRUCTW *>(lp)->lpCreateParams);
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        }
        if (self && msg == WM_INPUT) {
            UINT bytes = 0;
            if (GetRawInputData(reinterpret_cast<HRAWINPUT>(lp), RID_INPUT, nullptr, &bytes, sizeof(RAWINPUTHEADER)) == 0 && bytes <= 65536) {
                std::vector<BYTE> data(bytes);
                const UINT expected = bytes;
                const UINT read = bytes >= offsetof(RAWINPUT, data) + sizeof(RAWKEYBOARD) ?
                    GetRawInputData(reinterpret_cast<HRAWINPUT>(lp), RID_INPUT, data.data(), &bytes, sizeof(RAWINPUTHEADER)) : UINT(-1);
                if (read == expected && expected >= offsetof(RAWINPUT, data) + sizeof(RAWKEYBOARD)) {
                    const auto *raw = reinterpret_cast<const RAWINPUT *>(data.data());
                    if (raw->header.dwType == RIM_TYPEKEYBOARD) {
                        const auto &key = raw->data.keyboard;
                        const auto id = self->Identify(raw->header.hDevice);
                        self->UpdateFocus();
                        const auto scan = RawKeyboardScancode(key.MakeCode, key.Flags, key.VKey);
                        self->state.Key(id, scan, !(key.Flags & RI_KEY_BREAK));
                        // Pause has no physical break packet on some keyboards.
                        if (scan == SDL_SCANCODE_PAUSE) self->state.Key(id, scan, false);
                    }
                }
            }
        } else if (self && msg == WM_INPUT_DEVICE_CHANGE) {
            const auto handle = reinterpret_cast<HANDLE>(lp);
            if (wp == GIDC_ARRIVAL) self->Enumerate();
            else if (wp == GIDC_REMOVAL) {
                const auto it = self->handles.find(handle);
                if (it != self->handles.end()) { self->state.Disconnect(it->second); self->handles.erase(it); }
            }
        }
        return DefWindowProcW(hwnd, msg, wp, lp);
    }
    ~Impl() {
        if (sink) {
            RAWINPUTDEVICE device{1, 6, RIDEV_REMOVE, nullptr};
            RegisterRawInputDevices(&device, 1, sizeof(device));
            DestroyWindow(sink);
        }
    }
#endif
};
RawKeyboards::RawKeyboards() : impl_(std::make_unique<Impl>()) {}
RawKeyboards::~RawKeyboards() = default;
KeyboardState &RawKeyboards::State() { return impl_->state; }
const std::string &RawKeyboards::Error() const { return impl_->error; }
void RawKeyboards::PollFocus() {
#ifdef _WIN32
    impl_->UpdateFocus();
#endif
}
bool RawKeyboards::Retarget(SDL_Window *window) {
#ifdef _WIN32
    if (!impl_->sink) return Open(window);
    const auto owner = static_cast<HWND>(SDL_GetPointerProperty(SDL_GetWindowProperties(window), SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr));
    if (!owner) { impl_->error = "Keyboard input requires a Windows desktop window."; return false; }
    impl_->state.Focus(false); // held menu keys must be released before reaching the resumed game
    impl_->owner = owner;
    RAWINPUTDEVICE device{1, 6, RIDEV_INPUTSINK | RIDEV_DEVNOTIFY, impl_->sink};
    if (!RegisterRawInputDevices(&device, 1, sizeof(device))) {
        impl_->error = "Windows could not reattach separate keyboard input."; return false;
    }
    impl_->error.clear(); PollFocus(); return true;
#else
    (void)window; return false;
#endif
}
bool RawKeyboards::Open(SDL_Window *window) {
#ifdef _WIN32
    if (impl_->sink) return true;
    impl_->owner = static_cast<HWND>(SDL_GetPointerProperty(SDL_GetWindowProperties(window), SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr));
    if (!impl_->owner) { impl_->error = "Keyboard assignment requires a Windows desktop window."; return false; }
    WNDCLASSW cls{}; cls.lpfnWndProc = Impl::WindowProc; cls.hInstance = GetModuleHandleW(nullptr);
    cls.lpszClassName = L"CPS2InputLabKeyboards";
    if (!RegisterClassW(&cls) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) { impl_->error = "Could not create keyboard input window."; return false; }
    impl_->sink = CreateWindowExW(0, cls.lpszClassName, L"", 0, 0, 0, 0, 0, HWND_MESSAGE, nullptr, cls.hInstance, impl_.get());
    if (!impl_->sink) { impl_->error = "Could not create keyboard input window."; return false; }
    // No NOLEGACY/NOHOTKEYS: normal Windows shortcuts and launcher text input stay available.
    RAWINPUTDEVICE device{1, 6, RIDEV_INPUTSINK | RIDEV_DEVNOTIFY, impl_->sink};
    if (!RegisterRawInputDevices(&device, 1, sizeof(device))) {
        DestroyWindow(impl_->sink); impl_->sink = nullptr;
        impl_->error = "Windows could not open separate keyboard input."; return false;
    }
    impl_->Enumerate(); impl_->error.clear(); PollFocus(); return true;
#else
    (void)window; impl_->error = "Separate physical keyboards currently require Windows."; return false;
#endif
}
} // namespace port
