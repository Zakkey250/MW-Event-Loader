#pragma once
#include <array>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include "../../integration/MWCombatContract.h"
namespace hot_pursuit {
// One-time reinforcement activation, using the target's observed road speed.
// No launch impulse when the target is stopped; native steering owns driving.
inline float RollingSpeed(float targetMps){return std::isfinite(targetMps)?std::clamp(targetMps,0.f,30.f):0.f;}
// A distant reinforcement must not appear parked merely because its target
// is temporarily fighting at rest. Keep nearby contacts at native low speed.
inline float RollingSpeed(float targetMps,float targetGap){
 if(!std::isfinite(targetMps)||targetMps<0||!std::isfinite(targetGap)||targetGap<0)return 0;
 return std::max(RollingSpeed(targetMps),targetGap>35.f?8.f:0.f);
}

inline float PoliceContactScale(bool hotPursuit,unsigned victimDriver,unsigned otherDriver){
 return hotPursuit&&(victimDriver==0||victimDriver==3)&&otherDriver==2?.5f:1.f;
}
inline bool StartEligible(float racePercent){return std::isfinite(racePercent)&&racePercent>25.f&&racePercent<=100.f;}
struct ReplacementWindow {
 static constexpr std::uint64_t DelayMs=3000;
 struct Ticket {unsigned cop=0,target=0;std::uint64_t until=0;};
 std::array<Ticket,64> tickets{};
 std::uint64_t overflowUntil=0,refillAt=0;
 bool RefillReady(std::uint64_t now)const{return now>=refillAt;}
 bool Destroyed(unsigned cop,unsigned target,std::uint64_t now){
  if(!cop||!target)return false;
  for(const auto& t:tickets)if(t.cop==cop)return false;
  refillAt=now+DelayMs;
  for(auto& t:tickets)if(!t.cop){t={cop,target,now+DelayMs};return true;}
  overflowUntil=now+DelayMs;return false;
 }
 unsigned Held(unsigned target,std::uint64_t now)const{
  if(now<overflowUntil)return 2;
  unsigned result=0;for(const auto& t:tickets)if(t.cop&&t.target==target&&now<t.until)++result;return result;
 }
};
inline unsigned Quota(unsigned rank,unsigned racers,unsigned nativeCapacity=~0u,unsigned originalRacers=0){
 if(!rank||rank>racers||racers<1||racers>16)return 0;
 const auto budget=std::min(mwcombat::PoliceBudget(originalRacers?originalRacers:racers),nativeCapacity);
 return budget/racers+(rank<=budget%racers?1:0);
}
// Recycling is scoped to the actual target, never the human player's range.
inline bool Retain(bool racing,bool targetRunning,bool active,float horizontal,float vertical){
 return racing&&targetRunning&&active&&std::isfinite(horizontal)&&std::isfinite(vertical)&&horizontal<=900.f&&std::abs(vertical)<=40.f;
}
// Count genuine finished racers as survivors. Retirement is a native outcome,
// never a missing streamed vehicle. Wide arithmetic prevents reward overflow.
inline unsigned Reward(unsigned base,unsigned participants,unsigned retired,unsigned place){
 if(!participants||participants>8||retired>=participants||!place||place>participants)return 0;
 const auto cash=std::uint64_t(base)*(participants-retired)/place;
 return unsigned(std::min<std::uint64_t>(cash,2147483520u)); // x87 -> signed int safe
}
enum class Result {Running,Won,Lost};
// Native finish/busted flags and health retirements are observed, never
// inferred from a missing vehicle allocation during a streaming transition.
inline Result Outcome(bool kingBusted,unsigned racers,unsigned escaped,unsigned busted){
 if(kingBusted)return Result::Lost;
 if(escaped)return Result::Won;
 return racers&&busted>=racers?Result::Lost:Result::Running;
}
}
