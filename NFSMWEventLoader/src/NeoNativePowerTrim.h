#pragma once
#include <algorithm>
#include <cmath>
// Driver pedal control only. Native tyre forces and the already checked
// steering request remain authoritative; no brake/pose/velocity operation.
struct NeoNativePowerTrim {
 float held=0;
 float Step(bool active,float dt,float throttle,float beta,float betaRate){
  if(!active||!std::isfinite(dt)||dt<=0||dt>.1f||!std::isfinite(throttle)||
     !std::isfinite(beta)||!std::isfinite(betaRate)){held=0;return throttle;}
  const float growth=std::copysign(1.f,beta)*betaRate;
  if(std::abs(beta)>.4f&&growth>.35f&&std::abs(beta)+.25f*growth>.7f)held=.2f;
  else held=std::max(0.f,held-dt);
  // A stable large-angle slide retains power. Only rapidly increasing slip
  // near the existing .72rad unwind threshold asks the driver to lift briefly.
  // Meter power briefly instead of cutting it completely. The separate
  // measured-yaw guard prevents another rear-brake kick while rotating.
  // Keep the original native driver's half-gas envelope and entry momentum.
  return held>0?std::min(throttle,.5f):throttle;
 }
};
