#pragma once
#include <cstdint>
// Optional, read-only Event Loader -> World Loader scene preparation contract.
// Coordinates use the converted render/map axes (x,y,z), never native UMath.
// Entry zero is the first player-car pose and owns the initial prop focus.
// Camera entries include source cuts; no vehicle control or scene clock is
// acquired/advanced by publishing or reading this value.
namespace mw_world_load {
constexpr std::uint32_t Version=1,Capacity=128,Car=0,Camera=1;
constexpr unsigned Inactive=0,Ready=1,Invalid=2,Pending=3;
struct Point {float x=0,y=0,z=0;std::uint32_t kind=Car;};
struct Plan {
 std::uint32_t version=Version,generation=0,count=0,reserved=0;
 Point points[Capacity]{};
};
static_assert(sizeof(Point)==16&&sizeof(Plan)==2064);
using Query=unsigned(__cdecl*)(Plan*,unsigned);
}
