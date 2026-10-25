#include <windows.h>
#include <vector>
#include <map>
#include <set>
#include <array>
#include <memory>
#include <string>
#include <cstdio>
#include <cstdarg>
#include <cstring>
#include <cmath>
#include <algorithm>
#include <limits>
#include <stdexcept>
#include "NeoProviderBridge.h"
#include "NeoCourseDirection.h"
#include "NeoDriverBridge.h"
#include "EventBarriers.h"
#include "GeometryQueries.h"
#include "NeoAsyncWorker.h"
#include "NeoSnapshotGeometryApi.h"
#include "GridLaunchPolicy.h"
#include "GridSupport.h"
#include "../../World2MW/src/NeoAI/RouteGraph.h"
#include "../../World2MW/src/NeoAI/RespawnHistory.h"
#include "../../World2MW/src/NeoAI/RecoveryAnchor.h"
#include "../../World2MW/src/NeoAI/BlockedGuidance.h"
extern "C" unsigned __cdecl MWEL_LocalRaceTestV1();
bool MWELNeoDetailedLoggingEnabled();
namespace event_neo_provider {
using w2mw::Vec3;
using CollisionWorld=event_geometry::WorldQueries;
using RayHit=event_geometry::RayHit;
static CollisionWorld collision;
static constexpr bool editorStock=false;
static bool renderWorkProfiling=false;
static bool settingsLoaded=false;
static double Clock(){static const double scale=[](){LARGE_INTEGER f;QueryPerformanceFrequency(&f);return 1000.0/f.QuadPart;}();LARGE_INTEGER now;QueryPerformanceCounter(&now);return double(now.QuadPart)*scale;}
static float SimulationSeconds(){
 static const bool valid=[](){const BYTE expected[]={0xD9,0x05,0xD8,0x85,0x98,0,0xC3};return memcmp(reinterpret_cast<void*>(0x6E8DE0),expected,sizeof(expected))==0;}();
 return valid?reinterpret_cast<float(__cdecl*)()>(0x6E8DE0)():std::numeric_limits<float>::quiet_NaN();
}
static void Log(const char* format,...){if(!MWELNeoDetailedLoggingEnabled())return;char text[8192];va_list args;va_start(args,format);vsnprintf_s(text,sizeof(text),_TRUNCATE,format,args);va_end(args);MWELNeoLog(text);}
#define Log(...) do { if(MWELNeoDetailedLoggingEnabled()) (Log)(__VA_ARGS__); } while(false)
struct RenderQueryTimer {};
struct NeoPhaseTrace {double previous=0;std::array<double,8> ms{};void Mark(unsigned phase){if(previous){double now=Clock();ms[phase]+=now-previous;previous=now;}}};
struct NeoSettings {unsigned controller=2;bool enabled=true,testMode=false,autoStart=false,telemetry=false,asyncPlanning=true;mwneo::Profile profile{};};
static NeoSettings neoSettings;
static void LoadSettings(){
 if(settingsLoaded)return;settingsLoaded=true;char file[MAX_PATH]{};if(!MWELNeoSettingsFile(file,sizeof(file)))throw std::runtime_error("Event settings path unavailable");
 neoSettings.enabled=GetPrivateProfileIntA("NeoAI","Enabled",1,file)!=0;
 neoSettings.asyncPlanning=GetPrivateProfileIntA("NeoAI","AsyncPlanning",1,file)!=0;
 neoSettings.telemetry=MWELNeoDetailedLoggingEnabled()&&GetPrivateProfileIntA("NeoAI","DebugTelemetry",0,file)!=0;
 neoSettings.testMode=MWEL_LocalRaceTestV1()!=0;
 renderWorkProfiling=MWELNeoDetailedLoggingEnabled()&&GetPrivateProfileIntA("Debug","PerformanceProfiling",0,file)==1;
 // Retired MaxTargetSpeedKmh must not silently reintroduce a cap through an
 // existing installation's INI. Physics and the checked road set race pace.
 neoSettings.profile.maxSpeed=0;
 neoSettings.profile.lateralAccel=std::clamp(float(GetPrivateProfileIntA("NeoAI","MaxLateralAcceleration",50,file)),12.f,70.f);
 Log("EVENT_NEOAI_PROVIDER_CONFIG max_kmh=unlimited lateral_accel=%.1f provider_owner=NFSMWEventLoader world_state_owned=0",neoSettings.profile.lateralAccel);
}
static bool EventGeometryAvailable(){LoadSettings();return MWELNeoCurrentKey()!=0&&!collision.nodes.empty();}
static unsigned EditorRouteCourseHash(){return MWELNeoCurrentKey();}
static bool CourseFile(unsigned key,const char* suffix,char* output,unsigned size){return MWELNeoCourseFile(key,suffix,output,size);}
#include "NeoCourseReaders.inl"
#include "../../World2MW/src/NativeLoader/YellowRoadAim.h"
static std::vector<std::pair<Vec3,Vec3>> ownedWalls;
static thread_local std::vector<std::pair<Vec3,Vec3>> nearbyWalls;
static unsigned wallsKey=0;
static bool LoadWalls(unsigned key){
 if(wallsKey==key)return true;
 std::vector<MWELBarrierSegment> contacts;
 if(!MWELBarrierSegments(key,contacts)){Log("EVENT_NEOAI_WALLS_FAILED key=%08X",key);return false;}
 std::vector<std::pair<Vec3,Vec3>> staged;staged.reserve(contacts.size());
 for(const auto& wall:contacts)staged.push_back({{wall.ax,wall.ay,wall.az},{wall.bx,wall.by,wall.bz}});
 ownedWalls=std::move(staged);wallsKey=key;Log("EVENT_NEOAI_WALLS key=%08X count=%u source=owned_package world_event_state_shared=0",key,unsigned(ownedWalls.size()));return true;
}
static const std::vector<std::pair<Vec3,Vec3>>& YellowBarrierSnapshot(){return nearbyWalls;}
static void BeginYellowBarrierQuery(Vec3 center,unsigned key=0){
 nearbyWalls.clear();if(!LoadWalls(key?key:EditorRouteCourseHash()))throw std::runtime_error("Owned event wall geometry unavailable");
 for(const auto& wall:ownedWalls){auto a=wall.first,b=wall.second;if(center.y+.75f<a.y||center.y-1.25f>b.y||center.x<std::min(a.x,b.x)-180||center.x>std::max(a.x,b.x)+180||center.z<std::min(a.z,b.z)-180||center.z>std::max(a.z,b.z)+180)continue;nearbyWalls.push_back(wall);}
}
#include "../../World2MW/src/NativeLoader/YellowBarrierCorridor.h"
static bool YellowEditorWallCorridor(Vec3 a,Vec3 b,float width,bool contact=false,float padding=.2f,const Vec3* body=nullptr){return YellowBarrierSegmentsClear(nearbyWalls,a,b,width,contact,padding,body);}
#include "NeoProviderOwned.inl"
#ifdef MWEL_NEO_OBSERVE_ONLY
#include "NeoLandingTrace.inl"
#endif
}
#ifdef MWEL_NEO_OBSERVE_ONLY
void MWELNeoObservePhysicsFrame(void* vehicle,const MWELNeoFrameState& state,float steering,float throttle,float brake){
 if(MWELNeoDetailedLoggingEnabled())event_neo_provider::NeoObserveLandingFrame(vehicle,state,steering,throttle,brake);
}
int MWELNeoDiagnosticWheelCount(void* vehicle,const MWELNeoFrameState& state){
 if(!MWELNeoDetailedLoggingEnabled())return -1;using namespace event_neo_provider;
 if(!settingsLoaded||!neoSettings.testMode||!neoSettings.telemetry)return -1;
 for(const auto& f:neoFlightStates)if(f.vehicle==vehicle&&f.handle==state.handle&&f.race==*reinterpret_cast<void**>(0x91E000)){
  NeoLandingFrame sample;if(NeoReadLandingFrame(vehicle,f.body,state,sample))return sample.reset.wheels;
 }return -1;
}
#endif
void MWELResetNeoProviderRace(){using namespace event_neo_provider;++planRaceGeneration;if(planWorker)planWorker->Invalidate();for(auto& state:neoProviders)state={};for(auto& flight:neoFlightStates){flight.positionWatch.Invalidate();flight={};}neoRejectedBranches.clear();}
bool MWELPrepareNeoCourse(unsigned key){return key&&event_neo_provider::NeoLoadCourse(key);}
bool MWELFitGrid(unsigned key,unsigned count,const mwel_grid::Position& origin,float rotation,mwel_grid::Layout& layout){
 using namespace event_neo_provider;layout={};if(!key||collision.nodes.empty()){Log("EVENT_EXTENDED_GRID_REFUSED key=%08X requested=%u reason=world_geometry_unavailable flow=%u",key,count,*reinterpret_cast<unsigned*>(0x925E90));return false;}
 // The registered package key, not the previous GRaceStatus, owns this
 // preflight. It runs before the event is selectable or spawns any vehicle.
 if(!LoadWalls(key)){Log("EVENT_EXTENDED_GRID_REFUSED key=%08X requested=%u reason=owned_walls_unavailable",key,count);return false;}
 // Runs once per start/retry, before the native grid applies placement.
 // No collision-world writes or vehicle spawning from the render observer.
 auto clear=[&](const mwel_grid::Position& p,float dx,float dz){
  BeginYellowBarrierQuery({p[0],p[1],p[2]},key);
  return YellowEditorWallCorridor({p[0]-dx*2.8f,p[1],p[2]-dz*2.8f},{p[0]+dx*2.8f,p[1],p[2]+dz*2.8f},1.2f);
 };
 layout=mwel_grid::Supported(count,origin,rotation,collision,clear);
 Log("EVENT_EXTENDED_GRID_PLAN key=%08X requested=%u planned=%u columns=%u complete=%u native_spawn_unchanged=1",key,count,layout.count,layout.columns,unsigned(layout.valid));return layout.valid;
}
bool MWELNeoPendingGateEnd(unsigned handle,unsigned edge){
 for(const auto& state:event_neo_provider::neoProviders)if(state.key==handle&&state.leg==int(edge)){
  auto road=state.roads.find(int(edge));
  return road!=state.roads.end()&&road->second.points.size()>1&&state.station>=road->second.points.back().station-4;
 }return false;
}
bool MWELNeoCourseFacing(unsigned handle,const float* position,const float* heading,bool nativeFacing){
 using namespace event_neo_provider;
 if(!position||!heading||!MWELNeoCourseEnabled()||neoCourse.hash!=MWELNeoCurrentKey())return nativeFacing;
 auto race=*reinterpret_cast<BYTE**>(0x91E000);if(!race||*reinterpret_cast<unsigned*>(race+0x1960)!=1||!race[0x198A]||!race[0x1978])return nativeFacing;
 unsigned count=*reinterpret_cast<unsigned*>(race+0x1958);if(count>16)return nativeFacing;
 for(unsigned slot=0;slot<count;++slot){auto record=race+0x18+slot*0x194;
  if(*reinterpret_cast<unsigned*>(record)!=handle||record[0x21]||*reinterpret_cast<unsigned*>(record+0x1C))continue;
  int leg=*reinterpret_cast<int*>(record+0x2C);if(leg<0||unsigned(leg)>=yellowRecoveryLegs.size())return nativeFacing;
  mwneo::Point p{position[0],position[1],position[2]},h{heading[0],heading[1],heading[2]};NeoCourseDirection facing;
  auto consider=[&](const auto& points){for(unsigned j=1;j<points.size();++j){auto a=points[j-1],b=points[j];facing.Consider(p,h,{a[0],a[1],a[2]},{b[0],b[1],b[2]});}};
  const auto& road=yellowRecoveryLegs[leg];for(unsigned j=1;j<road.size();++j)facing.Consider(p,h,{road[j-1].p[0],road[j-1].p[1],road[j-1].p[2]},{road[j].p[0],road[j].p[1],road[j].p[2]});
  if(unsigned(leg)<yellowLocalBranchLegs.size())for(const auto& branch:yellowLocalBranchLegs[leg])consider(branch.points);
  return facing.Resolve(nativeFacing);
 }return nativeFacing;
}
bool MWELNeoRecoveryResumed(unsigned handle,unsigned edge){
 for(const auto& state:event_neo_provider::neoProviders)if(state.key==handle&&state.leg==int(edge)&&state.observedAt&&GetTickCount64()>=state.observedAt&&GetTickCount64()-state.observedAt<=200)
  return event_neo_provider::mwel_recovery_observation::Resumed(GetTickCount64(),state.verifiedPlacementAt,state.verifiedPlacementPosition,state.verifiedPlacementHeading,state.observedPosition,state.lastGuidanceStatus);
 return false;
}
bool MWELNeoRouteProgress(unsigned handle,unsigned edge,float* station,unsigned* revision){
 if(!station)return false;
 for(const auto& state:event_neo_provider::neoProviders)if(state.key==handle&&state.leg==int(edge)&&std::isfinite(state.station)){
  *station=state.station;if(revision)*revision=state.routeRevision;return true;
 }return false;
}
#pragma comment(linker,"/EXPORT:MWEL_NeoProfileV1=_MWEL_NeoProfileV1")
extern "C" __declspec(dllexport) unsigned __cdecl MWEL_NeoProfileV1(mwneo::Profile* out,unsigned size){
 if(!out||size!=sizeof(*out))return 0;event_neo_provider::LoadSettings();*out=event_neo_provider::neoSettings.profile;return 1;
}
#pragma comment(linker,"/EXPORT:MWEL_NeoCourseOwnedV1=_MWEL_NeoCourseOwnedV1")
extern "C" __declspec(dllexport) unsigned __cdecl MWEL_NeoCourseOwnedV1(){return MWELNeoCourseEnabled();}
#pragma comment(linker,"/EXPORT:MWEL_NeoControllerV1=_MWEL_NeoControllerV1")
extern "C" __declspec(dllexport) unsigned __cdecl MWEL_NeoControllerV1(){return MWELNeoCourseEnabled()?2u:0u;}

// Separate immutable backup road cache: never changes NeoAI's accepted graph,
// branch decisions or preparation result. Loaded once before native activation.
namespace event_gauntlet_road {
struct Point {float p[3]{};};
unsigned hash=0;std::vector<std::vector<Point>> legs;
}
bool MWELPrepareGauntletRoad(unsigned key){
 using namespace event_gauntlet_road;if(hash==key)return !legs.empty();
 hash=0;legs.clear();char path[MAX_PATH]{};if(!key||!MWELNeoCourseFile(key,"ai-recovery.bin",path,sizeof(path)))return false;
 FILE* file=nullptr;fopen_s(&file,path,"rb");if(!file)return false;
 unsigned h[4]{};bool ok=fread(h,4,4,file)==4&&h[0]==0x43523257&&h[1]==1&&h[2]==key&&h[3]>0&&h[3]<=257;
 std::vector<std::vector<Point>> candidate;unsigned total=0;
 if(ok)for(unsigned leg=0;leg<h[3];++leg){unsigned count=0;
  if(fread(&count,4,1,file)!=1||count<2||count>8192||(total+=count)>262144){ok=false;break;}
  std::vector<Point> points(count);if(fread(points.data(),sizeof(Point),count,file)!=count){ok=false;break;}
  for(const auto& point:points)for(float v:point.p)if(!std::isfinite(v)||std::abs(v)>100000)ok=false;
  if(!ok)break;candidate.push_back(std::move(points));
 }
 if(ok&&fgetc(file)!=EOF)ok=false;fclose(file);
 if(!ok)return false;hash=key;legs=std::move(candidate);return true;
}
// Backup placement uses immutable, already prewarmed course geometry. No I/O,
// navigator mutation or native-world streaming request from this query.
bool MWELCourseAhead(unsigned key,const float* position,unsigned leg,float metres,float* point,float* forward){
 using namespace event_gauntlet_road;
 if(!position||!point||!forward||hash!=key||leg>=legs.size()||!std::isfinite(metres)||metres<=0||metres>500)return false;
 const auto& road=legs[leg];if(road.size()<2)return false;
 float best=6400,uBest=0;unsigned segment=0;bool found=false;
 for(unsigned i=1;i<road.size();++i){const auto& a=road[i-1];const auto& b=road[i];
  float dx=b.p[0]-a.p[0],dy=b.p[1]-a.p[1],dz=b.p[2]-a.p[2],n=dx*dx+dy*dy+dz*dz;if(n<.001f)continue;
  float u=std::clamp(((position[0]-a.p[0])*dx+(position[1]-a.p[1])*dy+(position[2]-a.p[2])*dz)/n,0.f,1.f);
  float x=a.p[0]+u*dx-position[0],y=a.p[1]+u*dy-position[1],z=a.p[2]+u*dz-position[2],score=x*x+z*z+4*y*y;
  if(std::abs(y)<6&&score<best){best=score;uBest=u;segment=i;found=true;}
 }
 if(!found)return false;
 for(unsigned l=leg;l<legs.size();++l){const auto& path=legs[l];
  for(unsigned i=l==leg?segment:1;i<path.size();++i){const auto& a=path[i-1];const auto& b=path[i];
   float d[3]={b.p[0]-a.p[0],b.p[1]-a.p[1],b.p[2]-a.p[2]};float length=std::sqrt(d[0]*d[0]+d[1]*d[1]+d[2]*d[2]);
   if(length<.001f)continue;float start=l==leg&&i==segment?uBest:0,available=length*(1-start);
   if(metres<=available){float u=start+metres/length;for(unsigned j=0;j<3;++j){point[j]=a.p[j]+u*d[j];forward[j]=d[j]/length;}return true;}
   metres-=available;
  }
 }
 return false; // near the goal, prefer a checked rear placement
}
