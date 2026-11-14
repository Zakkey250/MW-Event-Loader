#pragma once
#include <array>
#include <cmath>
#include <algorithm>
namespace touge_fall_policy {
using Point=std::array<float,3>; // Authored X/Y ground plane, Z elevation.
struct Gate {Point center{};float depth=0,width=0,rotation=0;};
inline bool Inside(const Gate& g,const Point& p){
 float r=g.rotation*.017453292519943295f,dx=p[0]-g.center[0],dy=p[1]-g.center[1];
 return std::abs(dx*std::cos(r)+dy*std::sin(r))<=g.depth*.5f&&std::abs(dx*std::sin(r)-dy*std::cos(r))<=g.width*.5f;
}
inline bool Continuous(const Point& previous,const Point& current){
 float distance=0;for(unsigned i=0;i<3;++i){if(!std::isfinite(previous[i])||!std::isfinite(current[i]))return false;float d=current[i]-previous[i];distance+=d*d;}
 return distance<=80.f*80.f;
}
inline bool DownCrossing(const Gate& g,const Point& previous,const Point& current){
 for(unsigned i=0;i<3;++i)if(!std::isfinite(previous[i])||!std::isfinite(current[i])||!std::isfinite(g.center[i]))return false;
 if(!Continuous(previous,current)||previous[2]<=g.center[2]||current[2]>g.center[2])return false;
 float t=(previous[2]-g.center[2])/(previous[2]-current[2]);Point intersection;
 for(unsigned i=0;i<3;++i)intersection[i]=previous[i]+t*(current[i]-previous[i]);
 return Inside(g,intersection);
}
inline bool BelowRoad(float roadHeight,float bodyHeight,float verticalSpeed,float horizontalDistance){
 return std::isfinite(roadHeight)&&std::isfinite(bodyHeight)&&std::isfinite(verticalSpeed)&&std::isfinite(horizontalDistance)&&horizontalDistance<=120.f&&roadHeight-bodyHeight>=12.f&&verticalSpeed<-2.f;
}
}
