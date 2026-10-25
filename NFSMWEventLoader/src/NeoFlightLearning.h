#pragma once
#include <array>
#include <cmath>
#include <algorithm>
// Functional, per-body observations. No course/car identifiers, saved tuning,
// diagnostics or worker are needed. Unknown speed bands keep the old policy.
struct NeoFlightLearning {
 struct Observation {float time=0,p[3]{},v[3]{},gravity=0,pitch=0;int contacts=-1;unsigned collision=0;bool eligible=false;};
 struct Sample {float speed=0,acceleration=0,pitch=0;};
 Observation previous{};bool havePrevious=false;std::array<Sample,64> samples{};unsigned count=0,next=0;
 bool Observe(const Observation& o){
  if(!o.eligible||o.contacts||(o.collision&12)){previous=o;havePrevious=false;return false;}
  if(havePrevious&&o.time==previous.time)return false;
  if(havePrevious&&o.time<previous.time){*this={};}
  if(!havePrevious){previous=o;havePrevious=true;return false;}
  auto old=previous;float dt=o.time-old.time;
  if(std::isfinite(dt)&&dt>0&&dt<.02f)return false; // Accumulate 60/120 Hz input.
  previous=o;
  if(!std::isfinite(dt)||dt<.02f||dt>.2f)return false;
  for(unsigned axis=0;axis<3;++axis){float expected=(o.v[axis]+old.v[axis])*.5f*dt;
   if(!std::isfinite(o.p[axis])||!std::isfinite(o.v[axis])||std::abs(o.p[axis]-old.p[axis]-expected)>.35f+std::abs(expected)*.08f)return false;
  }
  float horizontal=std::hypot(o.v[0]-old.v[0],o.v[2]-old.v[2])/dt;
  float acceleration=-(o.v[1]-old.v[1])/dt,base=std::abs(o.gravity);
  if(horizontal>30||!std::isfinite(acceleration)||base<.1f||acceleration<base*.75f||acceleration>base*8)return false;
  float speed=(std::hypot(o.v[0],o.v[2])+std::hypot(old.v[0],old.v[2]))*.5f;
  if(speed<=15)return false;
  samples[next]={speed,acceleration,o.pitch};next=(next+1)%samples.size();count=std::min(unsigned(samples.size()),count+1);return true;
 }
 bool PitchRange(float current,float& low,float& high)const{
  if(!std::isfinite(current)||std::abs(current)>1.5f||count<4)return false;
  low=high=current;
  for(unsigned i=0;i<count;++i){if(!std::isfinite(samples[i].pitch)||std::abs(samples[i].pitch)>1.5f)return false;low=std::min(low,samples[i].pitch);high=std::max(high,samples[i].pitch);}
  return true;
 }
 bool Range(float speed,float& low,float& high)const{
  float minimum=1e30f,maximum=0,minSpeed=1e30f,maxSpeed=0;unsigned n=0;
  for(unsigned i=0;i<count;++i)if(std::abs(samples[i].speed-speed)<=5.f){auto s=samples[i];minimum=std::min(minimum,s.acceleration);maximum=std::max(maximum,s.acceleration);minSpeed=std::min(minSpeed,s.speed);maxSpeed=std::max(maxSpeed,s.speed);++n;}
  // Interpolation inside measured data only. No performance-band extrapolation.
  if(n<4||speed<minSpeed||speed>maxSpeed||maximum-minimum>std::max(4.f,minimum*.25f))return false;
  float error=std::max(1.f,(maximum-minimum)*.5f);low=std::max(.1f,minimum-error);high=maximum+error;return true;
 }
};
