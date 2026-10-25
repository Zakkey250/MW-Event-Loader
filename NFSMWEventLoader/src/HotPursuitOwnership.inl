// Included inside hot_pursuit_runtime after the validated native helpers.
Assignment* AssignmentFor(unsigned car){
 const auto h=Handle(car);for(auto& a:assignments)if(a.cop==h&&a.car==car)return &a;return nullptr;
}
bool LockValid(const Assignment& a){
 if(GetCurrentThreadId()!=combat_health_runtime::gameThread||!owner||owner!=MWELNeoCurrentKey()||!Active()||!combat_health_runtime::Racing()||
    !a.car||Handle(a.car)!=a.cop||!Table(a.car,0x8AA828)||!ReadyTarget(a.target)||
    !reinterpret_cast<bool(__thiscall*)(unsigned)>(0x688200)(a.car)||
    reinterpret_cast<bool(__thiscall*)(unsigned)>(0x688170)(a.car))return false;
 unsigned ai=0,target=0;if(!Read(a.car+0x54,ai)||!Table(ai,0x892480)||Roadblock(a.car,ai)||!Read(ai+0x54,target)||target!=a.targetObject)return false;
 return true; // intent survives native regrouping until release or target exit
}
void Remember(unsigned car,unsigned target){
 unsigned ai=0,object=0;if(!ReadyTarget(target)||!Read(car+0x54,ai)||!Table(ai,0x892480)||Roadblock(car,ai)||!Read(ai+0x54,object)||!Table(object,0x891100))return;
 auto a=AssignmentFor(car);if(a&&a->target==target&&LockValid(*a))return;
 if(!a)for(auto& entry:assignments)if(!entry.car||!LockValid(entry)){a=&entry;break;}
 if(a)*a={car,Handle(car),target,object,GetTickCount64()+8000,false};
}
unsigned FindPursuit(unsigned target){
 unsigned manager=0,head=0,node=0;
 if(!Read(0x92C67C,manager)||!Table(manager,0x891900)||!Read(manager-0x54+0x128,head)||!Read(head,node))return 0;
 for(unsigned i=0;node&&node!=head&&i<64;++i){unsigned pursuit=0;
  if(Read(node+8,pursuit)&&Table(pursuit,0x892770)&&TargetHandle(pursuit)==target)return pursuit;
  if(!Read(node,node))break;
 }return 0;
}
void ReleaseGroups(){
 if(!owner||(MWELNeoCurrentKey()!=0&&MWELNeoCurrentKey()!=owner))return;
 for(unsigned slot=1;slot<createdPursuits.size();++slot){const auto pursuit=createdPursuits[slot];
  // First establish native list membership. A stale address can still carry
  // an old vtable after the native result/retry path frees its activity.
  const auto groups=NativeGroups();
  if(pursuit&&std::find(groups.begin(),groups.end(),pursuit)!=groups.end()&&Table(pursuit,0x892770)){
   const auto target=TargetHandle(pursuit);
   if(target&&target==createdTargets[slot]&&FindPursuit(target)==pursuit)reinterpret_cast<void(__thiscall*)(unsigned)>(0x4198E0)(pursuit);
  }
 }
}
void BindPerpetrator(unsigned pursuit,unsigned target){
 const auto car=Vehicle(target);unsigned ai=0,current=0,com=0;
 if(!car||!Read(car+0x54,ai)||!Read(ai+0x70,current)||current==pursuit||!Read(car-0x80+4,com))return;
 const auto service=reinterpret_cast<unsigned(__thiscall*)(unsigned,unsigned)>(0x5D59F0)(com,0x402820);
 unsigned table=0,add=0;if(!Read(service,table)||!Read(table+4,add)||add<0x401000||add>=0x889000)return;
 if(Table(current,0x892770))reinterpret_cast<bool(__thiscall*)(unsigned,unsigned)>(0x4338B0)(current,car);
 reinterpret_cast<void(__thiscall*)(unsigned,unsigned)>(add)(service,pursuit);
 Read(ai+0x70,current);
 Log("HOT_PURSUIT_PERP_BIND racer=%u pursuit=%08X actual=%08X native_participant=1",target,pursuit,current);
}
// Native free-roam replenishment (00430F66..00431204) unconditionally uses
// the target vehicle's pursuit. A queued request can outlive event teardown.
// Reject that invalid request before activation/count/list side effects.
bool __fastcall SpawnForPursuit(unsigned manager,void*,unsigned pursuit,unsigned car){
 if((Active()||gauntlet_runtime::Duel())&&!roadblockDepth){
  ApplyDeployment(manager+0x54,DeploymentLimit());
  if(ChaseCount(0)>=DeploymentLimit())return false;
 }
 // Queued native/NFRR requests may predate our per-frame allocation. Enforce
 // individual eligibility at the last native activation boundary as well.
 if(Active()&&!roadblockDepth){
  const auto target=TargetHandle(pursuit);
  if(!ReadyTarget(target)||!replacements.RefillReady(GetTickCount64())||replacements.Held(target,GetTickCount64()))return false;
 }
 unsigned race=0,play=0;
 if(Read(0x91E000,race)&&Read(race+0x1960,play)&&play!=1&&Table(pursuit,0x892770)){
  unsigned target=0,sim=0,com=0,ownPursuit=0;Read(pursuit+0x74,target);Read(target+0x1C,sim);Read(sim+4,com);
  const auto ai=com?reinterpret_cast<unsigned(__thiscall*)(unsigned,unsigned)>(0x5D59F0)(com,0x403700):0;
  if(!Read(ai+0x70,ownPursuit)||!ownPursuit){Log("HOT_PURSUIT_STALE_REPLENISHMENT_REFUSED pursuit=%08X native_target_pursuit_missing=1",pursuit);return false;}
 }
 const bool spawned=originalPursuitSpawn(manager,pursuit,car);
 // Placement commits its physics transform after the native activation
 // callback. Finalize on the next simulation frame, never from the stale
 // pooled-body coordinates still visible at this return boundary.
 if(spawned&&Active()&&owner==MWELNeoCurrentKey()&&combat_health_runtime::Racing()){
  unsigned ai=0,driver=0;
  if(Read(car+0x94,driver)&&driver==2&&Read(car+0x54,ai)&&Table(ai,0x892480)&&!Roadblock(car,ai)&&!SUV(car)){
   auto it=std::find_if(spawnActivations.begin(),spawnActivations.end(),[&](const auto& a){return !a.car||a.car==car;});
   if(it!=spawnActivations.end()){*it={car,Handle(car),GetTickCount64()};Log("HOT_PURSUIT_ACTIVATION_QUEUED cop=%u await_physics=1",Handle(car));}
  }
 }
 return spawned;
}
void FinalizeSpawnActivations(){
 if(!owner||owner!=MWELNeoCurrentKey())return;
 const auto now=GetTickCount64();
 for(auto& a:spawnActivations){
  if(!a.car)continue;
  if(now<a.at+1)continue;
  if(now>a.at+750||Vehicle(a.handle)!=a.car){
   if(worldLogEnabled&&Vehicle(a.handle)==a.car){unsigned ai=0,pursuit=0;Read(a.car+0x54,ai);Read(ai+0x70,pursuit);combat_impact::Motion m;
    if(combat_health_runtime::NativeMotion(a.car-0x80,m)){
     const auto target=Vehicle(TargetHandle(pursuit));combat_impact::Motion targetMotion;float gap=-1;
     if(target&&combat_health_runtime::NativeMotion(target-0x80,targetMotion))gap=std::hypot(m.position.x-targetMotion.position.x,m.position.z-targetMotion.position.z);
     Log("HOT_PURSUIT_ACTIVATION_EXPIRED cop=%u racer=%u position=%.2f,%.2f,%.2f speed_mps=%.2f active=%u target_gap_m=%.2f",a.handle,TargetHandle(pursuit),m.position.x,m.position.y,m.position.z,std::hypot(m.velocity.x,m.velocity.z),unsigned(reinterpret_cast<bool(__thiscall*)(unsigned)>(0x688200)(a.car)),gap);
    }
   }a={};continue;
  }
  // A successful factory request can still refer to a dormant pooled body.
  // Wait for native activation; never reposition or accelerate that body.
  if(!reinterpret_cast<bool(__thiscall*)(unsigned)>(0x688200)(a.car))continue;
  if(reinterpret_cast<bool(__thiscall*)(unsigned)>(0x688170)(a.car)){a={};continue;}
  unsigned ai=0,pursuit=0,driver=0;
  if(!Read(a.car+0x54,ai)||!Table(ai,0x892480)||Roadblock(a.car,ai)||SUV(a.car)||
     !Read(a.car+0x94,driver)||driver!=2||!Read(ai+0x70,pursuit)){a={};continue;}
  const auto target=TargetHandle(pursuit),racer=Vehicle(target);if(!ReadyTarget(target)||!racer)continue;
  combat_impact::Motion motion,targetMotion;if(!combat_health_runtime::NativeMotion(a.car-0x80,motion)||!combat_health_runtime::NativeMotion(racer-0x80,targetMotion))continue;
  unsigned race=0,count=0,leg=0;const auto slot=Slot(target,race,count);if(slot>=count)continue;Read(race+0x18+slot*0x194+0x2C,leg);
  combat_impact::Vector forward{};float best=6400;bool direction=false;
  for(unsigned candidate:{leg,leg+1,leg?leg-1:leg}){
   float point[3]{},tangent[3]{};if(!roadReady||!MWELCourseAhead(owner,&motion.position.x,candidate,1,point,tangent))continue;
   const float dx=point[0]-motion.position.x,dy=point[1]-motion.position.y,dz=point[2]-motion.position.z,score=dx*dx+dz*dz+4*dy*dy;
   if(score<best){best=score;forward={tangent[0],tangent[1],tangent[2]};direction=true;}
  }
  if(!direction)continue; // wait for the native placement publication
  const float before=std::hypot(motion.velocity.x,motion.velocity.z);float dot=combat_impact::Dot(motion.forward,forward);
  bool aligned=dot>=.7f;
  if(!aligned){
   if(!reinterpret_cast<bool(__thiscall*)(unsigned,const combat_impact::Vector*,const combat_impact::Vector*)>(0x422730)(ai,&motion.position,&forward))continue;
   combat_impact::Motion placed;if(!combat_health_runtime::NativeMotion(a.car-0x80,placed))continue;
   const auto delta=placed.position-motion.position;dot=combat_impact::Dot(placed.forward,forward);
   aligned=std::hypot(delta.x,delta.z)<=3&&std::abs(delta.y)<=3&&dot>=.7f;
  }
  if(!aligned)continue;
  const auto targetDelta=motion.position-targetMotion.position;
  const float rolling=hot_pursuit::RollingSpeed(std::hypot(targetMotion.velocity.x,targetMotion.velocity.z),std::hypot(targetDelta.x,targetDelta.z));
  if(before<rolling)reinterpret_cast<void(__thiscall*)(unsigned,float)>(0x670210)(a.car,rolling);
  Log("HOT_PURSUIT_NATIVE_ROLLING cop=%u racer=%u before_mps=%.3f rolling_mps=%.3f heading_dot=%.3f aligned=1 activation_age_ms=%llu activation_only=1",a.handle,target,before,std::max(before,rolling),dot,now-a.at);
  a={};
 }
}
unsigned EnsurePursuit(unsigned slot,unsigned target){
 if(auto existing=FindPursuit(target)){BindPerpetrator(existing,target);return existing;}
 if(!slot||slot>=16||!ReadyTarget(target)||GetTickCount64()<nextPursuitCreate[slot])return 0;
 nextPursuitCreate[slot]=GetTickCount64()+5000;
 // Follow the registered AIPursuit factory and CopMgr attachment sequence in
 // native PursueAtHeatLevel (0042ECDB..0042ED73), replacing only the target sim.
 const auto key=reinterpret_cast<unsigned(__cdecl*)(const char*)>(0x5CC240)("AIPursuit");
 unsigned node=0,fn=0;Read(0x92C668,node);
 for(unsigned i=0;node&&i<256;++i){unsigned name=0;Read(node,name);if(name==key){Read(node+4,fn);break;}if(!Read(node+8,node))break;}
 if(fn!=0x443270)return 0;
 struct FactoryArgs {unsigned x,y,z,pad;};
 const auto activity=reinterpret_cast<unsigned(__cdecl*)(FactoryArgs)>(fn)({0,0,0,0});
 unsigned com=0;if(!Table(activity,0x8928D0)||!Read(activity+4,com))return 0;
 const auto pursuit=reinterpret_cast<unsigned(__thiscall*)(unsigned,unsigned)>(0x5D59F0)(com,0x4038C0);
 unsigned manager=0;if(!Table(pursuit,0x892770)||!Read(0x92C67C,manager)||!Table(manager-0x28,0x89195C))return 0;
 reinterpret_cast<bool(__thiscall*)(unsigned,unsigned)>(0x402900)(manager-0x28,pursuit);
 unsigned targetObject=0;Read(pursuit+0x74,targetObject);const auto car=Vehicle(target);
 if(car&&Table(targetObject,0x891100))originalTarget(targetObject,car-0x80);
 if(FindPursuit(target)!=pursuit)return 0;
 BindPerpetrator(pursuit,target);
 createdPursuits[slot]=pursuit;
 createdTargets[slot]=target;
 Log("HOT_PURSUIT_NATIVE_GROUP_CREATED racer=%u pursuit=%08X factory=00443270 cop_manager_registered=1",target,pursuit);
 return pursuit;
}
unsigned ActivityContainer(unsigned pursuit){
 unsigned container=0;return Table(pursuit,0x892770)&&Table(pursuit-0x1C,0x8928D0)&&Read(pursuit-4,container)?container:0;
}
unsigned ownershipDepth=0;
std::vector<unsigned> NativeGroups(){
 std::vector<unsigned> groups;unsigned manager=0,head=0,node=0;
 if(!Read(0x92C67C,manager)||!Table(manager,0x891900)||!Read(manager-0x54+0x128,head)||!Read(head,node))return groups;
 for(unsigned i=0;node&&node!=head&&i<64;++i){unsigned pursuit=0;if(Read(node+8,pursuit)&&Table(pursuit,0x892770))groups.push_back(pursuit);if(!Read(node,node))break;}
 return groups;
}
bool ExclusiveJoin(unsigned car,unsigned desired){
 const auto container=ActivityContainer(desired);if(!container)return false;
 unsigned ai=0,current=0;Read(car+0x54,ai);Read(ai+0x70,current);
 ++ownershipDepth;
 // Native OnAttached only stores the first mPursuit reference (00414F70).
 // Remove all stale memberships first, not just the cached reference. When
 // the cached owner changes, reattach desired too so native OnAttached runs.
 for(const auto group:NativeGroups())if(group!=desired||current!=desired)
  reinterpret_cast<bool(__thiscall*)(unsigned,unsigned)>(0x4338B0)(group,car);
 const bool added=originalActivity(container,car);
 --ownershipDepth;Read(ai+0x70,current);
 return added||current==desired;
}
void __fastcall AcquireTarget(unsigned self,void*,unsigned sim){
 if(!roadblockDepth&&sim&&owner)for(auto& a:assignments)if(a.targetObject==self&&LockValid(a)){
  const auto target=Vehicle(a.target);if(target&&sim!=target-0x80){sim=target-0x80;if(worldLogEnabled)++steeringRedirects;}break;
 }
 RedirectHelicopterTarget(self,sim);
 originalTarget(self,sim);
}
unsigned Allocated(unsigned target,const std::vector<unsigned>& cars){
 unsigned n=replacements.Held(target,GetTickCount64());for(const auto car:cars){unsigned driver=0,ai=0,pursuit=0;if(!Read(car+0x94,driver)||driver!=2||!Read(car+0x54,ai)||!Table(ai,0x892480)||Roadblock(car,ai)||reinterpret_cast<bool(__thiscall*)(unsigned)>(0x688170)(car))continue;
  if(auto a=AssignmentFor(car);a&&LockValid(*a)){if(a->target==target)++n;}
  else if(Read(ai+0x70,pursuit)&&TargetHandle(pursuit)==target)++n;
 }return n;
}
unsigned ChooseAssignment(unsigned incoming,unsigned car){
 std::vector<unsigned> cars;if(!List(cars))return 0;
 unsigned race=0,count=0;if(!Read(0x91E000,race)||!Read(race+0x1958,count)||count>16)return 0;
 unsigned prior=0,ai=0,pursuit=0;
 if(auto a=AssignmentFor(car);a&&LockValid(*a))prior=a->target;
 else if(Read(car+0x54,ai)&&Read(ai+0x70,pursuit))prior=TargetHandle(pursuit);
 unsigned best=0,bestCount=~0u,bestRank=~0u;
 for(unsigned slot=0;slot<count;++slot){unsigned target=0,rank=0;Read(race+0x18+slot*0x194,target);Read(race+0x18+slot*0x194+0x10,rank);
  if(!ReadyTarget(target)||!FindPursuit(target))continue;if(!rank||rank>count)rank=slot+1;
  const auto allocated=Allocated(target,cars);const auto amount=allocated-(prior==target&&allocated?1u:0u);if(amount>=TargetQuota(rank,count))continue;
  // Fill every racer's first slot before any second slot. Keep the native
  // incoming target only when it is tied for the least-covered racer.
  if(amount<bestCount||(amount==bestCount&&((target==incoming&&best!=incoming)||(best!=incoming&&rank<bestRank)))){
   best=target;bestCount=amount;bestRank=rank;}
 }
 (void)car;return best;
}
bool __fastcall AddActivity(unsigned self,void*,unsigned car){
 unsigned activityOwner=0,driver=0,ai=0;
 // The container stores IActivityOwner (+38), not IActivity (+2C).
 // Verified owner table 008928B4; IPursuit is another +10 bytes.
 if(!roadblockDepth&&!ownershipDepth&&owner&&!draining&&GetCurrentThreadId()==combat_health_runtime::gameThread&&
    Read(self+0x10,activityOwner)&&Table(activityOwner,0x8928B4)&&Table(car,0x8AA828)&&Read(car+0x94,driver)&&driver==2&&
    Read(car+0x54,ai)&&Table(ai,0x892480)&&!Roadblock(car,ai)){
  auto a=AssignmentFor(car);if(!a||!LockValid(*a)){const auto incoming=TargetHandle(activityOwner+0x10);const auto chosen=ChooseAssignment(incoming,car);
   if(chosen){Remember(car,chosen);a=AssignmentFor(car);}
   else return false;
  }
  if(a&&LockValid(*a)){
   const auto desired=FindPursuit(a->target),container=ActivityContainer(desired);
   if(container&&container!=self){if(worldLogEnabled)++ownershipRedirects;return ExclusiveJoin(car,desired);}
  }
 }
 return originalActivity(self,car);
}
void __fastcall ReleaseCop(unsigned manager,void*,unsigned car){
 if(!roadblockDepth&&owner&&!draining&&GetCurrentThreadId()==combat_health_runtime::gameThread){unsigned ai=0,current=0;Read(car+0x54,ai);Read(ai+0x70,current);
  ++ownershipDepth;for(const auto group:NativeGroups())if(group!=current)reinterpret_cast<bool(__thiscall*)(unsigned,unsigned)>(0x4338B0)(group,car);--ownershipDepth;
  const auto h=Handle(car);for(auto& a:assignments)if(a.car==car&&a.cop==h)a={};
  for(auto& d:detections)if(d.cop==h)d={};
 }
 originalRelease(manager,car);
}
void RefreshAssignments(const std::vector<unsigned>& cars){
 unsigned confirmed=0,mismatch=0,steeringMismatch=0;
 for(const auto car:cars){unsigned driver=0,ai=0,pursuit=0;
  if(!Read(car+0x94,driver)||driver!=2||!Read(car+0x54,ai)||!Table(ai,0x892480)||Roadblock(car,ai)||!Read(ai+0x70,pursuit))continue;
  const auto actual=TargetHandle(pursuit);auto a=AssignmentFor(car);
  // Native retirement can detach the activity before this frame. Keep the
  // exact car/handle assignment as evidence of its old target in that case.
  const auto previous=EndedTarget(actual)?actual:(a&&EndedTarget(a->target)?a->target:0u);
  const bool retiredTarget=previous!=0;
  if(retiredTarget){
   if(a)*a={};
   const auto chosen=ChooseAssignment(ReadyTarget(actual)?actual:0,car);
   if(!chosen)continue;
   Remember(car,chosen);a=AssignmentFor(car);
  }
  if(!a&&actual){const auto chosen=ChooseAssignment(actual,car);if(chosen){Remember(car,chosen);a=AssignmentFor(car);}}
  if(!a)continue;if(!LockValid(*a)){*a={};continue;}
  if(actual==a->target)a->confirmed=true;
  else if(const auto desired=FindPursuit(a->target);desired&&ActivityContainer(desired)){
   // Move between real native activities, retaining pool/controller ownership.
   // No cached mPursuit pointer, list count, or target position is fabricated.
   ExclusiveJoin(car,desired);
   Read(ai+0x70,pursuit);
   unsigned object=0;if(Read(ai+0x54,object)&&Table(object,0x891100))a->targetObject=object;
   if(TargetHandle(pursuit)==a->target)a->confirmed=true;
  }
  if(a->confirmed){++confirmed;if(TargetHandle(pursuit)!=a->target)++mismatch;}
  const auto target=Vehicle(a->target);if(target){originalTarget(a->targetObject,target-0x80);unsigned sim=0;Read(a->targetObject+0x1C,sim);if(sim!=target-0x80)++steeringMismatch;}
  if(retiredTarget&&a->confirmed)Log("HOT_PURSUIT_SURVIVOR_HANDOFF cop=%u previous=%u racer=%u native_membership=1 visible_car_retained=1",Handle(car),previous,a->target);
 }
 if(worldLogEnabled){static ULONGLONG logAt=0;if(GetTickCount64()>=logAt){logAt=GetTickCount64()+3000;
  Log("HOT_PURSUIT_OWNERSHIP confirmed=%u pursuit_mismatch=%u steering_mismatch=%u activity_redirects=%u steering_redirects=%u native_membership=1",confirmed,mismatch,steeringMismatch,ownershipRedirects,steeringRedirects);
 }}
}
