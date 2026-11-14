#pragma once
#include "SceneRoadPath.h"
namespace rpd_free_run {
// Native XYZ matrix; +Y is up and row 2 is IRigidBody::GetForwardVector.
inline bool RecoveryMatrix(const float* position,const float* heading,const float* normal,float* out){
 if(!position||!heading||!normal||!out)return false;
 using namespace mwel_scene_road;
 const auto dot=[](Point a,Point b){return a.x*b.x+a.y*b.y+a.z*b.z;};
 const auto length=[&](Point p){return std::sqrt(dot(p,p));};
 const auto unit=[&](Point p){float n=length(p);return n>.001f?p*(1.f/n):Point{};};
 Point p{position[0],position[1],position[2]},forward{heading[0],heading[1],heading[2]},up{normal[0],normal[1],normal[2]};
 if(!Finite(p)||!Finite(forward)||!Finite(up)||length(up)<.9f)return false;
 up=unit(up);if(up.y<.65f)return false;
 forward=unit(forward-up*dot(forward,up));if(length(forward)<.9f)return false;
 const Point right{up.y*forward.z-up.z*forward.y,up.z*forward.x-up.x*forward.z,up.x*forward.y-up.y*forward.x};
 const float matrix[]={right.x,right.y,right.z,0,up.x,up.y,up.z,0,forward.x,forward.y,forward.z,0,p.x,p.y,p.z,1};
 std::copy(std::begin(matrix),std::end(matrix),out);return true;
}
}
