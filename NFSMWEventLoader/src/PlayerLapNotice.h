#pragma once
#include <cmath>
// Observe the player's native completed-lap count, never a displayed rank.
struct PlayerLapNotice {
 const void* race=nullptr;const void* board=nullptr;unsigned key=0,handle=0,lap=0;float seconds=0;bool ready=false;
 bool Observe(const void* nextRace,const void* nextBoard,unsigned nextKey,unsigned nextHandle,unsigned completed,unsigned total,float now){
  if(!nextRace||!nextBoard||!nextKey||!nextHandle||!std::isfinite(now)||completed>total){*this={};return false;}
  bool reset=!ready||race!=nextRace||board!=nextBoard||key!=nextKey||handle!=nextHandle||now+.25f<seconds||completed<lap;
  bool notify=!reset&&completed==lap+1&&completed>0&&completed<total;
  race=nextRace;board=nextBoard;key=nextKey;handle=nextHandle;lap=completed;seconds=now;ready=true;return notify;
 }
};
