#pragma once
#include "../../World2MW/src/NeoAI/Types.h"
// A shared road has no unique global race direction. Resolve facing on the
// current ordered CP leg and its allowed branches, never the adjacent leg.
struct NeoCourseDirection {
 float distanceSquared=18.f*18.f,alignment=0;bool found=false;
 void Consider(mwneo::Point p,mwneo::Point heading,mwneo::Point a,mwneo::Point b){
  if(!mwneo::Finite(p)||!mwneo::Finite(heading)||!mwneo::Finite(a)||!mwneo::Finite(b))return;
  auto d=mwneo::Sub(b,a);float n=mwneo::DotXZ(d,d);if(n<.01f)return;
  float t=std::clamp(mwneo::DotXZ(mwneo::Sub(p,a),d)/n,0.f,1.f);
  mwneo::Point q{a.x+t*d.x,a.y+t*d.y,a.z+t*d.z};if(std::abs(p.y-q.y)>4.f)return;
  auto gap=mwneo::Sub(p,q);float distance=mwneo::DotXZ(gap,gap);if(distance>=distanceSquared)return;
  auto facing=mwneo::UnitXZ(heading);if(mwneo::LengthXZ(facing)<.9f)return;
  found=true;distanceSquared=distance;alignment=mwneo::DotXZ(facing,mwneo::UnitXZ(d));
 }
 bool Resolve(bool native)const{return !found||std::abs(alignment)<.25f?native:alignment<0;}
};
