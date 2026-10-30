#pragma once
#include <algorithm>
#include <cmath>
namespace mwel_drift_speed {
// Route/traffic clearance still wins over this power-slide envelope.
// These are AI planning speeds, never score factors or velocity writes.
// Tight hairpins may shed speed through native tyres; broad corners retain
// the high-speed envelope instead of requiring a strong velocity rotation.
constexpr float Minimum=14.f,Maximum=48.f,EntryMaximum=38.f,LateralAcceleration=12.f;
inline float Corner(float peak){return std::clamp(std::sqrt(LateralAcceleration/std::max(.003f,peak)),Minimum,Maximum);}
}
