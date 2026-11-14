#pragma once
#ifdef MWEL_ARMS_FRONTEND_STIMULUS
namespace native_player_reposition_test {
void Frame(){
 using combat_health_runtime::Read;
 static bool configured=false,enabled=false;static unsigned stage=0,phase=0;static ULONGLONG next=0,racingAt=0;
 static combat_impact::Vector anchor{};
 if(!configured){
  configured=true;const auto path=game/L"MWEventLoader.native-reposition-test";
  const auto marker=mwel::fs::exists(path)?mwel::Read(path,128):std::string();
  enabled=!productionRuntime&&(marker=="MWEL_NATIVE_REPOSITION_BASELINE_V1\n"||marker=="MWEL_NATIVE_REPOSITION_CHECKED_V1\n");
  native_player_reposition::bypass=enabled&&marker=="MWEL_NATIVE_REPOSITION_BASELINE_V1\n";
 }
 if(!enabled||stage>=(native_player_reposition::bypass?4u:6u)||!combat_health_runtime::Racing()||GetTickCount64()<next)return;
 if(!racingAt)racingAt=GetTickCount64();if(GetTickCount64()-racingAt<2000)return;
 unsigned race=0,handle=0;
 if(!Read(0x91E000,race)||!Read(race+0x18,handle))return;
 const auto car=gauntlet_runtime::Vehicle(handle);if(!car)return;
 auto sim=reinterpret_cast<NFSPluginSDK::MW05::IVehicle*>(car)->GetSimable();if(!sim||!sim->IsPlayer())return;
 unsigned com=0;if(!Read(car+4,com)||!com)return;
 const auto reset=reinterpret_cast<unsigned(__thiscall*)(unsigned,unsigned)>(0x5D59F0)(com,0x620250);
 if(!gauntlet_runtime::Table(reset,0x8ABA10))return;
 const auto primary=reset-0x4C;
 if(!phase){
  auto package=event_race_policies::Current();if(!package||package->authoring.at("gates").size()<6)return;
  const auto& gate=package->authoring.at("gates").at(4);const auto& p=gate.at("position");
  const float angle=gate.at("rotation").get<float>()*.017453292519943295f;
  const float forward[]={-std::sin(angle),0,std::cos(angle)};const float along=stage%2?10.f:-10.f;
  const float lateral=stage<4?0.f:stage==4?18.f:-18.f;
  float position[]={-p.at(1).get<float>()+forward[0]*along,p.at(2).get<float>()+2.f,p.at(0).get<float>()+forward[2]*along};
  const combat_impact::Vector streamAt{position[0],position[1],position[2]};
  const bool loaded=rpd_sprint_warp::StreamRoad(streamAt);
  MWELPrepareGauntletRoad(MWELNeoCurrentKey());
  Log("EXPERIMENT_REPOSITION_SCOPE stage=%u course_ready=%u driver=%u",stage,unsigned(MWELPlayerResetCourseReady(MWELNeoCurrentKey())),unsigned(reinterpret_cast<NFSPluginSDK::MW05::IVehicle*>(car)->GetDriverClass()));
  Log("EXPERIMENT_REPOSITION_STREAM stage=%u native_stream_ready=%u",stage,unsigned(loaded));
  const bool placed=reinterpret_cast<bool(__thiscall*)(unsigned,const float*,const float*,bool)>(0x6A45F0)(primary,position,forward,false);
  if(!placed){Log("EXPERIMENT_REPOSITION_SETUP_FAILED stage=%u",stage);enabled=false;return;}
  if(lateral){
   auto body=sim->GetRigidBody();if(!body){enabled=false;return;}
   auto off=body->GetPosition();reinterpret_cast<float*>(&off)[0]-=forward[2]*lateral;reinterpret_cast<float*>(&off)[2]+=forward[0]*lateral;
   body->SetPosition(off);NFSPluginSDK::MW05::UMath::Vector3 zero{};body->SetLinearVelocity(zero);body->SetAngularVelocity(zero);
   Log("EXPERIMENT_REPOSITION_STRANDED stage=%u artificial_body_offset=%.1f",stage,lateral);
  }
  // Synthetic CP/cookie fixture, never a genuine course-crossing claim.
  *reinterpret_cast<unsigned*>(race+0x18+0x2C)=(stage<2||stage==4)?3:4;
  const auto& actual=sim->GetPosition();
  reinterpret_cast<void(__thiscall*)(unsigned,const void*,const void*)>(0x6A4340)(reset,&actual,forward);
  *reinterpret_cast<unsigned*>(reset+0x1C+0xC)=1; // Native FindRoad cookie flag.
  phase=1;next=GetTickCount64()+3000; // Allow native reset invulnerability to expire.
  Log("EXPERIMENT_REPOSITION_SETUP stage=%u artificial_CP_and_cookie=1 native_find_road=1 native_reset_vehicle_pending=1",stage);
 }else if(phase==1){
  const auto& actual=sim->GetPosition();combat_impact::Motion before{};if(!combat_health_runtime::NativeMotion(car-0x80,before))return;
  reinterpret_cast<void(__thiscall*)(unsigned,const void*,const void*)>(0x6A4340)(reset,&actual,&before.forward);
  *reinterpret_cast<unsigned*>(reset+0x1C+0xC)=1;
  const auto cpBefore=*reinterpret_cast<unsigned*>(race+0x18+0x2C),lapBefore=*reinterpret_cast<unsigned*>(race+0x18+0x28);
  const bool result=reinterpret_cast<bool(__thiscall*)(unsigned,bool)>(0x6B08C0)(reset,true);
  anchor={};combat_impact::Motion body{};if(combat_health_runtime::NativeMotion(car-0x80,body))anchor=body.position;
  float station=0,offset=-1;const bool projected=MWELCourseProjection(MWELNeoCurrentKey(),&anchor.x,~0u,station,offset);
  const bool unchanged=cpBefore==*reinterpret_cast<unsigned*>(race+0x18+0x2C)&&lapBefore==*reinterpret_cast<unsigned*>(race+0x18+0x28);
  Log("EXPERIMENT_REPOSITION_RESULT stage=%u bypass=%u native_return=%u actual=%.3f,%.3f,%.3f projected=%u course_offset=%.3f CP_lap_unchanged=%u",stage,unsigned(native_player_reposition::bypass),unsigned(result),anchor.x,anchor.y,anchor.z,unsigned(projected),offset,unsigned(unchanged));
  phase=2;next=GetTickCount64()+3000;
 }else{
  combat_impact::Motion body{};combat_health_runtime::NativeMotion(car-0x80,body);
  Log("EXPERIMENT_REPOSITION_SETTLED stage=%u actual=%.3f,%.3f,%.3f moved=%.3f grounded=%u",stage,body.position.x,body.position.y,body.position.z,std::hypot(body.position.x-anchor.x,body.position.z-anchor.z),unsigned(drift_race::Grounded(reinterpret_cast<BYTE*>(car))));
  ++stage;phase=0;next=GetTickCount64()+1000;
 }
}
}
#endif
