#pragma once
#include "../../World2MW/src/NeoAI/Types.h"
namespace mwel_plan_shift {
struct Result {float mean=0,maximum=0,nearMean=0,nearMaximum=0;unsigned samples=0,nearSamples=0;};
// Compare the same authored stations, not changing node indices or origins.
// Missing overlap is unknown, never a zero-shift observation.
template<class Old,class New> Result Measure(float station,Old&& previous,New&& next){
 Result result;
 for(float ahead=8;ahead<=64;ahead+=8){mwneo::Point a{},b{};
  if(!previous(station+ahead,a)||!next(station+ahead,b)||!mwneo::Finite(a)||!mwneo::Finite(b))continue;
  float distance=mwneo::LengthXZ(mwneo::Sub(a,b));result.mean+=distance;result.maximum=std::max(result.maximum,distance);++result.samples;
  if(ahead<=24){result.nearMean+=distance;result.nearMaximum=std::max(result.nearMaximum,distance);++result.nearSamples;}
 }
 if(result.samples)result.mean/=result.samples;if(result.nearSamples)result.nearMean/=result.nearSamples;return result;
}
}
