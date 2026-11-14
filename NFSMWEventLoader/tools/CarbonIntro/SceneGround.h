#pragma once
#include "../../src/GeometryQueries.h"
#include "../../src/SceneRoadCamera.h"
// Use the same active-world geometry provider as grid placement and NeoAI.
// The old 44A3C0 helper only knows native MW height data in the W2C world.
struct SceneGround {
 event_geometry::WorldQueries geometry;
 bool diagnostics=false;unsigned hits=0,misses=0;
 bool CameraPocketClear(const D3DXVECTOR3& eye){
  if(geometry.nodes.empty())return false;
  event_geometry::WorldQueries::RayQueryScope scope(geometry);
  for(D3DXVECTOR3 d:{D3DXVECTOR3(.4f,0,0),D3DXVECTOR3(-.4f,0,0),D3DXVECTOR3(0,.4f,0),D3DXVECTOR3(0,-.4f,0),D3DXVECTOR3(0,0,.4f),D3DXVECTOR3(0,0,-.4f)}){
   const auto p=eye+d;event_geometry::RayHit hit;if(geometry.Ray({-eye.y,eye.z,eye.x},{-p.y,p.z,p.x},hit,true,true))return false;
  }return true;
 }
 bool ConstrainCamera(const D3DXVECTOR3& focus,D3DXVECTOR3& eye){
  if(geometry.nodes.empty())return false;
  event_geometry::WorldQueries::RayQueryScope scope(geometry);
  mwel_scene_road::Point point{eye.x,eye.y,eye.z};
  const bool clipped=mwel_scene_road::ClipCameraBoom({focus.x,focus.y,focus.z},point,[&](auto a,auto b,float& fraction){
   event_geometry::RayHit hit;if(!geometry.Ray({-a.y,a.z,a.x},{-b.y,b.z,b.x},hit,true,true))return false;fraction=hit.fraction;return true;
  });
  if(clipped)eye={point.x,point.y,point.z};return clipped;
 }
 bool Query(const D3DXVECTOR3& map,float& height,D3DXVECTOR3* normal=nullptr){
  if(!geometry.nodes.empty()){
   event_geometry::WorldQueries::RayQueryScope scope(geometry);
   event_geometry::RayHit hit;
   // Map (X,Y,Z up) -> game (-Y,Z up,X). Keep the ray local in height
   // so a bridge above the start cannot become the actor's ground.
   if(geometry.Ray({-map.y,map.z+5,map.x},{-map.y,map.z-12,map.x},hit,true,false)
      &&std::isfinite(hit.point.y)&&std::abs(hit.normal.y)>.6f){
    height=hit.point.y;if(normal){*normal={hit.normal.z,-hit.normal.x,hit.normal.y};if(normal->z<0)*normal=-*normal;}if(diagnostics)++hits;return true;
   }
   if(diagnostics)++misses;return false; // Never mix W2C and MW ground on a provider miss.
  }
  D3DXVECTOR3 p(-map.y,map.z+2,map.x),n;
  height=map.z;
  bool ok=reinterpret_cast<bool(__cdecl*)(const D3DXVECTOR3*,float*,D3DXVECTOR3*)>(0x44A3C0)(&p,&height,&n);
  if(ok&&std::isfinite(height)&&fabs(height-map.z)<20){if(normal)*normal={n.z,-n.x,n.y};if(diagnostics)++hits;return true;}
  if(diagnostics)++misses;return false;
 }
} sceneGround;
