#pragma once
#include <cstddef>
namespace rpd_heat_tier {
struct Band {std::size_t begin=0,end=0;};
// The ordinary MW heat range covers five ascending stock-performance tiers.
// Extended heat narrows tier five, without introducing uninstalled car names.
constexpr Band Candidates(std::size_t count,unsigned heat){
 if(!count||heat<1||heat>10)return {};
 const unsigned low=heat<=5?(heat-1)*20:80+(heat-5)*3;
 const unsigned high=heat<5?heat*20:100;
 const auto begin=count*low/100;
 const auto end=count*high/100;
 return {begin,end>begin?end:begin+1};
}
}
