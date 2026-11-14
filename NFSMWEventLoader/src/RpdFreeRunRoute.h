#pragma once
#include "SceneRoadPath.h"
#include <utility>
// Persistent connected blue-road guidance, independent of race gates. Physics,
// steering, braking and stuck recovery remain NFRR's native CustomAI actuators.
namespace rpd_free_run {
using namespace mwel_scene_road;
// Independent placement query. Never advance the suspect's live route cursor,
// substitute a nearby bridge, or invent a point beyond a connected road end.
inline bool ReinforcementAhead(const Graph& graph,Point position,Point forward,Frame& out){
 if(!Finite(position)||!Finite(forward))return false;
 forward.z=0;if(Length(forward)<.9f)return false;
 Path path;if(!path.Build(graph,position,Unit(forward),10,130))return false;
 return path.Sample(100,out)&&std::abs(out.center.z-position.z)<20;
}
class Route {
 Path path_;float station_=0,lane_=0;Point previous_{};bool sampled_=false,laneChosen_=false;
public:
 bool Recovery(float ahead,Frame& out,float laneShift=0)const{
  if(!std::isfinite(laneShift))return false;
  if(!path_.Sample(station_+ahead,out))return false;
  out.center=out.center+Point{out.forward.y,-out.forward.x,0}*std::clamp(path_.lateral+laneShift,-6.f,6.f);return true;
 }
 template<class Floor,class Clear>bool RebuildRecoveryClear(const Graph& graph,Point position,Point forward,Floor&& floor,Clear&& clear){
  // An off-road impact can invalidate the live lookahead. Rejoin a real
  // nearby road at the same height, then follow its connected continuation.
  float best=144;Point center{},direction{};unsigned chosen=~0u;
  for(unsigned edge=0;edge<graph.edges.size();++edge){const auto& e=graph.edges[edge];
   if(e.a>=graph.nodes.size()||e.b>=graph.nodes.size())continue;
   const auto a=graph.nodes[e.a],d=graph.nodes[e.b]-a;const float n=Dot(d,d);if(n<1)continue;
   const auto p=a+d*std::clamp(Dot(position-a,d)/n,0.f,1.f);
   // Graph heights are approximate. A real surface query owns deck identity;
   // merely increasing the graph tolerance could select a nearby overpass.
   if(std::abs(p.z-position.z)>6)continue;
   auto along=Unit(d);if(Dot(along,forward)<0)along=along*-1;
   const Point right{along.y,-along.x,0};const float current=std::clamp(Dot(position-p,right),-6.f,6.f);
   // A divided road's graph reference may itself be in an unsupported median.
   // Validate the actual lane, and retain that offset in every recovery sample.
   for(float lane:{current,current+2,current-2,current+4,current-4,current+6,current-6,0.f}){
    if(std::abs(lane)>6)continue;const auto candidate=p+right*lane;
    const float distance=Dot(candidate-position,candidate-position);if(distance>=best||!floor(candidate,position.z)||!clear(candidate,along))continue;
    center=candidate;direction=along;best=distance;chosen=edge;
   }
  }
  if(chosen==~0u)return false;
  // The surface callback has established the real deck. A second nearest-edge
  // search can prefer the unverified deck above because graph heights are
  // biased. Pin this anchor, then traverse only its connected continuation.
  Path next;if(!next.Build(graph,center,direction,10,120,{},chosen))return false;
  path_=std::move(next);station_=0;sampled_=false;return true;
 }
 template<class Floor>bool RebuildRecoverySupported(const Graph& graph,Point position,Point forward,Floor&& floor){
  return RebuildRecoveryClear(graph,position,forward,std::forward<Floor>(floor),[](Point,Point){return true;});
 }
 bool RebuildRecovery(const Graph& graph,Point position,Point forward){
  return RebuildRecoverySupported(graph,position,forward,[](Point p,float height){return std::abs(p.z-height)<=1.5f;});
 }
 void Reset(){path_={};station_=0;sampled_=false;laneChosen_=false;lane_=0;}
 bool Aim(const Graph& graph,Point position,Point forward,float speed,Point& aim){
  if(!Finite(position)||!Finite(forward)||!std::isfinite(speed)||Length(forward)<.9f)return false;
  if(sampled_&&Length(position-previous_)>60)Reset();
  previous_=position;sampled_=true;
  auto build=[&](){
   for(float extent:{400.f,240.f,120.f})if(path_.Build(graph,position,forward,10,extent)){station_=0;return true;}
   path_={};return false;
  };
  if(path_.samples.empty()&&!build())return false;
  float best=std::numeric_limits<float>::infinity(),projected=station_;
  // Search only this route's nearby forward window; never a parallel road or
  // a future loop. Reject another bridge by height before considering distance.
  for(const auto& sample:path_.samples){float s=sample.station-path_.anchor;
   if(s<station_-8||s>station_+65||std::abs(sample.p.z-position.z)>5)continue;
   float distance=Dot(sample.p-position,sample.p-position);
   if(distance<best){best=distance;projected=s;}
  }
  if(best>225){Reset();return false;}
  station_=std::max(station_,projected);
  if(path_.samples.back().station-path_.anchor-station_<75&&!build())return false;
  // Bounded physical-speed lookahead for connected road guidance. No
  // throttle boost, velocity/pose write or invented point beyond a road edge.
  Frame frame;float lookahead=std::clamp(std::abs(speed)*.8f+12.f,15.f,55.f);
  if(!path_.Sample(station_+lookahead,frame))return false;
  // The graph is a road reference, not a safe driving lane. Its centre can
  // run straight through the median columns of a divided tunnel.
  aim=frame.center+Point{frame.forward.y,-frame.forward.x,0}*std::clamp(path_.lateral,-6.f,6.f);return Finite(aim);
 }
 template<class Clear>bool AimClear(const Graph& graph,Point position,Point forward,float speed,Point& aim,Clear&& clear){
  if(!Aim(graph,position,forward,speed,aim))return false;
  auto select=[&](const Path& path,float station,Point& selected,float& selectedLane){
  Frame current;if(!path.Sample(station,current))return false;
  const Point right{current.forward.y,-current.forward.x,0};
  const float offset=std::clamp(Dot(position-current.center,right),-6.f,6.f);
  if(!laneChosen_){lane_=offset;laneChosen_=true;}
  const float horizon=std::clamp(std::abs(speed)*.8f+12.f,15.f,55.f);
  // Keep the last clear lane. Re-test the actual chassis-width corridor,
  // including the straight chord the native steering controller will see.
  // Shorter supported targets avoid cutting a bend across an inside pillar.
  const float lanes[]={lane_,offset,offset+2,offset-2,offset+4,offset-4,offset+6,offset-6,0};
  // Inspect beyond the steering target, far enough to start changing lane
  // before a high-speed car reaches a pillar or an unsupported graph branch.
  const float available=path.samples.back().station-path.anchor-station;
  const float safety=std::min(available-1,std::clamp(speed*speed/20.f+20.f,55.f,220.f));
  for(float distance:{horizon,horizon*.75f,horizon*.5f,8.f}){
   Frame end;if(!path.Sample(station+distance,end))continue;
   for(float lane:lanes){
    if(std::abs(lane)>6)continue;
    auto target=end.center+Point{end.forward.y,-end.forward.x,0}*lane;
    if(!clear(position,target))continue;
    Point previous=position;bool valid=true;
    const float inspected=std::max(distance,safety);
    const unsigned steps=std::max(1u,unsigned(std::ceil(inspected/4.f)));
    for(unsigned i=1;i<=steps;++i){Frame sample;const float ahead=inspected*i/steps;
     if(!path.Sample(station+ahead,sample)){valid=false;break;}
     const float blend=std::clamp(ahead/std::max(12.f,distance*.65f),0.f,1.f);
     const float lateral=offset+(lane-offset)*blend;
     auto next=sample.center+Point{sample.forward.y,-sample.forward.x,0}*lateral;
     if(!clear(previous,next)){valid=false;break;}previous=next;
    }
    if(valid){selectedLane=lane;selected=target;return true;}
   }
  }
  return false;
  };
  float selectedLane=lane_;if(select(path_,station_,aim,selectedLane)){lane_=selectedLane;return true;}
  // The visually straight graph branch is not necessarily driveable. Try
  // connected alternatives using the exact same full-footprint test before
  // publishing a new path; do not jump to a nearby bridge or parallel road.
  Path next;unsigned attempts=0;Point target;float nextLane=lane_;
  const float extent=std::clamp(speed*speed/20.f+40.f,100.f,240.f);
  if(ChoosePath(graph,position,forward,10,extent,next,[&](const Path& candidate){return select(candidate,0,target,nextLane);},attempts,16)){
   path_=std::move(next);station_=0;lane_=nextLane;aim=target;return true;
  }
  // A blocked corridor must not silently revert to an unchecked centre-line
  // destination. Native braking and the existing five-second recovery remain.
  aim=position;return true;
 }
};
}
