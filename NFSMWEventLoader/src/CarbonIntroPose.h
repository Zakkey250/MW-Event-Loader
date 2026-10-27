#pragma once
#include <cmath>
namespace mwel_carbon {
// Native racestart.Position/Rotation already use the editor's map XY/Z-up
// and degrees. Preserve those exact values: an animated car is not a locator.
inline bool StartMarkerPose(float* pose,const float* position,const float* rotation){
 if(!pose||!position||!rotation)return false;
 const float result[]={position[0],position[1],position[2],*rotation};
 for(float value:result)if(!std::isfinite(value)||std::abs(value)>100000)return false;
 for(unsigned i=0;i<4;++i)pose[i]=result[i];return true;
}
}
