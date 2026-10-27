#pragma once
// Explicit isolated-lab integration stimulus. This tests the real native loss
// event/script/menu path; it is not evidence of six physical weapon contacts.
namespace combat_defeat_experiment {
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
 visual=text=="MW_EVENT_LOADER_COMBAT_VISUAL_V1\n";
 arrest=text=="MW_EVENT_LOADER_GAUNTLET_ARREST_V1\n";
 survivalScenario=text=="MW_EVENT_LOADER_ARMS_SURVIVAL_V1\n"?1:text=="MW_EVENT_LOADER_ARMS_SURVIVAL_LOSS_V1\n"?2:text=="MW_EVENT_LOADER_ARMS_SURVIVAL_WAIT_V1\n"?3:0;
 survival=survivalScenario!=0;
 enabled=visual||arrest||survival||text=="MW_EVENT_LOADER_COMBAT_DEFEAT_V1\n";
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
