#pragma once
// Explicit isolated-lab integration stimulus. This tests the real native loss
// event/script/menu path; it is not evidence of six physical weapon contacts.
namespace combat_defeat_experiment {
bool enabled=false,visual=false,arrest=false,visualPlayerReleased=false;unsigned epoch=0,hits=0,stage=0,epochs=0;ULONGLONG next=0,visualBegan=0;unsigned visualStage=99;
void Configure(){
 if(productionRuntime)return;
 auto marker=game/L"MWEventLoader.combat-defeat-test";
 const auto text=mwel::fs::exists(marker)?mwel::Read(marker,128):std::string();
 visual=text=="MW_EVENT_LOADER_COMBAT_VISUAL_V1\n";
 arrest=text=="MW_EVENT_LOADER_GAUNTLET_ARREST_V1\n";
 enabled=visual||arrest||text=="MW_EVENT_LOADER_COMBAT_DEFEAT_V1\n";
}
void Frame(){
 if(!enabled||!combat_health_runtime::ledger.event)return;
 using namespace combat_health_runtime;
 unsigned race=0;BYTE staging=0,racing=0;
 if(!Read(0x91E000,race)||!Read(race+0x198A,staging)||!Read(race+0x1978,racing)||!staging||!racing)return;
 if(epoch!=generation){epoch=generation;hits=stage=0;visualBegan=GetTickCount64();visualStage=99;visualPlayerReleased=false;next=visualBegan+12000;++epochs;
  auto package=event_race_policies::Current();if(!visual&&package&&package->authoring.value("combatMode",std::string("none"))=="gauntlet")stage=1;
  Log("EXPERIMENT_COMBAT_DEFEAT_EPOCH epoch=%u iteration=%u retry_stimulus_disabled=%u",epoch,epochs,unsigned(epochs>1));}
 if(epochs>1)return;
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
