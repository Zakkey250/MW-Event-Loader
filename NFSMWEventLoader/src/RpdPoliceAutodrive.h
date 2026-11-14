#pragma once
#include "NativeCsisHandlePolicy.h"
#if defined(MWEL_PRIVATE_CSIS_BASELINE) && !defined(MWEL_ARMS_FRONTEND_STIMULUS)
#error Csis baseline is restricted to a private diagnostic build
#endif
// Real police pursuit membership for the human officer, with native player
// input retained. Only the isolated automatic test replaces InputPlayer.
namespace rpd_police_autodrive {
void* loanTrampoline=nullptr;
unsigned __cdecl DebugPlayerCar(){
 // Native debug startup uses the base template descriptor during player
 // creation. Select the same owned pvehicle as the safehouse's loan preset
 // for this isolated player call, without changing the debug racer field.
 if(race_start_experiment::enabled&&mwel_rpd::Armed(race_start_experiment::BootstrapPackage().authoring)&&
    !mwel_rpd::Undercover(race_start_experiment::BootstrapPackage().authoring)){
  const auto preset=race_start_experiment::BootstrapPackage().authoring.value("policeVehicle",std::string());
  const auto e=rpd_vehicle_catalog::Find(preset);const auto spec=e?rpd_player_vehicle::Prepare(e->source):nullptr;
  if(spec){Log("RPD_LAB_NATIVE_LOAN key=%08X native_debug_player_creation=1",spec->attributes->collection.key);return unsigned(spec->attributes->name.c_str());}
  return 0;
 }
 return *reinterpret_cast<unsigned*>(0x8F86A8);
}
void __declspec(naked) LoanQuery(){__asm{
 pushfd
 pushad
 call DebugPlayerCar
 mov [esp+28],eax
 popad
 popfd
 push 0x6F4899
 ret
}}
bool InstallLabLoan(){
 if(loanTrampoline)return true;
 const BYTE expected[]={0xA1,0xA8,0x86,0x8F,0};auto site=reinterpret_cast<void*>(0x6F4894);
 if(memcmp(site,expected,sizeof(expected))||MH_CreateHook(site,LoanQuery,&loanTrampoline)!=MH_OK)return false;
 if(MH_EnableHook(site)!=MH_OK){MH_RemoveHook(site);loanTrampoline=nullptr;return false;}return true;
}
unsigned handle=0,car=0,epoch=0,manualPrimary=0,manualInput=0;bool attempted=false,inputOverrideOwned=false;ULONGLONG nextLog=0;
bool Manual(){return !race_start_experiment::enabled||race_start_experiment::manualPlayer;}
bool ManualOwned(unsigned primary){
 return manualPrimary&&primary==manualPrimary&&car&&rpd_mission::Current()&&Manual()&&
  epoch==combat_health_runtime::generation&&
  gauntlet_runtime::Vehicle(handle)==car&&gauntlet_runtime::Table(primary,0x892560);
}
bool ManualDriving(unsigned primary){return GetCurrentThreadId()==combat_health_runtime::gameThread&&ManualOwned(primary);}
void* officerGoalTrampoline=nullptr;
unsigned reportedOfficerAction=0;
bool __cdecl KeepManualCopControls(unsigned primary){
 if(!ManualDriving(primary)||!combat_health_runtime::Racing()||rpd_mission::ResultPending())return false;
 // CopCar::Update dispatches the AI action separately from OnDriving. A
 // pursuit action can still request all drive channels and write IInput NOS
 // directly. Leave the officer registered, with native visibility, target
 // tracking and detection still running on either side of this dispatch.
 if(worldLogEnabled){
  unsigned goal=0,action=0,table=0;
  if(gauntlet_runtime::Read(primary+0xB8,goal)&&goal&&gauntlet_runtime::Read(goal+4,action)&&
     action&&gauntlet_runtime::Read(action,table)&&table!=reportedOfficerAction){
   reportedOfficerAction=table;
   Log("RPD_OFFICER_MANUAL_CONTROL handle=%u action=%08X native_goal_tick=0 pursuit_perception=1",handle,table);
  }
 }
 return true;
}
void __declspec(naked) OfficerGoalUpdate(){__asm{
 pushfd
 cmp edi,dword ptr [manualPrimary]
 jne nativeOfficer
 pushad
 mov ebp,esp
 sub esp,528
 and esp,0FFFFFFF0h
 fxsave [esp]
 push edi
 call KeepManualCopControls
 add esp,4
 fxrstor [esp]
 test al,al
 mov esp,ebp
 jnz manualOfficer
 popad
nativeOfficer:
 popfd
 jmp dword ptr [officerGoalTrampoline]
manualOfficer:
 popad
 popfd
 push 00432044h
 ret
}}
void* steeringFallback=nullptr;
bool steeringReported=false;
bool __cdecl UsePlayerSuspension(unsigned suspension){
 // Native already chose its NPC branch. A cached IHumanAI can be present
 // but decline human steering during the officer's behaviour transition.
 unsigned sim=0,h=0;
 if(!manualPrimary||!car||epoch!=combat_health_runtime::generation||
    !gauntlet_runtime::Read(suspension+0x34,sim)||sim!=car-0x80||
    !gauntlet_runtime::Read(sim+8,h)||h!=handle||
    !ManualOwned(manualPrimary)||!combat_health_runtime::Racing()||rpd_mission::ResultPending())return false;
 const auto vehicle=reinterpret_cast<NFSPluginSDK::MW05::IVehicle*>(car);
 if(vehicle->GetDriverClass()!=NFSPluginSDK::MW05::DriverClass::Human||vehicle->IsAnimating()||vehicle->IsStaging())return false;
 if(worldLogEnabled&&!steeringReported){
  steeringReported=true;
  Log("RPD_PLAYER_SUSPENSION handle=%u key=%08X native_human_steering=0069E820 npc_fallback_bypassed=1 input_curve=0",h,vehicle->GetVehicleKey());
 }
 return true;
}
void __declspec(naked) SteeringFallback(){__asm{
 pushfd
 pushad
 mov ebp,esp
 sub esp,528
 and esp,0FFFFFFF0h
 fxsave [esp]
 push esi
 call UsePlayerSuspension
 add esp,4
 fxrstor [esp]
 test al,al
 mov esp,ebp
 jnz player
 popad
 popfd
 jmp dword ptr [steeringFallback]
player:
 popad
 popfd
 push 0069E9FAh
 ret
}}
bool InstallSteering(){
 if(steeringFallback)return true;
 constexpr BYTE decision[]={0x8B,0x8E,0xF4,0,0,0,0x85,0xC9,0x57,0x74,0x17,0x8B,0x01,0xFF,0x50,0x08,0x84,0xC0,0x74,0x0E};
 constexpr BYTE player[]={0x8B,0x7C,0x24,0x2C,0x57,0x8B,0xCE,0xE8,0x1A,0xFE,0xFF,0xFF,0xEB,0x3F};
 constexpr BYTE npc[]={0x8B,0x7C,0x24,0x2C,0xC7,0x86,0x28,0x01,0,0,0,0,0x34,0x42};
 const auto site=reinterpret_cast<void*>(0x69EA08);
 if(memcmp(reinterpret_cast<void*>(0x69E9E6),decision,sizeof(decision))||
    memcmp(reinterpret_cast<void*>(0x69E9FA),player,sizeof(player))||memcmp(site,npc,sizeof(npc))||
    MH_CreateHook(site,SteeringFallback,&steeringFallback)!=MH_OK)return false;
 if(MH_EnableHook(site)==MH_OK)return true;
 MH_RemoveHook(site);steeringFallback=nullptr;return false;
}
void ObserveInput(unsigned input,unsigned channel,float value,unsigned caller){
 if(!worldLogEnabled||!manualInput||input!=manualInput||!ManualOwned(manualPrimary))return;
 static std::array<unsigned,128> seen{};const auto key=caller^(channel<<24)^(value>0?0x80000000u:0u);
 if(std::find(seen.begin(),seen.end(),key)!=seen.end())return;
 if(auto row=std::find(seen.begin(),seen.end(),0u);row!=seen.end()){*row=key;Log("RPD_PLAYER_INPUT_WRITER channel=%u value=%.3f caller=%08X",channel,value,caller);}
}
void(__thiscall* originalForceStopOn)(unsigned,unsigned)=nullptr;
bool(__thiscall* originalResetCar)(unsigned,const void*,const void*,bool)=nullptr;
bool __fastcall ResetCar(unsigned self,void*,const void* position,const void* direction,bool findRoad){
 unsigned v=0;const bool owned=GetCurrentThreadId()==combat_health_runtime::gameThread&&
  ManualOwned(manualPrimary)&&gauntlet_runtime::Read(self+0x48,v)&&v==car;
 if(!owned)return originalResetCar(self,position,direction,findRoad);
 const bool hadLease=rpd_controls::lease.held;rpd_controls::BeginTransition();bool ok=false;
 __try{ok=originalResetCar(self,position,direction,findRoad);}
 __finally{rpd_controls::Restore("native_rollover_reset");if(!hadLease)rpd_controls::Forget();}
 unsigned com=0;if(gauntlet_runtime::Read(v+4,com)&&com)manualInput=reinterpret_cast<unsigned(__thiscall*)(unsigned,unsigned)>(0x5D59F0)(com,0x403F90);
 if(worldLogEnabled)Log("RPD_PLAYER_NATIVE_RESET result=%u pad_binding_lease=1 input=%08X native_pose_and_reset_retained=1",unsigned(ok),manualInput);
 return ok;
}
void __fastcall ForceStopOn(unsigned vehicle,void*,unsigned flags){
 const auto caller=unsigned(_ReturnAddress());originalForceStopOn(vehicle,flags);
 if(worldLogEnabled&&vehicle==car&&Manual())Log("RPD_PLAYER_FORCE_STOP caller=%08X flags=%u handle=%u",caller,flags,handle);
}
bool Boost(unsigned vehicle,float* output){
 if(!vehicle||!output||!std::isfinite(*output)||*output<=0||!rpd_mission::Current()||
    GetCurrentThreadId()!=combat_health_runtime::gameThread||!combat_health_runtime::Racing()||rpd_mission::ResultPending())return false;
 unsigned driver=~0u,race=0,suspect=0;
 // Catch-up is for dispatched allies. Keep human engine output and grip
 // response consistent regardless of how far ahead the suspect is.
 if(!gauntlet_runtime::Table(vehicle,0x8AA828)||!gauntlet_runtime::Read(vehicle+0x94,driver)||driver!=2||
    !gauntlet_runtime::Read(0x91E000,race)||!gauntlet_runtime::Read(race+0x18+0x194,suspect))return false;
 if(MWELRpdHotPursuitScope())suspect=hot_pursuit_runtime::CopTarget(hot_pursuit_runtime::Handle(vehicle));
 const auto enemy=gauntlet_runtime::Vehicle(suspect);combat_impact::Motion police{},racer{};
 if(!enemy||!combat_health_runtime::NativeMotion(vehicle-0x80,police)||!combat_health_runtime::NativeMotion(enemy-0x80,racer))return false;
 const float dx=police.position.x-racer.position.x,dy=police.position.y-racer.position.y,dz=police.position.z-racer.position.z;
 const float distance=std::sqrt(dx*dx+dy*dy+dz*dz);
 const float scale=rpd_interceptor_policy::Catchup(distance),boosted=*output*scale;
 if(!std::isfinite(boosted))return false;
 *output=boosted;
 if(worldLogEnabled){static ULONGLONG next=0;if(GetTickCount64()>=next){next=GetTickCount64()+2000;Log("RPD_POLICE_CATCHUP handle=%u distance=%.2f scale=%.4f engine_output_only=1",hot_pursuit_runtime::Handle(vehicle),distance,scale);}}
 return true;
}
bool NOSWrite(unsigned input,bool pressed,unsigned caller){
 if(!manualInput||input!=manualInput||!ManualOwned(manualPrimary))return true;
 if(worldLogEnabled){
  static std::array<unsigned,48> seen{};const auto key=caller^(pressed?0x80000000u:0);
  if(std::find(seen.begin(),seen.end(),key)==seen.end())if(auto row=std::find(seen.begin(),seen.end(),0u);row!=seen.end()){
   *row=key;Log("RPD_NOS_WRITER caller=%08X pressed=%u thread=%u owner_thread=%u",caller,unsigned(pressed),GetCurrentThreadId(),combat_health_runtime::gameThread);
  }
 }
 // The cop's pursuit actions also write NOS directly, bypassing DoNOS.
 // InputPlayer (006A8CC1) remains the sole native button owner here.
 // Never synthesize a held button or swallow its real release.
 return caller!=0x414F5F&&caller!=0x42771B&&caller!=0x4291AA&&caller!=0x4281A1;
}
void(__thiscall* originalAudioInfo)(unsigned,unsigned)=nullptr;
void*(__thiscall* originalAudioSource)(unsigned,unsigned,unsigned)=nullptr;
void(__thiscall* originalDoNOS)(unsigned)=nullptr;
bool(__thiscall* originalAudioCallback)(unsigned,unsigned,unsigned)=nullptr;
void* heatTrampoline=nullptr;
ULONGLONG nextAudioLog=0;
int(__cdecl* originalCsisValidate)(unsigned,unsigned)=nullptr;
int __cdecl CsisValidate(unsigned reference,unsigned kind){
 // The 00831408 AV identifies this exact native WindWeather definition.
 if(reference!=0x9147D4)return originalCsisValidate(reference,kind);
 reinterpret_cast<void(__cdecl*)()>(0x831029)(); // native recursive bank mutex
 unsigned pointer=0,state=~0u;
 if(gauntlet_runtime::Read(reference,pointer)&&gauntlet_runtime::Read(reference+4,state)&&int(state)>=0){
  auto read=[](unsigned address,void* output,size_t bytes){return Copy(reinterpret_cast<void*>(address),output,bytes);};
  if(mwel_csis_handle::ClassDefinition(pointer,read)==mwel_csis_handle::Membership::Retired){
   // Native invalidation (-3) selects the caller's existing definition
   // lookup/rebind branch. No exception suppression or copied definitions.
   reinterpret_cast<void(__thiscall*)(unsigned)>(0x830EF7)(reference);
   if(worldLogEnabled)Log("RPD_AMBIENT_DEFINITION_RETIRED pointer=%08X generation=%08X native_rebind_required=1",pointer,state);
  }
 }
 const auto result=originalCsisValidate(reference,kind);
 reinterpret_cast<void(__cdecl*)()>(0x83103A)();
 return result;
}
struct AudioContext {unsigned self=0,sim=0,handle=0,voice=0;std::array<BYTE,24> native{};bool attempted=false,switched=false;ULONGLONG nextCallbackLog=0,retryAt=0;};
std::array<AudioContext,16> audioContexts{};unsigned nextAudioContext=0;
void* __fastcall AudioSource(unsigned self,void*,unsigned key,unsigned context){
 unsigned sim=0,driver=~0u;
 if(_ReturnAddress()==reinterpret_cast<void*>(0x6B47A1)&&key==0xA3F44E2E&&
    gauntlet_runtime::Table(self,0x8AB9D4)&&gauntlet_runtime::Table(context,0x8AB320)&&
    gauntlet_runtime::Read(self+0x34,sim)&&sim){
  // This call occurs inside the common SoundTraffic base constructor,
  // before SoundRacer publishes its final vtable. Native SoundCop passes
  // category 2 in this same VehicleSoundContext.
  // SoundRacer's player category never instantiates the siren graph, even
  // when FillAudioInfo contains a valid siren state. Keep its engine owner.
  unsigned prior=0,h=0;gauntlet_runtime::Read(context+0xC,prior);gauntlet_runtime::Read(sim+8,h);gauntlet_runtime::Read(sim+0x80+0x94,driver);
  if(prior<=2&&h){
   // The native allocator may reuse all addresses/handles on the next race.
   // A fresh constructor supersedes the retired object's retained context.
   for(auto& old:audioContexts)if(old.self==self)old={};
   // Pooled patrol constructors must never evict a live auxiliary voice.
   // Its callback and release still depend on this exact ownership row.
   AudioContext* retained=nullptr;
   for(unsigned n=0;n<audioContexts.size();++n){auto& slot=audioContexts[nextAudioContext++%audioContexts.size()];if(!slot.switched&&!slot.voice){retained=&slot;break;}}
   if(retained){auto& row=*retained;row={};row.self=self;row.sim=sim;row.handle=h;Copy(reinterpret_cast<void*>(context),row.native.data(),row.native.size());}
  }
  Log("RPD_AUDIO_CONTEXT self=%08X handle=%u driver=%u category=%u event=%08X native_bytes_retained=24",self,h,driver,prior,MWELNeoCurrentKey());
  Log("RPD_AUDIO_CONTEXT_NATIVE handle=%u model=%08X world=%08X category=%u spooled=%u identity=%u",h,*reinterpret_cast<unsigned*>(context+4),*reinterpret_cast<unsigned*>(context+8),prior,unsigned(*reinterpret_cast<BYTE*>(context+0x10)),*reinterpret_cast<unsigned*>(context+0x14));
 }
 return originalAudioSource(self,key,context);
}
void OfficerAudio(unsigned vehicle,bool restore=false){
 unsigned com=0;if(!gauntlet_runtime::Read(vehicle+4,com)||!com)return;
 const auto iface=reinterpret_cast<unsigned(__thiscall*)(unsigned,unsigned)>(0x5D59F0)(com,0x694080);
 const auto self=iface>=0x4C?iface-0x4C:0;
 if(!gauntlet_runtime::Table(self,0x8AC0FC)&&!gauntlet_runtime::Table(self,0x8AC0B4))return;
 for(auto& row:audioContexts)if(row.self==self&&row.sim==vehicle-0x80&&row.handle==hot_pursuit_runtime::Handle(vehicle)){
  if(!restore&&(row.voice||(row.attempted&&GetTickCount64()<row.retryAt)))return;
  unsigned voice=0;gauntlet_runtime::Read(self+0x5C,voice);
  if(restore){
   const auto auxiliary=row.voice;row.voice=0;row.switched=row.attempted=false;row.retryAt=0;
   if(auxiliary)reinterpret_cast<void(__thiscall*)(unsigned,unsigned)>(0x6EEA10)(self,auxiliary);
   Log("RPD_AUDIO_SOURCE_RELEASE player_source_untouched=1 native_event_lifecycle=1");return;
  }
  if(!voice)return;
  auto context=row.native;if(!restore)*reinterpret_cast<unsigned*>(context.data()+0xC)=2;
  // Keep this vehicle's own model, world and handle. The native unspooled
  // context commits the cop sound assets for this separate siren event.
  // Keep the original player event alive: it owns native simulation-ready
  // completion too. The additional native police event has a separate
  // callback/handle and is released before the vehicle/behaviour is retired.
  const auto replacement=reinterpret_cast<unsigned(__thiscall*)(unsigned,unsigned,const void*)>(0x6F4400)(self,0xA3F44E2E,context.data());
  row.voice=replacement;row.attempted=true;row.retryAt=GetTickCount64()+1000;row.switched=!restore&&replacement;
  Log("RPD_AUDIO_SOURCE voice=%08X restore=%u category=%u native_event_lifecycle=1 player_component_retained=1",replacement,unsigned(restore),*reinterpret_cast<unsigned*>(context.data()+0xC));
  return;
 }
}
bool SirenInfo(unsigned self,unsigned info);
bool __fastcall AudioCallback(unsigned self,void*,unsigned voice,unsigned info){
 for(auto& row:audioContexts)if(row.switched&&row.self==self&&row.voice==voice){
  unsigned paused=1;
  if(!info||!ManualOwned(manualPrimary)||row.sim!=car-0x80||!gauntlet_runtime::Read(0x9885C8,paused)||paused)return false;
  // This auxiliary graph owns the siren only. The primary voice already
  // supplies the engine and all four wheels via SoundRacer::FillAudioInfo.
  if(!SirenInfo(self,info))return false;
  auto packet=reinterpret_cast<BYTE*>(info);
  for(const unsigned offset:{4u,8u,0xCu,0x10u,0x14u,0x18u,0x34u,0x38u,0x3Cu,0x40u,0xDCu,0xE4u})
   *reinterpret_cast<unsigned*>(packet+offset)=0;
  packet[0xE0]=0;
  memset(packet+0x1C,0,4*sizeof(float)); // per-wheel samples at 006945A0
  memset(packet+0x98,0,0x40);           // per-wheel slip pairs and suspension contributions
  memset(packet+0xD8,0,4);             // per-wheel contact flags
  if(worldLogEnabled&&GetTickCount64()>=row.nextCallbackLog){row.nextCallbackLog=GetTickCount64()+1000;
   Log("RPD_AUDIO_AUX_CALLBACK voice=%08X siren=%u formation=%u thread=%u owner_thread=%u wheel_audio=0 engine_audio=0",voice,*reinterpret_cast<unsigned*>(info+0x2C),unsigned(*reinterpret_cast<BYTE*>(info+0x30)),GetCurrentThreadId(),combat_health_runtime::gameThread);
  }
  return true;
 }
 return originalAudioCallback(self,voice,info);
}
void __fastcall DoNOS(unsigned ai,void*){
 // IVehicleAI::DoNOS writes PInput::SetNitrous every tick, independently
 // of OnDriving. It must not cancel the human's held InputPlayer button.
 if(ManualOwned(ai-0x4C))return;
 originalDoNOS(ai);
}
float __cdecl HudHeat(unsigned perp){
 if(rpd_mission::Current()){
  unsigned race=0,player=0;
  if(gauntlet_runtime::Read(0x91E000,race)&&gauntlet_runtime::Read(race+0x18,player))
   return float(gauntlet_runtime::Heat(gauntlet_runtime::Vehicle(player)));
 }
 return reinterpret_cast<float(__thiscall*)(unsigned)>(0x409400)(perp);
}
void __declspec(naked) HudHeatQuery(){__asm{
 push edi
 call HudHeat
 add esp,4
 push 0x6F2113
 ret
}}
bool SirenInfo(unsigned self,unsigned info){
 const bool nativeCop=gauntlet_runtime::Table(self,0x8AC0B4);
 unsigned sim=0,ai=0;
 if(!info||!ManualOwned(manualPrimary)||(!gauntlet_runtime::Table(self,0x8AC0FC)&&!nativeCop)||
    !gauntlet_runtime::Read(self+0x34,sim)||sim!=car-0x80||!gauntlet_runtime::Read(car+0x54,ai)||ai-0x4C!=manualPrimary)return false;
 const auto cop=manualPrimary+0x758;
 if(!gauntlet_runtime::Table(cop,0x8923E8))return false;
 // SoundCop::FillAudioInfo (00694660) supplies these three fields to the
 // same native vehicle audio buffer. Keep the player's SoundRacer engine,
 // lifecycle and mixer, and supply the actual officer's pursuit values.
 const auto nativeSiren=reinterpret_cast<unsigned(__thiscall*)(unsigned)>(0x432400)(cop);
 // Native NPC sirens can go Off as formation/visibility changes, even while
 // the human officer is still chasing. His active mission owns a continuous
 // siren. Preserve native Wail/Yelp/Scream selection and the player engine.
 const auto siren=!rpd_mission::ResultPending()&&nativeSiren>2?0u:nativeSiren;
 unsigned table=0;gauntlet_runtime::Read(cop,table);
 const bool formation=reinterpret_cast<bool(__thiscall*)(unsigned)>(*reinterpret_cast<unsigned*>(table+0x34))(cop);
 const float unseen=reinterpret_cast<float(__thiscall*)(unsigned)>(0x432410)(cop);
 *reinterpret_cast<unsigned*>(info+0x2C)=siren;
 *reinterpret_cast<BYTE*>(info+0x30)=formation?1:0;
 *reinterpret_cast<float*>(info+0xF0)=unseen;
 if(worldLogEnabled&&GetTickCount64()>=nextAudioLog){nextAudioLog=GetTickCount64()+1000;
  unsigned voice=0;gauntlet_runtime::Read(self+0x5C,voice);
  Log("RPD_OFFICER_AUDIO native_player_mixer=1 native_voice=%08X cop=%08X siren_state=%u native_formation=%u unseen=%.3f",voice,cop,siren,unsigned(formation),unseen);
 }
 return true;
}
void __fastcall AudioInfo(unsigned self,void*,unsigned info){
 if(!info)return;
 originalAudioInfo(self,info);
 SirenInfo(self,info);
}
bool InstallAudio(){
 if(originalAudioInfo)return true;
 const BYTE expected[]={0x56,0x57,0x8B,0x7C,0x24,0x0C,0x57,0x8B,0xF1,0xE8,0x52,0xFA,0xFF,0xFF};auto site=reinterpret_cast<void*>(0x694700);
 const BYTE source[]={0x8B,0x44,0x24,8,0x56,0x50,0x8B,0xF1};
 const BYTE nos[]={0x83,0xEC,0x28,0x53,0x56,0x8B,0xF1};
 const BYTE heat[]={0x8B,0x17,0x8B,0xCF,0xFF,0x52,4};
 const BYTE callback[]={0xA1,0xC8,0x85,0x98,0,0x85,0xC0,0x75,0x1F};
 const BYTE baseAudio[]={0x6A,0xFF,0x64,0xA1,0,0,0,0};
 const BYTE forceStop[]={0x8A,0x91,0xA8,0,0,0,0x8A,0x44,0x24,4};
 const BYTE resetCar[]={0x6A,0xFF,0x68,0xAF,0xC9,0x87,0,0x64,0xA1};
 const BYTE goal[]={0x8B,0x8F,0xB8,0,0,0,0x85,0xC9,0x74,0x06,0x8B,0x11,0x56,0xFF,0x52,0x08,0x8B,0xCF,0xE8,0x25,0xD5,0xFF,0xFF};
 const BYTE csis[]={0x8B,0x4C,0x24,4,0x8B,0x41,4,0x85,0xC0,0x7C,0x1B};
 const BYTE bankLock[]={0x6A,0xFF,0xFF,0x35,0xCC,0x29,0x9C,0};
 const BYTE bankUnlock[]={0xFF,0x35,0xCC,0x29,0x9C,0};
 const BYTE invalidate[]={0x8B,0xC1,0xC7,0x40,4,0xFD,0xFF,0xFF,0xFF,0xC3};
 if(memcmp(reinterpret_cast<void*>(0x8313F3),csis,sizeof(csis))||memcmp(reinterpret_cast<void*>(0x831029),bankLock,sizeof(bankLock))||
    memcmp(reinterpret_cast<void*>(0x83103A),bankUnlock,sizeof(bankUnlock))||memcmp(reinterpret_cast<void*>(0x830EF7),invalidate,sizeof(invalidate)))return false;
 if(memcmp(reinterpret_cast<void*>(0x432034),goal,sizeof(goal)))return false;
 if(memcmp(reinterpret_cast<void*>(0x6A45F0),resetCar,sizeof(resetCar)))return false;
 if(memcmp(reinterpret_cast<void*>(0x669490),forceStop,sizeof(forceStop)))return false;
 if(memcmp(reinterpret_cast<void*>(0x694160),baseAudio,sizeof(baseAudio))&&!MWELCarbonOwnsAudioSurface())return false;
 if(memcmp(site,expected,sizeof(expected))||memcmp(reinterpret_cast<void*>(0x6F4400),source,sizeof(source))||memcmp(reinterpret_cast<void*>(0x414DB0),nos,sizeof(nos))||memcmp(reinterpret_cast<void*>(0x6F210C),heat,sizeof(heat))||memcmp(reinterpret_cast<void*>(0x6A4040),callback,sizeof(callback)))return false;
 struct Hook {unsigned site;void* detour;void** original;};
 const Hook hooks[]={
#ifndef MWEL_PRIVATE_CSIS_BASELINE
  {0x8313F3,CsisValidate,reinterpret_cast<void**>(&originalCsisValidate)},
#endif
  {0x694700,AudioInfo,reinterpret_cast<void**>(&originalAudioInfo)},{0x6F4400,AudioSource,reinterpret_cast<void**>(&originalAudioSource)},{0x414DB0,DoNOS,reinterpret_cast<void**>(&originalDoNOS)},{0x6F210C,HudHeatQuery,&heatTrampoline},{0x6A4040,AudioCallback,reinterpret_cast<void**>(&originalAudioCallback)},{0x669490,ForceStopOn,reinterpret_cast<void**>(&originalForceStopOn)},{0x6A45F0,ResetCar,reinterpret_cast<void**>(&originalResetCar)},{0x432034,OfficerGoalUpdate,&officerGoalTrampoline}};
 unsigned made=0;
 auto rollback=[&](){for(unsigned i=0;i<made;++i){MH_DisableHook(reinterpret_cast<void*>(hooks[i].site));MH_RemoveHook(reinterpret_cast<void*>(hooks[i].site));*hooks[i].original=nullptr;}return false;};
 for(const auto& h:hooks){if(MH_CreateHook(reinterpret_cast<void*>(h.site),h.detour,h.original)!=MH_OK)return rollback();++made;}
 for(const auto& h:hooks)if(MH_QueueEnableHook(reinterpret_cast<void*>(h.site))!=MH_OK)return rollback();
 if(MH_ApplyQueued()!=MH_OK)return rollback();return true;
}
unsigned Type(){return reinterpret_cast<unsigned(__cdecl*)(const char*)>(0x4519D0)("BEHAVIOR_MECHANIC_AI");}
unsigned InputType(){return reinterpret_cast<unsigned(__cdecl*)(const char*)>(0x4519D0)("BEHAVIOR_MECHANIC_INPUT");}
unsigned Factory(unsigned name){
 unsigned node=0;gauntlet_runtime::Read(0x92C660,node);
 for(unsigned i=0;node&&i<256;++i){unsigned key=0,fn=0;gauntlet_runtime::Read(node,key);if(key==name){gauntlet_runtime::Read(node+4,fn);return fn;}if(!gauntlet_runtime::Read(node+8,node))break;}return 0;
}
void Audit(unsigned v,const char* phase){
 if(!worldLogEnabled)return;unsigned com=0;gauntlet_runtime::Read(v+4,com);if(!com)return;
 native_runtime_diagnostics::Controls(phase);
 auto query=[&](unsigned key){return reinterpret_cast<unsigned(__thiscall*)(unsigned,unsigned)>(0x5D59F0)(com,key);};
 Log("RPD_POLICE_COMPONENTS phase=%s vehicle=%08X com=%08X suspension=%08X engine=%08X transmission=%08X human_ai=%08X input=%08X",phase,v,com,query(0x404040),query(0x404020),query(0x404010),query(0x403770),query(0x403F90));
}
void Reset(){
 if(car&&gauntlet_runtime::Vehicle(handle)==car&&gauntlet_runtime::Table(car,0x8AA828)){
  OfficerAudio(car,true);
  // Remove the officer from native activity membership while ICop still
  // exists. Restoring Racecar AI first leaves its IVehicle in the pursuit's
  // cop array; native formation selection then dereferences a null ICop at
  // 0041930E during roaming/HUD loading. Use the same native detach path as
  // exclusive pursuit reassignment, before destroying the behaviour.
  unsigned detached=0;
  ++hot_pursuit_runtime::ownershipDepth;
  for(const auto group:hot_pursuit_runtime::NativeGroups()){
   hot_pursuit_runtime::ReleaseGroundSupport(group,car);
   if(gauntlet_runtime::Table(group,0x892770)&&reinterpret_cast<bool(__thiscall*)(unsigned,unsigned)>(0x4338B0)(group,car))++detached;
  }
  --hot_pursuit_runtime::ownershipDepth;
  Log("RPD_OFFICER_DETACHED before_cop_component_restore=1 groups=%u native_remove_vehicle=004338B0",detached);
  reinterpret_cast<void(__thiscall*)(unsigned,unsigned)>(0x683500)(car,Type());
  if(inputOverrideOwned)reinterpret_cast<void(__thiscall*)(unsigned,unsigned)>(0x683500)(car,InputType());
  reinterpret_cast<void(__thiscall*)(unsigned)>(0x688370)(car);
 }
 handle=car=epoch=manualPrimary=manualInput=reportedOfficerAction=0;attempted=inputOverrideOwned=steeringReported=false;nextLog=0;
}
void EndAutodrive(){
 if(!car||
    gauntlet_runtime::Vehicle(handle)!=car||!rpd_mission::Current())return;
 // Disable the debug player's original IHumanAI autopilot immediately before
 // native result creation. Otherwise its post-race Race action is rebuilt
 // without this start-only mission's absent course/IRacer target.
 const bool automatic=!Manual();Reset();
 if(automatic)reinterpret_cast<void(__cdecl*)(bool)>(0x612420)(false);
 Log("RPD_LAB_AUTODRIVE_ENDED before_native_result=1 human_input_restored=1 native_autopilot_disabled=1");
}
void AuditTargets(unsigned suspect){
 if(!worldLogEnabled)return;
 const auto enemy=gauntlet_runtime::Vehicle(suspect);combat_impact::Motion actual;
 if(!enemy||!combat_health_runtime::NativeMotion(enemy-0x80,actual))return;
 std::vector<unsigned> cars;if(!hot_pursuit_runtime::List(cars))return;
 for(const auto v:cars){unsigned driver=~0u,ai=0,object=0,sim=0,target=0,group=0,groupObject=0;BYTE valid=0;
  if(!gauntlet_runtime::Read(v+0x94,driver)||(driver!=0&&driver!=2)||!gauntlet_runtime::Read(v+0x54,ai)||!gauntlet_runtime::Table(ai,0x892480))continue;
  gauntlet_runtime::Read(ai+0x54,object);gauntlet_runtime::Read(ai+0x70,group);gauntlet_runtime::Read(group+0x74,groupObject);
  if(!gauntlet_runtime::Table(object,0x891100))continue;
  gauntlet_runtime::Read(object+0x1C,sim);gauntlet_runtime::Read(sim+8,target);gauntlet_runtime::Read(object+0x2C,valid);
  combat_impact::Vector cached{},groupCached{};combat_health_runtime::Read(object+0x10,cached);
  if(gauntlet_runtime::Table(groupObject,0x891100))combat_health_runtime::Read(groupObject+0x10,groupCached);
  combat_impact::Motion body;if(!combat_health_runtime::NativeMotion(v-0x80,body))continue;
  unsigned goal=0,goalTable=0,action=0,actionTable=0,nav=0,navValid=0,navType=0,pathType=0,flags=0;
  gauntlet_runtime::Read(ai-0x4C+0xB8,goal);gauntlet_runtime::Read(goal,goalTable);gauntlet_runtime::Read(goal+4,action);gauntlet_runtime::Read(action,actionTable);
  gauntlet_runtime::Read(ai+0x24,nav);gauntlet_runtime::Read(nav+0x50,navValid);gauntlet_runtime::Read(nav+0x78,navType);gauntlet_runtime::Read(nav+0x7C,pathType);gauntlet_runtime::Read(ai+0x80,flags);
  Log("RPD_CHASE_ACTION_AUDIT cop=%u goal=%08X action=%08X nav_valid=%u nav_type=%u path_type=%u drive_flags=%X read_only=1",hot_pursuit_runtime::Handle(v),goalTable,actionTable,navValid,navType,pathType,flags);
  Log("RPD_CHASE_TARGET_AUDIT cop=%u driver=%u assigned=%u steering=%u valid=%u cop_pos=%.3f,%.3f,%.3f steering_pos=%.3f,%.3f,%.3f group_pos=%.3f,%.3f,%.3f suspect_pos=%.3f,%.3f,%.3f read_only=1",hot_pursuit_runtime::Handle(v),driver,hot_pursuit_runtime::TargetHandle(group),target,unsigned(valid),body.position.x,body.position.y,body.position.z,cached.x,cached.y,cached.z,groupCached.x,groupCached.y,groupCached.z,actual.position.x,actual.position.y,actual.position.z);
 }
}
void Frame(){
 if(!rpd_mission::Current()){
  if(car)Reset();return;
 }
 // Result callbacks leave the current physical simulation alive. Restoring
 // Racecar AI here selects its course action after the start-only pursuit
 // was removed, dereferencing a missing IRacer at 00428A31. Keep the owned
 // cop/input overrides until the existing race-exit/staging reset boundary.
 if(rpd_mission::ResultPending())return;
 // SetActive(false) starts native retry before the old timer is cleared.
 // Do not recreate the just-removed player Cop AI in that transition. Its
 // next acquisition belongs to the new race's active GO state only.
 if(!combat_health_runtime::Racing())return;
 unsigned race=0,h=0,suspect=0;BYTE timer=0;
 if(!MWELNeoDriverInstalled()||!gauntlet_runtime::Read(0x91E000,race)||!gauntlet_runtime::Read(race+0x1978,timer)||!timer||
    !gauntlet_runtime::Read(race+0x18,h)||!gauntlet_runtime::Read(race+0x18+0x194,suspect))return;
 if(handle!=h||epoch!=combat_health_runtime::generation){Reset();handle=h;epoch=combat_health_runtime::generation;}
 if(MWELRpdHotPursuitScope())suspect=MWELRpdSprintTarget(h);
 const auto v=gauntlet_runtime::Vehicle(h),enemy=gauntlet_runtime::Vehicle(suspect);unsigned ai=0,driver=~0u;
 if(!v||!enemy||!gauntlet_runtime::Read(v+0x94,driver)||driver!=0)return;
 if(!attempted){
  const auto name=reinterpret_cast<unsigned(__cdecl*)(const char*)>(0x5CC240)("AIVehicleCopCar");
  const auto fn=Factory(name);
  attempted=true;if(fn!=0x43D2C0){Log("RPD_POLICE_AUTODRIVE_REFUSED factory=%08X",fn);return;}
  car=v;
  Audit(car,"before_native_overrides");
  unsigned com=0;gauntlet_runtime::Read(car+4,com);
  manualInput=Manual()&&com?reinterpret_cast<unsigned(__thiscall*)(unsigned,unsigned)>(0x5D59F0)(com,0x403F90):0;
  reinterpret_cast<void(__thiscall*)(unsigned,unsigned,unsigned)>(0x6857A0)(car,Type(),name);
  if(Manual()&&gauntlet_runtime::Read(car+0x54,ai)&&gauntlet_runtime::Table(ai,0x892480))manualPrimary=ai-0x4C;
  // InputNIS is the native PInput-only implementation (factory 006A8E80).
  // It accepts native PID controls without InputPlayer overwriting them.
  if(!Manual()){
   const auto input=reinterpret_cast<unsigned(__cdecl*)(const char*)>(0x5CC240)("InputNIS");
   reinterpret_cast<void(__thiscall*)(unsigned,unsigned,unsigned)>(0x6857A0)(car,InputType(),input);inputOverrideOwned=true;
  }
  reinterpret_cast<void(__thiscall*)(unsigned)>(0x688370)(car);
  rpd_controls::Restore("player_behaviors_ready");rpd_controls::Forget();
  Audit(car,"after_native_overrides");
 }
 if(!gauntlet_runtime::Read(v+0x54,ai)||!gauntlet_runtime::Table(ai,0x892480))return;
 if(Manual())manualPrimary=ai-0x4C;
 if(Manual())OfficerAudio(v);
 unsigned group=0;gauntlet_runtime::Read(ai+0x70,group);
 if(hot_pursuit_runtime::TargetHandle(group)!=suspect){
  const auto desired=hot_pursuit_runtime::FindPursuit(suspect);
  if(desired){
   // The native police action requires a real pursuit membership. Quota
   // selection separately excludes the human officer from disposable allies.
   if(MWELRpdHotPursuitScope()&&hot_pursuit_runtime::TargetHandle(group))reinterpret_cast<bool(__thiscall*)(unsigned,unsigned)>(0x4338B0)(group,v);
   reinterpret_cast<bool(__thiscall*)(unsigned,unsigned)>(0x4338A0)(desired,v);
   unsigned target=0;gauntlet_runtime::Read(desired+0x74,target);
   unsigned simCom=0;gauntlet_runtime::Read(v-0x80+4,simCom);
   const auto cop=simCom?reinterpret_cast<unsigned(__thiscall*)(unsigned,unsigned)>(0x5D59F0)(simCom,0x4038E0):0;
   if(gauntlet_runtime::Table(cop,0x8923E8)&&target)
    reinterpret_cast<void(__thiscall*)(unsigned,unsigned,unsigned)>(0x42AB80)(cop,target,enemy-0x80);
  }
 }
 if(worldLogEnabled&&GetTickCount64()>=nextLog){nextLog=GetTickCount64()+500;
  AuditTargets(suspect);
  unsigned primary=0,target=0;gauntlet_runtime::Read(ai-0x4C,primary);gauntlet_runtime::Read(ai+0x70,group);target=hot_pursuit_runtime::TargetHandle(group);
  unsigned com=0;gauntlet_runtime::Read(v+4,com);
  const auto cop=com?reinterpret_cast<unsigned(__thiscall*)(unsigned,unsigned)>(0x5D59F0)(com,0x4038E0):0;
  const auto input=com?reinterpret_cast<unsigned(__thiscall*)(unsigned,unsigned)>(0x5D59F0)(com,0x403F90):0;
  unsigned siren=0;float unseen=0;if(gauntlet_runtime::Table(cop,0x8923E8)){gauntlet_runtime::Read(cop+0xC,siren);gauntlet_runtime::Read(cop+0x40,unseen);}
  Log("RPD_OFFICER_ROLE player=%u cop_interface=%08X primary=%08X pursuit=%08X suspect=%u manual=%u input_preserved=%u siren=%u time_since_seen=%.3f",h,cop,primary,group,target,unsigned(Manual()),unsigned(Manual()&&input&&input==manualInput),siren,unseen);
  BYTE stop=0;unsigned inputTable=0,getControls=0,controls=0;float gas=0,brake=0,handbrake=0;
  gauntlet_runtime::Read(v+0xA8,stop);
  if(gauntlet_runtime::Read(input,inputTable)&&gauntlet_runtime::Read(inputTable+8,getControls)&&getControls>=0x401000&&getControls<0x889000){
   controls=reinterpret_cast<unsigned(__thiscall*)(unsigned)>(getControls)(input);
   gauntlet_runtime::Read(controls+0x14,gas);gauntlet_runtime::Read(controls+0x18,brake);gauntlet_runtime::Read(controls+0x1C,handbrake);
  }
  Log("RPD_PLAYER_INPUT_STATE player=%u input=%08X controls=%08X gas=%.3f brake=%.3f handbrake=%.3f force_stop=%u",h,input,controls,gas,brake,handbrake,unsigned(stop));
  unsigned reset=0;float flipped=0;NFSPluginSDK::MW05::UMath::Vector3 up{};
  if(com)reset=reinterpret_cast<unsigned(__thiscall*)(unsigned,unsigned)>(0x5D59F0)(com,0x620250);
  if(gauntlet_runtime::Table(reset,0x8ABA10))gauntlet_runtime::Read(reset-0x4C+0x54,flipped);
  auto body=reinterpret_cast<NFSPluginSDK::MW05::ISimable*>(v-0x80)->GetRigidBody();if(body)body->GetUpVector(up);
  Log("RPD_PLAYER_RESET_STATE up_y=%.3f flipped_seconds=%.3f reset=%08X",reinterpret_cast<float*>(&up)[1],flipped,reset);
  combat_impact::Motion m;if(combat_health_runtime::NativeMotion(v-0x80,m))
   Log("RPD_POLICE_AUTODRIVE player=%u driver=%u primary=%08X pursuit=%08X suspect=%u actual_target=%u pos=%.3f,%.3f,%.3f mps=%.3f native_inputs=1 pose_writes=0",h,driver,primary,group,suspect,target,m.position.x,m.position.y,m.position.z,std::hypot(m.velocity.x,m.velocity.z));
 }
}
}
