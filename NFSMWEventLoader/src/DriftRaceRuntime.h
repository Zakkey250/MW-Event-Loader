#pragma once
#include "DriftScorePolicy.h"
#include "DriftProximityPolicy.h"
#include "DriftForecastPolicy.h"
#include "DriftContactPolicy.h"
#include "DriftDrivePolicy.h"
#include "../../MW-Arcade-Drift-public/src/DriftModeAPI.h"
bool MWELArmsPrepareRanking();
bool MWELEnsureDriftImpact();
namespace drift_race {
using expanded_leaderboard::Read;
std::array<mwel_drift::Score,16> scores{};
std::array<unsigned,16> handles{};
std::array<float,16> progress{};
std::array<bool,16> driveRead{};
unsigned owner=0,key=0,count=0,genuineMask=0;double lastTime=-1,lastReport=-1;
double fieldSimulation=-1;
unsigned wallContactMask=0;
unsigned forecastMask=0;bool finalScores=false;
std::array<mwel_drift_forecast::Performance,16> performance{};
std::array<unsigned,16> actualAtSettlement{},predictedAdditional{};
std::uint64_t playerToken=0;constexpr std::uint64_t LeaseOwner=0x4D57454C44525343ull;
bool faulted=false;
struct Proximity {double time=-1;float x=0,z=0,wall=10000,car=10000;};
std::array<Proximity,16> proximity{};
std::vector<std::pair<unsigned,mwel_drift_proximity::Hull>> traffic;
double trafficTime=-1;
bool Active(){auto p=event_race_policies::Current();return p&&p->authoring.value("combatMode",std::string("none"))=="drift";}
bool Current(){return Active()&&owner&&owner==Read<unsigned>(reinterpret_cast<void*>(0x91E000))&&key==MWELNeoCurrentKey();}
bool ResultPending(){return Current()&&(genuineMask&1u)!=0;}
void ReleasePlayer(){
 if(!playerToken)return;auto module=GetModuleHandleW(L"MWArcadeDrift.asi");
 auto release=module?reinterpret_cast<MWArcadeDriftReleaseSpecialV1Fn>(GetProcAddress(module,"MWArcadeDriftReleaseSpecialV1")):nullptr;
 if(release)release(LeaseOwner,playerToken);playerToken=0;
}
void Reset(){ReleasePlayer();scores={};handles={};progress={};driveRead={};proximity={};performance={};actualAtSettlement={};predictedAdditional={};forecastMask=0;finalScores=false;traffic.clear();trafficTime=-1;owner=key=count=genuineMask=wallContactMask=0;lastTime=lastReport=-1;faulted=false;}
void WallContact(unsigned handle,float impulse,float mass,float normalY,bool breakable,bool vehicle){
 if(!Current()||finalScores||!mwel_drift_contact::Wall(impulse,mass,normalY,breakable,vehicle))return;
 for(unsigned i=0;i<count;++i)if(handles[i]==handle&&!scores[i].finished){wallContactMask|=1u<<i;return;}
}
void Exit(){
 ReleasePlayer();
 // Native EndRace/Stop seals every score. This does not synthesize a goal
 // or change a finish/DNF flag on a trailing racer.
 if(Current()){for(unsigned i=0;i<count;++i)scores[i].finish(std::max(0.,lastTime));}
} // Keep immutable result values until staging.
void Player(){
 // The isolated lab explicitly gives the human body to the AI controller.
 // Never run the frozen player controller on that same body as well.
 if((race_start_experiment::enabled&&!race_start_experiment::manualPlayer&&!race_start_experiment::playerEngineTest)||runtime_lifecycle::careerAutodrive)return;
 auto module=GetModuleHandleW(L"MWArcadeDrift.asi");if(!module)return;
 auto renew=reinterpret_cast<MWArcadeDriftRenewSpecialV1Fn>(GetProcAddress(module,"MWArcadeDriftRenewSpecialV1"));
 auto acquire=reinterpret_cast<MWArcadeDriftAcquireSpecialV1Fn>(GetProcAddress(module,"MWArcadeDriftAcquireSpecialV1"));
 if(playerToken&&(!renew||!renew(LeaseOwner,playerToken,1500)))playerToken=0;
 if(!playerToken&&acquire)acquire(LeaseOwner,1500,&playerToken);
}
BYTE* Vehicle(unsigned handle){
 auto list=Read<BYTE**>(reinterpret_cast<void*>(0x92CD1C));auto n=Read<unsigned>(reinterpret_cast<void*>(0x92CD24));if(!list||n>128)return nullptr;
 for(unsigned i=0;i<n;++i){auto v=Read<BYTE*>(list,i*4);if(Read<unsigned>(v)==0x8AA828&&Read<unsigned>(v-0x78)==handle)return v;}
 return nullptr;
}
bool Grounded(BYTE* vehicle){
 // Both native Simple and Racer suspension implement contact slot 9.
 auto suspension=Read<void*>(vehicle,0x44);auto vt=Read<unsigned*>(suspension);
 if(!vt||(unsigned(vt)!=0x8ABAC8&&unsigned(vt)!=0x8ABC30))return false;
 auto contact=Read<unsigned>(vt,9*4);if(contact<0x401000||contact>=0x889000)return false;
 unsigned grounded=0;for(unsigned i=0;i<4;++i)grounded+=reinterpret_cast<bool(__thiscall*)(void*,unsigned)>(contact)(suspension,i)?1:0;
 return grounded>=3;
}
void Drive(unsigned slot,BYTE* vehicle){
 if(driveRead[slot])return;
 // Same verified mTransInfo field used by Arcade Drift. EN1.3 constructors
 // 006B1C14 and 006B2694 both place ISuspension at +4c and mTransInfo at +d4.
 // Decline a foreign suspension; never infer its data layout.
 const BYTE getter[]={0x8B,0x81,0xDC,0,0,0,0xD9,0x40,0x70};
 if(memcmp(reinterpret_cast<void*>(0x68DE00),getter,sizeof(getter)))return;
 auto suspension=Read<BYTE*>(vehicle,0x44);const auto vt=Read<unsigned>(suspension);
 const BYTE simple[]={0xC7,0x46,0x4C,0x30,0xBC,0x8A,0};
 if(vt!=0x8ABAC8&&(vt!=0x8ABC30||memcmp(reinterpret_cast<void*>(0x6B2694),simple,sizeof(simple))))return;
 auto trans=Read<BYTE*>(suspension,0x90);float split=-1;
 if(!trans||!Copy(trans+0x70,&split,4)||!std::isfinite(split)||split<0||split>1)return;
 // The native single-player race record at slot zero owns the player score,
 // genuine-goal mask and finish settlement throughout this runtime.
 driveRead[slot]=true;scores[slot].awardScale=mwel_drift_drive::AwardScale(split,slot==0);
 Log("DRIFT_DRIVE_BALANCE slot=%u front_share=%.4f award_scale=%.4f shared_transmission_writes=0",slot,split,scores[slot].awardScale);
}
mwel_drift_proximity::Hull Hull(const NeoBodyMotion& b){return {b.position[0],b.position[1],b.position[2],b.heading[0],b.heading[2],b.halfWidth,b.halfLength};}
void Bonus(unsigned slot,const NeoBodyMotion& body,double now,bool& wall,bool& car){
 wall=car=false;auto& p=proximity[slot];
 if(p.time<0||now-p.time>=.1||std::hypot(body.position[0]-p.x,body.position[2]-p.z)>3){
  p.time=now;p.x=body.position[0];p.z=body.position[2];p.wall=p.car=10000;
  MWELDriftWallClearance(body.position,body.heading,body.halfWidth,body.halfLength,&p.wall);
  // Capture the native roster once per 100ms, not once per racer. Retain
  // geometry/identities only; never keep borrowed vehicle pointers.
  if(trafficTime<0||now-trafficTime>=.1){
   trafficTime=now;traffic.clear();auto list=Read<BYTE**>(reinterpret_cast<void*>(0x92CD1C));auto n=Read<unsigned>(reinterpret_cast<void*>(0x92CD24));
   if(list&&n<=128)for(unsigned j=0;j<n;++j){auto v=Read<BYTE*>(list,j*4);NeoBodyMotion other{};
    if(Read<unsigned>(v)!=0x8AA828||!NeoReadBody(v,other))continue;
    auto hull=Hull(other);if(mwel_drift_proximity::Valid(hull))traffic.push_back({Read<unsigned>(v-0x78),hull});
   }
  }
  const auto self=Hull(body);for(const auto& other:traffic)if(other.first!=handles[slot])p.car=std::min(p.car,mwel_drift_proximity::Clearance(self,other.second));
 }
 wall=mwel_drift_proximity::NearWall(p.wall);car=mwel_drift_proximity::NearCar(p.car);
}
void Ranking(unsigned race){
 if(!Current()||race!=owner||faulted||count<1)return;
 for(unsigned i=0;i<count;++i)if(Read<unsigned>(reinterpret_cast<void*>(race+0x18+i*0x194))!=handles[i])return;
 unsigned retired=0;for(unsigned i=0;i<count;++i){auto info=reinterpret_cast<void*>(race+0x18+i*0x194);if(Read<unsigned>(info,0x1C)||Read<BYTE>(info,0x190))retired|=1u<<i;}
 auto order=mwel_drift::Order(scores,progress,count,retired);
 for(unsigned rank=0;rank<count;++rank){auto info=reinterpret_cast<BYTE*>(race+0x18+order[rank]*0x194);
  *reinterpret_cast<unsigned*>(info+0x10)=rank+1;
  *reinterpret_cast<float*>(info+0x120)=float(scores[order[rank]].standing());
 }
}
double Clock(BYTE* race){
 double now=Read<float>(race,0x18+0x150);
 if(Read<BYTE>(race,0x18+0x154))now+=reinterpret_cast<float(__cdecl*)()>(0x6E8DE0)()-Read<float>(race,0x18+0x14C);
 // The marker-gated full-field lab freezes the human's native finish clock
 // while the remaining real bodies continue. Score those bodies against
 // continuous simulation time rather than forecasting or freezing awards.
 const bool fieldLab=race_start_experiment::enabled&&race_start_experiment::neoObserver&&race_start_experiment::waitForField;
 if(fieldLab){
  const double simulation=reinterpret_cast<float(__cdecl*)()>(0x6E8DE0)();
  if(owner==unsigned(race)&&lastTime>=0&&fieldSimulation>=0&&std::isfinite(simulation)&&simulation>=fieldSimulation)
   now=lastTime+simulation-fieldSimulation;
  fieldSimulation=std::isfinite(simulation)?simulation:-1;
 }
 return now;
}
void Frame(){
 if(!Active()){ReleasePlayer();return;}if(faulted)return;
 auto race=Read<BYTE*>(reinterpret_cast<void*>(0x91E000));
 if(!race||Read<unsigned>(reinterpret_cast<void*>(0x925E90))!=6||Read<unsigned>(race,0x1960)!=1)return;
 // Acquire before GO; release at every native end/roaming/unload boundary.
 if(!Read<BYTE>(race,0x18+0x21))Player();
 if(!Read<BYTE>(race,0x198A)||!Read<BYTE>(race,0x1978))return;
 if(!MWELEnsureDriftImpact()){faulted=true;ReleasePlayer();Log("DRIFT_RACE_REFUSED native_contact_hook_unavailable=1");return;}
 const auto now=Clock(race);if(!std::isfinite(now)||now<0)return;
 if(owner!=unsigned(race)||key!=MWELNeoCurrentKey()||now+.25<lastTime){
  Reset();owner=unsigned(race);key=MWELNeoCurrentKey();count=Read<unsigned>(race,0x1958);
  if(!count||count>16||!MWELArmsPrepareRanking()){faulted=true;Log("DRIFT_RACE_REFUSED ranking_or_count=1");return;}
  for(unsigned i=0;i<count;++i)handles[i]=Read<unsigned>(race,0x18+i*0x194);
  Log("DRIFT_RACE_BEGIN key=%08X count=%u score_owner=EventLoader native_finish_required=1",key,count);
 }
 if(now<=lastTime)return;lastTime=now;
 for(unsigned i=0;i<count;++i){
  auto info=race+0x18+i*0x194;
  if(Read<unsigned>(info)!=handles[i]){faulted=true;ReleasePlayer();Log("DRIFT_RACE_REFUSED slot_identity_changed=%u",i);return;}
  if(scores[i].finished||Read<BYTE>(info,0x190)||Read<BYTE>(info,0x1D))continue;
  auto vehicle=Vehicle(handles[i]);NeoBodyMotion body{};if(!vehicle||!NeoReadBody(vehicle,body))continue;
  Drive(i,vehicle);
  float speed=std::hypot(body.velocity[0],body.velocity[2]);
  float angle=std::atan2(body.heading[0]*body.velocity[2]-body.heading[2]*body.velocity[0],body.heading[0]*body.velocity[0]+body.heading[2]*body.velocity[2])*57.295779513f;
  float value=Read<float>(info,0x18);if(!std::isfinite(value))continue;
  progress[i]=value+100.f*Read<unsigned>(info,0x28);
  auto prior=scores[i].notice;
  const bool ground=Grounded(vehicle);bool wall=false,car=false;
  if(ground&&speed>5&&std::abs(angle)>=9&&std::abs(angle)<=80)Bonus(i,body,now,wall,car);
  const auto priorBonus=scores[i].bonusFlags;
  const auto previousTime=scores[i].lastTime;
  const auto previousRearm=scores[i].progressRearm;
  const bool contact=(wallContactMask&(1u<<i))!=0;wallContactMask&=~(1u<<i);
  if(contact)Log("DRIFT_WALL_CONTACT slot=%u pending=%u banked=%u native_contact=1",i,scores[i].chain(),scores[i].total);
  const bool sliding=scores[i].update({now,body.position[0],body.position[2],speed,angle,progress[i],ground,contact,wall,car});
  if(previousRearm&&!scores[i].progressRearm)Log("DRIFT_SCORE_REARMED slot=%u progress=%.6f banked=%u measured_travel=%.3f forward_course_progress=1",i,progress[i],scores[i].total,scores[i].rearmTravel);
  performance[i].Observe(float(now-previousTime),speed,angle,sliding);
  if(scores[i].bonusFlags!=priorBonus)Log("DRIFT_PROXIMITY slot=%u flags=%u wall_m=%.3f car_m=%.3f",i,scores[i].bonusFlags,proximity[i].wall,proximity[i].car);
  if(scores[i].notice!=prior&&(scores[i].notice==mwel_drift::Notice::Lost||scores[i].notice==mwel_drift::Notice::Banked))
   Log("DRIFT_CHAIN slot=%u outcome=%u total=%u best=%u",i,unsigned(scores[i].notice),scores[i].total,scores[i].bestChain);
 }
 Ranking(unsigned(race));
 if(worldLogEnabled&&now-lastReport>=1){lastReport=now;for(unsigned i=0;i<count;++i){
  Log("DRIFT_SCORE slot=%u total=%u chain=%u multiplier=%u rank=%u finished=%u",i,scores[i].total,scores[i].chain(),scores[i].multiplier(),Read<unsigned>(race,0x18+i*0x194+0x10),unsigned(scores[i].finished));
  auto info=race+0x18+i*0x194;auto vehicle=Vehicle(handles[i]);NeoBodyMotion body{};
  const bool measured=vehicle&&NeoReadBody(vehicle,body);
  const float speed=measured?std::hypot(body.velocity[0],body.velocity[2]):0;
  const float angle=measured?std::atan2(body.heading[0]*body.velocity[2]-body.heading[2]*body.velocity[0],body.heading[0]*body.velocity[0]+body.heading[2]*body.velocity[2])*57.295779513f:0;
  Log("DRIFT_SAMPLE slot=%u time=%.3f sample_age=%.3f progress=%.6f high_water=%.6f progress_age=%.3f speed=%.3f angle=%.3f grounded=%u measured=%u retired=%u native_done=%u rearm=%u drift_seconds=%.3f",i,now,now-scores[i].lastTime,progress[i],scores[i].bestProgress,now-scores[i].progressTime,speed,angle,unsigned(measured&&Grounded(vehicle)),unsigned(measured),unsigned(Read<BYTE>(info,0x190)),unsigned(Read<BYTE>(info,0x1D)),unsigned(scores[i].progressRearm),scores[i].chainSeconds);
 }}
}
void SettleRemaining(){
 if(finalScores||!Current())return;finalScores=true;
 const auto started=GetTickCount64();const auto package=event_race_policies::Current();
 const auto laps=package?mwel::RaceLaps(package->authoring):1u;
 for(unsigned i=0;i<count;++i){
  actualAtSettlement[i]=scores[i].standing();if(scores[i].finished)continue;
  auto info=reinterpret_cast<BYTE*>(owner+0x18+i*0x194);
  if(Read<unsigned>(info,0x1C)||Read<BYTE>(info,0x190)){scores[i].finish(std::max(0.,lastTime));continue;}
  NeoBodyMotion before{},after{};auto vehicle=Vehicle(handles[i]);const bool measured=vehicle&&NeoReadBody(vehicle,before);
  mwel_drift_forecast::Route route;mwel_drift_forecast::Result prediction;
  if(MWELNeoDriftForecastRoute(handles[i],laps,route))prediction=mwel_drift_forecast::Predict(route,scores[i],performance[i]);
  if(!prediction.geometry||!prediction.complete){
   // Safe bounded history estimate when a route was never bound. Keep this
   // separate from geometry prediction in diagnostics; never move/spawn a car.
   const float fraction=std::clamp(Read<float>(info,0x18)*.01f,.05f,1.f);
   prediction.actual=actualAtSettlement[i];prediction.total=unsigned(std::min(double(mwel_drift::Maximum),std::round(prediction.actual/double(fraction))));
   prediction.added=prediction.total-prediction.actual;
  }
  scores[i].finish(std::max(0.,lastTime));scores[i].total=std::max(actualAtSettlement[i],prediction.total);
  predictedAdditional[i]=scores[i].total-actualAtSettlement[i];forecastMask|=1u<<i;
  float moved=0;if(measured&&NeoReadBody(vehicle,after))moved=std::hypot(before.position[0]-after.position[0],before.position[2]-after.position[2]);
  Log("DRIFT_FORECAST slot=%u actual=%u predicted_additional=%u final=%u remaining_m=%.3f efficiency=%.4f geometry=%u complete=%u steps=%u body_moved=%.6f native_physics_calls=0 genuine_goal_written=0",i,actualAtSettlement[i],predictedAdditional[i],scores[i].total,prediction.remaining,prediction.efficiency,unsigned(prediction.geometry),unsigned(prediction.complete),prediction.steps,moved);
 }
 Ranking(owner);Log("DRIFT_FINAL_SCORES genuine_mask=%X forecast_mask=%X elapsed_ms=%llu",genuineMask,forecastMask,GetTickCount64()-started);
}
void Finish(unsigned slot,bool genuine){
 if(!genuine||!Current()||slot>=count||scores[slot].finished)return;
 scores[slot].finish(std::max(0.,lastTime));Ranking(owner);
 genuineMask|=1u<<slot;
 if(slot==0&&!(race_start_experiment::enabled&&race_start_experiment::neoObserver&&race_start_experiment::waitForField))SettleRemaining();
 Log("DRIFT_GENUINE_FINISH slot=%u score=%u best_chain=%u rank=%u",slot,scores[slot].total,scores[slot].bestChain,Read<unsigned>(reinterpret_cast<void*>(owner+0x18+slot*0x194),0x10));
 if(slot==0)ReleasePlayer();
}
}
void MWELDriftFinish(unsigned slot,bool genuine){drift_race::Finish(slot,genuine);}
