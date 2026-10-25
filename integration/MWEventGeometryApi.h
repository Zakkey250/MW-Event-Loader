#pragma once
#include <cstdint>
// Immutable world queries cross the ASI boundary as POD values only. The world
// loader retains its BVH, vectors and query cache; consumers own AI/event state.
namespace mwelgeo {
struct Point {float x,y,z;};
struct Hit {unsigned triangle;float fraction;Point point,normal;};
struct Triangle {Point p[3];unsigned surface,wall;};
using IndexSink=void(__cdecl*)(void*,unsigned);
struct Api {
 unsigned size,version,worldKind;
 unsigned(__cdecl* available)();
 unsigned(__cdecl* begin)();
 void(__cdecl* end)(unsigned);
 unsigned(__cdecl* generation)();
 unsigned(__cdecl* ray)(Point,Point,Hit*,unsigned);
 unsigned(__cdecl* box)(Point,Point,unsigned,IndexSink,void*);
 unsigned(__cdecl* triangle)(unsigned,Triangle*);
 unsigned(__cdecl* overlaps)(unsigned,Point,Point);
 Point(__cdecl* padding)();
 unsigned(__cdecl* candidates)(Point,Point,Hit*,const unsigned*,unsigned,unsigned);
};
static_assert(sizeof(Point)==12&&sizeof(Hit)==32&&sizeof(Triangle)==44);
inline constexpr unsigned Version=1,NativeWorld=1,W2CWorld=2;
}
