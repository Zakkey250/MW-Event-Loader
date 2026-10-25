#pragma once
#include "../../World2MW/src/NeoAI/Types.h"
namespace mwel_recovery_observation {
// A stationary car never qualifies. At most one four-second verification
// window is available per unchanged committed-progress interval, after a
// verified placement has actually driven forward on a checked same-leg path.
inline bool Resumed(std::uint64_t now,std::uint64_t placed,mwneo::Point origin,mwneo::Point direction,mwneo::Point body,unsigned guidance){
 return placed&&now>=placed&&now-placed<=4000&&guidance==1&&mwneo::Finite(origin)&&mwneo::Finite(body)&&
  mwneo::DotXZ(mwneo::Sub(body,origin),mwneo::UnitXZ(direction))>=2.f;
}
inline bool VerifyOnce(bool& used,bool resumed){if(used||!resumed)return false;used=true;return true;}
}
