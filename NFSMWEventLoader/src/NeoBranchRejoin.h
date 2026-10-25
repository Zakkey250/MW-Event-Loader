#pragma once
#include "../../World2MW/src/NeoAI/Types.h"
namespace mwel_branch {
struct Rejoin {bool valid=false;float station=0,distance=0;};
// A branch and its main road have different distance coordinates. Find a
// physically reachable main-road point BEFORE changing the committed road.
// Failure returns no station; the caller must retain the existing branch.
template<class Road,class Accept> Rejoin FindRejoin(const Road& main,mwneo::Point position,float radius,Accept&& accept){
 Rejoin best;best.distance=radius;
 for(unsigned i=1;i<main.size();++i){const auto& a=main[i-1];const auto& b=main[i];
  float dx=b.p[0]-a.p[0],dz=b.p[2]-a.p[2],square=dx*dx+dz*dz;if(square<.001f)continue;
  float t=std::clamp(((position.x-a.p[0])*dx+(position.z-a.p[2])*dz)/square,0.f,1.f);
  mwneo::Point point{a.p[0]+t*dx,a.p[1]+t*(b.p[1]-a.p[1]),a.p[2]+t*dz};
  float distance=std::hypot(point.x-position.x,point.z-position.z);
  if(distance>=best.distance||!accept(point))continue;
  best={true,a.station+t*(b.station-a.station),distance};
 }
 return best;
}
}
