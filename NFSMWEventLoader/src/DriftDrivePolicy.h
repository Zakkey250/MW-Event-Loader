#pragma once
#include <cmath>
namespace mwel_drift_drive {
// MW competition balance, not a Carbon/UG2 scoring constant. Their verified
// scoring path has no drive-layout coefficient. Read the installed car's
// transmission, never infer drive from a model name or change shared VLT data.
inline float AwardScale(float frontShare,bool player=false){
 return std::isfinite(frontShare)&&frontShare>0&&frontShare<1?(player?.5f:.85f):1.f;
}
}
