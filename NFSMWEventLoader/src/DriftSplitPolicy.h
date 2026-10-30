#pragma once
#include <array>
#include <cstdint>
namespace mwel_drift_split {
struct Difference {bool valid=false;std::int32_t points=0;unsigned opponent=0;};
inline Difference Compare(const std::array<unsigned,16>& points,unsigned count,unsigned player,unsigned retired){
 if(count<2||count>16||player>=count||(retired&(1u<<player)))return {};
 unsigned opponent=16;
 for(unsigned i=0;i<count;++i)if(i!=player&&!(retired&(1u<<i))&&(opponent==16||points[i]>points[opponent]))opponent=i;
 if(opponent==16)return {};
 return {true,std::int32_t(std::int64_t(points[player])-points[opponent]),opponent};
}
}
