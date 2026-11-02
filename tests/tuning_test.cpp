// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Input Lab contributors
#include "../port/tuning.h"
#include "../recomp-common/settings.h"
#include <cassert>
#include <cstdio>
#include <filesystem>
#include <limits>
int main() {
    using namespace port;
    const auto path=(std::filesystem::temp_directory_path()/"recomp-tuning-test.ini").string();
    Tuning one; one.values={450,10,95,3,-1,0};
    assert(SaveTuning(path,one)); const auto loaded=LoadTuning(path);
    assert(loaded.values[0]==400 && loaded.values[1]==25 && loaded.values[2]==90);
    assert(loaded.values[3]==2 && loaded.values[4]==0 && loaded.values[5]==.1);
    recomp::Ini ini(path);ini.load();ini.set("features","untouched","yes");ini.save();
    assert(SaveTuning(path,Tuning{}));ini.load();assert(ini.get("features","untouched")=="yes");
    PictureTuning picture; uint32_t pixels[]={0xFF000000,0xFF807040,0xFFFFFFFF};
    picture.Apply(pixels,3,Tuning{});assert(pixels[0]==0xFF000000 && pixels[1]==0xFF807040 && pixels[2]==0xFFFFFFFF);
    Tuning black;black.values[3]=0;picture.Apply(pixels,3,black);assert((pixels[1]&0xFFFFFF)==0);
    std::filesystem::remove(path);
    std::puts("Sliders: bounds, per-file persistence, unrelated settings and identity picture transform passed");
}
