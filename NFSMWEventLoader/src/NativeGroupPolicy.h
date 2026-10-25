#pragma once
#include <array>
#include <cstdint>
namespace mwel_native_groups {
constexpr unsigned Count=4096;
using Flags=std::array<unsigned char,Count>;
// Native CARP skips only a nonzero group whose live scenery status is zero
// (783C77..783C89). Group zero is unconditional; bits 2/4 are valid states.
inline bool Enabled(std::uint16_t group,const Flags& flags){
 return group==0||(group<flags.size()&&flags[group]!=0);
}
}
