#pragma once
#include <cstdint>
#include "../../World2MW/src/NeoAI/Types.h"
namespace mwel_execution {
// A soft world-space preference, not a latch on steering angle. Every eligible
// candidate still needs a fresh full-body sweep; unsafe old aims cannot win.
struct Preference {
 mwneo::Point direction{};std::uint64_t at=0;float station=0;
 void Clear(){*this={};}
 void Observe(mwneo::Point delta,std::uint64_t now,float progress){direction=mwneo::UnitXZ(delta);at=now;station=progress;}
 float Penalty(mwneo::Point delta,std::uint64_t now,float progress)const{
  if(!at||now<at||now-at>750||progress<station-4||progress>station+120)return 0;
  auto next=mwneo::UnitXZ(delta);if(mwneo::LengthXZ(direction)<.9f||mwneo::LengthXZ(next)<.9f)return 0;
  float angle=std::abs(std::atan2(mwneo::CrossXZ(direction,next),mwneo::DotXZ(direction,next)));
  return std::min(5.f,angle*12.5f);
 }
};
}
