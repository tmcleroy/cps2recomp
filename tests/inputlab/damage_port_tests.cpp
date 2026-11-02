// SPDX-License-Identifier: GPL-3.0-only
// Runs the patched read against the actual original instruction and flags.
#include "damage_policy.h"
#include "machine.h"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <memory>
#include <vector>
using namespace cps2recomp;
namespace cps2recomp {
#ifdef LAB_SSF2T
void RegisterInputLabCheats();
void ApplyInputLabCharacter(uint32_t);
#else
void RegisterInputLabDamage();
#endif
}
struct Site { uint32_t pc; int dest, source; bool incoming, klass, grab; };
#if defined(LAB_SFA)
constexpr Site sites[]={{0xf88c,2,0,false,false,false},{0xf958,0,1,true,true,false},{0xf970,0,1,true,false,false},
    {0x21242,2,6,false,false,true},{0x212a6,1,6,false,false,true}};
constexpr int owner=0xc5,rank=0xc7; constexpr uint32_t first=0xff8400;
#elif defined(LAB_SFZ2AL)
constexpr Site sites[]={{0x1099e,2,0,false,false,false},{0x10a9c,0,1,true,true,false},{0x10ab4,0,1,true,false,false},
    {0xf322,2,0,false,false,false},{0xf33c,2,0,false,false,false},{0xf43c,0,1,true,true,false},{0xf454,0,1,true,false,false},
    {0x261fa,2,6,false,false,true},{0x26298,1,6,false,false,true}};
constexpr int owner=0xa5,rank=0xa7; constexpr uint32_t first=0xff8400;
#else
constexpr Site sites[]={{0x4af2,2,6,false,false,true},{0x4b9a,0,1,true,false,true},{0xbed38,0,2,true,false,false},
    {0xbed8a,0,6,false,false,false},{0xbeda0,0,0,false,false,false}};
constexpr int owner=0x38e,rank=0x58; constexpr uint32_t first=0xff844e;
#endif
static unsigned checks=0;
void Check(bool ok,const char *msg) { ++checks;if(!ok){fprintf(stderr,"FAIL %s\n",msg);exit(1);} }
std::vector<uint8_t> Read(const char *path) { std::ifstream f(path,std::ios::binary);return {std::istreambuf_iterator<char>(f),{}}; }
struct Result {
    std::array<uint32_t,8> d,a;
    uint32_t pc; bool x,n,z,v,c; uint64_t cycles;
    bool operator==(const Result&) const=default;
};
Result Capture(M68K &c) {
    Result r{};for(int i=0;i<8;++i){r.d[i]=c.d[i];r.a[i]=c.a[i];}
    r.pc=c.pc;r.x=c.flagX;r.n=c.flagN;r.z=c.flagZ;r.v=c.flagV;r.c=c.flagC;r.cycles=c.cycles;return r;
}
void Cpu(M68K &c,Site site,uint32_t actor,int low) {
    c.Reset(true);c.a[5]=0xff8000;c.a[7]=0xff7000;c.a[site.source]=actor;c.pc=site.pc;c.cycles=0;c.busWait=0;
    for(int i=0;i<8;++i)c.d[i]=0x12340000u|static_cast<unsigned>(low+i);
    c.flagX=true;c.flagC=true;c.flagV=true;c.flagN=false;c.flagZ=false;
}
int main(int argc,char **argv) {
    if(argc!=3){fprintf(stderr,"usage: test program.bin.data program.bin\n");return 2;}
    RomSet roms;roms.program=Read(argv[1]);roms.opcodes=Read(argv[2]);
    Check(roms.program.size()>0x100000 && roms.program.size()==roms.opcodes.size(),"local ROM views");
    auto m=std::make_unique<Machine>(roms);SetProgram(roms.program.data(),static_cast<uint32_t>(roms.program.size()));SetRam(m->ram.data());
#ifdef LAB_SFZ2AL
    AddRewrite(0x10972,"Fixture original damage",[](M68K&){});
#endif
#ifdef LAB_SSF2T
    RegisterInputLabCheats();
#else
    RegisterInputLabDamage();
#endif
    for(int dramatic : {0,1}) {
#ifdef LAB_SSF2T
        if(dramatic)continue;
#endif
        for(int pattern=0;pattern<4;++pattern)for(int cpuRank : {0,9,16,31})for(int policy=0;policy<3;++policy)
        for(const Site site:sites)for(int side=0;side<(dramatic?3:2);++side)for(int low : {0,0x7f,0xff}) {
            std::fill(m->ram.begin(),m->ram.end(),0);
            std::array<bool,3> human{!(pattern&1),!(pattern&2),false};
            std::array<uint16_t,3> ranks{7,static_cast<uint16_t>(cpuRank),31};
            if(human[0])ranks[0]=16;if(human[1])ranks[1]=11;
            for(int n=0;n<3;++n){
                const uint32_t p=first+n*0x400;
#ifdef LAB_SSF2T
                SetRam8(p+owner,human[n]?1:0);SetRam16(p+rank,ranks[n]);
#else
                SetRam8(p+owner,human[n]?0:1);SetRam8(p+rank,static_cast<uint8_t>(ranks[n]));
#endif
            }
#ifdef LAB_SFA
            SetRam8(0xffae38,dramatic);SetRam8(0xff8c4c,side&1);
#elif defined(LAB_SFZ2AL)
            SetRam16(0xff8138,dramatic?2:0);
#endif
            const bool mixed=dramatic?(human[0]||human[1]):human[0]!=human[1];
            const int h=human[0]?0:1, cp=dramatic?2:(human[0]?1:0);
            int reference=side;
            if(policy && mixed)reference=(policy==1)!=site.incoming?h:cp;
#ifdef LAB_SFA
            if(dramatic && mixed && policy==2 && !site.incoming && !site.grab)reference=side&1;
#endif
            const uint32_t actor=first+side*0x400;
            const int field=site.klass?owner:rank;
#ifdef LAB_SSF2T
            const unsigned saved=Ram16(actor+field),value=ranks[reference];
            SetRam16(actor+field,static_cast<uint16_t>(value));
#else
            const unsigned saved=Ram8(actor+field),value=site.klass?(human[reference]?0:1):ranks[reference];
            SetRam8(actor+field,static_cast<uint8_t>(value));
#endif
            LoadFeatures("","damage_hits=0");Cpu(m->cpu,site,actor,low);m->cpu.cycles+=m->cpu.Step();const Result expected=Capture(m->cpu);
#ifdef LAB_SSF2T
            SetRam16(actor+field,static_cast<uint16_t>(saved));
#else
            SetRam8(actor+field,static_cast<uint8_t>(saved));
#endif
            const auto before=m->ram;
            LoadFeatures("","damage_hits="+std::to_string(policy));Cpu(m->cpu,site,actor,low);
            const bool handled=RunHook(m->cpu,site.pc);if(!handled)m->cpu.cycles+=m->cpu.Step();
            if(!(Capture(m->cpu)==expected))fprintf(stderr,"site=%x policy=%d dramatic=%d pattern=%d side=%d\n",site.pc,policy,dramatic,pattern,side);
            Check(Capture(m->cpu)==expected,"original instruction registers flags cycles and live profile");
            Check(m->ram==before,"ownership, rank and every RAM byte unchanged");
            Check(handled==(policy!=0&&mixed),"Vanilla and unmixed matches pass through");
        }
    }
#ifdef LAB_SSF2T
    // Compare the port's boss bridge to original CPU initialization routines.
    struct MotionCase { int side,facing,strength,kind; };
    std::vector<MotionCase> motions;
    for(int side=0;side<2;++side)for(int facing=0;facing<2;++facing){
        for(int strength : {0,2,4})for(int kind=0;kind<5;++kind)motions.push_back({side,facing,strength,kind});
        for(int command=0;command<4;++command)motions.push_back({side,facing,command,5});
        for(int direction=0;direction<3;++direction)motions.push_back({side,facing,direction,6});
    }
    for(const auto t:motions){
        std::vector<uint32_t> reference;
        for(bool cpu : {true,false}){
            std::fill(m->ram.begin(),m->ram.end(),0);m->cpu.Reset(true);
            auto &c=m->cpu;const uint32_t p=first+t.side*0x400;const uint32_t other=p^0xc00;
            c.a[5]=0xff8000;c.a[6]=p;c.a[7]=0xff7000;
            SetRam8(p,1);SetRam8(p+1,1);SetRam8(p+2,2);SetRam8(p+3,12);
            SetRam16(p+6,500);SetRam16(p+10,t.kind==2?100:40);SetRam8(p+0x12,t.facing);SetRam8(p+0x1be,t.facing);
            SetRam8(p+0x381,t.side);SetRam16(p+0x39e,static_cast<uint16_t>(other));SetRam16(p+0x58,16);
            SetRam8(p+0x38e,cpu?0:1);SetRam8(p+0x3bd,cpu?1:2);SetRam16(p+0x2a,144);SetRam32(p+0x34,0x19fe6e);
            SetRam16(p+0x3c,0x200);SetRam16(p+0x3e,0x200);SetRam16(p+0x42,0x48);
            for(unsigned field : {0x93u,0x97u,0x9bu,0x187u})SetRam8(p+field,t.strength);
            SetRam8(0xff9545,1);SetRam16(0xff9568,0xe000);SetRam16(0xffe000,0x97a2);
            uint32_t entry=0;
            if(t.kind==0)entry=cpu?0xb82bc:0xa8dfc;
            if(t.kind==1)entry=cpu?0xb8518:0xa9054;
            if(t.kind==2)entry=cpu?0xb83e0:0xa8f2c;
            if(t.kind==5){entry=cpu?0xb8016:0xa8ba0;SetRam8(p+0xa1,t.strength);SetRam8(p+0x149,t.strength<2?t.strength:(t.strength^1));}
            if(t.kind==6){entry=cpu?0x7ca7e:0x7b47c;SetRam8(p+0x11,t.strength^t.facing);SetRam8(p+0x227,t.strength^t.facing);SetRam16(p+0x392,t.strength==0?2:t.strength==1?1:0);}
            const uint32_t stop=cpu?0xb8166:0xa8cf0;
            const uint8_t old0=roms.opcodes[stop],old1=roms.opcodes[stop+1];
            if(t.kind==3||t.kind==4){
                entry=cpu?0xb8110:0xa8c9e;SetRam8(p+0x88,t.kind==4?6:0);SetRam8(p+0x149,t.kind==4?1:0);SetRam8(p+0xe3,t.strength);
                roms.opcodes[stop]=0x4e;roms.opcodes[stop+1]=0x75;
            }
            LoadFeatures("","");Call(c,entry);
            roms.opcodes[stop]=old0;roms.opcodes[stop+1]=old1;
            std::vector<uint32_t> got;
            if(t.kind==3||t.kind==4){
                const uint32_t obj=0xff0000|Ram16(p+0x1d4);Check(obj!=0xff0000,"projectile allocated");
                for(unsigned n=0;n<0xc0;++n)got.push_back(Ram8(obj+n));
            }else{
                for(unsigned field:{6u,8u,10u,12u,0x3cu,0x40u,0x3eu,0x42u})got.push_back(Ram16(p+field));
                for(unsigned field:{0x181u,0x2b5u})got.push_back(Ram8(p+field));
                if(t.kind==5){got.push_back(Ram16(p+(cpu?0xc2:0xa2)));got.push_back(Ram8(p+(cpu?0xc4:0x1e)));}
                else{got.push_back(Ram32(p+0x1a));got.push_back(Ram8(p+0x1e));got.push_back(Ram16(p+0x14));got.push_back(Ram16(p+0x18));}
            }
            Check(Ram8(p+0x38e)==(cpu?0:1)&&Ram16(p+0x58)==16,"boss bridge preserves control/rank");
            if(cpu)reference=got;
            else {if(got!=reference)fprintf(stderr,"motion kind=%d side=%d face=%d strength=%d\n",t.kind,t.side,t.facing,t.strength);Check(got==reference,"native CPU and port boss initialization match");}
        }
    }
    for(int side=0;side<2;++side)for(unsigned entry:{0xa8302u,0xa81aau,0xa849au,0xa863cu,0xa801eu})
    for(int counter=0;counter<=20;++counter)for(int state:{0,10,12,14})for(int marker:{0,1,2}){
        std::fill(m->ram.begin(),m->ram.end(),0);const uint32_t p=first+side*0x400;auto &c=m->cpu;c.Reset(true);c.a[6]=p;c.pc=entry;
        SetRam8(p+3,state);SetRam8(p+0x3bd,marker);SetRam8(p+0x47,counter);SetRam8((p^0xc00)+0x47,14);
        auto before=m->ram;Check(!RunHook(c,entry),"cancel keeps original committed transition");
        const int expected=marker==2&&state==10?std::max(0,counter-4):counter;
        Check(Ram8(p+0x47)==expected,"CPU clamped cancel timing");before[(p+0x47)&0xffff]=expected;
        Check(m->ram==before,"cancel changes only attacker pause");
    }
    for(int player=0;player<2;++player)for(int choice=0;choice<19;++choice)for(int person : {0,1}) {
        std::fill(m->ram.begin(),m->ram.end(),0);const uint32_t p=first+player*0x400;
        SetRam8(p+owner,person);SetRam8(p+0x391,4);SetRam8(p+0x3bd,0);SetRam16(p+0x58,27);
        LoadFeatures("",std::string(player?"select_character_2=":"select_character_1=")+std::to_string(choice));
        ApplyInputLabCharacter(p);
        if(!choice||!person)Check(Ram8(p+0x391)==4&&Ram8(p+0x3bd)==0,"Off/CPU selection unchanged");
        else if(choice<=16)Check(Ram8(p+0x391)==choice-1&&Ram8(p+0x3bd)==0,"normal selection");
        else Check(Ram8(p+0x391)==0&&Ram8(p+0x3bd)==(choice==18?2:1),"Akuma/boss selection");
        Check(Ram16(p+0x58)==27&&Ram8(p+owner)==person,"selection preserves rank/control");
    }
    for(int policy=0;policy<=6;++policy)for(int value : {0,0x4f,0x50,0x60,0x70,0x80,0xfe,0xff}) {
        constexpr uint8_t speeds[]={0x70,0xff,0x80,0x70,0x60,0x50,0};
        SetRam8(0xff8cd3,speeds[policy]);Site site{0xe9944,0,5,false,false,false};
        Cpu(m->cpu,site,0xff8000,value);m->cpu.d[0]=value;m->cpu.cycles+=m->cpu.Step();auto expected=Capture(m->cpu);
        SetRam8(0xff8cd3,0x70);Cpu(m->cpu,site,0xff8000,value);m->cpu.d[0]=value;LoadFeatures("","select_speed="+std::to_string(policy));
        if(!RunHook(m->cpu,site.pc))m->cpu.cycles+=m->cpu.Step();
        Check(Capture(m->cpu)==expected,"native turbo rule operands and flags");Check(Ram8(0xff8cd3)==0x70,"turbo Off needs no restoration");
    }
#endif
    printf("DAMAGE_PORT_PASS %u checks\n",checks);
}
