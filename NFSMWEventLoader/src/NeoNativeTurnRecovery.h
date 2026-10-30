#pragma once
#include <cmath>
#include "../../MW-Arcade-Drift-public/src/NativeDriftSteering.h"
// A native turn request is not wheel lock. During an excessive slide, ask
// the existing native tyre feedback to unwind, instead of following the map
// while the intent controller is explicitly trying to leave that slide.
struct NeoNativeTurnRecovery {
 bool active=false;float settled=0;
 void Reset(){*this={};}
 bool Update(bool eligible,float beta,float yaw,float dt){
  if(!eligible){Reset();return false;}
  if(!std::isfinite(beta)||!std::isfinite(yaw)||!std::isfinite(dt)||dt<=0||dt>.1f){settled=0;return active;}
  if(std::abs(beta)>.72f){active=true;settled=0;}
  if(active){
   // Retain ownership through the zero-slip crossing until rotation has
   // actually settled. These are measured body samples, not phase flags.
   // Rejoin the road once back inside the ordinary slide envelope. Waiting
   // for straight-line grip carries a now-stable car past a continuing bend.
   settled=std::abs(beta)<.5f&&std::abs(yaw)<mcd::NativeDriftTurnLimit?settled+dt:0;
   if(settled>=.15f)Reset();
  }
  return active;
 }
};
