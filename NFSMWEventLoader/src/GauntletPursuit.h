#pragma once
#include "GauntletCombatPolicy.h"
namespace gauntlet_pursuit {
void* cooldownTrampoline=nullptr;void* meterTrampoline=nullptr;
bool __cdecl Owned(unsigned primary){
 unsigned manager=0,pursuit=0;
 if(hot_pursuit_runtime::Active()&&hot_pursuit_runtime::owner&&
    hot_pursuit_runtime::owner==MWELNeoCurrentKey()&&combat_health_runtime::Racing()&&
    gauntlet_runtime::Table(primary+0x40,0x892770)){
  const auto candidate=primary+0x40;
  const auto target=hot_pursuit_runtime::TargetHandle(candidate);BYTE busted=0;
  // Each real racer owns a native pursuit, not just the player's singleton.
  // Only a live event participant keeps the 95% escape cap; native arrest,
  // finish and teardown retain their ordinary transition paths.
  return hot_pursuit_runtime::ReadyTarget(target)&&
   hot_pursuit_runtime::FindPursuit(target)==candidate&&
   gauntlet_runtime::Read(candidate+0xE8,busted)&&!busted;
 }
 return gauntlet_runtime::Duel()&&gauntlet_runtime::Table(primary+0x40,0x892770)&&
  gauntlet_runtime::Read(0x993CC8,manager)&&gauntlet_runtime::Read(manager+0x130,pursuit)&&pursuit==primary+0x40;
}
void __cdecl Clamp(unsigned primary){
 if(!Owned(primary))return;
 auto meter=reinterpret_cast<float*>(primary+0x150);
 *meter=gauntlet_combat::EvadeMeter(*meter);
}
void __declspec(naked) CooldownGate(){
 __asm {
  pushfd
  pushad
  push esi
  call Owned
  add esp,4
  test al,al
  jz nativeGate
  popad
  popfd
  push 0x444A1F
  ret
 nativeGate:
  popad
  popfd
  jnz ordinaryMeter
  push 0x4448F5
  ret
 ordinaryMeter:
  push 0x444A1F
  ret
 }
}
void __declspec(naked) MeterStore(){
 __asm {
  fstp dword ptr [esi+0x150]
  pushfd
  pushad
  push esi
  call Clamp
  add esp,4
  popad
  popfd
  push 0x444C01
  ret
 }
}
bool Install(){
 if(cooldownTrampoline&&meterTrampoline)return true;
 const BYTE cooldown[]={0x0F,0x85,0x2A,0x01,0x00,0x00},meter[]={0xD9,0x9E,0x50,0x01,0x00,0x00};
 void* sites[]={reinterpret_cast<void*>(0x4448EF),reinterpret_cast<void*>(0x444BFB)};
 if(memcmp(sites[0],cooldown,sizeof(cooldown))||memcmp(sites[1],meter,sizeof(meter)))return false;
 void* detours[]={reinterpret_cast<void*>(CooldownGate),reinterpret_cast<void*>(MeterStore)};
 void** outputs[]={&cooldownTrampoline,&meterTrampoline};unsigned made=0;bool ok=true;
 for(unsigned i=0;i<2;++i){if(MH_CreateHook(sites[i],detours[i],outputs[i])!=MH_OK){ok=false;break;}++made;}
 if(ok)for(unsigned i=0;i<2;++i)if(MH_EnableHook(sites[i])!=MH_OK){ok=false;break;}
 if(!ok){for(unsigned i=0;i<made;++i){MH_DisableHook(sites[i]);MH_RemoveHook(sites[i]);}cooldownTrampoline=meterTrampoline=nullptr;}
 return ok;
}
unsigned tacticOwner=0,tacticHandle=0;ULONGLONG brakeUntil=0,brakeReady=0;
void Ram(unsigned ai,const combat_impact::Motion& player,const combat_impact::Motion& cop){
 const auto now=GetTickCount64();
 if(tacticOwner!=gauntlet_runtime::owner||tacticHandle!=gauntlet_runtime::opponentHandle){
  tacticOwner=gauntlet_runtime::owner;tacticHandle=gauntlet_runtime::opponentHandle;brakeUntil=brakeReady=0;
 }
 const auto front=gauntlet_combat::FrontBlock(player,cop);
 if(worldLogEnabled){static ULONGLONG next=0;if(GetTickCount64()>=next){next=GetTickCount64()+3000;
  auto delta=player.position-cop.position;Log("GAUNTLET_RAM_GEOMETRY gap=%.2f vertical=%.2f forward=%.2f heading=%.2f speed=%.2f eligible=%u",std::hypot(delta.x,delta.z),delta.y,combat_impact::Dot(delta,cop.forward),combat_impact::Dot(player.forward,cop.forward),std::hypot(cop.velocity.x,cop.velocity.z),unsigned(gauntlet_combat::Ram(player,cop)));
 }}
 static const unsigned ram=reinterpret_cast<unsigned(__cdecl*)(const char*)>(0x5CC240)("AIGoalRam"),headon=reinterpret_cast<unsigned(__cdecl*)(const char*)>(0x5CC240)("AIGoalHeadOnRam"),
  stop=reinterpret_cast<unsigned(__cdecl*)(const char*)>(0x5CC240)("AIGoalStopShort"),pit=reinterpret_cast<unsigned(__cdecl*)(const char*)>(0x5CC240)("AIGoalPit"),pursue=reinterpret_cast<unsigned(__cdecl*)(const char*)>(0x5CC240)("AIGoalPursuit");
 unsigned goal=0;const char* tactic="ram";
 if(brakeUntil){
  if(now<brakeUntil&&front==gauntlet_combat::Front::Brake&&std::hypot(cop.velocity.x,cop.velocity.z)>4)return;
  brakeUntil=0;brakeReady=now+1800;goal=pursue;tactic="resume_native_pursuit";
 }else if(front==gauntlet_combat::Front::Brake){
  if(now<brakeReady)return;goal=stop;tactic="front_brake_check";
 }else if(front==gauntlet_combat::Front::Pit){goal=pit;tactic="front_offset_pit";
 }else if(gauntlet_combat::Ram(player,cop)){goal=combat_impact::Dot(player.forward,cop.forward)<0?headon:ram;
 }else return;
 unsigned current=0;gauntlet_runtime::Read(ai+0x78,current);if(current==goal){
  // A native stop-short already in progress still gets a bounded release.
  if(goal==stop&&!brakeUntil)brakeUntil=now+650;
  if(worldLogEnabled){static ULONGLONG next=0;if(GetTickCount64()>=next){next=GetTickCount64()+5000;Log("GAUNTLET_RAM_ACTIVE goal=%08X native_driving=1",goal);}}return;
 }
 unsigned node=0;gauntlet_runtime::Read(0x90D8E8,node);bool registered=false;
 for(unsigned i=0;node&&i<256;++i){unsigned key=0;gauntlet_runtime::Read(node,key);if(key==goal){registered=true;break;}if(!gauntlet_runtime::Read(node+8,node))break;}
 if(!registered){Log("GAUNTLET_RAM_REFUSED goal=%08X native_factory_missing=1",goal);return;}
 const BYTE expected[]={0x6A,0xFF,0x64,0xA1};
 if(memcmp(reinterpret_cast<void*>(0x422480),expected,sizeof(expected)))return;
 // The native setter owns goal teardown/factory construction. Retain native
 // steering, avoidance, braking and collision; never write control inputs.
 reinterpret_cast<void(__thiscall*)(unsigned,const unsigned&)>(0x422480)(ai-0x4C,goal);
 if(goal==stop)brakeUntil=now+650;
 Log("GAUNTLET_RAM_REQUEST ai=%08X goal=%08X tactic=%s native_goal_factory=1",ai,goal,tactic);
}
}
bool MWELInstallGauntletPursuit(){return gauntlet_pursuit::Install();}
void MWELGauntletRam(unsigned ai,const combat_impact::Motion& player,const combat_impact::Motion& cop){gauntlet_pursuit::Ram(ai,player,cop);}
