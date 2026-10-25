#pragma once
#include <cstddef>
#include <cstdint>
namespace mwappearance {
constexpr unsigned RaceThreshold=6,RoamingThreshold=5;
constexpr unsigned GeometrySlots=76; // CarSlotId::BasePaint starts the texture/color slots.
constexpr bool RaceExterior(unsigned slot,unsigned count,std::uint32_t seed=0){
 if(count<2||count>16||slot>=count)return true; // Unknown native context remains native.
 if(count<RaceThreshold||slot==0)return true; // Never alter the player's car.
 const unsigned opponents=count-1,allowance=count/2-1;
 return (slot-1+opponents-seed%opponents)%opponents<allowance;
}
// Evaluate the actual live population plus the pending spawn. An existing
// excess is allowed to drain through native retirement; never rebuild a live car.
constexpr bool RoamingExterior(unsigned live,unsigned customized){
 const unsigned next=live+1;
 return next<RoamingThreshold||customized<next/2;
}
struct RoamingPopulation {unsigned live=0,customized=0;};
template<class Racers,class IsLive>RoamingPopulation CountRoaming(const Racers& racers,IsLive isLive){
 RoamingPopulation count;
 for(const auto& racer:racers)if(isLive(racer)){++count.live;if(racer.customExterior)++count.customized;}
 return count;
}
template<class Parts>void StockGeometry(Parts& destination,const Parts& stock){
 for(std::size_t i=0;i<GeometrySlots;++i)destination[i]=stock[i];
}
}
