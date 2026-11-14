#pragma once
// Explicit isolated-lab integration stimulus. This tests the real native loss
// event/script/menu path; it is not evidence of six physical weapon contacts.
namespace combat_defeat_experiment {
bool handoff=false,handoffNear=false,handoffNoAlly=false,handoffResult=false,rpdRecovery=false,rpdRollover=false,rpdArrest=false;unsigned heldAlly=0;
#ifdef MWEL_ARMS_FRONTEND_STIMULUS
bool handoffMoving=false;
bool rpdWreckResult=false;
bool rpdAIResume=false;std::array<float,3> aiResumeAnchor{};ULONGLONG aiResumeHeldAt=0,aiResumeDoneAt=0;
#endif
bool enabled=false,visual=false,arrest=false,survival=false,visualPlayerReleased=false;unsigned survivalScenario=0,epoch=0,hits=0,stage=0,epochs=0;ULONGLONG next=0,visualBegan=0;unsigned visualStage=99;
void Configure(){
#ifdef MWEL_ARMS_FRONTEND_STIMULUS
 // Private validation binary only; never part of the handoff candidate.
 if(productionRuntime){
  const auto validation=game/L"MWEventLoader.runtime-validation-test",autopilot=game/L"MWEventLoader.career-autodrive-test";
  if(!mwel::fs::exists(validation)||!mwel::fs::exists(autopilot)||mwel::Read(validation,128)!="MW_EVENT_LOADER_RUNTIME_VALIDATION_V1\n"||mwel::Read(autopilot,128)!="MW_EVENT_LOADER_CAREER_AUTODRIVE_V1\n")return;
 }
#else
 if(productionRuntime)return;
#endif
 auto marker=game/L"MWEventLoader.combat-defeat-test";
 const auto text=mwel::fs::exists(marker)?mwel::Read(marker,128):std::string();
 bool isolated=!productionRuntime;
#ifdef MWEL_ARMS_FRONTEND_STIMULUS
 // This private binary passed both exact runtime-validation and career
 // autopilot marker checks above. Exercise real GCharacter attachments too.
 isolated=true;
 rpdAIResume=text=="MW_EVENT_LOADER_RPD_AI_RESUME_V1\n"||text=="MW_EVENT_LOADER_RPD_AI_RESUME_BASELINE_V1\n";
 if(rpdAIResume){rpd_road_navigation::testBypassResume=text=="MW_EVENT_LOADER_RPD_AI_RESUME_BASELINE_V1\n";enabled=true;return;}
#endif
 rpdRecovery=isolated&&text=="MW_EVENT_LOADER_RPD_RECOVERY_V1\n";
 rpdRollover=isolated&&text=="MW_EVENT_LOADER_RPD_ROLLOVER_V1\n";
 rpdArrest=isolated&&text=="MW_EVENT_LOADER_RPD_ARREST_GATE_V1\n";
 handoffResult=isolated&&text=="MW_EVENT_LOADER_RPD_HANDOFF_RESULT_V1\n";
 handoffNear=handoffResult||(isolated&&text=="MW_EVENT_LOADER_RPD_HANDOFF_NEAR_V1\n");
#ifdef MWEL_ARMS_FRONTEND_STIMULUS
 handoffMoving=isolated&&text=="MW_EVENT_LOADER_RPD_HANDOFF_MOVING_V1\n";
 handoffNear=handoffNear||handoffMoving;
 rpdWreckResult=isolated&&text=="MW_EVENT_LOADER_RPD_WRECK_RESULT_V1\n";
 if(rpdWreckResult){enabled=true;return;}
#endif
 handoffNoAlly=isolated&&text=="MW_EVENT_LOADER_RPD_HANDOFF_NO_ALLY_V1\n";
 handoff=handoffNear||handoffNoAlly||(isolated&&text=="MW_EVENT_LOADER_RPD_HANDOFF_V1\n");
 visual=text=="MW_EVENT_LOADER_COMBAT_VISUAL_V1\n";
 arrest=text=="MW_EVENT_LOADER_GAUNTLET_ARREST_V1\n";
 survivalScenario=text=="MW_EVENT_LOADER_ARMS_SURVIVAL_V1\n"?1:text=="MW_EVENT_LOADER_ARMS_SURVIVAL_LOSS_V1\n"?2:text=="MW_EVENT_LOADER_ARMS_SURVIVAL_WAIT_V1\n"?3:0;
 survival=survivalScenario!=0;
 enabled=rpdArrest||rpdRollover||rpdRecovery||handoff||visual||arrest||survival||text=="MW_EVENT_LOADER_COMBAT_DEFEAT_V1\n";
}
void Frame(){
 if(enabled&&survival&&hits&&!visualPlayerReleased&&!combat_health_runtime::Racing()){
  // Only the explicit lab owns player autopilot. Release it at the result
  // boundary so the real result/retry UI can receive Computer Use input.
  const BYTE guard[]={0x8B,0x44,0x24,0x04,0x85,0xC0,0x75,0x12};
  if(!memcmp(reinterpret_cast<void*>(0x612420),guard,sizeof(guard))){
   reinterpret_cast<void(__cdecl*)(bool)>(0x612420)(false);visualPlayerReleased=true;
   Log("EXPERIMENT_ARMS_SURVIVAL_INPUT_RESTORED result_boundary=1 production_disabled=1");
  }
 }
 if(!enabled||!combat_health_runtime::ledger.event)return;
 using namespace combat_health_runtime;
 unsigned race=0;BYTE staging=0,racing=0;
 if(!Read(0x91E000,race)||!Read(race+0x198A,staging)||!Read(race+0x1978,racing)||!staging||!racing)return;
 if(epoch!=generation){epoch=generation;hits=stage=0;visualBegan=GetTickCount64();visualStage=99;visualPlayerReleased=false;next=visualBegan+12000;
#ifdef MWEL_ARMS_FRONTEND_STIMULUS
  if(productionRuntime&&survival)next=visualBegan+4000;
#endif
  ++epochs;
  auto package=event_race_policies::Current();if(!visual&&package&&package->authoring.value("combatMode",std::string("none"))=="gauntlet")stage=1;
  Log("EXPERIMENT_COMBAT_DEFEAT_EPOCH epoch=%u iteration=%u retry_stimulus_disabled=%u",epoch,epochs,unsigned(epochs>1));}
 if(epochs>(survival?2u:1u))return;
#ifdef MWEL_ARMS_FRONTEND_STIMULUS
 if(rpdAIResume){
  unsigned suspect=0;Read(race+0x18+0x194,suspect);const auto car=gauntlet_runtime::Vehicle(suspect);if(!car)return;
  unsigned ai=0,goal=0,action=0;float reverse=0;
  if(!Read(car+0x54,ai)||ai<0x4C||!Read(ai-0x4C+0xB8,goal)||!Read(goal+4,action)||!Read(ai-0x4C+0x94,reverse))return;
  combat_impact::Motion motion;if(!NativeMotion(car-0x80,motion))return;
  const auto elapsed=GetTickCount64()-visualBegan;
  if(!hits&&elapsed>=8000){hits=1;aiResumeHeldAt=GetTickCount64();aiResumeAnchor={motion.position.x,motion.position.y,motion.position.z};
   Log("EXPERIMENT_RPD_AI_RESUME_OBSTRUCTION handle=%u artificial_body_pin=1 force_stop=0 native_pedals_retained=1",suspect);}
  if(hits==1){
   auto body=rpd_officer_handoff::Sim(car)->GetRigidBody();if(body){NFSPluginSDK::MW05::UMath::Vector3 position{},zero{};memcpy(&position,aiResumeAnchor.data(),12);body->SetPosition(position);body->SetLinearVelocity(zero);body->SetAngularVelocity(zero);}
   if(GetTickCount64()-aiResumeHeldAt>=3500&&gauntlet_runtime::Table(action,0x8912C0)){
    hits=2;rpd_road_navigation::testRejectRoad=true;aiResumeDoneAt=GetTickCount64();
    Log("EXPERIMENT_RPD_AI_RESUME_RELEASE native_unstuck_action=1 simulated_native_road_refusal=1 body_writes_ended=1 production_relocation_disabled=1 baseline=%u",unsigned(rpd_road_navigation::testBypassResume));
   }else if(GetTickCount64()-aiResumeHeldAt>8000){Log("EXPERIMENT_RPD_AI_RESUME_FAILED native_unstuck_not_entered=1");hits=99;}
  }else if(hits==2){
   if(gauntlet_runtime::Table(action,0x891090)&&reverse<=0){hits=3;aiResumeAnchor={motion.position.x,motion.position.y,motion.position.z};aiResumeDoneAt=GetTickCount64();
    Log("EXPERIMENT_RPD_AI_RESUME_RACE native_goal_selected_race=1 reverse_finished=1 body_resumption_pending=1");
   }else if(GetTickCount64()-aiResumeDoneAt>4000){Log("EXPERIMENT_RPD_AI_RESUME_LATCHED current_action=%08X reverse=%.3f native_road_refused=1",action,reverse);hits=99;}
  }else if(hits==3){
   const float dx=motion.position.x-aiResumeAnchor[0],dz=motion.position.z-aiResumeAnchor[2];
   if(dx*dx+dz*dz>=144&&gauntlet_runtime::Table(action,0x891090)){hits=4;Log("EXPERIMENT_RPD_AI_RESUME_VERIFIED body_metres=%.3f native_race_action=1 vehicle_reposition=0",std::sqrt(dx*dx+dz*dz));}
   else if(GetTickCount64()-aiResumeDoneAt>6000){Log("EXPERIMENT_RPD_AI_RESUME_FAILED body_did_not_resume=1");hits=99;}
  }
  return;
 }
 if(rpdWreckResult){
  // Let native support/formation tasks run before testing their exit order.
  // This private lifecycle fixture keeps both actors alive until stimulus;
  // it is not a combat/balance acceptance run.
  unsigned supportCars=0,supportState=0;
  if(!hits&&rpd_mission::Current()){
   for(unsigned slot=0;slot<2;++slot){auto& hp=ledger.racers[slot];if(hp.occupied&&!hp.retired){hp.remaining=hp.maximum;Write(slot);}}
   unsigned suspect=0;Read(race+0x18+0x194,suspect);
   unsigned player=0;Read(race+0x18,player);
   const auto officer=gauntlet_runtime::Vehicle(player),target=gauntlet_runtime::Vehicle(suspect);combat_impact::Motion own;
   if(officer&&target&&NativeMotion(officer-0x80,own)){
    auto ownBody=rpd_officer_handoff::Sim(officer)->GetRigidBody(),targetBody=rpd_officer_handoff::Sim(target)->GetRigidBody();
    if(ownBody&&targetBody){auto position=own.position+own.forward*8.f;NFSPluginSDK::MW05::UMath::Vector3 p,zero{};memcpy(&p,&position,12);
     targetBody->SetPosition(p);targetBody->SetLinearVelocity(zero);targetBody->SetAngularVelocity(zero);ownBody->SetLinearVelocity(zero);ownBody->SetAngularVelocity(zero);
     if(visualStage!=0){visualStage=0;Log("EXPERIMENT_RPD_WRECK_SUPPORT_FIXTURE artificial_actor_placement=1 stationary_actors=1 native_support_request_required=1 physical_combat_acceptance=0");}
    }
   }
   if(const auto pursuit=hot_pursuit_runtime::FindPursuit(suspect)){
    Read(pursuit+0x194+0x14,supportCars);Read(pursuit+0x194+0x78,supportState);
   }
  }
  if(!hits&&GetTickCount64()-visualBegan>=120000&&!supportCars){enabled=false;Log("EXPERIMENT_RPD_WRECK_SUPPORT_UNAVAILABLE native_vehicle_snapshot_not_generated=1 lifecycle_acceptance_failed=1");return;}
  if(!hits&&rpd_mission::Current()&&GetTickCount64()-visualBegan>=8000&&supportCars&&supportCars<=64){
   const auto b=bindings[1];if(b.damage&&Participant(1,b.handle)&&Native(b.damage,b.handle)){
    hits=1;ledger.racers[1].remaining=0;ledger.racers[1].retired=true;Write(1);Retire(1);
    Log("EXPERIMENT_RPD_WRECK_RESULT artificial_HP=1 native_destroy=1 physical_weapon_contact=0 player_role_changed=0 native_support_cars=%u support_state=%u",supportCars,supportState);
   }
  }return;
 }
#endif
 if(rpdArrest){
  if(!rpd_mission::Current()||GetTickCount64()-visualBegan<8000)return;
  unsigned player=0,suspect=0;Read(race+0x18,player);Read(race+0x18+0x194,suspect);
  const auto officer=gauntlet_runtime::Vehicle(player),target=gauntlet_runtime::Vehicle(suspect);combat_impact::Motion own;
  if(!officer||!target||!NativeMotion(officer-0x80,own))return;
  auto body=rpd_officer_handoff::Sim(target)->GetRigidBody();if(!body)return;
  if(!hits){auto position=own.position+own.forward*8.f;NFSPluginSDK::MW05::UMath::Vector3 p;memcpy(&p,&position,12);body->SetPosition(p);hits=1;
   Log("EXPERIMENT_RPD_ARREST_GATE_STIMULUS artificial_suspect_placement=1 distance=8 native_visibility_and_arrest_required=1 native_outcome_writes=0");}
#ifdef MWEL_ARMS_FRONTEND_STIMULUS
  // Stage the two real actors together while the native arrest timer runs.
  // Racer weapons can now displace the officer; this private camera fixture
  // must not depend on the former player shockwave immunity.
  {auto position=own.position+own.forward*8.f;NFSPluginSDK::MW05::UMath::Vector3 p;memcpy(&p,&position,12);body->SetPosition(p);
   auto ownBody=rpd_officer_handoff::Sim(officer)->GetRigidBody();if(ownBody){NFSPluginSDK::MW05::UMath::Vector3 zero{};ownBody->SetLinearVelocity(zero);ownBody->SetAngularVelocity(zero);}}
  const auto elapsed=GetTickCount64()-visualBegan;
  const unsigned phase=elapsed<9000?0:elapsed<10500?1:2;auto& hp=ledger.racers[1];
  if(hp.occupied&&hp.handle==suspect&&!hp.retired){
   hp.remaining=hp.maximum*(phase==0?1.f:phase==1?.2501f:.25f);Write(1);
   if(visualStage!=phase){visualStage=phase;Log("EXPERIMENT_RPD_ARREST_HP_BOUNDARY phase=%u artificial_HP=1 hp=%.4f maximum=%.1f arsenal=%u native_outcome_writes=0",phase,hp.remaining,hp.maximum,unsigned(rpd_mission::arsenalPursuit));}
  }
#endif
  NFSPluginSDK::MW05::UMath::Vector3 zero{};body->SetLinearVelocity(zero);body->SetAngularVelocity(zero);return;
 }
 if(rpdRollover){
  if(!rpd_mission::Current())return;unsigned handle=0;if(!Read(race+0x18,handle))return;
  const auto v=gauntlet_runtime::Vehicle(handle);if(!v||!rpd_officer_handoff::Sim(v)->IsPlayer())return;
  auto body=rpd_officer_handoff::Sim(v)->GetRigidBody();if(!body)return;
  if(!hits&&GetTickCount64()-visualBegan>=8000){
   std::array<float,16> m{};if(!rpd_officer_handoff::BodyMatrix(body,m.data(),false))return;
   for(unsigned i=0;i<8;++i)m[i]=-m[i];
   auto position=body->GetPosition();reinterpret_cast<float*>(&position)[1]+=1.f;
   body->SetPosition(position);if(!rpd_officer_handoff::BodyMatrix(body,m.data(),true))return;hits=1;
   Log("EXPERIMENT_RPD_ROLLOVER_STIMULUS handle=%u artificial_orientation=1 native_reset_unmodified=1",handle);
  }return; // Native ResetCar alone must restore the vehicle and player input.
 }
 if(rpdRecovery){
  if(!rpd_mission::Current())return;
  const auto elapsed=GetTickCount64()-visualBegan;
  unsigned suspect=0;Read(race+0x18+0x194,suspect);const auto car=gauntlet_runtime::Vehicle(suspect);
  if(!car)return;
  if(!hits&&elapsed>=8000){
#ifdef MWEL_ARMS_FRONTEND_STIMULUS
   // Exercise both actual native entry points only after the production
   // ownership predicate has proved that neither may reach native placement.
   // The road selector is deliberately unused: the guard must run before it.
   unsigned ai=0;combat_impact::Motion before{},after{};
   if(!Read(car+0x54,ai)||!ai||!NativeMotion(car-0x80,before))return;
   if(MWELRpdNativePlacementAllowed(reinterpret_cast<void*>(ai))){Log("EXPERIMENT_RPD_NATIVE_RELOCATION_FAILED reason=owned_AI_not_guarded native_calls_skipped=1");return;}
   auto requested=before.position;requested.y+=8.f;
   const bool direct=reinterpret_cast<bool(__thiscall*)(unsigned,const void*,const void*)>(0x422730)(ai,&requested,&before.forward);
   const bool road=reinterpret_cast<bool(__thiscall*)(unsigned,unsigned,unsigned,unsigned)>(0x4226E0)(ai,0,0,0);
   const bool vehicle=reinterpret_cast<bool(__thiscall*)(unsigned,const void*,const void*)>(0x685810)(car,&requested,&before.forward);
   if(!NativeMotion(car-0x80,after))return;
   const auto delta=after.position-before.position;const float moved=combat_impact::Dot(delta,delta);
   const bool unrelated=MWELRpdNativePlacementAllowed(nullptr)&&MWELRpdNativePlacementAllowed(reinterpret_cast<void*>(car));
   Log("EXPERIMENT_RPD_NATIVE_RELOCATION_GUARD direct=%u road=%u vehicle=%u displacement_squared=%.6f unrelated_allowed=%u native_checkpoint_writes=0",unsigned(direct),unsigned(road),unsigned(vehicle),moved,unsigned(unrelated));
   if(!direct||!road||!vehicle||moved>.0001f||!unrelated){Log("EXPERIMENT_RPD_NATIVE_RELOCATION_FAILED reason=body_or_scope_changed");return;}
#endif
   hits=1;heldAlly=suspect;rpd_officer_handoff::Vehicle(car)->ForceStopOn(static_cast<NFSPluginSDK::MW05::IVehicle::ForceStopType>(3));
   Log("EXPERIMENT_RPD_RECOVERY_HOLD artificial_obstruction=1 native_force_stop=1 physical_collision_claimed=0");
  }
  if(hits==1){
   // Explicit isolated fixture: native driving can clear ForceStop. Hold the
   // real body still until the production detector places it, then release.
   auto body=rpd_officer_handoff::Sim(car)->GetRigidBody();
   if(body){NFSPluginSDK::MW05::UMath::Vector3 zero{};body->SetLinearVelocity(zero);body->SetAngularVelocity(zero);}
  }
  if(hits==1&&elapsed>=45000){hits=2;heldAlly=0;rpd_officer_handoff::Vehicle(car)->ForceStopOff(static_cast<NFSPluginSDK::MW05::IVehicle::ForceStopType>(3));Log("EXPERIMENT_RPD_RECOVERY_RELEASE actual_body_resumption_required=1");}
  return;
 }
 if(handoff){
  if(handoffResult&&hits==1&&rpd_officer_handoff::switched&&GetTickCount64()-visualBegan>=30000){
   const auto b=bindings[1];if(b.damage&&Participant(1,b.handle)&&Native(b.damage,b.handle)){
    hits=2;ledger.racers[1].remaining=0;ledger.racers[1].retired=true;Retire(1);
    Log("EXPERIMENT_RPD_HANDOFF_RESULT_STIMULUS artificial_suspect_HP=1 real_native_destroy_result=1 physical_arrest_claimed=0");
   }
  }
  if(heldAlly){const auto v=gauntlet_runtime::Vehicle(heldAlly);unsigned driver=~0u;
   if(v&&Read(v+0x94,driver)&&driver==0){rpd_officer_handoff::Vehicle(v)->ForceStopOff(static_cast<NFSPluginSDK::MW05::IVehicle::ForceStopType>(3));heldAlly=0;}
  }
  if(hits||!rpd_mission::Current()||GetTickCount64()-visualBegan<8000)return;
  auto& hp=ledger.racers[0];const auto b=bindings[0];
  if(!hp.occupied||!b.damage||!Participant(0,b.handle)||!Native(b.damage,b.handle))return;
  rpd_officer_handoff::testNoAlly=handoffNoAlly;
  if(handoffNear){
   // Establish the explicit nearby fixture before HP zero. Production takes
   // its one nearby-officer snapshot at that instant and never waits for one.
   rpd_officer_handoff::disabledVehicle=gauntlet_runtime::Vehicle(b.handle);
   const auto ally=rpd_officer_handoff::Ally(50000.f);combat_impact::Motion m;
   if(ally&&combat_health_runtime::NativeMotion(rpd_officer_handoff::disabledVehicle-0x80,m)){
    const auto position=m.position-m.forward*20.f;
    if(reinterpret_cast<bool(__thiscall*)(unsigned,const combat_impact::Vector*,const combat_impact::Vector*)>(0x685810)(ally,&position,&m.forward)){
#ifdef MWEL_ARMS_FRONTEND_STIMULUS
     if(handoffMoving){
      auto velocity=m.forward*25.f;NFSPluginSDK::MW05::UMath::Vector3 nativeVelocity{};memcpy(&nativeVelocity,&velocity,12);
      if(auto body=rpd_officer_handoff::Sim(ally)->GetRigidBody())body->SetLinearVelocity(nativeVelocity);
      Log("EXPERIMENT_RPD_MOVING_ALLY_STIMULUS native_placement=1 artificial_velocity_mps=25 force_stop=0");
     }else
#endif
     rpd_officer_handoff::Vehicle(ally)->ForceStopOn(static_cast<NFSPluginSDK::MW05::IVehicle::ForceStopType>(3));heldAlly=gauntlet_runtime::Handle(ally);
     Log("EXPERIMENT_RPD_NEAR_ALLY_STIMULUS native_placement=1 distance=20 physical_spawn_claimed=0");
    }
   }
   rpd_officer_handoff::disabledVehicle=0;
  }
  hits=1;hp.remaining=0;hp.retired=true;Write(0);Retire(0);
  Log("EXPERIMENT_RPD_HANDOFF_STIMULUS artificial_HP=1 physical_weapon_contact=0 native_destroy=0");return;
 }
 if(survival){
  auto package=event_race_policies::Current();
  if(!package||package->authoring.value("combatMode",std::string("none"))!="armsrace"||hits>=(survivalScenario==3?2u:1u)||GetTickCount64()<next)return;
  unsigned count=0;if(!Read(race+0x1958,count)||count<2||count>16)return;
  for(unsigned i=0;i<count;++i){const auto b=bindings[i];
   if(!b.damage||!Participant(i,b.handle)||!Native(b.damage,b.handle))return;
  }
  ++hits;
  if(package->authoring.value("mode",std::string())=="speedtraprace"){
   // Explicit score-order stress, not claimed physical speed-trap crossings.
   // Preserve a higher AI score to test survival priority over native score.
   for(unsigned i=0;i<count;++i)*reinterpret_cast<float*>(race+0x18+i*0x194+0x120)=float(i*1000+10);
   Log("EXPERIMENT_ARMS_SCORE_STRESS artificial_scores=1 AI_scores_above_player=1 physical_speedtrap_crossings=0");
  }
  Log("EXPERIMENT_ARMS_SURVIVAL_STIMULUS count=%u artificial_hp=1 physical_weapon_contact=0 native_destroy=1",count);
  const auto limit=survivalScenario==3&&hits==1?count-1:count;
  for(unsigned i=1;i<limit;++i){ledger.racers[i].remaining=0;ledger.racers[i].retired=true;Retire(i);}
  if(survivalScenario==2){ledger.racers[0].remaining=0;ledger.racers[0].retired=true;Retire(0);}
  if(survivalScenario==3&&hits==1){next=GetTickCount64()+10000;Log("EXPERIMENT_ARMS_SURVIVAL_WAIT final_AI_alive=1 hold_seconds=10");}
  return;
 }
 if(arrest){
  auto package=event_race_policies::Current();if(!package||package->authoring.value("combatMode",std::string("none"))!="gauntlet"||GetTickCount64()-visualBegan<6000)return;
  if(!visualPlayerReleased){
   reinterpret_cast<void(__cdecl*)(bool)>(0x6123B0)(false);visualPlayerReleased=true;
   Log("EXPERIMENT_GAUNTLET_ARREST_HOLD intentional_stationary=1 automatic_race_result=0");
  }
  const auto b=bindings[0];unsigned sim=0,body=0,table=0,linear=0,angular=0;
  if(b.driver==0&&Participant(0,b.handle)&&Native(b.damage,b.handle)&&Read(b.damage-0x18,sim)&&Read(sim+0x4C,body)&&Read(body,table)&&
     Read(table+0x60,linear)&&linear==0x696FD0&&Read(table+0x64,angular)&&angular==0x696FF0){
   const combat_impact::Vector zero{};reinterpret_cast<void(__thiscall*)(unsigned,const combat_impact::Vector*)>(linear)(body,&zero);reinterpret_cast<void(__thiscall*)(unsigned,const combat_impact::Vector*)>(angular)(body,&zero);
  }
  return;
 }
 if(visual){
  // Dedicated visibility fixture: hold each color long enough to inspect.
  // This is artificial HP, not evidence of real impacts or survival balance.
  const auto elapsed=GetTickCount64()-visualBegan;
  const unsigned phase=elapsed<12000?0:elapsed<32000?1:elapsed<65000?2:elapsed<85000?3:4;
  if(phase>=1&&!visualPlayerReleased){
   const BYTE expected[]={0x8B,0x44,0x24,4,0x85,0xC0,0x75,0x12};BYTE observed[8]{};
   if(Copy(reinterpret_cast<void*>(0x6123B0),observed,8)&&!memcmp(observed,expected,8)){
    reinterpret_cast<void(__cdecl*)(bool)>(0x6123B0)(false);visualPlayerReleased=true;
    Log("EXPERIMENT_VISUAL_PLAYER_AUTODRIVE released=1 intentional_stationary=1");
   }
  }
  if(phase!=visualStage){visualStage=phase;Log("EXPERIMENT_COMBAT_VISUAL_PHASE phase=%u artificial_hp=1 production_disabled=1",phase);}
  for(unsigned i=0;i<bindings.size();++i){const auto b=bindings[i];auto& hp=ledger.racers[i];
   if(!b.damage||!Participant(i,b.handle)||!Native(b.damage,b.handle)||hp.retired)continue;
   if(b.driver==0&&phase>=1){
    // Intentional stationary visual fixture, after initial movement proof.
    // Only known native rigid-body velocity setters; never a production path.
    unsigned sim=0,body=0,table=0,position=0,linear=0,angular=0;
    if(Read(b.damage-0x18,sim)&&Read(sim+0x4C,body)&&Read(body,table)&&Read(table+32,position)&&position==0x671180&&Read(table+0x60,linear)&&linear==0x696FD0&&Read(table+0x64,angular)&&angular==0x696FF0){
     const BYTE guard[]={0x8B,0x41,0x30,0x8B,0x08,0x8B,0x54,0x24,4};BYTE code[9]{};
     if(Copy(reinterpret_cast<void*>(linear),code,9)&&!memcmp(code,guard,9)&&Copy(reinterpret_cast<void*>(angular),code,9)&&!memcmp(code,guard,9)){
      const combat_impact::Vector zero{};reinterpret_cast<void(__thiscall*)(unsigned,const combat_impact::Vector*)>(linear)(body,&zero);reinterpret_cast<void(__thiscall*)(unsigned,const combat_impact::Vector*)>(angular)(body,&zero);
     }
    }
   }
   const bool target=b.driver==0||i==1;
   hp.remaining=target?(phase==0?300.f:phase==1?150.f:75.f):300.f;
   // Retire one AI after movement proof while still near the held player,
   // allowing an actual minimap color check. This remains a lab-only stimulus.
   if(target&&((i==1&&phase>=1)||(b.driver==0&&phase>=4))){hp.remaining=0;hp.retired=true;Retire(i);}
   else Write(i);
  }return;
 }
 if(hits>=6&&stage==0){stage=1;hits=0;next=GetTickCount64()+10000;}
 if(hits>=6||GetTickCount64()<next)return;
 for(unsigned i=0;i<bindings.size();++i){auto b=bindings[i];
  if(!b.damage||(stage==0?(i!=1||b.driver!=3):b.driver!=0)||!Participant(i,b.handle))continue;
  if(Effect(b.damage,b.handle,2,0)&&Effect(b.damage,b.handle,2,1)){
   ++hits;next=GetTickCount64()+1500;
   Log("EXPERIMENT_COMBAT_DEFEAT_STIMULUS slot=%u hit=%u hp=%.1f physical_contact=0 stage=%u",i,hits,ledger.racers[i].remaining,stage);
  }return;
 }
}
}
