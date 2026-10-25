#pragma once
#include <array>
#include <algorithm>
#include <cmath>

// An empirical, per-body short-horizon observation envelope, not a new force
// model. A changed controller/NOS/power/grade condition must be queried again.
// Unknown or sparsely observed conditions return false and retain PlanEnd.
struct NeoDepartureLearning {
 struct Observation {
  float time=0,p[3]{},v[3]{},filteredAcceleration=0,power=0,pitch=0;
  float throttle=0,brake=0,steering=0;
  unsigned mode=0,collision=0;int contacts=-1;bool known=false;
 };
 struct Sample {float time=0,speed=0,low=0,high=0,power=0,pitch=0;unsigned mode=0;bool valid=false;};
 // Keep rare NOS-on observations when ordinary driving adds many off samples.
 // The provider resets this memory on body/race/physical-identity changes;
 // elapsed time alone does not invalidate an unchanged physical condition.
 std::array<Sample,2048> samples{};unsigned count=0;std::array<unsigned,8> next{};
 Observation previous{};bool havePrevious=false;
 static bool ConditionsKnown(const Observation& o){
  if(!o.known||o.contacts<3||(o.collision&12))return false;
  for(float value:{o.time,o.filteredAcceleration,o.power,o.pitch,o.throttle,o.brake,o.steering})if(!std::isfinite(value))return false;
  for(unsigned i=0;i<3;++i)if(!std::isfinite(o.p[i])||!std::isfinite(o.v[i]))return false;
  return o.power>0;
 }
 static bool Eligible(const Observation& o){return ConditionsKnown(o)&&o.throttle>=.99f&&o.brake<=.01f&&std::abs(o.steering)<=.1f;}
 static bool Same(const Observation& a,const Observation& b){
  return a.mode==b.mode&&std::abs(a.power-b.power)<.001f&&std::abs(a.pitch-b.pitch)<.025f;
 }
 bool Observe(const Observation& o){
  if(havePrevious&&o.time<previous.time){*this={};}
  if(!Eligible(o)){previous=o;havePrevious=false;return false;}
  if(havePrevious&&o.time==previous.time)return false;
  if(!havePrevious){previous=o;havePrevious=true;return false;}
  if(!Same(previous,o)){previous=o;return false;}
  float dt=o.time-previous.time;if(dt>0&&dt<.02f)return false;
  auto old=previous;previous=o;
  if(dt<.02f||dt>.2f||!Same(old,o))return false;
  for(unsigned i=0;i<3;++i){float expected=(o.v[i]+old.v[i])*.5f*dt;
   if(std::abs(o.p[i]-old.p[i]-expected)>.35f+std::abs(expected)*.08f)return false;
  }
  float speed=std::hypot(o.v[0],o.v[2]),oldSpeed=std::hypot(old.v[0],old.v[2]);
  float raw=(speed-oldSpeed)/dt;if(!std::isfinite(raw)||speed<=15||oldSpeed<=15)return false;
  unsigned bucket=o.mode&7u,index=bucket*256+next[bucket];if(!samples[index].valid)++count;
  samples[index]={o.time,(speed+oldSpeed)*.5f,std::min(raw,o.filteredAcceleration),std::max(raw,o.filteredAcceleration),o.power,(o.pitch+old.pitch)*.5f,o.mode,true};
  next[bucket]=(next[bucket]+1)%256;return true;
 }
 bool Range(const Observation& current,float minimumSpeed,float maximumSpeed,float& low,float& high,unsigned* used=nullptr,unsigned* reason=nullptr)const{
  if(used)*used=0;
  if(reason)*reason=1;
  // Query known full-throttle observations even when the existing PlanEnd
  // currently commands braking. Learning above still requires actual gas.
  if(!ConditionsKnown(current)||!std::isfinite(minimumSpeed)||!std::isfinite(maximumSpeed)||minimumSpeed<=15||maximumSpeed<minimumSpeed)return false;
  low=1e30f;high=-1e30f;float minimumObserved=1e30f,maximumObserved=0;unsigned n=0;
  for(const auto& s:samples){
   if(!s.valid||current.time<s.time||s.mode!=current.mode||std::abs(s.power-current.power)>.001f||std::abs(s.pitch-current.pitch)>.025f||s.speed<minimumSpeed-5||s.speed>maximumSpeed+5)continue;
   low=std::min(low,s.low);high=std::max(high,s.high);minimumObserved=std::min(minimumObserved,s.speed);maximumObserved=std::max(maximumObserved,s.speed);++n;
  }
  if(used)*used=n;
  // Require interpolation over the whole requested speed interval. A few
  // filtered samples at a different speed are not a departure certificate.
  if(n<6){if(reason)*reason=2;return false;}
  if(minimumSpeed<minimumObserved||maximumSpeed>maximumObserved){if(reason)*reason=3;return false;}
  float error=std::max(1.f,(high-low)*.5f);low-=error;high+=error;if(reason)*reason=0;return true;
 }
};
