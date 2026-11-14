#pragma once
// Music belongs to the whole dispatch, not to a single suspect's pursuit.
namespace rpd_music_session {
unsigned ActiveTarget(unsigned race,unsigned slot){
 using gauntlet_runtime::Read;
  unsigned target=0,status=0;BYTE busted=0,escaped=0;
  if(!Read(race+0x18+slot*0x194,target)||!hot_pursuit_runtime::RunningTarget(target))return 0;
  const auto group=hot_pursuit_runtime::FindPursuit(target);
  // A native arrest can precede ledger retirement in this same callback.
  if(group&&((Read(group+0xE8,busted)&&busted)||
     (Read(group+0x218,status)&&status==3)))return 0;
  // A sprint suspect may lose one native pursuit and be reacquired before
  // reaching the finish. That is not an event-level escape.
  if(group&&!MWELRpdHotPursuitScope()&&((Read(group+0xE9,escaped)&&escaped)||status==4))return 0;
 return target;
}
unsigned Remaining(unsigned* raceOut=nullptr,unsigned* generationOut=nullptr){
 if(raceOut)*raceOut=0;if(generationOut)*generationOut=0;
 using gauntlet_runtime::Read;
 unsigned race=0,count=0,flow=0;BYTE loading=1,officerWreck=0;
 if(GetCurrentThreadId()!=combat_health_runtime::gameThread||!rpd_mission::Current()||
    rpd_mission::ResultPending()||
    !Read(0x925E90,flow)||flow!=6||!Read(0x91E000,race)||!race||
    !Read(race+0x195C,loading)||loading||!Read(race+0x1958,count)||count<2||count>9||
    !Read(race+0x18+0x1D,officerWreck)||officerWreck)return 0;
 if(raceOut)*raceOut=race;if(generationOut)*generationOut=combat_health_runtime::generation;
 // A suspect's native pursuit/scene can end before the multi-racer dispatch.
 // Once bound, the same authoritative retirement ledger drives music,
 // standings, driving and weapons, including transient cinematic timer gaps.
 if(MWELRpdHotPursuitScope()){
  const auto active=MWELRpdActiveSuspects();if(active!=~0u)return active;
 }
 if(!combat_health_runtime::Racing())return 0;
 unsigned remaining=0;
 for(unsigned slot=1;slot<count;++slot){
  if(!ActiveTarget(race,slot))continue;
  ++remaining; // A survivor without a newly constructed pursuit still counts.
 }
 return remaining;
}
}
#pragma comment(linker,"/EXPORT:MWEL_RpdMusicSessionV1=_MWEL_RpdMusicSessionV1")
extern "C" __declspec(dllexport) unsigned __cdecl MWEL_RpdMusicSessionV1(unsigned* race,unsigned* generation){
 return rpd_music_session::Remaining(race,generation);
}
