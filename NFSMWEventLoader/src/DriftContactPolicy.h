#pragma once
#include <cmath>
namespace mwel_drift_contact {
// MW impulse/mass is the measured velocity impulse. Ignore road support,
// disposable scenery, cars and numeric noise; side-wall scraping counts too.
inline bool Wall(float impulse,float mass,float normalY,bool breakable,bool vehicle){
 return !breakable&&!vehicle&&std::isfinite(impulse)&&std::isfinite(mass)&&std::isfinite(normalY)&&
  mass>=100&&mass<=20000&&std::abs(normalY)<.7f&&impulse/mass>=.25f;
}
}
