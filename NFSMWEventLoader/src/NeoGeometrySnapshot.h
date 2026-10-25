#pragma once
#include "../../World2MW/src/NativeLoader/CollisionWorld.h"
#include <stdexcept>

namespace mwel_async_geometry {
using w2mw::Vec3;
struct Bounds {Vec3 low{},high{};};
inline bool Finite(Vec3 p){return std::isfinite(p.x)&&std::isfinite(p.y)&&std::isfinite(p.z);}
inline bool Valid(Bounds b){return Finite(b.low)&&Finite(b.high)&&b.low.x<=b.high.x&&b.low.y<=b.high.y&&b.low.z<=b.high.z;}
inline bool Contains(Bounds b,Vec3 p){return Finite(p)&&p.x>=b.low.x&&p.x<=b.high.x&&p.y>=b.low.y&&p.y<=b.high.y&&p.z>=b.low.z&&p.z<=b.high.z;}
struct Snapshot {
 Bounds bounds{};Vec3 padding{.001f,.001f,.001f};
 unsigned providerQueryGeneration=0; // Query epoch, not race/world ownership.
 std::vector<w2mw::CollisionTriangle> triangles;
};
// Call on the game thread inside its existing synchronous geometry query.
// QueryBox(false) preserves both floor and wall triangles and the provider's
// current scenery activation filter. No module callback/pointer survives here.
template<class Source> bool Capture(const Source& source,Bounds bounds,Snapshot& output,unsigned maximumTriangles=32768){
 output={};if(!Valid(bounds)||maximumTriangles==0)return false;
 std::vector<unsigned> ids;source.QueryBox(bounds.low,bounds.high,ids,false);
 if(ids.empty()||ids.size()>maximumTriangles)return false; // Never truncate into unsafe emptiness.
 Snapshot candidate;candidate.bounds=bounds;candidate.padding=source.WallRayPadding();candidate.providerQueryGeneration=source.QueryGeneration();
 if(!Finite(candidate.padding)||candidate.padding.x<0||candidate.padding.y<0||candidate.padding.z<0)return false;
 candidate.triangles.reserve(ids.size());
 for(auto id:ids){auto t=source.triangles[id];w2mw::CollisionTriangle copy{};copy.surface=t.surface;copy.wall=t.wall;
  if(copy.wall>1)return false;for(unsigned i=0;i<3;++i){if(!Finite(t.p[i]))return false;copy.p[i]=t.p[i];}candidate.triangles.push_back(copy);}
 output=std::move(candidate);return true;
}
// Mutable query caches and the BVH are local to one worker plan. Queries
// leaving the captured region poison the result rather than pretending that
// uncaptured space is clear. A caller must reject !Complete() after planning.
class World:public w2mw::CollisionWorld {
 Bounds bounds{};Vec3 sourcePadding{.001f,.001f,.001f};mutable bool complete=true;
 unsigned BuildNode(unsigned first,unsigned count){
  const unsigned index=unsigned(nodes.size());nodes.push_back({});
  Vec3 low{INFINITY,INFINITY,INFINITY},high{-INFINITY,-INFINITY,-INFINITY};
  for(unsigned i=first;i<first+count;++i)for(auto p:triangles[i].p){low={std::min(low.x,p.x),std::min(low.y,p.y),std::min(low.z,p.z)};high={std::max(high.x,p.x),std::max(high.y,p.y),std::max(high.z,p.z)};}
  nodes[index].minimum=low;nodes[index].maximum=high;
  if(count<=8){nodes[index].left=first;nodes[index].right=0x80000000u|count;return index;}
  Vec3 extent=w2mw::Sub(high,low);unsigned axis=extent.y>extent.x?1:0;if(extent.z>(&extent.x)[axis])axis=2;
  const unsigned middle=first+count/2;
  std::nth_element(triangles.begin()+first,triangles.begin()+middle,triangles.begin()+first+count,[axis](const auto& a,const auto& b){return ((&a.p[0].x)[axis]+(&a.p[1].x)[axis]+(&a.p[2].x)[axis])<((&b.p[0].x)[axis]+(&b.p[1].x)[axis]+(&b.p[2].x)[axis]);});
  const unsigned left=BuildNode(first,middle-first),right=BuildNode(middle,first+count-middle);nodes[index].left=left;nodes[index].right=right;return index;
 }
 bool Region(Vec3 low,Vec3 high)const{if(!Valid({low,high})||!Contains(bounds,low)||!Contains(bounds,high)){complete=false;return false;}return true;}
public:
 explicit World(Snapshot snapshot):bounds(snapshot.bounds),sourcePadding(snapshot.padding){
  if(!Valid(bounds)||snapshot.triangles.empty())throw std::runtime_error("Empty or invalid geometry snapshot");
  triangles=std::move(snapshot.triangles);nodes.reserve(triangles.size()/2+1);BuildNode(0,unsigned(triangles.size()));
  if(!PrepareQueries())throw std::runtime_error("Snapshot BVH preparation failed");
 }
 World(const World&)=delete;World& operator=(const World&)=delete;
 bool Complete()const{return complete;}
 Vec3 WallRayPadding()const{return sourcePadding;}
 void QueryBox(Vec3 low,Vec3 high,std::vector<unsigned>& result,bool walls)const{if(!Region(low,high)){result.clear();return;}w2mw::CollisionWorld::QueryBox(low,high,result,walls);}
 bool Ray(Vec3 from,Vec3 to,w2mw::RayHit& hit,bool floors=false,bool walls=false)const{if(!Region({std::min(from.x,to.x),std::min(from.y,to.y),std::min(from.z,to.z)},{std::max(from.x,to.x),std::max(from.y,to.y),std::max(from.z,to.z)})){hit={};return false;}return w2mw::CollisionWorld::Ray(from,to,hit,floors,walls);}
 bool RayCandidatesSpan(Vec3 from,Vec3 to,w2mw::RayHit& hit,std::span<const unsigned> candidates,bool all=false)const{if(!Region({std::min(from.x,to.x),std::min(from.y,to.y),std::min(from.z,to.z)},{std::max(from.x,to.x),std::max(from.y,to.y),std::max(from.z,to.z)})){hit={};return false;}return w2mw::CollisionWorld::RayCandidatesSpan(from,to,hit,candidates,all);}
 bool RayCandidates(Vec3 from,Vec3 to,w2mw::RayHit& hit,const std::vector<unsigned>& candidates,bool all=false)const{return RayCandidatesSpan(from,to,hit,candidates,all);}
 struct RayQueryScope {World const& world;unsigned previous;explicit RayQueryScope(World const& value):world(value),previous(value.BeginQuery()){}~RayQueryScope(){world.EndQuery(previous);}};
};
}
