#pragma once
#include <cmath>
#include <cstdint>
namespace police_deployment {
inline bool Room(unsigned limit,unsigned active,unsigned pending,unsigned reserved=0){
 return limit&&limit<=16&&active<limit&&pending<limit-active&&reserved<limit-active-pending;
}
inline bool Near(float ax,float ay,float az,float bx,float by,float bz,float radius){
 return std::isfinite(ax)&&std::isfinite(ay)&&std::isfinite(az)&&std::isfinite(bx)&&std::isfinite(by)&&std::isfinite(bz)&&
  std::abs(ay-by)<8.f&&std::hypot(ax-bx,az-bz)<radius;
}
}
