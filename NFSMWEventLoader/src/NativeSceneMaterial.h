#pragma once
#include <d3dx9effect.h>
namespace event_material_guard {
#include "../../World2MW/src/NativeLoader/RenderTransformState.h"
void (__cdecl* sceneOriginal)(void*,unsigned)=nullptr;
void InvalidateSceneCaches(){
 *reinterpret_cast<void**>(0x988550)=nullptr;
 *reinterpret_cast<void**>(0x988554)=nullptr;
 *reinterpret_cast<void**>(0x982CC8)=nullptr;
}
// Same native prelit scope as World2MW's independent NIS renderer. SunSet
// binds queued models, but native immediate NIS geometry needs its own pass.
void __cdecl Scene(void* view,unsigned flags){
 auto& current=*reinterpret_cast<BYTE**>(0x982C80);
 if(current){sceneOriginal(view,flags);return;}
 auto manager=*reinterpret_cast<void**>(0x9885C8);if(!manager)return;
 auto scene=reinterpret_cast<void*(__thiscall*)(void*)>((*reinterpret_cast<void***>(manager))[19])(manager);
 if(!scene||*reinterpret_cast<void**>(static_cast<BYTE*>(scene)+0x18)==static_cast<BYTE*>(scene)+0x14)return;
 auto effect=reinterpret_cast<BYTE**>(0x93DE78)[17];
 auto dev=*reinterpret_cast<IDirect3DDevice9**>(0x982BDC);
 if(!effect||!dev||*reinterpret_cast<unsigned*>(effect+4)!=17)return;
 auto fx=*reinterpret_cast<ID3DXEffect**>(effect+0x48);
 auto decl=*reinterpret_cast<IDirect3DVertexDeclaration9**>(effect+0x4C);
 auto technique=*reinterpret_cast<D3DXHANDLE*>(effect+0x10);
 if(!fx||!decl||!technique)return;
 IDirect3DStateBlock9* state=nullptr;
 if(FAILED(dev->CreateStateBlock(D3DSBT_ALL,&state)))return;
 RenderTransformState transforms;transforms.Capture(dev);
 bool started=false,begun=false,pass=false;
 __try{
  if(SUCCEEDED(fx->SetTechnique(technique))){
   current=effect;
   reinterpret_cast<void(__thiscall*)(void*)>((*reinterpret_cast<void***>(effect))[1])(effect);started=true;
   dev->SetVertexDeclaration(decl);
   UINT count=0;begun=SUCCEEDED(fx->Begin(&count,0));
   if(begun&&count)pass=SUCCEEDED(fx->BeginPass(0));
   if(pass){
    InvalidateSceneCaches();
    if(worldLogEnabled){static DWORD last=0;auto now=GetTickCount();if(now-last>=5000){last=now;Log("EVENT_NATIVE_SCENE_PASS native_nis=1 prelit=17 world_dependency=0");}}
    sceneOriginal(view,flags);
   }
  }
 }__finally{
  if(pass)fx->EndPass();
  if(started)reinterpret_cast<void(__thiscall*)(void*)>((*reinterpret_cast<void***>(effect))[2])(effect);
  if(begun)fx->End();
  current=nullptr;InvalidateSceneCaches();
  state->Apply();transforms.Restore(dev);state->Release();
 }
}
bool InstallScene(){
 const BYTE expected[]={0x8B,0x0D,0xC8,0x85,0x98,0,0x85,0xC9};BYTE actual[8]{};
 auto site=reinterpret_cast<void*>(0x44B190);
 if(!Copy(site,actual,8)||memcmp(actual,expected,8)){Log("EVENT_NATIVE_SCENE_PASS_UNAVAILABLE unknown_hook_surface=1");return false;}
 if(MH_CreateHook(site,Scene,reinterpret_cast<void**>(&sceneOriginal))!=MH_OK)return false;
 if(MH_EnableHook(site)!=MH_OK){MH_RemoveHook(site);sceneOriginal=nullptr;return false;}
 Log("EVENT_NATIVE_SCENE_PASS_INSTALLED world_dependency=0 native_effect_lifetime=1");return true;
}
}
