#pragma once
// Native AIPursuit::FleeCopOfType scans its whole police population, including
// a local officer temporarily using native police AI. That body is controlled
// by the user/test owner, not a disposable dispatch slot. Exclude it only from
// this quota-selection query; do not change vehicle state, type or driver.
namespace rpd_police_population {
void* trampoline=nullptr;
bool __cdecl Excluded(unsigned vehicle){
 unsigned race=0,player=0,handle=0;
 if(rpd_mission::Current()&&gauntlet_runtime::Read(0x91E000,race)&&
    gauntlet_runtime::Read(race+0x18,player)&&gauntlet_runtime::Table(vehicle,0x8AA828)&&
    (handle=hot_pursuit_runtime::Handle(vehicle))&&player&&handle==player)return true;
 // The native list stores IVehicle (+7C = IsDestroyed), not ISimable.
 return reinterpret_cast<bool(__thiscall*)(unsigned)>((*reinterpret_cast<unsigned**>(vehicle))[0x7C/4])(vehicle);
}
void __declspec(naked) Query(){__asm{
 push edi
 call Excluded
 add esp,4
 push 0x4192DA
 ret
}}
bool Install(){
 if(trampoline)return true;
 const BYTE expected[]={0x8B,0x07,0x8B,0xCF,0xFF,0x50,0x7C,0x84,0xC0,0x0F,0x85,0x02,0x01,0,0};auto site=reinterpret_cast<void*>(0x4192D3);
 if(memcmp(site,expected,sizeof(expected))||MH_CreateHook(site,Query,&trampoline)!=MH_OK)return false;
 if(MH_EnableHook(site)!=MH_OK){MH_RemoveHook(site);trampoline=nullptr;return false;}return true;
}
}
