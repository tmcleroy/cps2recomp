// SPDX-License-Identifier: GPL-3.0-only
// Exercise actual ROM palette copies, not merely the chosen palette index.
#include "damage_policy.h"
#include "machine.h"
#include <fstream>
#include <iterator>
#include <memory>
#include <cstdio>
#include <cstdlib>
using namespace cps2recomp;
namespace cps2recomp { void RegisterInputLabCheats(); void ApplyInputLabColour(uint32_t); }
unsigned checks=0;
void Check(bool ok,const char *why){++checks;if(!ok){fprintf(stderr,"FAIL %s\n",why);exit(1);}}
std::vector<uint8_t> Read(const char *path){std::ifstream f(path,std::ios::binary);return {std::istreambuf_iterator<char>(f),{}};}
int main(int argc,char **argv){
    if(argc!=3)return 2;
    RomSet roms;roms.program=Read(argv[1]);roms.opcodes=Read(argv[2]);
    Check(roms.program.size()>0x100000,"local ROM data");
    auto m=std::make_unique<Machine>(roms);SetProgram(roms.program.data(),roms.program.size());SetRam(m->ram.data());RegisterInputLabCheats();
    unsigned cases=0;
    for(int side=0;side<2;++side)for(int character=0;character<18;++character)for(int choice=0;choice<=10;++choice){
        std::fill(m->ram.begin(),m->ram.end(),0);std::fill(m->videoRam.begin(),m->videoRam.end(),0);
        const uint32_t p=0xff844e + side*0x400,other=0xff844e + (1-side)*0x400;
        const int marker=character<16?0:character-15;
        SetRam8(p+0x38e,1);SetRam8(p+0x391,marker?0:character);SetRam8(p+0x3bd,marker);
        SetRam8(p+0x3b1,marker?10:3);SetRam8(other+0x3b1,0);
        SetRam16(p+0x58,27);
        LoadFeatures("",std::string(side?"select_colour_2=":"select_colour_1=")+std::to_string(choice));
        auto before=m->ram;ApplyInputLabColour(p);
        const int selected=choice-1;
        const int expected=choice?(marker?((selected<=2||selected==8)?10:11):selected):(marker?10:3);
        before[(p+0x3b1)&0xffff]=expected;
        Check(m->ram==before,"only selected player's colour changes");
        auto &c=m->cpu;c.Reset(true);c.a[5]=0xff8000;c.a[7]=0xff7000;
        Call(c,0xbfa78);
        const uint32_t list=0xbfaf2+m->Read16(0xbfaf2+(marker?16:character)*2);
        const uint32_t source=list+m->Read16(list+expected*2);const int count=m->Read16(source);
        Check(count>0&&count<=5,"native palette table bounds");
        for(int n=0;n<count*16;++n)Check(m->Read16((side?0x900200:0x900020)+n*2)==m->Read16(source+2+n*2),"original palette copied to VRAM");
        ++cases;
    }
    for(int side=0;side<2;++side){
        const uint32_t p=0xff844e + side*0x400;SetRam8(p+0x38e,0);SetRam8(p+0x3b1,10);
        LoadFeatures("",std::string(side?"select_colour_2=":"select_colour_1=")+"4");auto before=m->ram;
        ApplyInputLabColour(p);Check(m->ram==before,"CPU palette untouched");
    }
    printf("COLOUR_PORT_PASS cases=%u checks=%u all_16_characters_akuma_shin_both_players_Off_and_buttons\n",cases,checks);
}
