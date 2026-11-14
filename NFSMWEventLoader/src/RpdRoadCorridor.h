#pragma once
#include "SceneRoadPath.h"
namespace rpd_free_run {
// A footprint query may already contain the real floor height. Applying the
// graph-to-floor offset to its Z again lowers the tyre rays below the road.
inline float ProjectedFloorHeight(const mwel_scene_road::Frame& frame,mwel_scene_road::Point p,float centerFloor){
 const float horizontal=frame.forward.x*frame.forward.x+frame.forward.y*frame.forward.y;
 if(!mwel_scene_road::Finite(frame.center)||!mwel_scene_road::Finite(frame.forward)||!mwel_scene_road::Finite(p)||!std::isfinite(centerFloor)||horizontal<.0001f)return std::numeric_limits<float>::quiet_NaN();
 return centerFloor+((p.x-frame.center.x)*frame.forward.x+(p.y-frame.center.y)*frame.forward.y)*frame.forward.z/horizontal;
}
// Map coordinates. The road graph's height is approximate; project each short
// footprint onto the real surface before sweeping the chassis. A long 3-D ray
// through an uphill road incorrectly reports the road itself as an obstacle.
template<class Floor,class Wall>bool RoadCorridor(mwel_scene_road::Point a,mwel_scene_road::Point b,Floor&& floor,Wall&& wall){
 using namespace mwel_scene_road;
 if(!Finite(a)||!Finite(b))return false;
 const float length=Length(b-a);if(length<.01f)return false;
 const auto along=Unit(b-a);const Point side{along.y,-along.x,0};
 float previousHeight=0;if(!floor(a,previousHeight))return false;
 Point previous=a;previous.z=previousHeight;
 const unsigned steps=std::max(1u,unsigned(std::ceil(length/3.f)));
 for(unsigned i=1;i<=steps;++i){
  auto next=a+(b-a)*(float(i)/steps);float height=0;
  if(!floor(next,height)||std::abs(height-previousHeight)>Length(next-previous)*.45f+.35f)return false;
  next.z=height;
  for(float lateral:{-1.1f,1.1f}){float tyreHeight=0;if(!floor(next+side*lateral,tyreHeight)||std::abs(tyreHeight-height)>.7f)return false;}
  for(float lateral:{-1.4f,0.f,1.4f})for(float lift:{.45f,1.25f}){
   auto p=previous+side*lateral,q=next+side*lateral;p.z+=lift;q.z+=lift;
   if(wall(p,q))return false;
  }
  previous=next;previousHeight=height;
 }
 return true;
}
}
