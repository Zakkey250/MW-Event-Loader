#pragma once
#include <cmath>
// Native action and mechanic callbacks can both occur in one physical step.
// Advancing a controller/recovery timer twice would distort every course.
struct NeoSimulationStep {
 float previous=-1;
 bool Begin(float now){
  if(!std::isfinite(now)||now<0||now==previous)return false;
  previous=now;return true;
 }
};
