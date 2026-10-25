#pragma once
#include "../../World2MW/src/NeoAI/Types.h"
namespace mwel_lane_progress {
inline bool BoundedAdvance(float prior,float candidate,float moved,float elapsed,float speed){
 if(!std::isfinite(prior)||!std::isfinite(candidate)||!std::isfinite(moved)||!std::isfinite(elapsed)||!std::isfinite(speed)||
    moved<0||elapsed<=0||elapsed>.5f||speed<0)return false;
 return candidate<=prior+std::min(90.f,std::max(moved*1.5f,speed*elapsed)+2.f);
}
// A planner rooted at the current car is not independent route evidence.
// Require its checked continuation to reconnect to the authored corridor.
template<class Road,class Sample> bool HasForwardJoin(const Road& lane,float station,Sample&& sample){
 for(const auto& q:lane)if(q.station>=station&&q.station<=station+160){
  mwneo::Point original;if(!sample(q.station,original))continue;
  if(mwneo::Distance({q.p[0],q.p[1],q.p[2]},original)<=24.f&&std::abs(q.p[1]-original.y)<=5.f)return true;
 }
 return false;
}
}
