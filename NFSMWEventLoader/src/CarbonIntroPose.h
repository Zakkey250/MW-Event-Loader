#pragma once
#include <cmath>
namespace mwel_carbon {
// Scene matrices use map XY with Z up; the live body uses game XYZ with Y up.
inline bool NativeStartPose(float* pose,const float* source,const float* position,const float* heading){
 if(!pose||!source||!position||!heading)return false;
 for(unsigned i=0;i<3;++i)if(!std::isfinite(position[i])||!std::isfinite(heading[i]))return false;
 const unsigned indices[]={0,1,12,13};for(unsigned i:indices)if(!std::isfinite(source[i]))return false;
 if(std::hypot(heading[0],heading[2])<.01f||std::hypot(source[0],source[1])<.01f)return false;
 // EN1.3's 45E1A0 maps render row 0 to the physical forward row as (-Y,Z,X).
 // Subtract that axis's XY angle. The side-axis/swapped-angle expression
 // adds the source yaw instead, reversing cars for a +/-90-degree source.
 const float radians=std::atan2(-heading[0],heading[2])-std::atan2(source[1],source[0]);
 const float c=std::cos(radians),s=std::sin(radians);
 const float result[]={position[2]-(source[12]*c-source[13]*s),
                       -position[0]-(source[12]*s+source[13]*c),
                       position[1],radians*180.f/3.141592654f+90.f};
 for(unsigned i=0;i<4;++i)pose[i]=result[i];return true;
}
}
