#pragma once
#include "../../World2MW/src/NeoAI/TrafficPolicy.h"
#include "RaceOpponentPolicy.h"
namespace event_race_policies {
float(__cdecl* originalTraffic)()=nullptr;
unsigned trafficPercent=30;
unsigned originalOpponentTestKey=0; // Explicit isolated-lab fixture only.
const mwel::EventPackage* Current(){
 auto key=MWELNeoCurrentKey();auto clone=registration_experiment::Find(key);
 return clone&&clone->race?clone->package:nullptr;
}
// Start heat is an event option, independent of optional scene resources.
// The two native boundaries have one owner; Carbon receives callbacks rather
// than competing for the same hooks or disabling heat when its assets are absent.
void(__cdecl* originalStage)(bool)=nullptr;void(__cdecl* originalDone)()=nullptr;
bool pendingHeat=false;unsigned heatOwner=0,heatAttempts=0;DWORD nextHeat=0;
static bool SetHeat(float wanted,float& actual){
 unsigned count=0;BYTE** players=nullptr;if(!Copy(reinterpret_cast<void*>(0x92D884),&count,4)||!count||count>8||!Copy(reinterpret_cast<void*>(0x92D87C),&players,4)||!players)return false;
 BYTE* player=nullptr;if(!Copy(players,&player,4)||!player)return false;void** table=nullptr;if(!Copy(player,&table,4)||!table)return false;
 auto sim=reinterpret_cast<BYTE*(__thiscall*)(void*)>(table[1])(player);void* com=nullptr;if(!sim||!Copy(sim+4,&com,4)||!com)return false;
 auto perp=reinterpret_cast<BYTE*(__thiscall*)(void*,unsigned)>(0x5D59F0)(com,0x4037E0);void** vtable=nullptr;
 if(!perp||!Copy(perp,&vtable,4)||!vtable||vtable[1]!=reinterpret_cast<void*>(0x409400))return false;
 reinterpret_cast<void(__cdecl*)(float)>(0x612660)(wanted);
 if(!Copy(perp+0x1C,&actual,4)||!std::isfinite(actual))return false;return std::abs(actual-wanted)<.01f;
}
static bool TrySetHeat(float wanted,float& actual){__try{return SetHeat(wanted,actual);}__except(EXCEPTION_EXECUTE_HANDLER){return false;}}
void HeatFrame(){
 if(!pendingHeat||GetTickCount()-nextHeat>0x7fffffff)return;
 auto package=Current();if(!package||MWELNeoCurrentKey()!=heatOwner){pendingHeat=false;return;}
 auto heat=package->authoring.find("startHeatLevel");if(heat==package->authoring.end()||heat->is_null()){pendingHeat=false;return;}
 float actual=-1,wanted=heat->get<float>();bool applied=TrySetHeat(wanted,actual);
 ++heatAttempts;nextHeat=GetTickCount()+100;
 if(applied||heatAttempts>=16){pendingHeat=false;Log("EVENT_START_HEAT owner=%08X requested=%.1f actual=%.1f applied=%u attempts=%u persistent_override=0",heatOwner,wanted,actual,unsigned(applied),heatAttempts);}
}
void __cdecl Stage(bool value){
 if(value){
  // An in-place retry reuses the event key, race allocation, handles and CP.
  // Clear measured progress and invalidate worker generations at the native
  // staging boundary, before the new grid/countdown can request guidance.
  MWELResetNeoDriverRace();MWELResetNeoProviderRace();MWELCombatNewGeneration();
  Log("EVENT_NATIVE_STAGE_NEW_GENERATION owner=%08X neo_progress_reset=1 worker_generation_invalidated=1",MWELNeoCurrentKey());
 }
 MWELCarbonStageBefore(value);originalStage(value);
 pendingHeat=false;if(value){heatOwner=MWELNeoCurrentKey();heatAttempts=0;nextHeat=GetTickCount();pendingHeat=heatOwner!=0;HeatFrame();}
 MWELCarbonStageAfter(value);
}
void __cdecl Done(){HeatFrame();pendingHeat=false;MWELCarbonDoneBefore();originalDone();}
bool InstallStaging(){
 const BYTE stage[]={0x8B,0x0D,0,0xE0,0x91,0,0x83,0xEC},done[]={0x8B,0x0D,0,0xE0,0x91,0,0x56,0x33};
 const BYTE heat[]={0xA1,0x84,0xD8,0x92,0,0x85,0xC0};
 if(memcmp(reinterpret_cast<void*>(0x60DF70),stage,sizeof(stage))||memcmp(reinterpret_cast<void*>(0x60E160),done,sizeof(done))||memcmp(reinterpret_cast<void*>(0x612660),heat,sizeof(heat)))return false;
 auto a=reinterpret_cast<void*>(0x60DF70),b=reinterpret_cast<void*>(0x60E160);
 if(MH_CreateHook(a,Stage,reinterpret_cast<void**>(&originalStage))!=MH_OK)return false;
 if(MH_CreateHook(b,Done,reinterpret_cast<void**>(&originalDone))!=MH_OK){MH_RemoveHook(a);return false;}
 if(MH_QueueEnableHook(a)!=MH_OK||MH_QueueEnableHook(b)!=MH_OK||MH_ApplyQueued()!=MH_OK){MH_DisableHook(a);MH_DisableHook(b);MH_RemoveHook(a);MH_RemoveHook(b);return false;}
 Log("EVENT_STAGING_BOUNDARIES_ACTIVE heat_without_scene_assets=1 native_countdown_retained=1");return true;
}
float __cdecl Traffic(){
 auto density=originalTraffic();auto owned=Current();
 return mwneo::RaceTrafficDensity(density,owned!=nullptr,trafficPercent);
}
struct RaceCollection {BYTE table[16];RaceCollection* mParent;void* mClass;void* mLayout;unsigned refs,mKey;};
static_assert(offsetof(RaceCollection,mParent)==0x10&&offsetof(RaceCollection,mKey)==0x20);
static unsigned VehicleHash(const char* text){return reinterpret_cast<unsigned(__cdecl*)(const char*)>(0x5CC240)(text);}
static RaceCollection* VehicleCollection(unsigned key){return reinterpret_cast<RaceCollection*(__cdecl*)(unsigned,unsigned)>(0x455FD0)(VehicleHash("pvehicle"),key);}
static bool RaceVehicle(unsigned key){
 __try{auto collection=VehicleCollection(key);return mwel_opponents::RacerDescendant(collection,VehicleHash("racers"),[](auto c){return c->mParent;},[](auto c){return c->mKey;},[](auto c){return c->mClass;});}__except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
static bool InstalledModel(RaceCollection* collection){
 if(!collection||!collection->mLayout)return false;auto model=*reinterpret_cast<const char**>(static_cast<BYTE*>(collection->mLayout)+0x1C);if(!model||strnlen_s(model,32)>=32||!*model)return false;
 return mwcartype::FindPlayable(model)>=0;
}
static unsigned SafeNativeOpponent(unsigned original){
 if(RaceVehicle(original)&&InstalledModel(VehicleCollection(original)))return original;
 // This fallback is needed before a no-save player's native car exists.
 // Only read currently installed metadata; never change receiving banks.
 __try{auto collection=VehicleCollection(original);auto cls=collection?collection->mClass:nullptr;if(!cls)return original;
  unsigned count=reinterpret_cast<unsigned(__thiscall*)(void*)>(0x453FC0)(cls);if(count>8192)return original;
  unsigned key=reinterpret_cast<unsigned(__thiscall*)(void*)>(0x456B00)(cls);for(unsigned i=0;key&&i<count;++i){auto candidate=reinterpret_cast<RaceCollection*(__thiscall*)(void*,unsigned)>(0x455960)(cls,key);if(RaceVehicle(key)&&InstalledModel(candidate)){Log("EVENT_OPPONENT_TRAFFIC_REJECTED original=%08X fallback=%08X live_racer_attributes=1",original,key);return key;}key=reinterpret_cast<unsigned(__thiscall*)(void*,unsigned)>(0x456B20)(cls,key);}
 }__except(EXCEPTION_EXECUTE_HANDLER){Log("EVENT_OPPONENT_FALLBACK_FAULT original=%08X",original);}
 return original;
}
static std::vector<unsigned> usedExtraModels;static unsigned lastExtraSlot=16;
static bool ExtraCharacter(void* info,unsigned& slot){
 auto race=*reinterpret_cast<BYTE**>(0x91E000);if(!race||!info)return false;
 auto distance=uintptr_t(info)-uintptr_t(race+0x18);if(distance%0x194||distance/0x194>=16)return false;slot=unsigned(distance/0x194);
 BYTE* character=nullptr;if(!Copy(static_cast<BYTE*>(info)+4,&character,4)||!character)return false;
 auto key=reinterpret_cast<unsigned(__thiscall*)(void*)>(0x452430)(character+4);auto clone=registration_experiment::Find(key);return clone&&clone->character&&clone->ownerKey==MWELNeoCurrentKey();
}
static unsigned OrdinaryModelUnsafe(unsigned original){
 {auto collection=VehicleCollection(original);auto cls=collection?collection->mClass:nullptr;if(!cls)return 0;
  unsigned count=reinterpret_cast<unsigned(__thiscall*)(void*)>(0x453FC0)(cls);if(count>8192)return 0;std::vector<unsigned> pool;
  unsigned key=reinterpret_cast<unsigned(__thiscall*)(void*)>(0x456B00)(cls);
  const auto randomField=VehicleHash("RandomOpponent");
  for(unsigned i=0;key&&i<count;++i){auto c=reinterpret_cast<RaceCollection*(__thiscall*)(void*,unsigned)>(0x455960)(cls,key);
   if(key!=original&&RaceVehicle(key)&&InstalledModel(c)&&std::find(usedExtraModels.begin(),usedExtraModels.end(),key)==usedExtraModels.end()){
    auto allowed=reinterpret_cast<const BYTE*(__thiscall*)(void*,unsigned,unsigned)>(0x454190)(c,randomField,0);
    if(mwel_opponents::OrdinaryCandidate(true,true,allowed?*allowed:0))pool.push_back(key);
    else Log("EVENT_EXTRA_RACER_POOL_EXCLUDED key=%08X reason=native_random_opponent_disabled",key);
   }
   key=reinterpret_cast<unsigned(__thiscall*)(void*,unsigned)>(0x456B20)(cls,key);
  }
  return pool.empty()?0:pool[(GetTickCount()^unsigned(usedExtraModels.size()*2654435761u))%pool.size()];
 }
}
static unsigned OrdinaryModel(unsigned original){__try{return OrdinaryModelUnsafe(original);}__except(EXCEPTION_EXECUTE_HANDLER){return 0;}}
static bool IsExtraCharacter(void* info,unsigned& slot){__try{return ExtraCharacter(info,slot);}__except(EXCEPTION_EXECUTE_HANDLER){return false;}}
unsigned __cdecl Opponent(unsigned original,void* info){
 auto package=Current();if(!package)return original;
 if(originalOpponentTestKey)return SafeNativeOpponent(originalOpponentTestKey); // Explicit, marker-gated single-model lab field.
 unsigned slot=0;bool extra=IsExtraCharacter(info,slot);
 bool regulation=mwel::Feature(package->authoring,"nativeOpponentAssist");
 if(!extra&&!regulation)return SafeNativeOpponent(original);
 if(extra&&slot<=lastExtraSlot)usedExtraModels.clear();if(extra)lastExtraSlot=slot;
 auto module=GetModuleHandleW(L"NFSMWNativeFreeRoamRacers.asi");
 auto choose=regulation&&module?reinterpret_cast<unsigned(__cdecl*)()>(GetProcAddress(module,"NFR_SelectEventOpponentV1")):nullptr;
 auto selected=choose?choose():0u;if(selected&&(!RaceVehicle(selected)||!InstalledModel(VehicleCollection(selected))))selected=0;
 if(extra){for(unsigned retry=0;retry<12&&selected&&(selected==original||std::find(usedExtraModels.begin(),usedExtraModels.end(),selected)!=usedExtraModels.end());++retry){selected=choose?choose():0;if(selected&&(!RaceVehicle(selected)||!InstalledModel(VehicleCollection(selected))))selected=0;}
  if(!selected||selected==original||std::find(usedExtraModels.begin(),usedExtraModels.end(),selected)!=usedExtraModels.end())selected=OrdinaryModel(original);
  if(selected)usedExtraModels.push_back(selected);Log("EVENT_EXTRA_RACER_MODEL slot=%u original=%08X selected=%08X preset_cleared=1",slot,original,selected);
 }
 Log("EVENT_OPPONENT_SELECTION original=%08X selected=%08X native_spawn=1 owner=EventASI",original,selected);
 return selected?selected:SafeNativeOpponent(original);
}
__declspec(naked) void OpponentBridge(){__asm{
 pushfd
 pushad
 push ebp
 push esi
 call Opponent
 add esp,8
 mov [esp+4],eax
 popad
 popfd
 push 0
 push 0
 push esi
 push 05E8F63h
 ret
}}
#include "CareerOpponentTuning.inl"
bool Install(){
 const BYTE density[]={0xA1,0xC8,0x85,0x98,0x00,0x85,0xC0,0x75,0x13};
 const BYTE caller[]={0xE8,0x5E,0x50,0xFE,0xFF,0xD9,0x54,0x24,0x0C};
 const BYTE opponent[]={0x6A,0x00,0x6A,0x00,0x56,0x8D,0x4C,0x24,0x64,0xE8,0x34,0xBF,0xEF,0xFF};
 const BYTE racer[]={0xC7,0x44,0x24,0x48,0x03,0x00,0x00,0x00};
 auto site=reinterpret_cast<BYTE*>(0x5E8F5E);
 if(memcmp(reinterpret_cast<void*>(0x41EC20),density,sizeof(density))||memcmp(reinterpret_cast<void*>(0x439BBD),caller,sizeof(caller))||memcmp(site,opponent,sizeof(opponent))||memcmp(reinterpret_cast<void*>(0x5E9069),racer,sizeof(racer)))return false;
 trafficPercent=unsigned(std::clamp(int(GetPrivateProfileIntW(L"Runtime",L"RaceTrafficPercent",30,mwel_paths::Settings(game).c_str())),0,100));
 if(GetFileAttributesW((game/L"MWEventLoader.local-test").c_str())!=INVALID_FILE_ATTRIBUTES)originalOpponentTestKey=unsigned(GetPrivateProfileIntW(L"Diagnostics",L"OriginalOpponentTestKey",0,mwel_paths::Settings(game).c_str()));
 if(MH_CreateHook(reinterpret_cast<void*>(0x41EC20),Traffic,reinterpret_cast<void**>(&originalTraffic))!=MH_OK)return false;
 if(MH_EnableHook(reinterpret_cast<void*>(0x41EC20))!=MH_OK){MH_RemoveHook(reinterpret_cast<void*>(0x41EC20));originalTraffic=nullptr;return false;}
 DWORD old=0;if(!VirtualProtect(site,5,PAGE_EXECUTE_READWRITE,&old)){
  MH_DisableHook(reinterpret_cast<void*>(0x41EC20));MH_RemoveHook(reinterpret_cast<void*>(0x41EC20));originalTraffic=nullptr;return false;
 }
 site[0]=0xE9;*reinterpret_cast<unsigned*>(site+1)=unsigned(OpponentBridge)-unsigned(site+5);
 DWORD ignored=0;VirtualProtect(site,5,old,&ignored);FlushInstructionCache(GetCurrentProcess(),site,5);
 if(!InstallCareerTuning())Log("EVENT_CAREER_TUNING_REFUSED hook_surface_mismatch_or_conflict=1");
 Log("EVENT_RACE_POLICIES_ACTIVE owner=EventASI traffic_percent=%u original_events_unchanged=1 opponent_provider=NFR_SelectEventOpponentV1",trafficPercent);return true;
}
}
