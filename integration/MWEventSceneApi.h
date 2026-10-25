#pragma once
#include <cstdint>
namespace mwelscene {
struct Point {float x,y,z;};
struct Barrier {Point a;std::uint32_t flags;Point b;float inverseLength;std::uint32_t extra[2];};
static_assert(sizeof(Barrier)==40);
using Contacts=unsigned(__cdecl*)(const Point*,float,Barrier*,unsigned);
using Draw=void(__cdecl*)(void*);
}
