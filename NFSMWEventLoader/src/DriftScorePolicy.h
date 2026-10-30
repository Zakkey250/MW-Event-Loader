#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
// Carbon EN DriftScoring::Update 006BE570: speed > 5 m/s, 9 degree
// slip threshold, speed^4 / 1000, display conversion * .2; straight grace
// 1.5 seconds. MW adaptations: retain fractional points between samples,
// require measured translation/course progress, reject recovery teleports,
// bank at a genuine finish, lose only the pending chain on a heavy impact.
// No Carbon edge-band bonus: MW has no verified drift_scoring_band surface.
namespace mwel_drift {
constexpr std::uint32_t Maximum=10000000;
enum class Notice {None,Drifting,Banked,Lost};
struct Sample {
 double time=0;float x=0,z=0,speed=0,angle=0,progress=0;
 bool grounded=false,impact=false,nearWall=false,nearCar=false;
};
struct Score {
 double lastTime=-1,quiet=0,chainSeconds=0,pending=0,progressTime=0,rearmTime=-1,rearmTravel=0;
 float x=0,z=0,speed=0,bestProgress=0,displayAngle=0;
 float rearmProgress=0,rearmLast=0;
 std::uint32_t total=0,bestChain=0,noticeAmount=0;bool finished=false,primed=false,progressRearm=false;
 Notice notice=Notice::None;double noticeUntil=0;unsigned bonusFlags=0;
 float awardScale=1.f;
 // Event rule: +1x per two seconds of actual qualifying drift, capped at 10x.
 // Straight grace retains the chain but never advances its drift clock.
 // As in Carbon 006BDEBA..006BDF23, multiply the whole pending combo.
 unsigned multiplier()const{return 1u+unsigned(std::min(9.,std::floor((chainSeconds+1e-6)/2.)));}
 std::uint32_t chain()const{return std::uint32_t(std::min(double(Maximum),pending*multiplier()));}
 std::uint32_t standing()const{return std::uint32_t(std::min(double(Maximum),double(total)+chain()));}
 void bank(double now){
  auto points=chain();if(points){noticeAmount=std::min(Maximum-total,points);total+=noticeAmount;bestChain=std::max(bestChain,points);notice=Notice::Banked;noticeUntil=now+2;}
  pending=quiet=chainSeconds=0;bonusFlags=0;displayAngle=0;
 }
 void lose(double now){if(pending>0){noticeAmount=chain();notice=Notice::Lost;noticeUntil=now+2;}pending=quiet=chainSeconds=0;bonusFlags=0;displayAngle=0;progressRearm=true;rearmTime=-1;rearmTravel=0;}
 bool update(const Sample& s){
  if(finished||!std::isfinite(s.time)||!std::isfinite(s.x)||!std::isfinite(s.z)||!std::isfinite(s.speed)||!std::isfinite(s.angle)||!std::isfinite(s.progress)||s.speed<0||s.speed>150)return false;
  if(!primed){primed=true;lastTime=progressTime=s.time;x=s.x;z=s.z;speed=s.speed;bestProgress=s.progress;return false;}
  const auto dt=s.time-lastTime;if(dt<=0)return false;
  const auto moved=std::hypot(s.x-x,s.z-z);const auto previousSpeed=speed;
  lastTime=s.time;x=s.x;z=s.z;speed=s.speed;
  // A pause does not bank a chain. A resample establishes a new physical
  // baseline; a repeated timestamp never awards points twice.
  if(dt>.25){lose(s.time);bestProgress=s.progress;progressTime=s.time;return false;}
  if(moved>std::max(2.,double(std::max(s.speed,previousSpeed))*dt*2+1.)||s.impact||(previousSpeed-s.speed>8&&dt<=.15)){lose(s.time);return false;}
  if(progressRearm){
   // A crash/recovery can move the course projection behind its old high
   // water. A rejected impact sample must not permanently own that anchor.
   // Re-arm only after continuous grounded movement and measured forward
   // course progress; idle, reverse, repeated impacts and teleports cannot.
   if(rearmTime<0||!s.grounded||moved<dt*2||s.progress<rearmLast-.001f){rearmTime=s.time;rearmProgress=s.progress;rearmTravel=0;}
   else rearmTravel+=moved;
   rearmLast=s.progress;
   if(!s.grounded||s.time-rearmTime<.35||rearmTravel<2||s.progress<=rearmProgress+.0001f)return false;
   progressRearm=false;bestProgress=s.progress;progressTime=s.time;
  }
  const bool advancing=s.progress>=bestProgress-.25f;
  if(s.progress>bestProgress+.0001f){bestProgress=s.progress;progressTime=s.time;}
  const float angle=std::abs(s.angle);
  bool sliding=s.grounded&&s.speed>5&&angle>=9&&angle<=80&&moved>=dt*2&&advancing&&s.time-progressTime<2;
  if(sliding){
   quiet=0;chainSeconds+=dt;displayAngle=angle;
   const double v2=double(s.speed)*s.speed;
   bonusFlags=unsigned(s.nearWall)|(unsigned(s.nearCar)<<1);
   const double earned=v2*v2*.0002*dt*(1.+.25*unsigned(s.nearWall)+.25*unsigned(s.nearCar));
   // The hidden AWD balance scales awards, including proximity awards,
   // without changing the duration threshold or multiplier cadence.
   const double scale=std::isfinite(awardScale)?std::clamp(double(awardScale),0.,1.):1.;
   pending=std::min(double(Maximum),pending+earned*scale);
   notice=Notice::Drifting;noticeUntil=s.time+.25;return true;
  }
  bonusFlags=0;displayAngle=0;if(pending>0){quiet+=dt;if(quiet>=1.5)bank(s.time);}
  return false;
 }
 void finish(double now){if(finished)return;bank(now);finished=true;}
};
inline std::array<unsigned,16> Order(const std::array<Score,16>& score,const std::array<float,16>& progress,unsigned count,unsigned retired=0){
 std::array<unsigned,16> out{};for(unsigned i=0;i<16;++i)out[i]=i;
 count=std::min(count,16u);
 std::stable_sort(out.begin(),out.begin()+count,[&](unsigned a,unsigned b){
  if(bool(retired&(1u<<a))!=bool(retired&(1u<<b)))return !(retired&(1u<<a));
  const auto x=score[a].standing(),y=score[b].standing();
  if(x!=y)return x>y;
  // A deterministic tie follows genuine course progress, then grid slot.
  return progress[a]!=progress[b]?progress[a]>progress[b]:a<b;
 });return out;
}
}
