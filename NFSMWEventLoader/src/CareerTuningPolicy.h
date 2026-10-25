#pragma once
#include <array>
#include <algorithm>
namespace mwel_career_tuning {
using Levels=std::array<unsigned,7>;
// Native 007A4D40 shop-package counts. Vehicle physics levels are local to
// each model, and are not interchangeable with these career shop tiers.
constexpr Levels ShopCounts{3,4,3,4,4,3,3};
template<class Unlocked>
bool Ceilings(const Levels& maximum,Levels& ceiling,Unlocked unlocked){
 for(auto n:maximum)if(n>6)return false;
 ceiling={};
 for(unsigned type=0;type<7;++type)for(unsigned level=1;level<=maximum[type];++level){
  int shop=int(ShopCounts[type])+int(level)-int(maximum[type]);
  if(shop>=1&&shop<=int(ShopCounts[type])&&unlocked(type,unsigned(shop)))ceiling[type]=level;
 }
 return true;
}
// Retain the generated difficulty below the ceiling; never give stock AI
// maximum unlocked parts merely because a later career tier is available.
inline Levels Limit(const Levels& original,const Levels& ceiling){
 Levels result{};for(unsigned type=0;type<7;++type)result[type]=std::min(original[type],ceiling[type]);return result;
}
inline bool Applies(bool owned,bool neo,bool regulation,bool career,bool loaded,unsigned slot,unsigned count){
 return owned&&(neo||regulation)&&career&&loaded&&count>=2&&count<=16&&slot>0&&slot<count;
}
}
