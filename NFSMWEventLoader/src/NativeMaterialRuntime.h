#pragma once
#include "NativeMaterialAdapters.h"
#include "NativeSceneMaterial.h"
namespace event_material_guard {
void* blendOriginal=nullptr;void* tailOriginal=nullptr;bool installed=false;
bool WorldGuard(unsigned site,bool tail){
 BYTE branch[5]{};if(!Copy(reinterpret_cast<void*>(site),branch,5)||branch[0]!=0xE9)return false;
 int displacement=0;memcpy(&displacement,branch+1,4);auto target=site+5+displacement;
 MEMORY_BASIC_INFORMATION region{};const auto module=GetModuleHandleW(L"World2MWNativeLoader.asi");
 if(!module||!VirtualQuery(reinterpret_cast<void*>(target),&region,sizeof(region))||region.AllocationBase!=module)return false;
 const BYTE blend[]={0xB8,0x80,0x2C,0x98,0,0x8B,0,0x85,0xC0};
 const BYTE end[]={0xB9,0x80,0x2C,0x98,0,0x8B,0x09,0x85,0xC9};BYTE code[9]{};
 return Copy(reinterpret_cast<void*>(target),code,9)&&!memcmp(code,tail?end:blend,9);
}
bool Install(){
 if(installed)return true;
 // World2MW already owns these native material/NIS scopes. It can initialize
 // after EventLoader, so do not race its guarded installation.
 if(GetModuleHandleW(L"World2MWNativeLoader.asi")){
  Log("EVENT_NATIVE_MATERIAL_GUARD delegated=World2MW duplicate_hook=0");return true;
 }
 if(!GetModuleHandleW(L"SunSet.asi"))return true;
 if(WorldGuard(0x6C6A2D,false)&&WorldGuard(0x6C6A5B,true)){
  installed=true;Log("EVENT_NATIVE_MATERIAL_GUARD owner=World2MW existing_verified_null_guard=1 duplicate_hook=0");return true;
 }
 const BYTE blend[]={0xA1,0x80,0x2C,0x98,0,0x39,0x78,0x04};
 const BYTE tail[]={0x8B,0x0D,0x80,0x2C,0x98,0,0x8B,0x41,0x44};BYTE code[9]{};
 if(!Copy(reinterpret_cast<void*>(0x6C6A2D),code,sizeof(blend))||memcmp(code,blend,sizeof(blend))||
    !Copy(reinterpret_cast<void*>(0x6C6A5B),code,sizeof(tail))||memcmp(code,tail,sizeof(tail))){
  Log("EVENT_NATIVE_MATERIAL_GUARD_UNAVAILABLE unknown_hook_surface=1 native_chain_unchanged=1");return false;
 }
 void* sites[]={reinterpret_cast<void*>(0x6C6A2D),reinterpret_cast<void*>(0x6C6A5B)};
 void* hooks[]={reinterpret_cast<void*>(Blend),reinterpret_cast<void*>(Tail)};void** outputs[]={&blendOriginal,&tailOriginal};
 unsigned made=0;bool ok=true;
 for(unsigned i=0;i<2;++i){if(MH_CreateHook(sites[i],hooks[i],outputs[i])!=MH_OK){ok=false;break;}++made;}
 if(ok)for(auto site:sites)if(MH_QueueEnableHook(site)!=MH_OK){ok=false;break;}
 if(ok)ok=MH_ApplyQueued()==MH_OK;
 if(!ok){for(unsigned i=0;i<made;++i){MH_DisableHook(sites[i]);MH_RemoveHook(sites[i]);}blendOriginal=tailOriginal=nullptr;return false;}
 installed=true;InstallScene();Log("EVENT_NATIVE_MATERIAL_GUARD owner=EventLoader fixed_function_preserved=1 world_dependency=0");return true;
}
}
