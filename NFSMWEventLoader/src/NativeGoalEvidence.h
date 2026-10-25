// Passive, marker-gated lab evidence. Native callback and physical body
// proximity are required; EndRace's estimated results are recorded separately.
void(__cdecl* originalFinish)(void*);
void(__cdecl* originalCheckpoint)(void*,unsigned);
void(__thiscall* originalEnd)(void*);
unsigned genuineGoals=0;
bool forcedEstimationObserved=false;
bool waitForField=false;
void* deferredPlayer=nullptr;
BYTE* deferredRace=nullptr;
float deferredPlayerSeconds=0;
bool deferredEnd=false,deferredResults=false,deferredStop=false,deferredShow=false,deferredShowArgument=false;
void(__cdecl* originalResults)();
void(__cdecl* originalStop)();
void(__cdecl* originalShow)(bool);
void(__thiscall* originalDeactivateTrigger)(void*);
struct DeferredTrigger {void* object=nullptr;unsigned key=0,calls=0;};
std::vector<DeferredTrigger> deferredTriggers;
bool OwnedRace(BYTE* race){
 auto parameters=Read<BYTE*>(race,0x1968),record=Read<BYTE*>(parameters,8);
 auto collection=Read<registration_experiment::Collection*>(record,4);
 registration_experiment::Collection* seen[16]{};
 for(unsigned i=0;collection&&i<16;++i){
  for(unsigned j=0;j<i;++j)if(seen[j]==collection)return false;
  seen[i]=collection;if(auto clone=registration_experiment::Find(Read<unsigned>(collection,0x20));clone&&clone->race)return true;
  collection=Read<registration_experiment::Collection*>(collection,0x10);
 }
 return false;
}
void __cdecl Checkpoint(void* actor,unsigned checkpoint){
 auto race=*reinterpret_cast<BYTE**>(0x91E000);int slot=actor?reinterpret_cast<int(__cdecl*)(void*)>(0x60DAE0)(actor)-1:-1;
 const bool owned=race&&OwnedRace(race)&&slot>=0&&slot<16;
 auto before=owned?Read<int>(race,0x18+slot*0x194+0x2C):-1;
 originalCheckpoint(actor,checkpoint);
 if(owned&&race==*reinterpret_cast<BYTE**>(0x91E000))Log("EXPERIMENT_NATIVE_CHECKPOINT_CALLBACK slot=%d argument=%u before=%d after=%d body=%.3f,%.3f,%.3f",slot,checkpoint,before,Read<int>(race,0x18+slot*0x194+0x2C),motions[slot].last[0],motions[slot].last[1],motions[slot].last[2]);
}
unsigned FieldMask(BYTE* race){return mwel_difficulty::FieldMask(Read<unsigned>(race,0x1958));}
bool FieldComplete(BYTE* race){
 auto mask=FieldMask(race);if(!mask)return false;unsigned observed=genuineGoals;
 // Eliminated knockout cars cannot later cross the finish line.
 for(unsigned i=0;i<16&&(mask&(1u<<i));++i)if(Read<unsigned>(race,0x18+i*0x194+0x1C))observed|=1u<<i;
 return (observed&mask)==mask;
}
bool FieldPending(){return waitForField&&deferredPlayer&&deferredRace==*reinterpret_cast<BYTE**>(0x91E000)&&OwnedRace(deferredRace)&&!FieldComplete(deferredRace);}
void __fastcall DeactivateTrigger(void* self,void*){
 // Full-field lab hold must also preserve physical trigger registrations.
 // Native script cleanup can run outside Finish/EndRace and remove them as
 // soon as the player finishes, leaving physically driving rivals unable to
 // receive CP callbacks. This hold does not advance CP or finish state.
 if(FieldPending())for(const auto& pair:registration_experiment::geometryObjects)if(pair.second==self){
  auto clone=registration_experiment::Find(pair.first);
  if(clone&&!clone->race&&clone->templateKey!=0xAAF8BB9C){
   auto found=std::find_if(deferredTriggers.begin(),deferredTriggers.end(),[&](const DeferredTrigger& d){return d.object==self&&d.key==pair.first;});
   if(found==deferredTriggers.end()){deferredTriggers.push_back({self,pair.first,1});Log("EXPERIMENT_NATIVE_TRIGGER_HOLD key=%08X object=%08X enabled=%u active=%u native_CP_writes=0",pair.first,unsigned(self),Read<unsigned>(self,0x74),unsigned(Read<BYTE>(self,0xC4)));}
   else if(found->calls<4096)++found->calls;
   return;
  }
 }
 originalDeactivateTrigger(self);
}
void __cdecl Results(){if(FieldPending()){deferredResults=true;return;}originalResults();}
void __cdecl Stop(){if(FieldPending()){deferredStop=true;return;}originalStop();}
void __cdecl Show(bool argument){if(FieldPending()){deferredShow=true;deferredShowArgument=argument;return;}
 auto race=*reinterpret_cast<BYTE**>(0x91E000);auto params=Read<void*>(race,0x1968);
 Log("EXPERIMENT_RESULT_SHOW_ENTER win=%u context=%u params=%08X native_pursuit_presentation=%u rank=%u",unsigned(argument),Read<unsigned>(race,0x1964),unsigned(params),unsigned(params&&reinterpret_cast<bool(__thiscall*)(void*)>(0x5FBE70)(params)),Read<unsigned>(race,0x28));
 originalShow(argument);
 Log("EXPERIMENT_RESULT_SHOW_RETURN win=%u state=%u racing=%u timer=%u",unsigned(argument),*reinterpret_cast<unsigned*>(0x925E90),unsigned(Read<BYTE>(race,0x198A)),unsigned(Read<BYTE>(race,0x1978)));
}
void ReleaseField(){
 if(!waitForField||!deferredPlayer||!FieldComplete(deferredRace))return;
 auto race=*reinterpret_cast<BYTE**>(0x91E000);if(race!=deferredRace||!OwnedRace(race))return;
 auto actor=deferredPlayer;
 auto slot=reinterpret_cast<int(__cdecl*)(void*)>(0x60DAE0)(actor)-1;
 if(slot<0||slot>=16)return;
 auto info=race+0x18+slot*0x194;
 if(Read<BYTE>(info,0x154)||Read<float>(info,0x150)!=deferredPlayerSeconds){
  aborted=true;Log("EXPERIMENT_RACE_ABORT held_native_goal_timer_changed slot=%d captured=%.6f actual=%.6f running=%u",slot,deferredPlayerSeconds,Read<float>(info,0x150),unsigned(Read<BYTE>(info,0x154)));return;
 }
 deferredPlayer=nullptr;deferredRace=nullptr;
 Log("EXPERIMENT_NATIVE_FIELD_RELEASE all_field_observed=1 expected_mask=%X genuine_goal_mask=%X native_times_preserved=1",FieldMask(race),genuineGoals);
 for(const auto& entry:deferredTriggers){
  auto found=registration_experiment::geometryObjects.find(entry.key);
  if(found!=registration_experiment::geometryObjects.end()&&found->second==entry.object){
   for(unsigned i=0;i<entry.calls;++i)originalDeactivateTrigger(entry.object);
  }
 }
 deferredTriggers.clear();
 MWELHotPursuitFinish(actor,true,true,slot,originalFinish);
 if(!forcedEstimationObserved){
  int slot=reinterpret_cast<int(__cdecl*)(void*)>(0x60DAE0)(actor)-1;
   if(auto package=event_race_policies::Current();package&&slot>=0&&slot<16&&race==*reinterpret_cast<BYTE**>(0x91E000))profile_progress::Record(*package,race+0x18+slot*0x194,true,false);
 }
 if(race==*reinterpret_cast<BYTE**>(0x91E000)){
  if(deferredResults)originalResults();else{if(deferredEnd)originalEnd(race);if(deferredStop)originalStop();if(deferredShow)originalShow(deferredShowArgument);}
 }
 deferredEnd=deferredResults=deferredStop=deferredShow=false;
}
void __cdecl Finish(void* actor){
 auto race=*reinterpret_cast<BYTE**>(0x91E000);
 int slot=actor?reinterpret_cast<int(__cdecl*)(void*)>(0x60DAE0)(actor)-1:-1;
 bool verified=false;float distance=-1;
 const auto observedPackage=event_race_policies::Current();
 if(race&&slot>=0&&slot<16&&observedPackage){
  const auto& package=*observedPackage;auto info=race+0x18+slot*0x194;
  auto& motion=motions[slot];auto now=GetTickCount64();
  for(const auto& gate:package.authoring.at("gates"))if(gate.at("kind")=="finish"){
   auto p=gate.at("position"),dimensions=gate.at("dimensions");
   float dx=motion.last[0]+p.at(1).get<float>(),dy=motion.last[1]-p.at(2).get<float>(),dz=motion.last[2]-p.at(0).get<float>();
   distance=std::sqrt(dx*dx+dy*dy+dz*dz);
   float radius=0;for(const auto& component:dimensions){float size=component.get<float>();radius+=size*size;}radius=.5f*std::sqrt(radius)+3.f;
   verified=registration_experiment::authoredGeometry&&OwnedRace(race)&&motion.handle==Read<unsigned>(info)&&motion.moved&&now-motion.moved<1000&&distance<=radius&&Read<int>(info,0x28)>=int(mwel::RaceLaps(package.authoring))&&!Read<BYTE>(info,0x190)&&!forcedEstimationObserved;
   if(verified)genuineGoals|=1u<<slot;
   float nativeSeconds=Read<float>(info,0x150);
   if(Read<BYTE>(info,0x154))nativeSeconds+=reinterpret_cast<float(__cdecl*)()>(0x6E8DE0)()-Read<float>(info,0x14C);
   Log("EXPERIMENT_NATIVE_GOAL_TIMER slot=%d handle=%u native_seconds=%.6f native_timer_written=0",slot,motion.handle,nativeSeconds);
   const auto actorTable=actor?Read<void**>(actor):nullptr;
   const bool human=actorTable&&reinterpret_cast<bool(__thiscall*)(void*)>(actorTable[9])(actor);
   Log("EXPERIMENT_NATIVE_GOAL_CALLBACK slot=%d handle=%u lap=%d checkpoint=%d DNF=%u distance_to_authored_goal=%.3f physical_body_age_ms=%llu genuine_goal_verified=%u all_field_observed=%u player=%u",slot,motion.handle,Read<int>(info,0x28),Read<int>(info,0x2C),unsigned(Read<BYTE>(info,0x190)),distance,now-motion.moved,unsigned(verified),unsigned(FieldComplete(race)),unsigned(human));
   break;
  }
 }
 if(waitForField&&verified&&!FieldComplete(race)&&actor){
  auto table=Read<void**>(actor);
  if(table&&reinterpret_cast<bool(__thiscall*)(void*)>(table[9])(actor)){
   // 005E97B0 also sets mFinishedRacing. Native scripts can observe that
   // flag before the deferred Finish callback and terminate rival triggers.
   // Stop only GTimer, using the exact native clock and arithmetic verified
   // at 005E97C0..005E97DF. Do not alter checkpoints or finish flags.
   auto info=race+0x18+slot*0x194;
   if(Read<BYTE>(info,0x154)){
    float total=reinterpret_cast<float(__cdecl*)()>(0x6E8DE0)()-Read<float>(info,0x14C)+Read<float>(info,0x150);
    if(!std::isfinite(total)||total<0){aborted=true;Log("EXPERIMENT_RACE_ABORT native_goal_timer_invalid slot=%d",slot);return;}
    memcpy(info+0x150,&total,sizeof(total));info[0x154]=0;
   }
   deferredPlayerSeconds=Read<float>(info,0x150);
   deferredPlayer=actor;deferredRace=race;
   Log("EXPERIMENT_NATIVE_FIELD_HOLD slot=%d native_timer_stopped=1 native_seconds=%.6f finished_flag=%u native_CP_writes=0 actual_goal_required=1",slot,deferredPlayerSeconds,unsigned(Read<BYTE>(info,0x21)));return;
  }
 }
 bool player=false;auto actorTable=actor?Read<void**>(actor):nullptr;
 if(actorTable)player=reinterpret_cast<bool(__thiscall*)(void*)>(actorTable[9])(actor);
 auto productionMarker=game/L"MWEventLoader.production-finish-test";
 if(mwel::fs::exists(productionMarker)){
  auto mode=mwel::Read(productionMarker,128);
  if(mode=="MW_EVENT_LOADER_PRODUCTION_FINISH_V1\n"||mode=="MW_EVENT_LOADER_NULL_PLAYER_FINISH_V1\n"){
   Log("EXPERIMENT_PRODUCTION_FINISH_DELEGATE slot=%d player=%u physical_goal=%u null_player=%u",slot,unsigned(player),unsigned(verified),unsigned(player&&mode=="MW_EVENT_LOADER_NULL_PLAYER_FINISH_V1\n"));
   MWELLabProductionFinish(player&&mode=="MW_EVENT_LOADER_NULL_PLAYER_FINISH_V1\n"?nullptr:actor,originalFinish);return;
  }
 }
 MWELHotPursuitFinish(actor,verified,player,slot,originalFinish);
 if(player&&verified&&observedPackage&&race==*reinterpret_cast<BYTE**>(0x91E000))profile_progress::Record(*observedPackage,race+0x18+slot*0x194,true,forcedEstimationObserved);
}
void __fastcall End(void* self,void*){
 if(FieldPending()&&self==deferredRace){deferredEnd=true;return;}
 // Speed-trap completion requests EndRace before the player's Finish callback
 // (sprint requests it afterwards). The isolated full-field observer must hold
 // that request at an actual player goal crossing, otherwise native estimates
 // stop trailing bodies before they can provide genuine goal evidence.
 const auto observedPackage=event_race_policies::Current();
 if(waitForField&&!deferredPlayer&&self==*reinterpret_cast<BYTE**>(0x91E000)&&OwnedRace(static_cast<BYTE*>(self))&&!FieldComplete(static_cast<BYTE*>(self))&&observedPackage){
  auto race=static_cast<BYTE*>(self);auto& motion=motions[0];auto now=GetTickCount64();
  if(motion.handle==Read<unsigned>(race,0x18)&&motion.moved&&now-motion.moved<1000&&Read<int>(race,0x18+0x28)>=int(mwel::RaceLaps(observedPackage->authoring)))
   for(const auto& gate:observedPackage->authoring.at("gates"))if(gate.at("kind")=="finish"){
    const auto& p=gate.at("position");float radius=0;for(const auto& c:gate.at("dimensions")){float size=c.get<float>();radius+=size*size;}
    float distance=std::sqrt(std::pow(motion.last[0]+p.at(1).get<float>(),2)+std::pow(motion.last[1]-p.at(2).get<float>(),2)+std::pow(motion.last[2]-p.at(0).get<float>(),2));
    if(distance<=.5f*std::sqrt(radius)+3.f){deferredEnd=true;Log("EXPERIMENT_NATIVE_END_BEFORE_PLAYER_FINISH_HELD physical_goal_distance=%.3f native_finish_flags_written=0",distance);return;}
   }
 }
 unsigned count=Read<unsigned>(self,0x1958),unfinished=0;
 if(count<=16)for(unsigned i=0;i<count;++i)unfinished+=!Read<BYTE>(self,0x18+i*0x194+0x21);
 if(unfinished)forcedEstimationObserved=true;
 Log("EXPERIMENT_NATIVE_END_RACE unfinished_before_end=%u estimated_results_possible=%u genuine_goal_mask=%X",unfinished,unsigned(unfinished!=0),genuineGoals);
 auto productionMarker=game/L"MWEventLoader.production-finish-test";
 if(mwel::fs::exists(productionMarker)){
  auto mode=mwel::Read(productionMarker,128);
  if(mode=="MW_EVENT_LOADER_PRODUCTION_FINISH_V1\n"||mode=="MW_EVENT_LOADER_NULL_PLAYER_FINISH_V1\n"){
   Log("EXPERIMENT_PRODUCTION_END_DELEGATE native_end_cleanup=1");MWELLabProductionEnd(self,originalEnd);return;
  }
 }
 originalEnd(self);
}
bool InstallFieldHooks(std::vector<void*>& installed){
 const struct Hook{unsigned address;void* detour;void** original;const char* guard;unsigned length;} hooks[]={
  {0x611F20,Results,reinterpret_cast<void**>(&originalResults),"\x8B\x0D\x00\xE0\x91\x00\x83\xEC\x14",9},
  {0x60DEB0,Stop,reinterpret_cast<void**>(&originalStop),"\x8B\x0D\x00\xE0\x91\x00\xE8\xE5\xAC\xFD\xFF",11},
  {0x60AC20,Show,reinterpret_cast<void**>(&originalShow),"\x64\xA1\x00\x00\x00\x00\x8B\x0D\x00\xE0\x91\x00",12}
  ,{0x5E85A0,DeactivateTrigger,reinterpret_cast<void**>(&originalDeactivateTrigger),"\x56\x8B\xF1\x8B\x86\xC8\x00\x00\x00",9}
 };
 const BYTE timer[]={0x83,0xEC,0x10,0x56,0x8B,0xF1,0x8A,0x86,0x54,0x01,0,0};
 if(memcmp(reinterpret_cast<void*>(0x5E97B0),timer,sizeof(timer)))return false;
 const BYTE clock[]={0xD9,0x05,0xD8,0x85,0x98,0,0xC3};
 if(memcmp(reinterpret_cast<void*>(0x6E8DE0),clock,sizeof(clock)))return false;
 for(auto& hook:hooks)if((waitForField||hook.address==0x60AC20)&&memcmp(reinterpret_cast<void*>(hook.address),hook.guard,hook.length))return false;
 for(auto& hook:hooks){if(!waitForField&&hook.address!=0x60AC20)continue;auto address=reinterpret_cast<void*>(hook.address);if(MH_CreateHook(address,hook.detour,hook.original)!=MH_OK)return false;installed.push_back(address);}
 return true;
}
