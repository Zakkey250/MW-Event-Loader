#pragma once
#include "RpdHotPursuitLedger.h"
namespace hot_pursuit_arrest {
constexpr float Required(bool policePlayer){return policePlayer?8.f:5.f;}
inline bool PlayerPriority(float horizontal,float vertical){
 return std::isfinite(horizontal)&&std::isfinite(vertical)&&horizontal<=24.f&&std::abs(vertical)<=1.75f;
}
struct Countdown {
 rpd_hot_pursuit::Contact contact;
 bool Advance(float dt,bool enter,bool within,bool sameRoad,bool priority,bool policePlayer){
  // Transfer control to the nearby player without spending an AI timer while
  // they perform their own shorter contact arrest. Escape still cancels it.
  const bool pause=priority&&contact.latched&&sameRoad&&within;
  return contact.Advance(pause?0.f:dt,enter&&!priority,within,sameRoad,Required(policePlayer))&&!priority;
 }
 bool RecoveryHeld()const{return contact.latched;}
};
}
