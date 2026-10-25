#pragma once
#include "../../World2MW/src/NeoAI/RouteGraph.h"
#include <limits>
namespace mwel_race_route {
// Ranking model only: this value never enters throttle, speed targets or
// physics. Prefer a short smooth route instead of a uniformly random detour.
inline float TurnCost(const mwneo::Graph& graph,unsigned a,unsigned b,float grip){
 const auto& before=graph.edges[a];const auto& after=graph.edges[b];
 float curve=mwneo::Curve(graph.nodes[before.from].position,graph.nodes[before.to].position,graph.nodes[after.to].position);
 float speed=curve>.0001f?std::min(90.f,std::sqrt(grip/curve)):90.f;
 return std::min({before.length,after.length,60.f})*(1/speed-1/90.f);
}
template<class Excluded>std::vector<float> Costs(const mwneo::Graph& graph,float grip,Excluded excluded){
 const float inf=std::numeric_limits<float>::infinity();std::vector<float> costs(graph.edges.size(),inf);
 std::vector<unsigned> degree(graph.nodes.size()),order;
 for(const auto& edge:graph.edges)++degree[edge.to];
 for(unsigned n=0;n<degree.size();++n)if(!degree[n])order.push_back(n);
 for(unsigned i=0;i<order.size();++i)for(auto e:graph.nodes[order[i]].successors)if(--degree[graph.edges[e].to]==0)order.push_back(graph.edges[e].to);
 if(order.size()!=graph.nodes.size()||!std::isfinite(grip)||grip<=0)return costs;
 for(auto n=order.rbegin();n!=order.rend();++n)for(auto e:graph.nodes[*n].successors){
  const auto& edge=graph.edges[e];if(excluded(edge.branch))continue;
  float tail=edge.to==graph.finish?0.f:inf;
  for(auto next:graph.nodes[edge.to].successors)tail=std::min(tail,costs[next]+TurnCost(graph,e,next,grip));
  costs[e]=edge.length/90.f+tail;
 }
 return costs;
}
inline unsigned Choose(const mwneo::Graph& graph,const std::vector<float>& costs,unsigned node,unsigned previous,unsigned mix,float grip){
 const auto& choices=graph.nodes[node].successors;unsigned best=~0u;float score=std::numeric_limits<float>::infinity();
 for(auto e:choices){float value=costs[e]+(previous<graph.edges.size()?TurnCost(graph,previous,e,grip):0);
  if(value<score){score=value;best=e;}}
 // Keep variety only between effectively equivalent alternatives. A slower
 // choice is not justified just because it has a different random seed.
 if(best!=~0u){unsigned count=0;for(auto e:choices){float value=costs[e]+(previous<graph.edges.size()?TurnCost(graph,previous,e,grip):0);if(value<=score+.15f)++count;}
  if(count){unsigned pick=mix%count;for(auto e:choices){float value=costs[e]+(previous<graph.edges.size()?TurnCost(graph,previous,e,grip):0);if(value<=score+.15f){if(!pick)return e;--pick;}}}}
 return best;
}
}
