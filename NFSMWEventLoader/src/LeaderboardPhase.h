#pragma once
// Advance at the native board's flip boundaries, retaining its fade animation.
// A new board/race starts with names (or HP if first observed mid-value phase).
struct LeaderboardPhase {
 bool initialized=false,threeWay=false,lastValues=false;
 unsigned phase=0;
 unsigned Observe(bool hotPursuitSpeedtrap,bool values){
  if(!initialized||threeWay!=hotPursuitSpeedtrap){
   initialized=true;threeWay=hotPursuitSpeedtrap;lastValues=values;phase=values?1u:0u;
  }else if(values!=lastValues){lastValues=values;phase=threeWay?(phase+1)%3:(values?1u:0u);}
  return phase;
 }
};
