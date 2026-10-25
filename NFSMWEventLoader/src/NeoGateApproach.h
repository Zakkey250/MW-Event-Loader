#pragma once
// A curved/out-and-back road can pass beside a pending gate's plane before
// its real approach. Scope the aperture funnel to the final contiguous
// reference suffix inside that aperture. Native trigger state is untouched.
template<class Road> static float NeoGateApproachBegin(const Road& road,const YellowGatePassage& gate){
 if(road.size()<2||!gate.active)return -1e30f;
 const float width=std::max(0.f,gate.halfWidth-gate.bodyReserve);
 float begin=road.back().station;bool entered=false;
 for(float s=road.back().station;s>=road.front().station;s=std::max(road.front().station,s-2.f)){
  Vec3 p;if(!YellowLineSample(road,s,p))break;
  float dx=p.x-gate.center.x,dz=p.z-gate.center.z;
  float lateral=-dx*gate.normal.z+dz*gate.normal.x,along=dx*gate.normal.x+dz*gate.normal.z;
  if(std::abs(lateral)>width||std::abs(along)>18||std::abs(p.y-gate.center.y)>gate.halfHeight)break;
  entered=true;begin=s;if(s==road.front().station)break;
 }
 return entered?begin:-1e30f;
}
