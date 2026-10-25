#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <vector>

// Canonical station measures the authored main road. Branch endpoints map
// back to that station so a shorter alternative does not change race length.
namespace mwel_progress {
struct Point {std::array<float,3> position{};float station=0;};
using Path=std::vector<Point>;
struct Leg {Path main;std::vector<Path> alternatives;float Length()const{return main.empty()?0:main.back().station;}};
struct Projection {float station=0,distanceSquared=std::numeric_limits<float>::infinity();bool valid=false;};
inline Projection Project(const Leg& leg,const std::array<float,3>& body){
 Projection result;for(auto value:body)if(!std::isfinite(value))return result;
 auto visit=[&](const Path& path){for(size_t i=1;i<path.size();++i){auto& a=path[i-1];auto& b=path[i];float denominator=0,numerator=0;
  for(unsigned k=0;k<3;++k){float d=b.position[k]-a.position[k];denominator+=d*d;numerator+=(body[k]-a.position[k])*d;}
  if(denominator<.000001f)continue;float t=std::clamp(numerator/denominator,0.f,1.f),distance=0;
  for(unsigned k=0;k<3;++k){float d=body[k]-a.position[k]-t*(b.position[k]-a.position[k]);distance+=d*d;}
  if(distance<result.distanceSquared){result={a.station+t*(b.station-a.station),distance,true};}
 }};visit(leg.main);for(auto& path:leg.alternatives)visit(path);return result;
}
struct Progress {float station=0,remaining=0,lapPercent=0,racePercent=0;bool valid=false;};
inline Progress Measure(const std::vector<Leg>& legs,unsigned checkpoint,unsigned lap,unsigned laps,const std::array<float,3>& body){
 if(checkpoint>=legs.size()||!laps||lap>=laps)return {};
 float length=0,prefix=0;for(size_t i=0;i<legs.size();++i){float n=legs[i].Length();if(!std::isfinite(n)||n<=0)return {};length+=n;if(i<checkpoint)prefix+=n;}
 auto p=Project(legs[checkpoint],body);if(!p.valid||length<=0)return {};
 float station=std::clamp(p.station,0.f,legs[checkpoint].Length()),along=prefix+station;
 return {station,legs[checkpoint].Length()-station,100.f*along/length,100.f*(lap*length+along)/(laps*length),true};
}
}
