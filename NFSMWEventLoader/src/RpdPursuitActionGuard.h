#pragma once

namespace rpd_pursuit_action_guard {
using combat_health_runtime::Read;
using gauntlet_runtime::Table;
void(__thiscall* originalUpdate)(unsigned,float)=nullptr;
ULONGLONG nextLog=0;

bool TargetSim(unsigned target,unsigned& sim){
 return Table(target,0x891100)&&Read(target+0x1C,sim)&&Table(sim,0x8AA940);
}

bool TargetInterfaces(unsigned sim){
 unsigned com=0;
 if(!Read(sim+4,com)||!com)return false;
 const auto query=reinterpret_cast<unsigned(__thiscall*)(unsigned,unsigned)>(0x5D59F0);
 return query(com,0x403700)&&query(com,0x4037E0); // IVehicleAI, IPerpetrator
}

#ifdef MWEL_ARMS_FRONTEND_STIMULUS
enum class TestMode { Off, Guarded, Baseline };
TestMode testMode=TestMode::Off;
unsigned testAction=0;
bool testResumed=false;
void Configure(){
 if(productionRuntime||!worldLogEnabled)return;
 const auto marker=game/L"MWEventLoader.rpd-pursuit-gap-test";
 if(!mwel::fs::is_regular_file(marker))return;
 const auto mode=mwel::Read(marker,128);
 if(mode=="MWEL_RPD_PURSUIT_GAP_GUARDED\n")testMode=TestMode::Guarded;
 if(mode=="MWEL_RPD_PURSUIT_GAP_BASELINE\n")testMode=TestMode::Baseline;
}
#endif

void __fastcall Update(unsigned action,void*,float dt){
 if(!rpd_mission::Current()){originalUpdate(action,dt);return;}
 unsigned ai=0,aiTable=0,steering=0,sim=0,group=0,groupTarget=0;
 if(!Read(action+0x48,ai)||!Read(ai,aiTable))return;
 if(aiTable!=0x892480&&aiTable!=0x891FF8){originalUpdate(action,dt);return;}
 const bool hasGroup=Read(ai+0x70,group)&&Table(group,0x892770)&&
  Read(group+0x74,groupTarget)&&Table(groupTarget,0x891100);
 const bool hasSteering=Read(ai+0x54,steering)&&Table(steering,0x891100);
 bool ready=hasSteering&&TargetSim(steering,sim)&&TargetInterfaces(sim);
#ifdef MWEL_ARMS_FRONTEND_STIMULUS
 unsigned fixtureSim=0;
 if(testMode!=TestMode::Off&&!testAction&&ready&&hasGroup&&steering!=groupTarget&&
    TargetSim(groupTarget,fixtureSim)&&fixtureSim==sim&&combat_health_runtime::Racing()){
  testAction=action;
  // Use the game's position-only AITarget binding to create the same null
  // steering vehicle as the field crash, retaining the real pursuit group.
  combat_impact::Vector position{};
  Read(steering+0x10,position);
  reinterpret_cast<void(__thiscall*)(unsigned,const combat_impact::Vector*)>(0x4238B0)(steering,&position);
  ready=false;
  Log("EXPERIMENT_RPD_PURSUIT_GAP action=%08X mode=%s steering_vehicle=0 group_retained=1",action,
      testMode==TestMode::Baseline?"baseline":"guarded");
  if(testMode==TestMode::Baseline){originalUpdate(action,dt);return;}
 }
#endif
 // AIActionPursuit requires both the private steering vehicle (42051D)
 // and the pursuit group (427186). A valid cached position alone is not
 // sufficient. Rebind only from this officer's existing live group.
 if(!hasGroup||!ready){
  unsigned groupSim=0,reboundSim=0;
  const bool rebound=!ready&&hasSteering&&hasGroup&&steering!=groupTarget&&
   hot_pursuit_runtime::originalTarget&&TargetSim(groupTarget,groupSim)&&TargetInterfaces(groupSim)&&
   !MWELRpdPursuitCompleted(hot_pursuit_runtime::Handle(groupSim+0x80))&&
   (hot_pursuit_runtime::originalTarget(steering,groupSim),TargetSim(steering,reboundSim)&&reboundSim==groupSim);
  if(worldLogEnabled){
   const auto now=GetTickCount64();
   if(now>=nextLog){nextLog=now+1000;Log("RPD_PURSUIT_ACTION_DEFER action=%08X group_ready=%u steering_ready=%u rebound=%u",
     action,unsigned(hasGroup),unsigned(ready),unsigned(rebound));}
  }
#ifdef MWEL_ARMS_FRONTEND_STIMULUS
  if(fixtureSim)Log("EXPERIMENT_RPD_PURSUIT_DEFERRED action=%08X rebound=%u",action,unsigned(rebound));
#endif
  return;
 }
 originalUpdate(action,dt);
#ifdef MWEL_ARMS_FRONTEND_STIMULUS
 if(action==testAction&&!testResumed){testResumed=true;Log("EXPERIMENT_RPD_PURSUIT_RESUMED action=%08X native_update_returned=1",action);}
#endif
}

bool Install(){
 if(originalUpdate)return true;
 constexpr BYTE entry[]={0x83,0xEC,0x60,0xA1,0xB4,0x2F,0x8A,0x00,0x8B,0x15,0xBC,0x2F,0x8A,0x00,0x56,0x8B,0xF1};
 constexpr BYTE lookup[]={0x8B,0x16,0x8B,0xCE,0xFF,0x92,0xB4,0x00,0x00,0x00};
 const auto site=reinterpret_cast<void*>(0x427040);
 if(memcmp(site,entry,sizeof(entry))||memcmp(reinterpret_cast<void*>(0x42051D),lookup,sizeof(lookup))||
    !Table(0x891328,0x427040)||!Table(0x892480+0x64,0x431D20)||!Table(0x891FF8+0x64,0x431D20)||
    !Table(0x892480+0xA0,0x431D70)||!Table(0x891FF8+0xA0,0x431D70))return false;
 if(MH_CreateHook(site,Update,reinterpret_cast<void**>(&originalUpdate))!=MH_OK)return false;
 if(MH_EnableHook(site)==MH_OK)return true;
 MH_RemoveHook(site);originalUpdate=nullptr;return false;
}
}
