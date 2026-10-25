#pragma once
#include "NeoGeometrySnapshot.h"
#include "GeometryQueries.h"
namespace mwel_async_geometry {
// Adapter callbacks have no context in geometry ABI V1. A scoped thread-local
// binding lets existing planners use a private WorldQueries without reaching
// the native provider. It is not an exported service or a new external ABI.
inline thread_local const World* boundWorld=nullptr;
inline thread_local std::vector<unsigned> boxScratch;
inline Vec3 V(mwelgeo::Point p){return {p.x,p.y,p.z};}
inline unsigned __cdecl Available(){return boundWorld!=nullptr;}
inline unsigned __cdecl Begin(){return boundWorld?boundWorld->BeginQuery():2u;}
inline void __cdecl End(unsigned previous){if(boundWorld&&previous<2)boundWorld->EndQuery(previous);}
inline unsigned __cdecl Generation(){return boundWorld?boundWorld->QueryGeneration():0;}
inline unsigned __cdecl Ray(mwelgeo::Point from,mwelgeo::Point to,mwelgeo::Hit* output,unsigned flags){
 if(!boundWorld||!output||flags>3)return 0;w2mw::RayHit hit;
 const bool found=boundWorld->Ray(V(from),V(to),hit,(flags&1)!=0,(flags&2)!=0);memcpy(output,&hit,sizeof(hit));return unsigned(found);
}
inline unsigned __cdecl Box(mwelgeo::Point low,mwelgeo::Point high,unsigned walls,mwelgeo::IndexSink sink,void* context){
 if(!boundWorld||!sink||walls>1)return 0;boundWorld->QueryBox(V(low),V(high),boxScratch,walls!=0);for(auto id:boxScratch)sink(context,id);return boundWorld->Complete()?1u:0u;
}
inline unsigned __cdecl Triangle(unsigned id,mwelgeo::Triangle* output){if(!boundWorld||!output||id>=boundWorld->triangles.size())return 0;memcpy(output,&boundWorld->triangles[id],sizeof(*output));return 1;}
inline unsigned __cdecl Overlaps(unsigned id,mwelgeo::Point low,mwelgeo::Point high){return boundWorld&&id<boundWorld->triangles.size()&&Finite(V(low))&&Finite(V(high))&&boundWorld->TriangleOverlaps(id,V(low),V(high));}
inline mwelgeo::Point __cdecl Padding(){auto p=boundWorld?boundWorld->WallRayPadding():Vec3{.001f,.001f,.001f};return {p.x,p.y,p.z};}
inline unsigned __cdecl Candidates(mwelgeo::Point from,mwelgeo::Point to,mwelgeo::Hit* output,const unsigned* ids,unsigned count,unsigned complete){
 if(!boundWorld||!output||(!ids&&count)||count>boundWorld->triangles.size()||complete>1)return 0;
 std::span<const unsigned> values(ids,count);for(auto id:values)if(id>=boundWorld->triangles.size())return 0;
 w2mw::RayHit hit;const bool found=boundWorld->RayCandidatesSpan(V(from),V(to),hit,values,complete!=0);memcpy(output,&hit,sizeof(hit));return unsigned(found);
}
inline mwelgeo::Api Api(){return {sizeof(mwelgeo::Api),mwelgeo::Version,mwelgeo::NativeWorld,Available,Begin,End,Generation,Ray,Box,Triangle,Overlaps,Padding,Candidates};}
class Binding {
 const World* previous;
public:
 explicit Binding(const World& world):previous(boundWorld){boundWorld=&world;}
 ~Binding(){boundWorld=previous;}
 Binding(const Binding&)=delete;Binding& operator=(const Binding&)=delete;
};
}
