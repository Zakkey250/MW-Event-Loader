#pragma once
#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>
#include <queue>
#include <set>
// Map coordinates (XY, Z up). One connected road is selected at staging;
// samples never jump to a nearby parallel road or a different bridge.
// Branch alternatives only select connected continuations; geometry decides
// whether an entire scene can use a candidate before any vehicle is bound.
namespace mwel_scene_road {
struct Point {float x=0,y=0,z=0;};
inline Point operator+(Point a,Point b){return {a.x+b.x,a.y+b.y,a.z+b.z};}
inline Point operator-(Point a,Point b){return {a.x-b.x,a.y-b.y,a.z-b.z};}
inline Point operator*(Point a,float b){return {a.x*b,a.y*b,a.z*b};}
inline float Dot(Point a,Point b){return a.x*b.x+a.y*b.y;}
inline float Length(Point a){return std::hypot(a.x,a.y);}
inline Point Unit(Point a){float n=Length(a);return n>.001f?a*(1/n):Point{};}
inline bool Finite(Point a){return std::isfinite(a.x)&&std::isfinite(a.y)&&std::isfinite(a.z);}
struct Edge {unsigned a=0,b=0;};
struct Graph {std::vector<Point> nodes;std::vector<Edge> edges;};
struct Knot {Point p;float station=0;};
struct Frame {Point center,forward;};
struct Path {
 std::vector<Knot> samples;float anchor=0,lateral=0;std::vector<unsigned> decisionCounts,selectedBranches;
 bool Sample(float station,Frame& out)const{
  station+=anchor;if(samples.size()<2||station<0||station>samples.back().station)return false;
  auto it=std::upper_bound(samples.begin(),samples.end(),station,[](float s,const Knot& k){return s<k.station;});
  auto i=std::clamp(size_t(it-samples.begin()),size_t(1),samples.size()-1);
  const auto& a=samples[i-1];const auto& b=samples[i];float span=b.station-a.station;
  if(span<.0001f)return false;
  const float t=(station-a.station)/span;
  out.center=a.p+(b.p-a.p)*t;
  auto tangent=[&](size_t n){return Unit(samples[std::min(n+1,samples.size()-1)].p-samples[n?n-1:0].p);};
  out.forward=Unit(tangent(i-1)*(1-t)+tangent(i)*t);return true;
 }
 bool Build(const Graph& graph,Point origin,Point forward,float before,float after,const std::vector<unsigned>& branches={},unsigned anchorEdge=~0u){
  samples.clear();anchor=lateral=0;decisionCounts.clear();selectedBranches.clear();
  if(!Finite(origin)||!Finite(forward)||!std::isfinite(before)||!std::isfinite(after)||before<0||after<0||before+after>3000||graph.edges.size()>65535||graph.nodes.size()>65535)return false;
  forward=Unit(forward);if(Length(forward)<.9f)return false;
  std::vector<std::vector<unsigned>> adjacency(graph.nodes.size());
  float best=std::numeric_limits<float>::infinity();unsigned chosen=~0u;bool reverse=false;Point center{};
  for(unsigned i=0;i<graph.edges.size();++i){auto e=graph.edges[i];if(e.a>=graph.nodes.size()||e.b>=graph.nodes.size()||e.a==e.b)return false;
   auto a=graph.nodes[e.a],b=graph.nodes[e.b];if(!Finite(a)||!Finite(b))return false;
   adjacency[e.a].push_back(i);adjacency[e.b].push_back(i);auto d=b-a;float n=Dot(d,d);if(n<.01f)continue;
   if(anchorEdge!=~0u&&i!=anchorEdge)continue;
   float t=std::clamp(Dot(origin-a,d)/n,0.f,1.f);auto p=a+d*t;float align=Dot(Unit(d),forward);
   if(std::abs(p.z-origin.z)>6||std::abs(align)<.75f)continue;
   float score=Dot(origin-p,origin-p)+std::pow((p.z-origin.z)*2,2)+(1-std::abs(align))*16;
   if(score<best){best=score;chosen=i;center=p;reverse=align<0;}
  }
  if(chosen==~0u||best>144)return false;
  auto edge=graph.edges[chosen];Point along=Unit(graph.nodes[edge.b]-graph.nodes[edge.a]);if(reverse)along=along*-1;
  lateral=Dot(origin-center,{along.y,-along.x,0});
  auto walk=[&](bool backwards,float needed,std::vector<Point>& points){
   unsigned current=reverse?edge.a:edge.b,previous=reverse?edge.b:edge.a;if(backwards)std::swap(current,previous);
   Point last=center,direction=backwards?along*-1:along;float distance=0;std::vector<bool> used(graph.edges.size());used[chosen]=true;
   for(unsigned steps=0;steps<graph.edges.size()+1;++steps){
    auto p=graph.nodes[current];float length=Length(p-last);if(length>.01f){points.push_back(p);distance+=length;direction=Unit(p-last);last=p;}
    if(distance>=needed+20)return true;
    struct Choice{unsigned node,edge;float fit;};std::vector<Choice> candidates;
    for(auto id:adjacency[current]){if(used[id])continue;auto e=graph.edges[id];unsigned n=e.a==current?e.b:e.a;if(n==previous)continue;
     float fit=Dot(direction,Unit(graph.nodes[n]-p));if(fit>.25f)candidates.push_back({n,id,fit});}
    if(candidates.empty())return false;
    std::stable_sort(candidates.begin(),candidates.end(),[](auto a,auto b){return a.fit>b.fit;});
    unsigned choice=0;if(candidates.size()>1){unsigned index=decisionCounts.size();decisionCounts.push_back(unsigned(candidates.size()));choice=index<branches.size()?branches[index]:0;selectedBranches.push_back(choice);if(choice>=candidates.size())return false;}
    auto next=candidates[choice];used[next.edge]=true;previous=current;current=next.node;
   }return false;
  };
  std::vector<Point> back,ahead;if(!walk(true,before,back)||!walk(false,after,ahead))return false;
  std::reverse(back.begin(),back.end());std::vector<Point> chain=back;const auto pivot=chain.size();chain.push_back(center);chain.insert(chain.end(),ahead.begin(),ahead.end());
  // C1 Hermite interpolation avoids heading snaps at blue-line vertices.
  // Bound tangent lengths; collision/grade preflight remains authoritative.
  samples.push_back({chain.front(),0});float distance=0;
  auto tangent=[&](size_t i){auto a=i?chain[i-1]:chain[i],b=i+1<chain.size()?chain[i+1]:chain[i];return Unit(b-a);};
  for(size_t i=1;i<chain.size();++i){auto a=chain[i-1],b=chain[i];float span=Length(b-a);auto ta=tangent(i-1)*span,tb=tangent(i)*span;unsigned count=std::max(2u,unsigned(std::ceil(span/.75f)));
   for(unsigned j=1;j<=count;++j){float t=float(j)/count,t2=t*t,t3=t2*t;auto p=a*(2*t3-3*t2+1)+ta*(t3-2*t2+t)+b*(-2*t3+3*t2)+tb*(t3-t2);float step=Length(p-samples.back().p);if(step<.00001f)continue;distance+=step;samples.push_back({p,distance});}
   if(i==pivot)anchor=distance;
  }
  Frame frame;return Sample(-before,frame)&&Sample(after,frame);
 }
};
// Prefer the original straight continuation. If its complete physical preflight
// fails, try bounded alternatives at connected junctions, closest preference
// first. Never publish a partially checked path or alter the requested locator.
template<class Validate>bool ChoosePath(const Graph& graph,Point origin,Point forward,float before,float after,
 Path& accepted,Validate&& validate,unsigned& attempts,unsigned limit=64){
 struct Candidate {std::vector<unsigned> branches;unsigned cost=0,order=0;};
 const auto later=[](const Candidate& a,const Candidate& b){return a.cost!=b.cost?a.cost>b.cost:a.order>b.order;};
 std::priority_queue<Candidate,std::vector<Candidate>,decltype(later)> pending(later);
 std::set<std::vector<unsigned>> seen;pending.push({});seen.insert(std::vector<unsigned>{});attempts=0;unsigned serial=0;limit=std::min(limit,64u);
 while(!pending.empty()&&attempts<limit){
  auto candidate=pending.top();pending.pop();Path path;++attempts;
  if(path.Build(graph,origin,forward,before,after,candidate.branches)&&validate(path)){accepted=std::move(path);return true;}
  for(unsigned i=0;i<path.decisionCounts.size()&&i<path.selectedBranches.size()&&i<64&&seen.size()<limit*4;++i){
   if(path.selectedBranches[i]+1>=path.decisionCounts[i])continue;
   std::vector<unsigned> branches(path.selectedBranches.begin(),path.selectedBranches.begin()+i+1);++branches.back();
   if(!seen.insert(branches).second)continue;
   unsigned cost=unsigned(branches.size());for(auto branch:branches)cost+=512*branch;
   pending.push({std::move(branches),cost,++serial});
  }
 }
 return false;
}
}
