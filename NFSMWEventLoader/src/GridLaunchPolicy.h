#pragma once
#include "../../World2MW/src/NeoAI/Types.h"
#include <vector>
namespace mwel_grid {
// Native CP/lap progress can change before the body reaches the next road's
// canonical entry. A moving car must physically approach that entry; only
// the original initial-grid case keeps its separate speed/count restriction.
inline bool ApproachCandidate(unsigned count,unsigned lap,int leg,float station,float speed){
 return count>=1&&count<=16&&leg>=0&&std::isfinite(station)&&station>=0&&station<.1f&&std::isfinite(speed)&&
  (leg>0||lap>0||(count>4&&std::abs(speed)<15));
}
inline bool BeginApproach(bool& considered,unsigned count,unsigned lap,int leg,float station,float speed){
 if(considered||!ApproachCandidate(count,lap,leg,station,speed))return false;
 considered=true;return true;
}
inline bool BeforeRecoveryEntry(float committed,bool prefix,float localCursor,float signedEntry,float halfLength){
 return committed<.1f&&(!prefix||localCursor<.1f)&&signedEntry < -std::max(2.f,halfLength);
}
// Driving and recovery use exactly the same approach-to-canonical coordinate
// transform. The prefix begins at local zero; the canonical grid is at length.
template<class Point>void AppendCanonical(const std::vector<Point>& road,float length,std::vector<Point>& prefix){
 for(auto point:road){point.station+=length;if(!prefix.empty()&&point.station<=prefix.back().station+.001f)continue;prefix.push_back(point);}
}
struct Launch {
 mwneo::Point origin{},gate{},direction{};float length=0;
 bool Initialize(mwneo::Point body,mwneo::Point first,mwneo::Point second,float sideLimit=8){
  if(!mwneo::Finite(body)||!mwneo::Finite(first)||!mwneo::Finite(second))return false;
  auto delta=mwneo::Sub(second,first);if(mwneo::LengthXZ(delta)<.1f)return false;direction=mwneo::UnitXZ(delta);
  auto offset=mwneo::Sub(body,first);float behind=-mwneo::DotXZ(offset,direction),side=std::abs(mwneo::CrossXZ(direction,offset));
  if(!std::isfinite(sideLimit)||sideLimit<0||sideLimit>42||behind<1||behind>180||side>sideLimit||std::abs(offset.y)>12)return false;
  length=mwneo::Distance(body,first);origin=body;gate=first;return std::isfinite(length)&&length>=1;
 }
 template<class Corridor> bool InitializeChecked(mwneo::Point body,mwneo::Point first,mwneo::Point second,float sideLimit,Corridor clear){
  Launch candidate;if(!candidate.Initialize(body,first,second,sideLimit)||!clear(body,first))return false;
  *this=candidate;return true;
 }
 bool Active(mwneo::Point body)const{return length>0&&mwneo::DotXZ(mwneo::Sub(body,gate),direction)<-.5f;}
 float Cursor(mwneo::Point body)const{
  auto delta=mwneo::Sub(gate,origin);float square=mwneo::DotXZ(delta,delta);if(square<=.001f)return 0;
  return length*std::clamp(mwneo::DotXZ(mwneo::Sub(body,origin),delta)/square,0.f,1.f);
 }
};
template<class Point>std::vector<Point> ApproachPrefix(const Launch& launch,const std::vector<Point>& surface){
 if(!surface.empty())return surface;
 std::vector<Point> prefix;
 if(!(launch.length>0))return prefix;
 unsigned steps=unsigned(std::ceil(launch.length/4));prefix.reserve(steps);
 for(unsigned i=0;i<steps;++i){float t=float(i)/steps;auto a=launch.origin,b=launch.gate;
  prefix.push_back({{a.x+(b.x-a.x)*t,a.y+(b.y-a.y)*t,a.z+(b.z-a.z)*t},launch.length*t});
 }return prefix;
}
}
