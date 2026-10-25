#pragma once
#include <cstdint>
// Optional interface between our W2C loader and NeoAI. All fields are copied;
// no native object pointer or ownership crosses the DLL boundary. Main thread.
namespace mwneo_obstacles {
constexpr std::uint32_t Version=1;
struct Volume {
 std::uint32_t size=sizeof(Volume),owner=0;
 float position[3]{},heading[3]{0,0,1};
 float halfWidth=0,halfLength=0,halfHeight=0,mass=0;
};
static_assert(sizeof(Volume)==48);
using Query=unsigned(__cdecl*)(unsigned version,const float* origin,float range,Volume* output,unsigned capacity);
}
