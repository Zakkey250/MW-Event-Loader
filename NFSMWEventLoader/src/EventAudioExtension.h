#pragma once
#include "EventAudioPolicy.h"
#include "../../NFSMWNativeFreeRoamRacers/include/AudioExpansionPolicy.h"
namespace event_audio {
unsigned requested=4;
void* originalBudget=nullptr;
unsigned OwnedPreparationKey(){
 // Mirror the game's actual choice of parameters, including the frontend
 // branch before GRaceStatus is constructed. Never assume an event from an INI.
 auto race=runtime_lifecycle::Read<BYTE*>(reinterpret_cast<void*>(0x91E000));
 auto params=race?runtime_lifecycle::Read<BYTE*>(race,0x1968):runtime_lifecycle::Read<BYTE*>(runtime_lifecycle::Read<BYTE*>(reinterpret_cast<void*>(0x91E004)),0x2C);
 auto record=runtime_lifecycle::Read<BYTE*>(params,8);
 auto collection=runtime_lifecycle::Read<registration_experiment::Collection*>(record,4);
 registration_experiment::Collection* seen[16]{};
 for(unsigned i=0;collection&&i<16;++i){for(unsigned j=0;j<i;++j)if(seen[j]==collection)return 0;seen[i]=collection;
  auto key=runtime_lifecycle::Read<unsigned>(collection,0x20);auto clone=registration_experiment::Find(key);if(clone&&clone->race)return key;
  collection=runtime_lifecycle::Read<registration_experiment::Collection*>(collection,0x10);
 }return 0;
}
unsigned __cdecl Select(unsigned nativeCount){
 __try {
  const auto key=OwnedPreparationKey();if(!key||nativeCount<=4)return 0;
  nfsmw_limitbreaker::ResourceLimitsV1 status{};unsigned result=0;
  auto module=GetModuleHandleW(L"NFSMWLimitBreaker.asi");auto query=module?reinterpret_cast<nfsmw_limitbreaker::QueryResourceLimitsV1>(GetProcAddress(module,"NFSMWLB_QueryResourceLimitsV1")):nullptr;
  if(query)result=query(&status,sizeof(status));
  const bool capable=native_freeroam::SupportsSixAudioSlots(result,status);
  const unsigned chosen=mwel_audio::ExpandedCount(nativeCount,requested,true,capable);
  Log("EVENT_NATIVE_AUDIO_PREPARE key=%08X native_source_count=%u requested=%u selected=%u expanded_mixer=%u mixer_bytes=%u preparation_only=1 camera_pause_volume_owner=native",key,nativeCount,requested,chosen?chosen:4,unsigned(capable),status.audioMixerPoolBytes);
  return chosen;
 }__except(EXCEPTION_EXECUTE_HANDLER){return 0;}
}
__declspec(naked) void Budget(){__asm {
 pushfd
 pushad
 mov ebx,esp
 and esp,0FFFFFFF0h
 sub esp,512
 fxsave [esp]
 push dword ptr [ebx+28]
 call Select
 add esp,4
 test eax,eax
 jz native
 mov [ebx+28],eax
 fxrstor [esp]
 mov esp,ebx
 popad
 popfd
 push 04E5CF3h
 ret
native:
 fxrstor [esp]
 mov esp,ebx
 popad
 popfd
 jmp dword ptr [originalBudget]
}}
bool Install(){
 requested=unsigned(std::clamp(int(GetPrivateProfileIntW(L"Audio",L"NativeEngineVoices",4,mwel_paths::Settings(game).c_str())),4,6));
 if(requested==4){Log("EVENT_NATIVE_AUDIO_CONFIG requested=4 native_default=1");return true;}
 // CMP EAX,4 / JLE 4E5CF3, a five-byte instruction boundary. The native
 // <=2 alternate path, budget accumulation and allocator remain intact.
 const BYTE guard[]={0x83,0xF8,0x04,0x7E,0x15};auto site=reinterpret_cast<void*>(0x4E5CD9);
 if(memcmp(site,guard,sizeof(guard))){Log("EVENT_NATIVE_AUDIO_REFUSED preparation_guard");return false;}
 if(MH_CreateHook(site,Budget,&originalBudget)!=MH_OK)return false;
 if(MH_EnableHook(site)!=MH_OK){MH_RemoveHook(site);originalBudget=nullptr;return false;}
 Log("EVENT_NATIVE_AUDIO_CONFIG requested=%u hook=1 fallback=4 required_mixer_bytes=8388608 allocation_changed_at_native_preparation_only=1",requested);return true;
}
}
