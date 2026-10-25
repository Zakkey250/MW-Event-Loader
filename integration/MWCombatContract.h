#pragma once
#include <cstdint>
#include <cstring>
// Authored-module ABI. Never expose STL objects, borrowed game pointers or saves.
namespace mwcombat {
constexpr std::uint32_t Version=1;
enum class Mode:std::uint32_t {None=0,ArmsRace=1,Gauntlet=2,HotPursuit=3,Drift=4,Invalid=0xffffffffu};
inline Mode Parse(const char* name){
 if(!name||!std::strcmp(name,"none"))return Mode::None;
 if(!std::strcmp(name,"armsrace"))return Mode::ArmsRace;
 if(!std::strcmp(name,"gauntlet"))return Mode::Gauntlet;
 if(!std::strcmp(name,"hotpursuit"))return Mode::HotPursuit;
 if(!std::strcmp(name,"drift"))return Mode::Drift;
 return Mode::Invalid;
}
inline bool Weapons(Mode m){return m==Mode::ArmsRace||m==Mode::Gauntlet||m==Mode::HotPursuit;}
inline bool HudRacer(unsigned driver){return driver==0||driver==3;}
inline bool Compatible(Mode m,const char* base,unsigned count){
 if(m==Mode::None)return true;
 if(m==Mode::Invalid||count<1||count>16)return false;
 if(m==Mode::HotPursuit&&count>8)return false;
 // Police are pursuit actors, not entrants. Read legacy two-slot packages too.
 if(m==Mode::Gauntlet)return (count==1||count==2)&&!std::strcmp(base,"tollboothrace");
 if(m==Mode::Drift)return !std::strcmp(base,"sprint")||!std::strcmp(base,"circuit");
 if(m==Mode::ArmsRace)return !std::strcmp(base,"sprint")||!std::strcmp(base,"speedtraprace")||!std::strcmp(base,"circuit")||!std::strcmp(base,"lapknockout");
 return !std::strcmp(base,"sprint")||!std::strcmp(base,"speedtraprace");
}
constexpr unsigned Bit(Mode m){return m==Mode::ArmsRace?1u:m==Mode::Gauntlet?2u:m==Mode::HotPursuit?4u:m==Mode::Drift?8u:0u;}
struct EventV1 {
 std::uint32_t size=sizeof(EventV1),version=Version,eventKey=0,epoch=0;
 Mode mode=Mode::None;std::uint32_t baseMode=0,racerCount=0,flags=0;
};
static_assert(sizeof(EventV1)==32,"combat ABI must remain stable on x86/x64");
using QueryEvent=unsigned(__cdecl*)(std::uint32_t status,std::uint32_t params,EventV1*,unsigned size);
using Contract=unsigned(__cdecl*)(unsigned version);
// Optional event-local ammunition. Omitted mask bits retain ArmsAssist INI
// allocation. Named counts are independent of the player's equipped slots.
struct AmmoV1 {
 std::uint32_t size=sizeof(AmmoV1),version=Version,eventKey=0,epoch=0,mask=0;
 std::uint32_t counts[5]{}; // EMP, Turbo, Spike, Shockwave, Jammer
};
static_assert(sizeof(AmmoV1)==40,"event ammo ABI layout");
constexpr unsigned AmmoLimit=510;
using QueryAmmo=unsigned(__cdecl*)(unsigned status,unsigned params,AmmoV1*,unsigned size);
struct HealthV1 {
 std::uint32_t size=sizeof(HealthV1),version=Version,eventKey=0,epoch=0,handle=0,slot=0;
 float remaining=0,maximum=0;std::uint32_t flags=0,driver=0;
};
static_assert(sizeof(HealthV1)==40,"health ABI layout");
using ObserveHealth=unsigned(__cdecl*)(unsigned slot,unsigned handle,unsigned damage,unsigned driver,HealthV1*,unsigned size);
using QueryHealth=unsigned(__cdecl*)(unsigned slot,unsigned handle,HealthV1*,unsigned size);
using EffectHealth=unsigned(__cdecl*)(unsigned damage,unsigned handle,unsigned kind,unsigned phase);
struct RacerHudV1 {HealthV1 health;float position[3]{};std::uint32_t reserved=0;};
static_assert(sizeof(RacerHudV1)==56,"racer HUD ABI layout");
using DrawHud=unsigned(__cdecl*)(void* device,const RacerHudV1*,unsigned count,unsigned visible);
// Optional event notification, copied immediately by ArmsAssist. No borrowed
// names or native pointers cross module boundaries. Receiver resolves racers.
struct NoticeV1 {
 std::uint32_t size=sizeof(NoticeV1),version=Version,eventKey=0,epoch=0;
 std::uint32_t kind=0,actor=0,target=0,reserved=0; // kind 1: crashed, 2: busted, 3: Cross regen (reserved=15)
};
static_assert(sizeof(NoticeV1)==32,"combat notice ABI layout");
using SubmitNotice=unsigned(__cdecl*)(const NoticeV1*,unsigned size);
inline unsigned PoliceBudget(unsigned racers){return racers>=1&&racers<=16?racers*2:0;}
inline float RewardFraction(unsigned place,unsigned participants){
 if(place<1||place>participants||participants<1||participants>16)return 0;
 return participants==1?1.f:1.f-.9f*float(place-1)/float(participants-1);
}
}
