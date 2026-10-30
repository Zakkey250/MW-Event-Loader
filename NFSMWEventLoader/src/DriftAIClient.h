#pragma once
#include <windows.h>
#include "DriftAIAPI.h"

namespace mcdai {
// One instance per NeoRuntimeState. Call from its simulation writer only.
// Explicit Reset is mandatory on race exit/restart/retirement; not from DllMain.
class EventLoaderClient {
 std::uint64_t owner_,token_=0;
 std::uint32_t vehicle_=0,handle_=0,mode_=0,race_=0,scope_=Race;
 std::uint64_t generation_=0,nextAttempt_=0;
 HMODULE module_=nullptr;
 GetCapabilitiesV1Fn capabilities_=nullptr;AcquireV1Fn acquire_=nullptr;
 RefreshV1Fn refresh_=nullptr;ReleaseV1Fn release_=nullptr;GetStateV1Fn state_=nullptr;
 SetIntentV1Fn intent_=nullptr;
 SetPathAssistV1Fn pathAssist_=nullptr;
 bool resolve(){
  auto current=GetModuleHandleW(L"MWArcadeDriftAI.asi");
  if(current!=module_){module_=current;token_=0;capabilities_=nullptr;acquire_=nullptr;refresh_=nullptr;release_=nullptr;state_=nullptr;intent_=nullptr;pathAssist_=nullptr;}
  if(!current)return false;
  if(!capabilities_){
#pragma warning(push)
#pragma warning(disable:4191)
   capabilities_=reinterpret_cast<GetCapabilitiesV1Fn>(GetProcAddress(current,"MWArcadeDriftAI_GetCapabilitiesV1"));
   acquire_=reinterpret_cast<AcquireV1Fn>(GetProcAddress(current,"MWArcadeDriftAI_AcquireV1"));
   refresh_=reinterpret_cast<RefreshV1Fn>(GetProcAddress(current,"MWArcadeDriftAI_RefreshV1"));
   release_=reinterpret_cast<ReleaseV1Fn>(GetProcAddress(current,"MWArcadeDriftAI_ReleaseV1"));
   state_=reinterpret_cast<GetStateV1Fn>(GetProcAddress(current,"MWArcadeDriftAI_GetStateV1"));
   intent_=reinterpret_cast<SetIntentV1Fn>(GetProcAddress(current,"MWArcadeDriftAI_SetIntentV1"));
   pathAssist_=reinterpret_cast<SetPathAssistV1Fn>(GetProcAddress(current,"MWArcadeDriftAI_SetPathAssistV1"));
#pragma warning(pop)
  }
  CapabilitiesV1 caps;
  return capabilities_&&acquire_&&refresh_&&release_&&state_&&capabilities_(&caps)==Ok&&caps.ready&&caps.capacity>=16&&caps.modeMask==6;
 }
public:
 explicit EventLoaderClient(std::uint64_t owner=0x4d57454c44524654ull):owner_(owner){}
 EventLoaderClient(const EventLoaderClient&)=delete;
 EventLoaderClient& operator=(const EventLoaderClient&)=delete;
 bool Available(){return resolve();}
 void Reset(){
  if(token_&&GetModuleHandleW(L"MWArcadeDriftAI.asi")==module_&&release_)release_(owner_,token_);
  token_=generation_=nextAttempt_=0;vehicle_=handle_=mode_=race_=0;scope_=Race;
 }
 void Pause(){
  if(!token_||GetModuleHandleW(L"MWArcadeDriftAI.asi")!=module_||!refresh_)return;
  RefreshV1 command;command.owner=owner_;command.token=token_;command.leaseMs=500;command.flags=0;
  if(refresh_(&command)!=Ok){token_=0;nextAttempt_=0;}
 }
 // mode=1 Arcade / 2 Special. Mode 0 or !scopeOwned releases immediately.
 // race is the genuine GRaceStatus pointer, generation is a race-start counter.
 Result Frame(std::uint32_t vehicle,std::uint32_t handle,std::uint32_t mode,
              std::uint32_t race,std::uint64_t generation,bool scopeOwned,bool correctionAllowed,bool localHuman=false,const IntentV1* intent=nullptr,float pathStrength=1.f){
  if(!scopeOwned||!mode){Reset();return Disabled;}
  if(!owner_||!vehicle||!handle||mode>2||!race||!generation){Reset();return BadRequest;}
  const auto scope=localHuman?LocalAutodriveRace:Race;
  if(vehicle_!=vehicle||handle_!=handle||mode_!=mode||race_!=race||generation_!=generation||scope_!=scope){
   Reset();vehicle_=vehicle;handle_=handle;mode_=mode;race_=race;generation_=generation;scope_=scope;
  }
  if(!resolve())return NotReady;
  const auto now=GetTickCount64();
  if(!token_){
   if(now<nextAttempt_)return NotReady;
   BindingV1 binding;binding.vehicle=vehicle;binding.simHandle=handle;binding.mode=mode;binding.owner=owner_;binding.generation=generation;binding.scope=scope;binding.race=race;binding.leaseMs=500;
   auto result=Result(acquire_(&binding,&token_));
   if(result!=Ok){nextAttempt_=now+(result==ProfileError?1000:250);return result;}
  }
  RefreshV1 command;command.owner=owner_;command.token=token_;command.leaseMs=500;command.flags=correctionAllowed?AllowCorrection:0;
  auto result=Result(refresh_(&command));
  if(result==Ok&&intent_){IntentV1 request;if(intent&&correctionAllowed)request=*intent;request.owner=owner_;request.token=token_;result=Result(intent_(&request));}
  if(result==Ok&&pathAssist_&&intent&&intent->active&&correctionAllowed){PathAssistV1 request;request.owner=owner_;request.token=token_;request.strength=pathStrength;result=Result(pathAssist_(&request));}
  if(result!=Ok){token_=0;nextAttempt_=now+250;}return result;
 }
 Result Read(StateV1& out){
  out={};if(!token_||!resolve())return NotOwned;
  auto result=Result(state_(owner_,token_,&out));if(result!=Ok){token_=0;nextAttempt_=0;}return result;
 }
};
}
