#pragma once
// Read-only HUD adapter. Native LocalPlayer::UpdateHUD (006F13F0) obtains
// IVehicleAI::GetPursuit at 006F14BF, then passes that IPursuit to the native
// Busted/Evade/cooldown setters (006F1C34..006F1C98). Substitute only this
// presentation query; the officer's simulation/pursuit ownership stays native.
namespace rpd_pursuit_presentation {
void* trampoline=nullptr;
void* humanTrampoline=nullptr;void* wrongWayTrampoline=nullptr;
void* timerQueryTrampoline=nullptr;void* warningQueryTrampoline=nullptr;
using ScriptFn=void(__thiscall*)(void*,unsigned*,void*);
ScriptFn evaluateScript=nullptr;
struct Tint {unsigned* object=nullptr;unsigned* data=nullptr;std::array<unsigned,3> native{},shown{};};
std::array<Tint,2> tints{};
char meterPackage[64]{};
bool colorsActive=false;
bool Interceptor(){
 const auto p=event_race_policies::Current();
 return p&&mwel::RpdStartOnly(p->authoring);
}
void RestoreTint(Tint& tint,unsigned* live){
 // Only a freshly resolved package-owned object, or the object supplied by
 // native evaluation, establishes lifetime. Never inspect tint.object itself.
 if(live&&live==tint.object&&reinterpret_cast<unsigned*>(live[11])==tint.data&&live[12]>=16){
  auto data=reinterpret_cast<unsigned*>(live[11]);
  if(std::equal(tint.shown.begin(),tint.shown.end(),data)){
   std::copy(tint.native.begin(),tint.native.end(),data);live[7]|=0x400000;
  }
 }
 tint={};
}
void Colors(void* info){
 using namespace expanded_leaderboard;
 const auto name=Read<const char*>(info,0x10);
 auto engine=Read<void*>(reinterpret_cast<void*>(0x91CADC));
 auto package=engine&&name?reinterpret_cast<Object(__thiscall*)(void*,const char*)>(0x516AF0)(engine,name):nullptr;
 if(!package){colorsActive=false;meterPackage[0]=0;tints={};return;}
 const bool active=Interceptor();
 // Verified native PURSUIT_METER_GROUP image keys. Do not tint the group,
 // cooldown, labels, textures or any other HUD element.
 auto arrest=Find(package,0xE0E0169B),escape=Find(package,0x30F39A6F);
 if(active!=colorsActive){
  RestoreTint(tints[0],arrest);RestoreTint(tints[1],escape);
  for(auto object:{arrest,escape})if(object)object[7]|=0x400000;
  Log("RPD_INTERCEPTOR_METER_COLORS active=%u arrest_green=1 escape_red=1 native_alpha_retained=1",unsigned(active));
 }
 colorsActive=active;
 if(!active)return;
 strcpy_s(meterPackage,name);
}
void __fastcall EvaluateScript(void* self,void*,unsigned* object,void* state){
 const auto key=object?object[4]:0;
 const int index=key==0xE0E0169B?0:key==0x30F39A6F?1:-1;
 if(index<0){evaluateScript(self,object,state);return;}
 auto& tint=tints[index];RestoreTint(tint,object);
 evaluateScript(self,object,state);
 if(!colorsActive||!Interceptor()||object[6]!=1||!object[11]||object[12]<16)return;
 auto live=reinterpret_cast<expanded_leaderboard::Object(__cdecl*)(const char*,unsigned)>(0x524850)(meterPackage,key);
 if(live!=object)return;
 auto data=reinterpret_cast<unsigned*>(object[11]);
 tint.object=object;tint.data=data;std::copy_n(data,3,tint.native.begin());
 const auto brightness=std::min(255u,*std::max_element(tint.native.begin(),tint.native.end()));
 tint.shown={};tint.shown[index==0?1:2]=brightness;
 // An evaluated-frame overlay, never a script/texture edit. Restore the
 // native RGB before the next evaluation. Native alpha, geometry and tracks
 // remain untouched, including pause, cooldown and the exit to other modes.
 std::copy(tint.shown.begin(),tint.shown.end(),data);object[7]|=0x400000;
}
bool __cdecl TimerPresentation(){
 // Only this HUD query is extended. Do not turn the race into a Tollbooth
 // or change its native clock, timeout, music, restart or game rules.
 return Interceptor()||reinterpret_cast<bool(__cdecl*)()>(0x50E1B0)();
}
int __fastcall WarningRaceType(void* params,void*){
 // Hud_TimeExtension (57B780) owns the stock last-ten-seconds message,
 // animation and warning SFX. The original type query excludes Interceptor.
 return Interceptor()?4:reinterpret_cast<int(__thiscall*)(void*)>(0x5FAA20)(params);
}
void __declspec(naked) TimerQuery(){__asm{
 call TimerPresentation
 push 0x6F1CA0
 ret
}}
void __declspec(naked) WarningQuery(){__asm{
 call WarningRaceType
 push 0x57B7A1
 ret
}}
bool __cdecl PoliceTestHud(){
 unsigned race=0,handle=0,ai=0;
 if(!rpd_mission::Current()||
    !gauntlet_runtime::Read(0x91E000,race)||!gauntlet_runtime::Read(race+0x18,handle))return false;
 const auto car=gauntlet_runtime::Vehicle(handle);
 return car&&gauntlet_runtime::Read(car+0x54,ai)&&gauntlet_runtime::Table(ai,0x892480);
}
void __declspec(naked) HumanGate(){
 __asm {jnz hasHuman
  pushfd
  pushad
  call PoliceTestHud
  test al,al
  jz missingHuman
  popad
  popfd
 hasHuman:
  push 0x6F144E
  ret
 missingHuman:
  popad
  popfd
  push 0x6F2262
  ret
 }
}
bool __cdecl SkipPoliceWrongWay(){return MWELRpdHotPursuitScope();}
void __declspec(naked) WrongWay(){
 __asm {pushfd
  pushad
  call SkipPoliceWrongWay
  test al,al
  jnz skipPolice
  popad
  popfd
  mov ecx,[esp+0x44]
  test ecx,ecx
  jz noHumanWrongWay
  mov esi,[esp+0x40]
  push 0x6F20E6
  ret
 noHumanWrongWay:
  push 0x6F20F3
  ret
 skipPolice:
  popad
  popfd
  push 0x6F20F3
  ret
 }
}
unsigned __cdecl Select(unsigned ai){
 if(MWELRpdHotPursuitScope())return 0;
 unsigned table=0,method=0;
 if(!gauntlet_runtime::Read(ai,table)||!gauntlet_runtime::Read(table+0xA0,method))return 0;
 const auto native=reinterpret_cast<unsigned(__thiscall*)(unsigned)>(method)(ai);
 const auto p=event_race_policies::Current();unsigned race=0,count=0,handle=0;
 if(!p||!mwel::RpdStartOnly(p->authoring)||!gauntlet_runtime::Read(0x91E000,race)||
    !gauntlet_runtime::Read(race+0x1958,count)||count!=2||!gauntlet_runtime::Read(race+0x18+0x194,handle))return native;
 const auto pursuit=hot_pursuit_runtime::FindPursuit(handle);
 if(worldLogEnabled){static ULONGLONG next=0;if(GetTickCount64()>=next){next=GetTickCount64()+2000;
  float meter=0,evade=0,busted=0;BYTE bailed=0;
  gauntlet_runtime::Read(pursuit+0x110,meter);gauntlet_runtime::Read(pursuit+0xFC,evade);
  gauntlet_runtime::Read(pursuit+0xDC,busted);gauntlet_runtime::Read(pursuit+0xE9,bailed);
  Log("RPD_PURSUIT_HUD_QUERY native_player_pursuit=%08X suspect_pursuit=%08X meter=%.3f evade=%.3f busted_time=%.3f bailed=%u",native,pursuit,meter,evade,busted,unsigned(bailed));
 }}
 return pursuit;
}
void __declspec(naked) Query(){
 __asm {push ecx
  call Select
  add esp,4
  push 0x6F14C5
  ret
 }
}
bool Install(){
 if(trampoline&&humanTrampoline&&wrongWayTrampoline&&timerQueryTrampoline&&warningQueryTrampoline&&evaluateScript)return true;
 // HUD's 00403770 requirement is IHumanAI, not IRacer (00403850).
 // Native cop AI legitimately has no human steering/wrong-way interface.
 // Keep all native HUD values/setters, skipping only that optional human
 // guard and its one late IsFacingWrongWay call in the isolated AI test.
 const BYTE expected[]={0xFF,0x92,0xA0,0,0,0},human[]={0x0F,0x84,0x14,0x0E,0,0},wrong[]={0x8B,0x4C,0x24,0x44,0x8B,0x74,0x24,0x40};
 const BYTE timer[]={0xE8,0x10,0xC5,0xE1,0xFF},warning[]={0xE8,0x7F,0xF2,0x07,0x00};
 const BYTE color[]={0x8B,0x54,0x24,0x04,0x8A,0x41,0x14};
 void* sites[]={reinterpret_cast<void*>(0x6F14BF),reinterpret_cast<void*>(0x6F1448),reinterpret_cast<void*>(0x6F20DE),reinterpret_cast<void*>(0x6F1C9B),reinterpret_cast<void*>(0x57B79C),reinterpret_cast<void*>(0x5BEC90)};
 if(memcmp(sites[0],expected,sizeof(expected))||memcmp(sites[1],human,sizeof(human))||memcmp(sites[2],wrong,sizeof(wrong))||memcmp(sites[3],timer,sizeof(timer))||memcmp(sites[4],warning,sizeof(warning))||memcmp(sites[5],color,sizeof(color)))return false;
 void* detours[]={Query,HumanGate,WrongWay,TimerQuery,WarningQuery,EvaluateScript};void** outputs[]={&trampoline,&humanTrampoline,&wrongWayTrampoline,&timerQueryTrampoline,&warningQueryTrampoline,reinterpret_cast<void**>(&evaluateScript)};unsigned made=0;bool ok=true;
 for(unsigned i=0;i<std::size(sites);++i){if(MH_CreateHook(sites[i],detours[i],outputs[i])!=MH_OK){ok=false;break;}++made;}
 if(ok)for(unsigned i=0;i<std::size(sites);++i)if(MH_EnableHook(sites[i])!=MH_OK){ok=false;break;}
 if(!ok){for(unsigned i=0;i<made;++i){MH_DisableHook(sites[i]);MH_RemoveHook(sites[i]);}for(auto output:outputs)*output=nullptr;return false;}
 Log("RPD_PURSUIT_HUD_ADAPTER native_meter_setters=1 simulation_target_changed=0 interceptor_native_time_warning=1");return true;
}
}
