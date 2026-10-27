#pragma once
#include "CombatHealthPolicy.h"
namespace arms_survival {
// Survival victory supersedes route progress or speed-trap totals. Preserve
// native order among the wrecked rivals, moving only the survivor to first.
inline bool WinningRanks(std::array<unsigned,16>& ranks,unsigned count){
 if(count<2||count>ranks.size())return false;
 unsigned seen=0;
 for(unsigned i=0;i<count;++i){if(ranks[i]<1||ranks[i]>count||(seen&(1u<<ranks[i])))return false;seen|=1u<<ranks[i];}
 const auto prior=ranks[0];
 for(unsigned i=1;i<count;++i)if(ranks[i]<prior)++ranks[i];
 ranks[0]=1;return true;
}
// Unknown/unobserved slots are not wrecks. Native totalled flags supplement
// the event HP ledger so a KO, despawn, finish or transient loss is not a kill.
inline bool PlayerAlone(const combat_health::Ledger& hp,unsigned count,
                        const std::array<unsigned,16>& handles,unsigned totalled){
 if(!hp.event||!hp.epoch||count<2||count>hp.racers.size())return false;
 for(unsigned i=0;i<count;++i){
  const auto& car=hp.racers[i];
  if(!handles[i]||!car.occupied||car.handle!=handles[i]||!std::isfinite(car.remaining))return false;
  for(unsigned j=0;j<i;++j)if(handles[j]==handles[i])return false;
  if(!i){if(car.retired||car.remaining<=0||(totalled&1u))return false;}
  else if(!car.retired||car.remaining!=0||!(totalled&(1u<<i)))return false;
 }
 return true;
}
}
