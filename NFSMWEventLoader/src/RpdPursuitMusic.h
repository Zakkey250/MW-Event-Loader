#pragma once
#include "RpdMusicSession.h"
#include "RpdMusicProximity.h"
#include "RpdMusicCapture.h"
// Preserve the native music foreground and expose local racer proximity.
// No independent BGM request or forced section is issued.
namespace rpd_pursuit_music {
void* musicStatusTrampoline=nullptr;
// Legacy read-only ABI retained for older clients; current audio never retargets to it.
unsigned LeadPursuit(unsigned* racer=nullptr){
 if(racer)*racer=0;
 if(GetCurrentThreadId()!=combat_health_runtime::gameThread||!rpd_mission::Current()||
    !combat_health_runtime::Racing()||rpd_mission::ResultPending())return 0;
 unsigned target=0;
 if(MWELRpdHotPursuitScope())target=rpd_hot_pursuit_runtime::Lead().handle;
 else{unsigned race=0;if(gauntlet_runtime::Read(0x91E000,race)&&race)gauntlet_runtime::Read(race+0x18+0x194,target);}
 if(!target||!hot_pursuit_runtime::RunningTarget(target))return 0;
 const auto pursuit=hot_pursuit_runtime::FindPursuit(target);
 if(racer)*racer=target; // The current leader remains authoritative while its group is rebuilt.
 return pursuit;
}
unsigned __cdecl Status(unsigned nativePursuit){
 // Keep the normal native foreground. Only prevent an individual retirement
 // from ending music while the event still owns surviving racers.
 const auto pursuit=nativePursuit;
 const auto table=*reinterpret_cast<unsigned*>(pursuit);
 const auto getter=*reinterpret_cast<unsigned*>(table+0x114);
 const auto status=reinterpret_cast<unsigned(__thiscall*)(unsigned)>(getter)(pursuit);
 return (status==3||status==4)&&rpd_music_session::Remaining()?0u:status;
}
void __declspec(naked) MusicStatus(){__asm{
 pushfd
 pushad
 mov ebp,esp
 sub esp,528
 and esp,0FFFFFFF0h
 fxsave [esp]
 push ecx
 call Status
 add esp,4
 mov [ebp+28],eax
 fxrstor [esp]
 mov esp,ebp
 popad
 popfd
 push 071B1C0h
 ret
}}
bool Install(){
 rpd_intro_music::Prepare();
 if(musicStatusTrampoline)return true;
 // This follows the native non-null test and keeps Speech's own foreground
 // and EA Trax's independent 71B1AE guard untouched.
 const BYTE expected[]={0x8B,0x01,0xFF,0x90,0x14,0x01,0x00,0x00};
 auto site=reinterpret_cast<void*>(0x71B1B8);
 if(memcmp(site,expected,sizeof(expected))||MH_CreateHook(site,MusicStatus,&musicStatusTrampoline)!=MH_OK)return false;
 if(MH_EnableHook(site)==MH_OK)return true;
 MH_RemoveHook(site);musicStatusTrampoline=nullptr;return false;
}
void Frame(){
 rpd_intro_music::AfterGo();
 if(!worldLogEnabled||!rpd_mission::Current())return;
 static ULONGLONG next=0;if(GetTickCount64()<next)return;next=GetTickCount64()+2000;
 unsigned sound=0;if(!gauntlet_runtime::Read(0x911FA8,sound)||!sound)return;
 const auto music=reinterpret_cast<unsigned(__thiscall*)(unsigned,unsigned)>(0x4AEAA0)(sound,0x40010010);
 unsigned mode=0,section=0,context=0,flags=0,speech=0,foreground=0,speechState=0;
 if(!gauntlet_runtime::Table(music,0x899B80))return;
 gauntlet_runtime::Read(music+0x148,mode);gauntlet_runtime::Read(music+0x11C,section);
 gauntlet_runtime::Read(music+0x150,context);gauntlet_runtime::Read(music+0x114,flags);
 if(gauntlet_runtime::Read(0x993CC8,speech)){gauntlet_runtime::Read(speech+0x130,foreground);gauntlet_runtime::Read(speech+0x1D4,speechState);}
 Log("RPD_NATIVE_MUSIC_STATE mode=%u section=%u context=%u flags=%08X foreground=%08X speech_state=%u independent_request=0",mode,section,context,flags,foreground,speechState);
 unsigned nearby=0,remaining=0,arrested=0,initial=0;float distance=300.f;
 if(MWEL_RpdMusicProximityV3(&nearby,&remaining,&distance,&arrested,&initial))
  Log("RPD_MUSIC_PROXIMITY nearby=%u remaining=%u nearest_m=%.1f arrested=%u initial=%u radius_m=300 leader_retarget=0",nearby,remaining,distance,arrested,initial);
}
}
#pragma comment(linker,"/EXPORT:MWEL_RpdMusicPursuitV1=_MWEL_RpdMusicPursuitV1")
extern "C" __declspec(dllexport) unsigned __cdecl MWEL_RpdMusicPursuitV1(unsigned* racer){return rpd_pursuit_music::LeadPursuit(racer);}
