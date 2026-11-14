#pragma once
#include <array>
#include <cmath>
namespace rpd_arrest_nis {
inline constexpr std::array<const char*,26> scenes={
 "ArrestF02","ArrestF02b","ArrestF06","ArrestF06b","ArrestF07","ArrestF07b",
 "ArrestF14","ArrestF14b","ArrestF18","ArrestF18b","ArrestF23","ArrestF23b",
 "ArrestM01","ArrestM01b","ArrestM04","ArrestM04b","ArrestM06","ArrestM06b",
 "ArrestM07","ArrestM07b","ArrestM14","ArrestM14b","ArrestM16","ArrestM16b","ArrestM19","ArrestM19b"};
inline bool HoldBeforeEnd(float elapsed,float end){return std::isfinite(elapsed)&&std::isfinite(end)&&end>.3f&&elapsed>=end-.18f;}
}
