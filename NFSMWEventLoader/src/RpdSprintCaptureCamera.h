#pragma once
#include "RpdCaptureSlowMotion.h"

namespace rpd_sprint_capture_camera {
using combat_health_runtime::Read;
using combat_impact::Motion;
using combat_impact::Vector;
using rpd_hot_pursuit::Run;

struct Shot {
 Motion pose{};
 unsigned vehicle=0;
 float queuedAt=0;
 bool wreck=false;
};

Run owner{};
std::array<Shot,3> pending{};
unsigned count=0;
Shot active{};
float elapsed=0;
ULONGLONG sampledAt=0;
bool rendered=false;
bool slowObserved=false;
constexpr float Duration=2.f; // Real-time seconds; native simulation remains slowed.

void Reset(){rpd_capture_slow_motion::Reset();owner={};count=0;active={};elapsed=0;sampledAt=0;rendered=slowObserved=false;}

bool Current(Run run){
 unsigned race=0,flow=0;
 return run&&run.generation==combat_health_runtime::generation&&
  Read(0x925E90,flow)&&flow==6&&Read(0x91E000,race)&&race==run.race&&run.eventKey==MWELNeoCurrentKey();
}

bool MotionFor(unsigned handle,Motion& pose,unsigned& car){
 car=gauntlet_runtime::Vehicle(handle);
 return car&&combat_health_runtime::NativeMotion(car-0x80,pose)&&pose.valid&&pose.handle==handle;
}

void Enqueue(Run run,unsigned officer,unsigned suspect,bool wreck){
 if(!suspect||suspect==officer||GetCurrentThreadId()!=combat_health_runtime::gameThread||!Current(run))return;
 if(!(owner==run)){Reset();owner=run;}
 if(count==pending.size())return;
 Motion player{},target{};unsigned playerCar=0,targetCar=0;float clock=0;
 if(!MotionFor(officer,player,playerCar)||!Read(0x9885D8,clock)||!std::isfinite(clock))return;
 if(!MotionFor(suspect,target,targetCar)){
  if(!wreck)return;
  for(const auto& cached:combat_health_runtime::motions)
   if(cached.valid&&cached.handle==suspect){target=cached;break;}
  if(!target.valid)return;
 }
 const auto delta=target.position-player.position;
 // Stay in the loaded area and avoid taking the driver's view for distant
 // AI captures. Their ledger, cut-in and bounty follow the same path.
 if(combat_impact::Dot(delta,delta)>70.f*70.f||std::abs(delta.y)>8.f)return;
 if(!rpd_capture_slow_motion::Install())Log("RPD_CAPTURE_SLOW_UNAVAILABLE native_dt_surface_mismatch=1");
 pending[count++]={target,targetCar,clock,wreck};
}

void Frame(){
 // Retain the rendered shot's scale across simulation ticks. The frame
 // callback can run before the native dt calculation and before rendering.
 rpd_capture_slow_motion::Reset();
 if(!count&&!active.pose.valid)return;
 if(!Current(owner)){Reset();return;}
 const auto now=GetTickCount64();
 const bool paused=reinterpret_cast<bool(__cdecl*)()>(0x468390)();
 if(active.pose.valid&&sampledAt&&!paused)elapsed+=std::min(float(now-sampledAt)/1000.f,.1f);
 sampledAt=now;
 if(active.pose.valid&&elapsed>=Duration){
  if(rendered&&worldLogEnabled)Log("RPD_SPRINT_CAPTURE_CAMERA_END suspect=%u chase_view_restored=1 slow_scale=1 real_seconds=%.3f",active.pose.handle,elapsed);
  active={};rendered=slowObserved=false;
 }
 if(active.pose.valid&&rendered&&!paused)rpd_capture_slow_motion::Apply();
}

bool View(float* matrix){
 if(!matrix||(!count&&!active.pose.valid)||GetCurrentThreadId()!=combat_health_runtime::gameThread)return false;
 Frame();
 if(!Current(owner)){Reset();return false;}
 float clock=0;
 if(!Read(0x9885D8,clock)||!std::isfinite(clock)){Reset();return false;}
 while(!active.pose.valid&&count){
  const auto next=pending[0];
  for(unsigned i=1;i<count;++i)pending[i-1]=pending[i];
  --count;
  if(clock>=next.queuedAt&&clock-next.queuedAt<=1.25f){active=next;elapsed=0;sampledAt=GetTickCount64();rendered=slowObserved=false;}
 }
 if(!active.pose.valid)return false;
 auto& pose=active.pose;
 Motion live{};unsigned car=0;
 if(active.vehicle&&MotionFor(pose.handle,live,car)&&car==active.vehicle)pose.position=live.position;
 const float span=std::hypot(pose.forward.x,pose.forward.z);
 if(span<.1f){active={};rendered=slowObserved=false;rpd_capture_slow_motion::Reset();return false;}
 const Vector forward{pose.forward.x/span,0,pose.forward.z/span};
 const Vector right{forward.z,0,-forward.x};
 const auto focus=pose.position+Vector{0,.35f,0};
 const float t=std::clamp(elapsed/Duration,0.f,1.f);
 const float length=std::clamp(pose.halfLength+3.f,4.5f,7.5f);
 const float width=std::clamp(pose.halfWidth+2.8f,3.5f,5.f);
 Vector eye{},look{};
 for(const float side:{1.f,-1.f}){
  eye=focus-forward*(length-t*.55f)+right*(side*(width+t*.35f))+Vector{0,2.f+t*.25f,0};
  look=focus;
  MWELRpdConstrainResultCamera(&focus.x,&eye.x,&look.x);
  if(combat_impact::Dot(eye-focus,eye-focus)>=9.f)break;
 }
 if(combat_impact::Dot(eye-focus,eye-focus)<9.f){rendered=false;rpd_capture_slow_motion::Reset();return false;}
 const float renderEye[]={eye.z,-eye.x,eye.y},renderLook[]={look.z,-look.x,look.y},up[]={0,0,1};
 reinterpret_cast<void(__cdecl*)(float*,const float*,const float*,const float*)>(0x6CF0A0)(matrix,renderEye,renderLook,up);
 if(!rendered){
  rendered=true;
  Log("RPD_SPRINT_CAPTURE_CAMERA_BEGIN suspect=%u wreck=%u target=%.2f,%.2f,%.2f slow_ready=%u",pose.handle,unsigned(active.wreck),focus.x,focus.y,focus.z,unsigned(rpd_capture_slow_motion::trampoline!=nullptr));
 }
 if(!reinterpret_cast<bool(__cdecl*)()>(0x468390)())rpd_capture_slow_motion::Apply();
 if(worldLogEnabled&&!slowObserved){
  unsigned world=0;float raw=0,native=0,scaled=0;
  if(Read(0x9885E0,world)&&Read(world+0x18,raw)&&Read(world+0x24,native)&&Read(world+0x1C,scaled)&&raw>0&&native>0){
   const float measured=scaled/(raw*native);
   if(std::abs(measured-.35f)<.02f){slowObserved=true;Log("RPD_CAPTURE_SLOW_NATIVE scale=%.3f raw_dt=%.6f native_scale=%.3f scaled_dt=%.6f",measured,raw,native,scaled);}
  }
 }
 return true;
}
}
