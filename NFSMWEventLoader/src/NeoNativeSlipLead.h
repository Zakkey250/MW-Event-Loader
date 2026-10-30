#pragma once
#include <algorithm>
#include <cmath>
#include "../../MW-Arcade-Drift-public/src/NativeDriftSteering.h"
// Input-only lead compensation for a selected native drift driver. The
// existing tyre feedback is u=3.45*requestedYaw-3*yaw-1.8*beta before clipping.
// Asking for yaw-1.8*(betaLead)/3.45 is therefore the same wheel request as
// observing betaLead earlier. No body angle, velocity or tyre force is written.
inline float NeoNativeSlipLead(bool active,float requestedYaw,float beta,float betaRate){
 if(!std::isfinite(requestedYaw))return 0;
 if(!active||!std::isfinite(beta)||!std::isfinite(betaRate)||beta*betaRate<=0)return requestedYaw;
 // Leave the observed ordinary .3-.5rad slide formation untrimmed. Lead
 // only extra outward growth beyond .5rad, continuously, before the existing
 // .72rad unwind. Stable angles and inward return keep the original demand.
 // Bound a noisy sample to 0.25rad of lead; never change an actual angle.
 const float blend=std::clamp((std::abs(beta)-.5f)/.2f,0.f,1.f);
 const float lead=std::clamp(.25f*betaRate,-.25f,.25f)*blend;
 return std::clamp(requestedYaw-(1.8f/3.45f)*lead,-mcd::NativeDriftTurnLimit,mcd::NativeDriftTurnLimit);
}
