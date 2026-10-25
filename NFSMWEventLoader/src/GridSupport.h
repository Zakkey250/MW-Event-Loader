#pragma once
#include "GridPolicy.h"
#include "GeometryQueries.h"
namespace mwel_grid {
// The same road/body/barrier checks serve runtime preflight and replay tests.
template<class WallClear> Layout Supported(unsigned count,Position origin,float rotation,event_geometry::WorldQueries& world,WallClear clear){
 if(world.nodes.empty())return {};
 event_geometry::WorldQueries::RayQueryScope scope(world);
 return Plan(count,origin,rotation,[&](Position& p,float dx,float dz){
  float low=1e30f,high=-1e30f,center=0;unsigned n=0;
  for(auto offset:std::array<std::array<float,2>,5>{{{0,0},{-2,-1.1f},{-2,1.1f},{2,-1.1f},{2,1.1f}}}){
   const float x=p[0]+dx*offset[0]-dz*offset[1],z=p[2]+dz*offset[0]+dx*offset[1];event_geometry::RayHit hit;
   if(!world.Ray({x,p[1]+5,z},{x,p[1]-6,z},hit,true,false)||!std::isfinite(hit.point.y)||std::abs(hit.normal.y)<.65f)return false;
   if(!n++)center=hit.point.y;low=std::min(low,hit.point.y);high=std::max(high,hit.point.y);
  }
  if(high-low>2.f)return false;p[1]=center+.25f;
  w2mw::Vec3 corners[4];unsigned k=0;
  for(auto offset:std::array<std::array<float,2>,4>{{{-2.8f,-1.2f},{-2.8f,1.2f},{2.8f,1.2f},{2.8f,-1.2f}}})
   corners[k++]={p[0]+dx*offset[0]-dz*offset[1],p[1]+.8f,p[2]+dz*offset[0]+dx*offset[1]};
  event_geometry::RayHit wall;
  for(unsigned i=0;i<4;++i)if(world.Ray(corners[i],corners[(i+1)%4],wall,false,true))return false;
  if(world.Ray(corners[0],corners[2],wall,false,true)||world.Ray(corners[1],corners[3],wall,false,true))return false;
  return clear(p,dx,dz);
 });
}
}
