#pragma once
#include "RpdUndercoverStrobe.h"
// Transient, exact-car ownership. GlareOn/GlareOff are the native IVehicle
// functions; existing head/brake/turn lights are never changed.
namespace rpd_officer_signals {
unsigned handle=0,car=0,original=0;
bool Surface(unsigned vehicle){
 unsigned table=0,on=0,off=0;
 return combat_health_runtime::Read(vehicle,table)&&table==0x8AA828&&
  combat_health_runtime::Read(table+0xA0,on)&&on==0x669360&&
  combat_health_runtime::Read(table+0xA4,off)&&off==0x669380;
}
void Reset(){
 rpd_undercover_strobe::Set(0);
 if(handle&&car&&gauntlet_runtime::Vehicle(handle)==car&&Surface(car)){
  reinterpret_cast<void(__thiscall*)(unsigned,unsigned)>(0x669380)(car,0x7000);
  if(original)reinterpret_cast<void(__thiscall*)(unsigned,unsigned)>(0x669360)(car,original);
 }
 handle=car=original=0;
}
void Frame(){
 if(!rpd_mission::Current()||rpd_mission::ResultPending()){if(handle)Reset();return;}
 unsigned race=0,live=0;BYTE timer=0;
 if(!combat_health_runtime::Read(0x91E000,race)||!combat_health_runtime::Read(race+0x1978,timer)||!timer||!combat_health_runtime::Read(race+0x18,live)||!live)return;
 const auto vehicle=gauntlet_runtime::Vehicle(live);if(!vehicle||!Surface(vehicle))return;
 // CopCar AI still applies NPC visibility/formation rules to a human driver.
 // Retain an exact-car warning-light lease for the active interceptor too.
 if(live!=handle||vehicle!=car){Reset();handle=live;car=vehicle;unsigned lights=0;
  if(!combat_health_runtime::Read(car+0x9C,lights)){Reset();return;}original=lights&0x7000;
 }
 unsigned mask=0x7000;
 const auto package=event_race_policies::Current();
 const bool undercover=package&&MWELRpdUsesOwnCar(*package);
 rpd_undercover_strobe::Set(undercover?car:0);
 if(undercover)mask=0x3000;
 unsigned lights=0;if(combat_health_runtime::Read(car+0x9C,lights)&&(lights&mask)!=mask)
  reinterpret_cast<void(__thiscall*)(unsigned,unsigned)>(0x669360)(car,mask);
}
}
