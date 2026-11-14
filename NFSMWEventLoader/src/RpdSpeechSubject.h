#pragma once
#include "WorldProviderModule.h"
// Only Speech's verified local GetSimable consumers change viewpoint. The
// real IPlayer, car ownership and pursuit activity remain native. Speech's
// multiple-suspect announcements use the live RPD racers, excluding officers.
// Never broaden IsPlayerPursuit for career/bounty code.
namespace rpd_speech_subject {
void* trampolines[21]{};
// UINT_MAX means the native speech policy owns this event.
unsigned (*trackedCount)(bool)=nullptr;
unsigned (*officerTarget)(unsigned)=nullptr;
bool (*stillInSight)(unsigned)=nullptr;
unsigned Tracked(bool additional=false){return trackedCount?trackedCount(additional):~0u;}
unsigned Suspect(){
 using gauntlet_runtime::Read;
 unsigned race=0,handle=0,player=0,count=0,driver=0;
 if(!rpd_mission::Current()||rpd_mission::ResultPending()||!combat_health_runtime::Racing()||
    GetCurrentThreadId()!=combat_health_runtime::gameThread||!Read(0x91E000,race)||
    !Read(race+0x1958,count)||count<2||count>9||!Read(race+0x18,player))return 0;
 if(MWELRpdHotPursuitScope()){
  handle=officerTarget?officerTarget(player):0;
  if(!handle)handle=MWELRpdSprintTarget(player);
 }
 else if(!Read(race+0x18+0x194,handle))return 0;
 if(!handle||handle==player)return 0;
 const auto car=gauntlet_runtime::Vehicle(handle);
 return car&&Read(car+0x94,driver)&&driver==3?car:0;
}
bool __cdecl OfficerListening(){return Suspect()!=0;}
unsigned __stdcall Subject(unsigned player,unsigned site){
 using gauntlet_runtime::Read;
 if(const auto car=Suspect()){
   const auto handle=gauntlet_runtime::Handle(car);
   if(worldLogEnabled){static std::array<ULONGLONG,13> next{};const auto now=GetTickCount64();
    if(site<next.size()&&now>=next[site]){next[site]=now+10000;Log("RPD_SPEECH_SUBJECT site=%u suspect=%u sim=%08X transient_foreground_independent=1 native_event_rules=1",site,handle,car-0x80);}
   }return car-0x80;
 }
 unsigned table=0,fn=0;if(!Read(player,table)||!Read(table+4,fn))return 0;
 return reinterpret_cast<unsigned(__thiscall*)(unsigned)>(fn)(player);
}
#define RPD_SPEECH_SITE(Name,Index,Source,Resume) void __declspec(naked) Name(){__asm{push Index} __asm{push Source} __asm{call Subject} __asm{push Resume} __asm{ret}}
RPD_SPEECH_SITE(Query0,0,esi,0x70A636)
RPD_SPEECH_SITE(Query1,1,ecx,0x70A79C)
RPD_SPEECH_SITE(Query2,2,ecx,0x70ACC7)
RPD_SPEECH_SITE(Query3,3,ecx,0x70AD5E)
RPD_SPEECH_SITE(Query4,4,ecx,0x71E98C)
RPD_SPEECH_SITE(Query5,5,ecx,0x71F8DC)
RPD_SPEECH_SITE(Query9,9,ebx,0x71D1A5)
RPD_SPEECH_SITE(Query10,10,ecx,0x71D1CF)
RPD_SPEECH_SITE(Query12,12,ecx,0x71D6AF)
#undef RPD_SPEECH_SITE
void __declspec(naked) Query11(){__asm{push 11} __asm{push esi} __asm{call Subject} __asm{mov edx,[eax]} __asm{push 0x70A677} __asm{ret}}
#define RPD_SPEECH_COM_SITE(Name,Index,Resume) void __declspec(naked) Name(){__asm{push Index} __asm{push ecx} __asm{call Subject} __asm{mov ecx,[eax+4]} __asm{push Resume} __asm{ret}}
RPD_SPEECH_COM_SITE(Query6,6,0x71ABF0)
RPD_SPEECH_COM_SITE(Query7,7,0x71D0AB)
RPD_SPEECH_COM_SITE(Query8,8,0x71D0C1)
#undef RPD_SPEECH_COM_SITE
unsigned __stdcall Position(unsigned player){
 if(const auto car=Suspect()){
  unsigned table=0,fn=0;
  if(car&&gauntlet_runtime::Read(car,table)&&gauntlet_runtime::Read(table+0xC,fn))
   return reinterpret_cast<unsigned(__thiscall*)(unsigned)>(fn)(car);
 }
 unsigned table=0,fn=0;if(!gauntlet_runtime::Read(player,table)||!gauntlet_runtime::Read(table+0xC,fn))return 0;
 return reinterpret_cast<unsigned(__thiscall*)(unsigned)>(fn)(player);
}
void __declspec(naked) Position0(){__asm{push ecx} __asm{call Position} __asm{push 0x71D2B0} __asm{ret}}
void __declspec(naked) Position1(){__asm{push ebx} __asm{call Position} __asm{push 0x71EDCB} __asm{ret}}
// The roadblock dispatcher encodes singular/plural from Speech's cached
// racer count (+15Ch). GRacerInfo includes the officer in RPD. Adapt only
// these voice-parameter consumers; never change native race/global lists.
unsigned __stdcall Count(unsigned cached){
 const auto tracked=Tracked();if(tracked!=~0u)return tracked;
 if(!Suspect())return cached;
 unsigned race=0,count=0,targets=0;gauntlet_runtime::Read(0x91E000,race);gauntlet_runtime::Read(race+0x1958,count);
 for(unsigned slot=1;slot<count&&slot<9;++slot){unsigned handle=0,driver=0,flags=0;BYTE finished=0;
  if(!gauntlet_runtime::Read(race+0x18+slot*0x194,handle)||!gauntlet_runtime::Read(race+0x18+slot*0x194+0x1C,flags)||flags||
     !gauntlet_runtime::Read(race+0x18+slot*0x194+0x21,finished)||finished||MWELRpdSprintRetired(handle))continue;
  const auto car=gauntlet_runtime::Vehicle(handle);if(car&&gauntlet_runtime::Read(car+0x94,driver)&&driver==3)++targets;
 }
 if(worldLogEnabled){static ULONGLONG next=0;if(GetTickCount64()>=next){next=GetTickCount64()+3000;Log("RPD_SPEECH_TARGET_COUNT cached=%u racers=%u suspects=%u officer_excluded=1",cached,count,targets);}}
 return targets;
}
unsigned __stdcall Others(unsigned cached){
 // Native speech treats IPlayer as the primary suspect, and AIRacers as
 // additional suspects (+15Dh). Our primary suspect is itself in AIRacers.
 // Remove that one implicit duplicate only from Speech's cached view.
 const auto tracked=Tracked(true);if(tracked!=~0u)return tracked;
 if(!Suspect())return cached;
 const auto total=Count(cached);return total?total-1:0;
}
bool __stdcall SingleTarget(){
 const auto tracked=Tracked();return tracked!=~0u?tracked<=1:Suspect()&&Count(0)<=1;
}
unsigned __stdcall MultiplePolicy(){
 const auto tracked=Tracked();
 if(tracked!=~0u)return tracked>1?2u:1u;
 return SingleTarget()?1u:0u;
}
// Native Speech assumes its human IPlayer is another suspect. In RPD that
// assumption also sets +54h bit 800h and announces a split/focus change even
// after correcting the cached AIRacer count. Skip only that multi-suspect
// calculation when Event Loader owns the pursuit roster. The native setup
// report reads this bit to choose its single/multiple suspect voice variant.
void __declspec(naked) MultipleFlag(){__asm{
 pushfd
 pushad
 mov ebp,esp
 sub esp,528
 and esp,0FFFFFFF0h
 fxsave [esp]
 call MultiplePolicy
 cmp al,2
 je trackedMultiple
 test al,al
 jz nativeMultiple
 fxrstor [esp]
 mov esp,ebp
 popad
 popfd
 push 0x720ECE
 ret
 trackedMultiple:
 fxrstor [esp]
 mov esp,ebp
 popad
 popfd
 mov eax,[ebp+0x54]
 or eax,0x800
 push 0x720ED6
 ret
 nativeMultiple:
 fxrstor [esp]
 mov esp,ebp
 popad
 popfd
 mov eax,[ebp+0x184]
 push 0x720E9A
 ret
}}
void __fastcall FocusChange(unsigned self,void*){
 if(SingleTarget()){
  if(worldLogEnabled)Log("RPD_SPEECH_SINGLE_TARGET suppressed=AnytimeEvents_FocusChange officer_excluded=1");
  return;
 }
 reinterpret_cast<void(__thiscall*)(unsigned)>(trampolines[19])(self);
}
#define RPD_COUNT_SITE(Name,Source,Destination,Resume) void __declspec(naked) Name(){__asm{pushfd} __asm{pushad} __asm{mov ebp,esp} __asm{sub esp,528} __asm{and esp,0FFFFFFF0h} __asm{fxsave [esp]} __asm{movzx eax,byte ptr [Source+0x15C]} __asm{push eax} __asm{call Count} __asm{mov byte ptr [ebp+Destination],al} __asm{fxrstor [esp]} __asm{mov esp,ebp} __asm{popad} __asm{popfd} __asm{push Resume} __asm{ret}}
RPD_COUNT_SITE(Count0,edi,24,0x7176EF)
RPD_COUNT_SITE(Count1,edi,28,0x717785)
RPD_COUNT_SITE(Count2,ebx,24,0x71EFFF)
#undef RPD_COUNT_SITE
void __declspec(naked) OtherCount(){__asm{
 pushfd
 pushad
 mov ebp,esp
 sub esp,528
 and esp,0FFFFFFF0h
 fxsave [esp]
 movzx eax,byte ptr ds:[0x92CE44]
 push eax
 call Others
 mov byte ptr [ebp+24],al
 fxrstor [esp]
 mov esp,ebp
 popad
 popfd
 push 0x720E25
 ret
}}
using DispatchFn=unsigned(__cdecl*)(unsigned,unsigned,unsigned,unsigned,unsigned);
DispatchFn originalDispatch=nullptr;
unsigned reportEpoch=0;
ULONGLONG nextReportCheck=0,multipleSince=0;
bool reportedMultiple=false,refreshingReport=false;
void ReportScope(){
 if(reportEpoch==combat_health_runtime::generation)return;
 reportEpoch=combat_health_runtime::generation;
 nextReportCheck=multipleSince=0;reportedMultiple=false;
}
struct ContactEvidence {unsigned handle=0,epoch=0;float at=-100;};
ContactEvidence contact;
void SuspectContact(unsigned handle,float units,float seconds){
 if(units>.02f&&std::isfinite(seconds)){
  const auto car=Suspect();if(car&&gauntlet_runtime::Handle(car)==handle)contact={handle,combat_health_runtime::generation,seconds};
 }
}
bool RecentContact(unsigned handle,float seconds){return handle&&contact.handle==handle&&contact.epoch==combat_health_runtime::generation&&std::isfinite(seconds)&&seconds>=contact.at&&seconds-contact.at<=1.5f;}
unsigned __cdecl Dispatch(unsigned bytes,unsigned payload,unsigned declaration,unsigned variables,unsigned speaker){
 // LostVisual / LostSuspect are stock dispatch lines. Reject a stale report
 // only if a live unit still has fresh visual on that same pursuit target.
 // Do not touch native sight, escape state or helicopter-specific reports.
 if((declaration==0x901E0C||declaration==0x901E1C)&&stillInSight&&stillInSight(declaration))return 0;
 // Setup_InitPursuit can be emitted on the same tick the groups are bound,
 // before the native speech update that normally refreshes bit 0x800.
 if(declaration==0x901C04&&bytes==8){
  const auto tracked=Tracked();unsigned voice[2]{};
  if(tracked!=~0u&&tracked&&Copy(reinterpret_cast<void*>(payload),voice,sizeof(voice))){
   ReportScope();reportedMultiple|=tracked>1;
   voice[1]=tracked>1?2:1;
   Log("HOT_PURSUIT_RADIO_SETUP targets=%u native_variant=%u refreshed=%u",tracked,voice[1],unsigned(refreshingReport));
   return originalDispatch(bytes,unsigned(voice),declaration,variables,speaker);
  }
 }
 // These native events carry a voice variant, not the struck car's handle.
 // Confirm them against actual suspect-body contacts before dispatch. Native
 // IPlayer-only collision notifications must never describe the RPD officer.
 if(declaration==0x901DA4||declaration==0x901DAC){
  if(const auto car=Suspect()){
   float seconds=0;gauntlet_runtime::Read(0x9885D8,seconds);const auto handle=gauntlet_runtime::Handle(car);
   if(!RecentContact(handle,seconds)){
    if(worldLogEnabled)Log("RPD_SPEECH_COLLISION_REJECTED declaration=%08X suspect=%u actual_suspect_contact=0 officer_collision_not_target=1",declaration,handle);
    return 0;
   }
  }
 }
#ifdef MWEL_ARMS_FRONTEND_STIMULUS
 // Private read-only field diagnosis. Never compiled into handoff binaries.
 if(worldLogEnabled&&GetCurrentThreadId()==combat_health_runtime::gameThread&&rpd_mission::Current()){
  unsigned name=0,speech=0,flags=0;char label[96]{},values[240]{};BYTE cached=0,ai=0;
  gauntlet_runtime::Read(declaration,name);if(name)Copy(reinterpret_cast<void*>(name),label,sizeof(label)-1);
  gauntlet_runtime::Read(0x993CC8,speech);gauntlet_runtime::Read(speech+0x15C,cached);gauntlet_runtime::Read(speech+0x15D,ai);
  gauntlet_runtime::Read(speech+0x54,flags);
  size_t written=0;for(unsigned offset=0;offset<std::min(bytes,64u)&&written<sizeof(values)-12;offset+=4){unsigned v=0;if(!gauntlet_runtime::Read(payload+offset,v))break;written+=sprintf_s(values+written,sizeof(values)-written,"%08X,",v);}
  Log("EXPERIMENT_RPD_RADIO event=%s bytes=%u payload=%s cached_racers=%u AI_racers=%u flags=%08X speaker=%08X",label,bytes,values,unsigned(cached),unsigned(ai),flags,speaker);
 }
#endif
 return originalDispatch(bytes,payload,declaration,variables,speaker);
}
void ReportMultiple(){
 if(!originalDispatch||GetCurrentThreadId()!=combat_health_runtime::gameThread)return;
 ReportScope();if(reportedMultiple)return;
 const auto now=GetTickCount64();if(now<nextReportCheck)return;
 nextReportCheck=now+750;
 if(reinterpret_cast<bool(__cdecl*)()>(0x468390)()){multipleSince=0;return;}
 const auto tracked=Tracked();
 if(tracked==~0u||tracked<2){multipleSince=0;return;}
 if(!multipleSince){multipleSince=now;return;}
 // Give the normal setup sequence time to report the group first. On a
 // gradual pursuit start its single-suspect setup may already be complete.
 if(now-multipleSince<3000)return;
 using gauntlet_runtime::Read;
 unsigned speech=0,foreground=0,speaker=0,table=0,setup=0;
 if(!Read(0x993CC8,speech)||!speech||
    ((!Read(speech+0x130,foreground)||!foreground)&&(!Read(speech+0x134,foreground)||!foreground)))return;
 const BYTE selectGuard[]={0x6A,0xFF,0x68,0x68,0x01,0x88,0x00,0x64,0xA1};
 const BYTE setupGuard[]={0xA1,0xC8,0x3C,0x99,0x00,0x83,0xEC,0x08};
 if(memcmp(reinterpret_cast<void*>(0x7153B0),selectGuard,sizeof(selectGuard))||
    memcmp(reinterpret_cast<void*>(0x718340),setupGuard,sizeof(setupGuard)))return;
 // Same live speaker selection used by the native focus-change report.
 speaker=reinterpret_cast<unsigned(__thiscall*)(unsigned,unsigned,unsigned)>(0x7153B0)(speech,0,0);
 if(!speaker||!Read(speaker,table)||!Read(table+0x88,setup)||setup!=0x718340)return;
 refreshingReport=true;
 reinterpret_cast<void(__thiscall*)(unsigned)>(setup)(speaker);
 refreshingReport=false;
}
bool InstallDispatch(){
 if(originalDispatch)return true;
 const BYTE expected[]={0x64,0xA1,0,0,0,0,0x6A,0xFF,0x68,0x81,0,0x88,0};auto site=reinterpret_cast<void*>(0x713B20);
 if(memcmp(site,expected,sizeof(expected))){
  const auto code=static_cast<const BYTE*>(site);HMODULE owner=nullptr;
  const auto target=code[0]==0xE9?unsigned(code+5)+*reinterpret_cast<const int*>(code+1):0;
  if(!target||!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,reinterpret_cast<LPCWSTR>(target),&owner)||owner!=mwel_world_provider::Module()){
   Log("RPD_SPEECH_DISPATCH_CONFLICT unsupported_hook_owner=1");return false;
  }
 }
 const bool ok=MH_CreateHook(site,Dispatch,reinterpret_cast<void**>(&originalDispatch))==MH_OK&&MH_EnableHook(site)==MH_OK;
 if(ok)combat_health_runtime::speechSuspectContact=SuspectContact;return ok;
}
bool Install(){
 if(trampolines[0])return true;
 const unsigned sites[]={0x70A631,0x70A797,0x70ACC2,0x70AD59,0x71E987,0x71F8D7,0x71ABEA,0x71D0A5,0x71D0BB,0x71D1A0,0x71D1CA,0x71D2AB,0x71EDC4,0x70A672,0x7176E9,0x71777F,0x71EFF9,0x720E1F,0x720E94,0x719060,0x71D6AA};
 void* hooks[]={Query0,Query1,Query2,Query3,Query4,Query5,Query6,Query7,Query8,Query9,Query10,Position0,Position1,Query11,Count0,Count1,Count2,OtherCount,MultipleFlag,FocusChange,Query12};
 const BYTE expected[][7]={{0x8B,0xCE,0xFF,0x52,4},{0x8B,0x11,0xFF,0x52,4},{0x8B,0x11,0xFF,0x52,4},{0x8B,0x11,0xFF,0x52,4},{0x8B,0x11,0xFF,0x52,4},{0x8B,0x11,0xFF,0x52,4},{0xFF,0x52,4,0x8B,0x48,4},{0xFF,0x52,4,0x8B,0x48,4},{0xFF,0x50,4,0x8B,0x48,4},{0x8B,0xCB,0xFF,0x52,4},{0x8B,0x01,0xFF,0x50,4},{0x8B,0x11,0xFF,0x52,0xC},{0x8B,0x03,0x8B,0xCB,0xFF,0x50,0xC},{0xFF,0x50,4,0x8B,0x10},{0x8A,0x8F,0x5C,1,0,0},{0x8A,0x87,0x5C,1,0,0},{0x8A,0x8B,0x5C,1,0,0},{0x8A,0x0D,0x44,0xCE,0x92,0},{0x8B,0x85,0x84,1,0,0},{0x83,0xEC,8,0x8B,0x51,0xC},{0x8B,0x11,0xFF,0x52,4}};
 for(unsigned i=0;i<21;++i)if(memcmp(reinterpret_cast<void*>(sites[i]),expected[i],(i>=6&&i<=8)||(i>=14&&i<=19)?6:i==12?7:5)){Log("RPD_SPEECH_SURFACE_CONFLICT site=%08X",sites[i]);return false;}
 unsigned made=0;bool ok=true;
 for(unsigned i=0;i<21;++i){auto status=MH_CreateHook(reinterpret_cast<void*>(sites[i]),hooks[i],&trampolines[i]);if(status!=MH_OK){Log("RPD_SPEECH_CREATE_UNAVAILABLE site=%08X status=%u",sites[i],unsigned(status));ok=false;break;}++made;}
 if(ok)for(auto site:sites)if(MH_EnableHook(reinterpret_cast<void*>(site))!=MH_OK){ok=false;break;}
 if(ok)ok=InstallDispatch();
 if(!ok)for(unsigned i=0;i<made;++i){MH_DisableHook(reinterpret_cast<void*>(sites[i]));MH_RemoveHook(reinterpret_cast<void*>(sites[i]));trampolines[i]=nullptr;}
 return ok;
}
}
