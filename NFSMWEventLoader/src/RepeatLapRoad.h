#pragma once
#include <algorithm>
#include <cmath>
#include <vector>

// The grid approach belongs to lap zero only. Retain the chosen road's
// suffix from the lap line, without changing native CP/lap state or snapping
// to another nearby road. Points expose p[3] and cumulative station.
namespace mwel_lap {
template<class Point>
bool TrimApproach(std::vector<Point>& road,std::vector<unsigned>& flags,const float finish[3]){
 if(road.size()<2||flags.size()!=road.size())return false;
 for(unsigned k=0;k<3;++k)if(!std::isfinite(finish[k]))return false;
 float best=1e30f,station=0,tChosen=0;size_t segment=0;
 for(size_t i=1;i<road.size();++i){
  const auto& a=road[i-1];const auto& b=road[i];float n=0,dot=0;
  if(!std::isfinite(a.station)||!std::isfinite(b.station)||b.station<a.station)return false;
  for(unsigned k=0;k<3;++k){
   if(!std::isfinite(a.p[k])||!std::isfinite(b.p[k]))return false;
   float v=b.p[k]-a.p[k];n+=v*v;dot+=(finish[k]-a.p[k])*v;
  }
  if(n<.000001f)continue;
  float t=std::clamp(dot/n,0.f,1.f),distance=0;
  for(unsigned k=0;k<3;++k){float d=a.p[k]+t*(b.p[k]-a.p[k])-finish[k];distance+=d*d;}
  float s=a.station+t*(b.station-a.station);
  // Coincident but nonadjacent course visits cannot select a future loop.
  if(distance<=.0625f&&best<=.0625f&&std::abs(s-station)>1)return false;
  if(distance<best){best=distance;station=s;segment=i;tChosen=t;}
 }
 if(best>.0625f||road.back().station-station<1)return false;
 Point start=road[segment-1];
 for(unsigned k=0;k<3;++k)start.p[k]+=tChosen*(road[segment].p[k]-start.p[k]);
 start.station=0;
 std::vector<Point> suffix{start};std::vector<unsigned> suffixFlags{flags[segment]};
 for(size_t i=segment;i<road.size();++i){
  auto p=road[i];p.station-=station;
  if(p.station<=.001f)continue;
  suffix.push_back(p);suffixFlags.push_back(flags[i]);
 }
 if(suffix.size()<2)return false;
 road=std::move(suffix);flags=std::move(suffixFlags);return true;
}
}
