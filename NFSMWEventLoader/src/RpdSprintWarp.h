#pragma once

namespace rpd_sprint_warp {
using namespace rpd_hot_pursuit_runtime;
using combat_impact::Motion;
using combat_impact::Vector;

constexpr float DistanceRequired=5000.f;
constexpr float FadeSeconds=5.f;
constexpr float BlackStart=2.f;
constexpr float BlackEnd=3.f;
constexpr float BranchClearance=18.f;
constexpr unsigned AnyLeg=~0u;

void* inputTrampoline=nullptr;
bool inputAttempted=false;
Run run{};
bool eligible=false,notified=false,pressed=false,warping=false,placementAttempted=false,placementSucceeded=false,checkingGround=false;
float elapsed=0,clock=-1,fade=0;
ULONGLONG nextCheck=0;
struct Landing {Vector position{},forward{};unsigned leader=0;};
Landing landing{};
Motion origin{};
float placedAt=0;

void Reset(){
 run={};eligible=notified=pressed=warping=placementAttempted=placementSucceeded=checkingGround=false;
 elapsed=fade=0;clock=-1;nextCheck=0;
 landing={};
}

bool __cdecl ObserveAction(unsigned action){
 unsigned id=0;
 if((!eligible&&!warping)||!Read(action,id)||id!=46)return false;
 if(eligible)pressed=true;
 return true;
}

// Observe the item returned to the native HUD. This site is after the peek
// call so NFRR's existing queue observer still sees the original return PC.
void __declspec(naked) Input(){__asm{
 pushfd
 pushad
 mov eax,[esp+38h]
 mov ebp,esp
 sub esp,528
 and esp,0FFFFFFF0h
 fxsave [esp]
 push eax
 call ObserveAction
 add esp,4
 fxrstor [esp]
 test al,al
 jnz consumed
 mov esp,ebp
 popad
 popfd
 jmp dword ptr [inputTrampoline]
consumed:
 mov esp,ebp
 popad
 popfd
 push 0x57C9E2
 ret
}}

bool InstallInput(){
 if(inputAttempted)return inputTrampoline!=nullptr;
 inputAttempted=true;
 const BYTE expected[]={0x8B,0x55,0x18,0x0B,0x55,0x1C};
 const BYTE drain[]={0x8B,0xCF,0xE8,0x97,0x73,0x0B,0x00};
 auto site=reinterpret_cast<void*>(0x57C742);
 if(memcmp(site,expected,sizeof(expected))||memcmp(reinterpret_cast<void*>(0x57C9E2),drain,sizeof(drain))||
    MH_CreateHook(site,Input,&inputTrampoline)!=MH_OK)return false;
 if(MH_EnableHook(site)!=MH_OK){MH_RemoveHook(site);inputTrampoline=nullptr;return false;}
 Log("RPD_SPRINT_WARP_INPUT native_hud_action=46 native_queue_drain=1");
 return true;
}

bool Notice(bool noBranch=false){
 static const unsigned key=event_titles::titles.emplace(expanded_leaderboard::Hash("MWEL_RPD_SPRINT_WARP"),
  event_titles::Title{L"Use Engage Event to respawn near the lead racer", "RPD sprint warp", {},
   L"\u300c\u30a4\u30d9\u30f3\u30c8\u53c2\u52a0\u300d\u3067\u30c8\u30c3\u30d7\u30ec\u30fc\u30b5\u30fc\u306e\u8fd1\u304f\u306b\u30ea\u30b9\u30dd\u30fc\u30f3\u3059\u308b"}).first->first;
 static const unsigned noBranchKey=event_titles::titles.emplace(expanded_leaderboard::Hash("MWEL_RPD_SPRINT_WARP_NO_BRANCH"),
  event_titles::Title{L"No clear road near the lead racer", "RPD sprint warp unavailable", {},
   L"\u5148\u982d\u4ed8\u8fd1\u306b\u5b89\u5168\u306a\u9053\u8def\u304c\u3042\u308a\u307e\u305b\u3093"}).first->first;
 unsigned list=0,count=0,player=0,component=0;
 if(!Read(0x92D884,count)||!count||!Read(0x92D87C,list)||!Read(list,player)||!player)return false;
 const auto hud=reinterpret_cast<unsigned(__thiscall*)(unsigned)>(0x6F8F10)(player);
 if(!Read(hud+4,component)||!component)return false;
 const auto generic=reinterpret_cast<unsigned(__thiscall*)(unsigned,unsigned)>(0x5D59F0)(component,0x5650B0);
 const auto text=event_titles::Lookup(nullptr,noBranch?noBranchKey:key);
 return generic&&text&&reinterpret_cast<bool(__thiscall*)(unsigned,const char*,bool,unsigned,unsigned,unsigned,unsigned)>(0x568030)
  (generic,text,false,0x8AB83EDB,0,0,2);
}

struct Field {unsigned first=0,last=0,lastLeg=0;};
Field Survivors(){
 Field field{};
 unsigned firstRank=~0u,lastRank=0;
 for(unsigned slot=1;slot<NativeSlots;++slot){
  const auto handle=roster[slot].handle;
  if(ledger.State(run,slot,handle)!=Status::Active)continue;
  const auto info=unsigned(run.race)+FirstRacer+slot*RacerStride;
  unsigned rank=0,leg=0,outcome=0;BYTE finished=0;
  if(!Read(info+0x10,rank)||!Read(info+0x2C,leg)||!Read(info+0x1C,outcome)||!Read(info+0x21,finished)||outcome||finished)continue;
  if(!rank||rank>NativeSlots)rank=slot+1;
  if(rank<firstRank){firstRank=rank;field.first=handle;}
  if(rank>=lastRank){lastRank=rank;field.last=handle;field.lastLeg=leg;}
 }
 return field;
}

bool Lagged(unsigned officer,const Field& field){
 Motion police{},last{};
 if(!field.last||!Sample(officer,police)||!Sample(field.last,last))return false;
 float policeStation=0,policeOffset=0,lastStation=0,lastOffset=0;
 return MWELCourseProjection(run.eventKey,&police.position.x,AnyLeg,policeStation,policeOffset)&&policeOffset<=300.f&&
  MWELCourseProjection(run.eventKey,&last.position.x,field.lastLeg,lastStation,lastOffset)&&lastOffset<=80.f&&
  lastStation-policeStation>=DistanceRequired;
}

bool ClearOfCourse(const Vector& position){
 float station=0,offset=0;
 // No course segment on this deck is also clear. The branch's own deck is
 // then verified with the road-floor and footprint probes before placement.
 return !MWELCourseProjection(run.eventKey,&position.x,AnyLeg,station,offset)||offset>=BranchClearance;
}

bool Unoccupied(unsigned car,const Vector& destination){
 std::vector<unsigned> cars;if(!gauntlet_runtime::List(cars))return false;
 for(const auto other:cars){
  Motion motion{};if(other==car||!reinterpret_cast<Vehicle*>(other)->IsActive()||!combat_health_runtime::NativeMotion(other-0x80,motion))continue;
  const auto gap=motion.position-destination;
  if(std::abs(gap.y)<5.f&&std::hypot(gap.x,gap.z)<18.f)return false;
 }
 return true;
}

Landing FindLanding(unsigned car,unsigned leader){
 Motion lead{};if(!leader||!Sample(leader,lead))return {};
 mwel_scene_road::Graph graph;
 if(!MWELCarbonRoadGraph(graph))return {};
 struct Candidate {Vector position,forward;float distance,score;};
 std::vector<Candidate> candidates;
 for(const auto& edge:graph.edges){
  const auto from=graph.nodes[edge.a],to=graph.nodes[edge.b];
  const Vector a{-from.y,from.z,from.x},b{-to.y,to.z,to.x};
  const auto delta=b-a;const float length=std::hypot(delta.x,delta.z);
  if(length<5.f)continue;
  const float projection=((lead.position.x-a.x)*delta.x+(lead.position.z-a.z)*delta.z)/(length*length);
  const float margin=std::min(.4f,6.f/length);
  const auto center=a+delta*std::clamp(projection,margin,1.f-margin);
  const auto gap=center-lead.position;const float distance=std::hypot(gap.x,gap.z);
  const float height=std::abs(gap.y);
  if(distance<25.f||distance>480.f||height>25.f)continue;
  candidates.push_back({center,{delta.x/length,0,delta.z/length},distance,distance+6.f*height});
 }
 std::sort(candidates.begin(),candidates.end(),[](const auto& a,const auto& b){return a.score<b.score;});
 unsigned checked=0;
 for(const auto& c:candidates){
  if(!ClearOfCourse(c.position))continue;
  if(checked++==32)break;
  mwel_grid::Layout layout;
  const float rotation=std::atan2(-c.forward.x,c.forward.z)*57.295779513f;
  if(!MWELFitGrid(run.eventKey,1,{c.position.x,c.position.y,c.position.z},rotation,layout))continue;
  const auto& p=layout.positions[0];const Vector destination{p[0],p[1],p[2]};
  if(!ClearOfCourse(destination)||!ClearOfCourse(destination+c.forward*8.f)||!ClearOfCourse(destination-c.forward*8.f))continue;
  if(!Unoccupied(car,destination))continue;
  Log("RPD_SPRINT_WARP_BRANCH leader=%u near_m=%.1f checked=%u xyz=%.1f,%.1f,%.1f",leader,c.distance,checked,destination.x,destination.y,destination.z);
  return {destination,c.forward,leader};
 }
 Log("RPD_SPRINT_WARP_BRANCH_MISSING candidates=%u checked=%u",unsigned(candidates.size()),checked);
 return {};
}

bool QueueRoad(const Vector& position){
 static const bool supported=[](){
  const BYTE request[]={0x55,0x8B,0xEC,0x83,0xE4,0xF0};
  const BYTE busy[]={0x8A,0x41,0x19,0x84,0xC0};
  return !memcmp(reinterpret_cast<const void*>(0x6F1170),request,sizeof(request))&&
         !memcmp(reinterpret_cast<const void*>(0x735620),busy,sizeof(busy));
 }();
 unsigned world=0;
 if(!supported||!Read(0x9885E0,world)||!world||reinterpret_cast<bool(__thiscall*)(unsigned)>(0x735620)(0x9B0FA0))return false;
 // WarpPlayerToTrigger requests the destination before grounding the car.
 // Let the normal frame pump service the request during the blackout.
 *reinterpret_cast<BYTE*>(0x9AAC81)=0;
 reinterpret_cast<void(__cdecl*)(const Vector*,bool)>(0x6F1170)(&position,false);
 return true;
}

bool RoadPending(){
 int pending=0,loaded=0,ready=0,required=0;
 return !Read(0x9AA9D0+0x188,pending)||!Read(0x9AA9D0+0x24,loaded)||
  !Read(0x9AA9D0+0x28,ready)||!Read(0x9AA9D0+0x2C0,required)||pending||loaded+ready<required;
}

bool StreamRoad(const Vector& position){
 if(!QueueRoad(position))return false;
 // The native teleport pumps I/O and the track streamer before the next
 // simulation tick can replace the requested position with the old camera.
 const auto deadline=GetTickCount64()+650;
 while(RoadPending()){
  if(GetTickCount64()>=deadline)return false;
  reinterpret_cast<void(__cdecl*)()>(0x6626B0)();
  reinterpret_cast<void(__thiscall*)(unsigned)>(0x730370)(0x9AA9D0);
  if(RoadPending())Sleep(1);
 }
 return true;
}

bool Place(unsigned officer,unsigned car){
 unsigned table=0,ground=0;
 if(!gauntlet_runtime::Table(car,0x8AA828)||!Read(car,table)||!Read(table+0x68,ground)||ground!=0x685810||
    !Sample(officer,origin)||!Unoccupied(car,landing.position))return false;
 const auto setGround=reinterpret_cast<bool(__thiscall*)(unsigned,const Vector*,const Vector*)>(ground);
 const bool grounded=setGround(car,&landing.position,&landing.forward);
 Motion after{};
 if(!Sample(officer,after)){QueueRoad(origin.position);setGround(car,&origin.position,&origin.forward);return false;}
 const auto error=after.position-landing.position;
 const float horizontal=std::hypot(error.x,error.z);
 if(horizontal<=4.f&&std::abs(error.y)<=2.f){
  placedAt=elapsed;
  Log("RPD_SPRINT_WARP_POSE player=%u leader=%u native_return=%u error_xz=%.2f error_y=%.2f",officer,landing.leader,unsigned(grounded),horizontal,error.y);
  return true;
 }
 QueueRoad(origin.position);
 setGround(car,&origin.position,&origin.forward);
 Log("RPD_SPRINT_WARP_GROUND_REJECTED native_ground=%u error_xz=%.2f error_y=%.2f",unsigned(grounded),horizontal,error.y);
 return false;
}

bool ConfirmLanding(unsigned officer,unsigned car){
 Motion sample{};
 if(!Sample(officer,sample))return false;
 const auto error=sample.position-landing.position;
 if(std::hypot(error.x,error.z)>5.f||std::abs(error.y)>2.f||!drift_race::Grounded(reinterpret_cast<BYTE*>(car)))return false;
 ClearContact();focus=0;
 Log("RPD_SPRINT_WARP_PLACED player=%u leader=%u off_course=1 wheel_contact=1 error_y=%.2f",officer,landing.leader,error.y);
 return true;
}

void Frame(){
 if(!Owns()||Result()||ledger.Result(owner).Complete()||!combat_health_runtime::Racing()){Reset();return;}
 if(!(run==owner)){Reset();run=owner;}
 const auto officer=roster[0].handle;
 const auto car=gauntlet_runtime::Vehicle(officer);
 if(!car||reinterpret_cast<Vehicle*>(car)->IsDestroyed()||combat_health_runtime::ledger.racers[0].retired){Reset();return;}
 if(!InstallInput())return;
 static bool rendererAvailable=false;
 if(!rendererAvailable){
  const auto arms=GetModuleHandleW(L"MWArmsAssist.asi");
  rendererAvailable=arms&&GetProcAddress(arms,"MWAA_RpdWarpPresentationV1");
  if(!rendererAvailable)return;
 }
 float now=0;if(!Read(0x9885D8,now)||!std::isfinite(now))return;
 const float dt=clock<0?0.f:std::clamp(now-clock,0.f,.15f);
 clock=now;
 if(reinterpret_cast<bool(__cdecl*)()>(0x468390)()){pressed=false;return;}
 if(warping){
  elapsed+=dt;
  fade=std::clamp(std::min(elapsed/BlackStart,(FadeSeconds-elapsed)/(FadeSeconds-BlackEnd)),0.f,1.f);
  if(checkingGround&&elapsed-placedAt>=.12f){
   placementSucceeded=ConfirmLanding(officer,car);
   if(placementSucceeded)checkingGround=false;
   else if(elapsed>=BlackEnd){
    checkingGround=false;
    Motion rejected{};Sample(officer,rejected);
    QueueRoad(origin.position);
    reinterpret_cast<bool(__thiscall*)(unsigned,const Vector*,const Vector*)>(0x685810)(car,&origin.position,&origin.forward);
    Log("RPD_SPRINT_WARP_SKIPPED reason=wheel_contact_missing delta_y=%.2f",rejected.position.y-landing.position.y);
   }
  }
  if(!placementAttempted&&elapsed>=BlackStart){
   placementAttempted=true;
   landing=FindLanding(car,Survivors().first);
   if(landing.leader){
    const auto begin=GetTickCount64();
    const bool streamed=StreamRoad(landing.position);
    Log("RPD_SPRINT_WARP_STREAM ready=%u ms=%llu",unsigned(streamed),GetTickCount64()-begin);
    checkingGround=streamed&&Place(officer,car);
    if(!checkingGround){Motion current{};if(Sample(officer,current))QueueRoad(current.position);}
   }
   if(!checkingGround)Log("RPD_SPRINT_WARP_SKIPPED safe_branch_unavailable=1");
  }
  if(elapsed>=FadeSeconds){
   warping=false;fade=0;nextCheck=GetTickCount64()+1000;
   if(!placementSucceeded)Notice(true);
   Log("RPD_SPRINT_WARP_COMPLETE seconds=%.3f placed=%u",elapsed,unsigned(placementSucceeded));
  }
  return;
 }
 if(pressed){
  pressed=false;
  if(eligible&&Lagged(officer,Survivors())){
   rpd_sprint_capture_camera::Reset();
   eligible=false;warping=true;elapsed=0;placementAttempted=placementSucceeded=checkingGround=false;landing={};
   Log("RPD_SPRINT_WARP_BEGIN fade_seconds=5 lag_threshold_m=5000 mapped_action=46");
   return;
  }
 }
 if(GetTickCount64()<nextCheck)return;
 nextCheck=GetTickCount64()+350;
 eligible=Lagged(officer,Survivors());
 if(!eligible)notified=false;
 else if(!notified&&Notice()){
  notified=true;
  Log("RPD_SPRINT_WARP_AVAILABLE threshold_m=5000 native_generic_message=1");
 }
}
}

#pragma comment(linker,"/EXPORT:MWEL_RpdWarpFadeV1=_MWEL_RpdWarpFadeV1")
extern "C" __declspec(dllexport) float __cdecl MWEL_RpdWarpFadeV1(){return rpd_sprint_warp::fade;}
