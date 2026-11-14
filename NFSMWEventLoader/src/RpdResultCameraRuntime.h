#pragma once
#include "RpdResultCameraPolicy.h"
#include "RpdNativeArrestNis.h"
namespace rpd_finish_camera {
using combat_health_runtime::Read;
std::array<rpd_result_camera::Track,2> tracks;
rpd_result_camera::Timeline timeline;
unsigned raceOwner=0,keyOwner=0,suspect=0,officer=0,kind=0,generation=0,stopOwned=0,stoppedHandle=0;
unsigned suspectStopOwned=0,stoppedVehicle=0,suspectVehicle=0;
combat_impact::Motion anchor;
combat_impact::Motion remembered;
unsigned rememberedRace=0,rememberedKey=0,rememberedEpoch=0;
std::array<float,16> heldView{};bool viewReady=false,terminalViewReady=false,assetsReady=false,lensOwned=false;
unsigned short savedLens=0,writtenLens=0;ULONGLONG lastTick=0;
bool raceClockStopped=false;
void ReleaseStop(){
 if(stopOwned){const auto v=gauntlet_runtime::Vehicle(stoppedHandle);
  if(v&&v==stoppedVehicle)reinterpret_cast<NFSPluginSDK::MW05::IVehicle*>(v)->ForceStopOff(static_cast<NFSPluginSDK::MW05::IVehicle::ForceStopType>(stopOwned));}
 stopOwned=stoppedHandle=stoppedVehicle=0;
}
void Reset(){
 rpd_native_arrest_nis::Release();
 ReleaseStop();
 if(suspectStopOwned){const auto v=gauntlet_runtime::Vehicle(suspect);if(v&&v==suspectVehicle)reinterpret_cast<NFSPluginSDK::MW05::IVehicle*>(v)->ForceStopOff(static_cast<NFSPluginSDK::MW05::IVehicle::ForceStopType>(suspectStopOwned));}suspectStopOwned=suspectVehicle=0;
 if(lensOwned&&*reinterpret_cast<unsigned short*>(0x986870+0xC4)==writtenLens)*reinterpret_cast<unsigned short*>(0x986870+0xC4)=savedLens;
 timeline={};raceOwner=keyOwner=suspect=officer=generation=0;anchor={};heldView={};viewReady=terminalViewReady=lensOwned=false;lastTick=0;
 remembered={};rememberedRace=rememberedKey=rememberedEpoch=0;
 raceClockStopped=false;
}
void Remember(unsigned race,unsigned key,unsigned epoch,unsigned target){
 if(timeline.active)return;
 const auto car=gauntlet_runtime::Vehicle(target);combat_impact::Motion pose;
 if(car&&combat_health_runtime::NativeMotion(car-0x80,pose)&&pose.handle==target){remembered=pose;rememberedRace=race;rememberedKey=key;rememberedEpoch=epoch;}
}
bool Context(){unsigned race=0;return timeline.active&&Read(0x91E000,race)&&race==raceOwner&&keyOwner==MWELNeoCurrentKey()&&generation==combat_health_runtime::generation;}
bool RetainResult(){
 if(!Context()||!timeline.held||!terminalViewReady)return false;
 ReleaseStop();Log("RPD_RESULT_CAMERA_RETAINED officer=%u suspect=%u native_result_boundary=1",officer,suspect);return true;
}
bool Load(){
 if(assetsReady)return true;
 try{assetsReady=rpd_result_camera::Load(mwel::Read(mwel_paths::Root(game)/L"Runtime"/L"RpdFinishCamera"/L"cameras.bin",40000),tracks);}
 catch(const std::exception&){assetsReady=false;}
 return assetsReady;
}
void Tick(){
 if(!timeline.active)return;
 if(!Context()||*reinterpret_cast<unsigned*>(0x925E90)!=6){Reset();return;}
 if(!kind){
  const bool playing=rpd_native_arrest_nis::Tick();
  if(!playing){timeline.held=true;return;}
  timeline.held=rpd_native_arrest_nis::terminal;return;
 }
 const auto now=GetTickCount64();const float dt=lastTick?float(now-lastTick)/1000.f:0;lastTick=now;
 const bool wasHeld=timeline.held;
 timeline.Tick(dt,reinterpret_cast<bool(__cdecl*)()>(0x468390)(),tracks[kind].end);
 if(!wasHeld&&timeline.held){ReleaseStop();Log("RPD_RESULT_CAMERA_TERMINAL kind=%s officer=%u suspect=%u seconds=%.3f angle_held=1 script_end_dispatched=0",kind?"wreck":"arrest",officer,suspect,timeline.seconds);}
}
bool Hold(unsigned player,unsigned target,bool wreck){
 if(!timeline.active){
  if(wreck&&!Load()){Log("RPD_RESULT_CAMERA_UNAVAILABLE file=Runtime/RpdFinishCamera/cameras.bin native_result_fallback=1");return false;}
  unsigned race=0;if(!Read(0x91E000,race)||!target||target==player)return false;
  const BYTE stopClock[]={0x56,0x8B,0xF1,0x8A,0x86,0x78,0x19,0,0,0x84,0xC0};
  if(memcmp(reinterpret_cast<void*>(0x5E8BA0),stopClock,sizeof(stopClock)))return false;
  const auto car=gauntlet_runtime::Vehicle(target);
  combat_impact::Motion pose;if(!car||!combat_health_runtime::NativeMotion(car-0x80,pose)){
   if(remembered.valid&&remembered.handle==target&&rememberedRace==race&&rememberedKey==MWELNeoCurrentKey()&&rememberedEpoch==combat_health_runtime::generation)pose=remembered;
   else {for(const auto& cached:combat_health_runtime::motions)if(cached.valid&&cached.handle==target){pose=cached;break;}
    if(!pose.valid){Log("RPD_RESULT_CAMERA_UNAVAILABLE suspect=%u reason=no_owned_body_sample native_result_fallback=1",target);return false;}}
  }
  raceOwner=race;keyOwner=MWELNeoCurrentKey();generation=combat_health_runtime::generation;suspect=target;officer=player;kind=wreck?1u:0u;anchor=pose;
  timeline.Begin();lastTick=GetTickCount64();viewReady=terminalViewReady=false;
  // Native StopRaceTimer preserves the actual arrest/destruction time. The
  // presentation must not add its own duration to saved best times.
  if(wreck){reinterpret_cast<void(__thiscall*)(unsigned)>(0x5E8BA0)(race);raceClockStopped=true;}
  // GRacerInfo owns a separate GTimer used by Winner and saved best times.
  // Mirror its verified 005E97C0..DF stop arithmetic, without setting the
  // native finish flag before the presentation has completed.
  const auto info=race+0x18;unsigned handle=0;BYTE running=0;float start=0,total=0;
  if(Read(info,handle)&&handle==player&&Read(info+0x154,running)&&running&&Read(info+0x14C,start)&&Read(info+0x150,total)){
   const float stopped=reinterpret_cast<float(__cdecl*)()>(0x6E8DE0)()-start+total;
   if(std::isfinite(stopped)&&stopped>=0){*reinterpret_cast<float*>(info+0x150)=stopped;*reinterpret_cast<BYTE*>(info+0x154)=0;Log("RPD_RESULT_TIMER_STOP seconds=%.6f native_outcome=1 finish_flag_written=0",stopped);}
  }
  const auto own=gauntlet_runtime::Vehicle(player);
  if(own){auto vehicle=reinterpret_cast<NFSPluginSDK::MW05::IVehicle*>(own);stopOwned=3u&~unsigned(vehicle->GetForceStop());stoppedHandle=player;stoppedVehicle=own;vehicle->ForceStopOn(static_cast<NFSPluginSDK::MW05::IVehicle::ForceStopType>(3));}
  if(!wreck&&!rpd_native_arrest_nis::Begin(target,pose)){Log("RPD_NATIVE_ARREST_NIS_UNAVAILABLE suspect=%u",target);Reset();return false;}
  Log("RPD_RESULT_CAMERA_ACTOR handle=%u active=%u xyz=%.3f,%.3f,%.3f",target,car?unsigned(reinterpret_cast<NFSPluginSDK::MW05::IVehicle*>(car)->IsActive()):0u,anchor.position.x,anchor.position.y,anchor.position.z);
  Log("RPD_RESULT_CAMERA_BEGIN kind=%s officer=%u suspect=%u native_outcome=1 actor_ownership_changed=0 duration=%.3f native_nis=%u terminal_script_disabled=1",kind?"wreck":"arrest",player,target,kind?tracks[kind].end:0.f,unsigned(!kind));
 }
 Tick();return Context()&&(!timeline.held||!terminalViewReady);
}
bool View(float* matrix){
 Tick();if(!matrix||!Context())return false;
 if(!kind){
  if(timeline.held&&terminalViewReady){std::copy(heldView.begin(),heldView.end(),matrix);return true;}
  std::copy(matrix,matrix+16,heldView.begin());viewReady=true;if(timeline.held)terminalViewReady=true;
  return false; // Native NIS owns its own camera, characters and sound.
 }
 if(timeline.held&&terminalViewReady){
  std::copy(heldView.begin(),heldView.end(),matrix);
  if(lensOwned)*reinterpret_cast<unsigned short*>(0x986870+0xC4)=writtenLens;
  if(worldLogEnabled){static ULONGLONG next=0;const auto now=GetTickCount64();if(now>=next){next=now+2000;const auto car=gauntlet_runtime::Vehicle(suspect);combat_impact::Motion live;const bool read=car&&combat_health_runtime::NativeMotion(car-0x80,live);Log("RPD_RESULT_CAMERA_HOLD officer=%u suspect=%u terminal_view_applied=1 active=%u body_read=%u anchor=%.2f,%.2f,%.2f live=%.2f,%.2f,%.2f",officer,suspect,car?unsigned(reinterpret_cast<NFSPluginSDK::MW05::IVehicle*>(car)->IsActive()):0u,unsigned(read),anchor.position.x,anchor.position.y,anchor.position.z,live.position.x,live.position.y,live.position.z);}}
  return true;
 }
 combat_impact::Motion live;const auto car=gauntlet_runtime::Vehicle(suspect);
 if(car&&combat_health_runtime::NativeMotion(car-0x80,live)&&live.handle==suspect)anchor.position=live.position;
 const auto sample=rpd_result_camera::FrameSuspect(tracks[kind].At(timeline.seconds),anchor.halfWidth,anchor.halfLength);
 auto forward=anchor.forward;forward.y=0;float length=std::sqrt(combat_impact::Dot(forward,forward));if(length<.1f)return false;forward=forward*(1/length);
 const combat_impact::Vector right{forward.z,0,-forward.x};
 auto point=[&](const float* p){return anchor.position+right*p[0]+forward*p[1]+combat_impact::Vector{0,p[2],0};};
 auto eye=point(sample.eye),look=point(sample.look);auto focus=anchor.position+combat_impact::Vector{0,.25f,0};
 ::MWELRpdConstrainResultCamera(&focus.x,&eye.x,&look.x);
 const float renderEye[]={eye.z,-eye.x,eye.y},renderLook[]={look.z,-look.x,look.y},up[]={0,0,1};
 reinterpret_cast<void(__cdecl*)(float*,const float*,const float*,const float*)>(0x6CF0A0)(matrix,renderEye,renderLook,up);
 if(!lensOwned){savedLens=*reinterpret_cast<unsigned short*>(0x986870+0xC4);lensOwned=true;}
 writtenLens=static_cast<unsigned short>(std::clamp(2.f*std::atan(18.f/sample.lens)*65536.f/6.28318530718f,5000.f,22000.f));
 *reinterpret_cast<unsigned short*>(0x986870+0xC4)=writtenLens;
 std::copy(matrix,matrix+16,heldView.begin());viewReady=true;if(timeline.held)terminalViewReady=true;return true;
}
}
