#pragma once
#include "../../src/GeometryQueries.h"
// Use the same active-world geometry provider as grid placement and NeoAI.
// The old 44A3C0 helper only knows native MW height data in the W2C world.
struct SceneGround {
 event_geometry::WorldQueries geometry;
 bool diagnostics=false;unsigned hits=0,misses=0;
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
