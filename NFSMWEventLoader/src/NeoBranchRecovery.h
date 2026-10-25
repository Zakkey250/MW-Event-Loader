#pragma once
#include "../../World2MW/src/NeoAI/Types.h"
namespace mwel_branch {
struct RecoveryFork {bool valid=false;float station=0;};
// Native scenery can block a branch that the immutable planner geometry
// considers clear. Repeated checked recovery attempts without real station
// advance are evidence that this branch is unusable in the current race.
inline bool RepeatedJam(unsigned attempts,float anchor,float station,float speed){
 return attempts>=2&&std::isfinite(anchor)&&std::isfinite(station)&&std::isfinite(speed)&&
  station>=anchor&&station<=anchor+30.f&&std::abs(speed)<5.f;
}
// Only the common departure node gives an unambiguous backward mapping
// between differently measured branch/main roads. Proximity at a crossing
// or on another deck does not establish that identity.
template<class Road> RecoveryFork FindRecoveryFork(const Road& main,mwneo::Point fork,mwneo::Point body,
 unsigned previousAttempts,std::uint64_t now,std::uint64_t staticFailureAt){
 if(!previousAttempts||!staticFailureAt||now<staticFailureAt||now-staticFailureAt>5000||
    !mwneo::Finite(fork)||!mwneo::Finite(body)||mwneo::Distance(body,fork)>220)return {};
 RecoveryFork found;
 for(const auto& node:main)if(mwneo::Distance(fork,{node.p[0],node.p[1],node.p[2]})<.25f){
  if(found.valid)return {}; // Ambiguous repeated intersection: keep old road.
  found={true,node.station};
 }
 return found;
}
}
