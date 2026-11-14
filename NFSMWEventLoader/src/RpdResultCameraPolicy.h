#pragma once
#include <array>
#include <vector>
#include <cmath>
#include <algorithm>
#include <cstring>
#include <cstdint>
#include <string>
namespace rpd_result_camera {
struct Sample {float eye[3]{},look[3]{},dutch=0,lens=24;};
static_assert(sizeof(Sample)==32);
inline Sample FrameSuspect(Sample source,float halfWidth,float halfLength){
 // HP LOOK includes its source actor's trajectory. Retarget its viewing
 // direction to the live MW suspect, rather than translating that trajectory
 // a second time. Fit the MW body while retaining the source camera angle.
 float distance=0;for(unsigned i=0;i<3;++i){source.eye[i]-=source.look[i];distance+=source.eye[i]*source.eye[i];source.look[i]=0;}
 const float minimum=std::clamp(2.7f*std::hypot(halfWidth,halfLength),6.f,14.f);
 distance=std::sqrt(distance);if(distance>.01f&&distance<minimum)for(auto& axis:source.eye)axis*=minimum/distance;
 source.eye[2]+=.25f;source.look[2]=.25f;return source;
}
struct Track {
 float end=0,rate=0;std::vector<Sample> samples;
 bool Valid()const{
  if(!std::isfinite(end)||end<=0||end>15||rate!=30||samples.size()<2||samples.size()>451||std::abs(float(samples.size()-1)/rate-end)>.034f)return false;
  for(const auto& s:samples){
   for(unsigned i=0;i<3;++i)if(!std::isfinite(s.eye[i])||!std::isfinite(s.look[i])||std::abs(s.eye[i])>100||std::abs(s.look[i])>100)return false;
   float distance=0;for(unsigned i=0;i<3;++i){float d=s.eye[i]-s.look[i];distance+=d*d;}
   if(distance<.01f||!std::isfinite(s.dutch)||std::abs(s.dutch)>1||!std::isfinite(s.lens)||s.lens<10||s.lens>200)return false;
  }return true;
 }
 Sample At(float seconds)const{
  const auto f=std::clamp(seconds,0.f,end)*rate;const auto a=std::min(unsigned(f),unsigned(samples.size()-1));const auto b=std::min(a+1,unsigned(samples.size()-1));
  Sample result=samples[a];const float t=std::clamp(f-a,0.f,1.f);
  // The source includes real camera cuts. Never sweep the eye through a car
  // or a building by interpolating across a discontinuity.
  float jump=0;for(unsigned i=0;i<3;++i){const float d=samples[b].eye[i]-samples[a].eye[i];jump+=d*d;}
  if(jump>16)return result;
  for(unsigned i=0;i<3;++i){result.eye[i]+=(samples[b].eye[i]-result.eye[i])*t;result.look[i]+=(samples[b].look[i]-result.look[i])*t;}
  result.lens+=(samples[b].lens-result.lens)*t;return result;
 }
};
inline bool Load(const std::string& bytes,std::array<Track,2>& output){
 if(bytes.size()<8||bytes.size()>40000||memcmp(bytes.data(),"RPC1",4))return false;
 unsigned tracks=0;memcpy(&tracks,bytes.data()+4,4);if(tracks!=2)return false;
 size_t at=8;std::array<Track,2> parsed;
 for(auto& track:parsed){
  unsigned count=0;if(at+12>bytes.size())return false;memcpy(&count,bytes.data()+at,4);memcpy(&track.end,bytes.data()+at+4,4);memcpy(&track.rate,bytes.data()+at+8,4);at+=12;
  if(count<2||count>451||at+count*sizeof(Sample)>bytes.size())return false;
  track.samples.resize(count);memcpy(track.samples.data(),bytes.data()+at,count*sizeof(Sample));at+=count*sizeof(Sample);if(!track.Valid())return false;
 }
 if(at!=bytes.size())return false;output=std::move(parsed);return true;
}
struct Timeline {
 float seconds=0;bool active=false,held=false;
 void Begin(){seconds=0;active=true;held=false;}
 void Tick(float dt,bool paused,float end){
  if(!active||held||paused||!std::isfinite(dt)||dt<=0)return;
  seconds=std::min(end,seconds+std::min(dt,.1f));held=seconds>=end;
 }
};
}
