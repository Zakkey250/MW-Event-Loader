#pragma once
#include "../../World2MW/src/NeoAI/Types.h"
// Isolated full-field observer only: a verified finisher must not become a
// brake-locked roadblock inside the goal trigger. No throttle, teleport,
// checkpoint write or extra scoring. The caller supplies a fresh body sweep.
struct NeoPostGoalCoast {
 mwneo::Point origin{};std::uint64_t began=0;bool ready=false,done=false;
 bool Step(mwneo::Point position,float speed,std::uint64_t now,bool safe){
  if(!ready){origin=position;began=now;ready=true;}
  if(done)return false;
  if(!safe||!mwneo::Finite(position)||!std::isfinite(speed)||speed<2||speed>60||now<began||now-began>=4000||mwneo::Distance(position,origin)>=30){done=true;return false;}
  return true;
 }
};
