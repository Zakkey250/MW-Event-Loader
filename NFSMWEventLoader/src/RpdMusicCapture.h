#pragma once
// Read-only music context: 1 = human officer securing the last live suspect,
// 2 = this dispatch's native arrest NIS loading/playing. Never infer an
// arrest from a generic loading screen (retry/intro/safehouse use it too).
namespace rpd_music_capture {
unsigned Read(unsigned* raceOut,unsigned* generationOut){
 if(raceOut)*raceOut=0;if(generationOut)*generationOut=0;
 using gauntlet_runtime::Read;
 unsigned race=0,flow=0,player=0,count=0;BYTE wreck=0,loading=1;
 if(GetCurrentThreadId()!=combat_health_runtime::gameThread||!rpd_mission::Current()||
    !Read(0x925E90,flow)||flow!=6||!Read(0x91E000,race)||!race||
    !Read(race+0x18,player)||!player||!Read(race+0x18+0x1D,wreck)||wreck)return 0;
 if(raceOut)*raceOut=race;if(generationOut)*generationOut=combat_health_runtime::generation;
 // The loading callback may run inside originalRequest, before owned is set.
 // The request receipt and epoch distinguish it from unrelated loading.
 if(!rpd_native_arrest_nis::victory&&rpd_native_arrest_nis::requestSuspect&&
    rpd_native_arrest_nis::Requested(race,MWELNeoCurrentKey(),combat_health_runtime::generation,rpd_native_arrest_nis::requestSuspect)&&
    (rpd_native_arrest_nis::nativeSetup||rpd_native_arrest_nis::Owns())){
  if(MWELRpdHotPursuitScope()&&MWELRpdActiveSuspects()!=0)return 0;
  BYTE fast=0;
  if((Read(0x925854,fast)&&fast)||rpd_native_arrest_nis::Owns())return 2;
 }
 if(rpd_mission::ResultPending()||!combat_health_runtime::Racing()||
    !Read(race+0x195C,loading)||loading||!Read(race+0x1958,count)||count<2||count>9)return 0;
 // Intermediate captures must not override the dispatch's ordinary music
 // situation. The same survivor ledger that prevents premature outros owns
 // this decision; a missing ledger falls back to the live native roster.
 if(rpd_music_session::Remaining()!=1)return 0;
 for(unsigned slot=1;slot<count;++slot){
  const auto target=rpd_music_session::ActiveTarget(race,slot);if(!target)continue;
  if(MWELRpdHotPursuitScope()?MWELRpdSprintArresting(target):rpd_mission::LocalArrestInProgress(target))return 1;
 }
 return 0;
}
}
#pragma comment(linker,"/EXPORT:MWEL_RpdMusicCaptureV1=_MWEL_RpdMusicCaptureV1")
extern "C" __declspec(dllexport) unsigned __cdecl MWEL_RpdMusicCaptureV1(unsigned* race,unsigned* generation){
 return rpd_music_capture::Read(race,generation);
}
