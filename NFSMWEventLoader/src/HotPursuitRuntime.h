#pragma once
#include "HotPursuitPolicy.h"
// Experimental authored Hot Pursuit controller. Explicit event mode and the
// normal ArmsAssist/Arsenal dependency checks scope this path in every build.
namespace hot_pursuit_runtime {
bool draining=false;
unsigned owner=0;ULONGLONG next=0,nextRequest=0,nextLog=0;bool requested=false,roadReady=false;
std::string model;

std::array<unsigned,16> eligibleHandles{};
bool playerPursuitStarted=false;
hot_pursuit::ReplacementWindow replacements;
struct Perp {unsigned handle=0,component=0;float original=0,written=0;};
std::array<Perp,16> perps{};
struct Intercept {unsigned target=0;ULONGLONG until=0;combat_impact::Vector position{};std::vector<unsigned> before;ULONGLONG detectAfter=0;combat_impact::Vector forward{};unsigned rollingCop=0;};
Intercept intercept;
ULONGLONG nextRetainLog=0;
struct Detection {unsigned cop=0,target=0;ULONGLONG until=0;};
std::array<Detection,32> detections{};
struct Lost {unsigned cop=0,target=0;ULONGLONG since=0;};
std::array<Lost,32> lost{};


bool Current(){auto p=event_race_policies::Current();return p&&p->authoring.value("combatMode",std::string("none"))=="hotpursuit";}
bool Active(){unsigned race=0,play=0,count=0;return !draining&&Current()&&combat_health_runtime::Read(0x91E000,race)&&
 combat_health_runtime::Read(race+0x1960,play)&&play==1&&combat_health_runtime::Read(race+0x1958,count)&&count>1&&count<=8;}
unsigned Budget(){unsigned race=0,count=0;return Active()&&combat_health_runtime::Read(0x91E000,race)&&combat_health_runtime::Read(race+0x1958,count)?mwcombat::PoliceBudget(count):0;}
using DetectFn=bool(__thiscall*)(unsigned,unsigned);
DetectFn originalDetect=nullptr;
using TargetFn=void(__thiscall*)(unsigned,unsigned);
using ActivityFn=bool(__thiscall*)(unsigned,unsigned);
TargetFn originalTarget=nullptr,originalRelease=nullptr;
ActivityFn originalActivity=nullptr;
using PursuitSpawnFn=bool(__thiscall*)(unsigned,unsigned,unsigned);
PursuitSpawnFn originalPursuitSpawn=nullptr;
struct Assignment {unsigned car=0,cop=0,target=0,targetObject=0;ULONGLONG until=0;bool confirmed=false;};
std::array<Assignment,32> assignments{};
struct SpawnActivation {unsigned car=0,handle=0;ULONGLONG at=0;};
std::array<SpawnActivation,32> spawnActivations{};
unsigned ownershipRedirects=0,steeringRedirects=0;
std::array<unsigned,16> createdPursuits{};
std::array<unsigned,16> createdTargets{};
std::array<ULONGLONG,16> nextPursuitCreate{};
void ReleaseGroups();
std::vector<unsigned> NativeGroups();

extern unsigned roadblockDepth;
extern bool(__thiscall* originalRoadblock)(unsigned,unsigned,unsigned,unsigned,unsigned);
extern void* roadblockCommitTrampoline;
unsigned SupportLeader();
bool SupportAllowed(unsigned pursuit);
void ResetSupport();
void RefreshHelicopters(const std::vector<unsigned>& cars);
bool SupportSurface();
bool Install();
bool Surface(){
 const BYTE detect[]={0x6A,0xFF,0x68,0xA8,0x81,0x86,0,0x64},heat[]={0x6A,0xFF,0x68,0xBC,0x79,0x86,0};BYTE bytes[8]{};
 const BYTE offscreen[]={0xD9,0x81,0xD4,0,0,0,0xC3};
 const BYTE acquireTarget[]={0x56,0x8B,0xF1,0x8B,0x46,0x1C},activity[]={0x51,0x56,0x8B,0xF1,0x8B,0x4C,0x24,0x0C},release[]={0x55,0x56,0x8B,0x74,0x24,0x0C};
 const BYTE pursuitSpawn[]={0x6A,0xFF,0x68,0xC6,0x84,0x86,0,0x64};
 const BYTE block[]={0x6A,0xFF,0x68,0x16,0x95,0x86,0},commit[]={0x8B,0x0E,0xC7,0x46,0x78,2,0,0,0};
 const BYTE factory[]={0x6A,0xFF,0x68,0x02,0x99,0x86,0,0x64};
 const BYTE patrol[]={0xC7,0x86,0x90,0,0,0};
 const BYTE total[]={0x89,0x86,0x84,0,0,0},platform[]={0x89,0x86,0x8C,0,0,0},active[]={0x89,0x86,0x98,0,0,0};
 return SupportSurface()&&(originalRoadblock||(Copy(reinterpret_cast<void*>(0x43DD30),bytes,7)&&!memcmp(bytes,block,7)))&&
  (roadblockCommitTrampoline||(Copy(reinterpret_cast<void*>(0x43E7DF),bytes,8)&&!memcmp(bytes,commit,8)&&*reinterpret_cast<const BYTE*>(0x43E7E7)==0))&&
  (originalPursuitSpawn||(Copy(reinterpret_cast<void*>(0x430E60),bytes,8)&&!memcmp(bytes,pursuitSpawn,8)))&&
  Copy(reinterpret_cast<void*>(0x443270),bytes,8)&&!memcmp(bytes,factory,8)&&
  (originalTarget||(Copy(reinterpret_cast<void*>(0x423860),bytes,6)&&!memcmp(bytes,acquireTarget,6)))&&
  (originalActivity||(Copy(reinterpret_cast<void*>(0x6FBF80),bytes,8)&&!memcmp(bytes,activity,8)))&&
  (originalRelease||(Copy(reinterpret_cast<void*>(0x406450),bytes,6)&&!memcmp(bytes,release,6)))&&
  Copy(reinterpret_cast<void*>(0x688120),bytes,7)&&!memcmp(bytes,offscreen,7)&&
  Copy(reinterpret_cast<void*>(0x43CA00),bytes,6)&&!memcmp(bytes,patrol,6)&&
  Copy(reinterpret_cast<void*>(0x43C9E9),bytes,6)&&!memcmp(bytes,total,6)&&
  Copy(reinterpret_cast<void*>(0x43C9FA),bytes,6)&&!memcmp(bytes,platform,6)&&
  Copy(reinterpret_cast<void*>(0x43CA10),bytes,6)&&!memcmp(bytes,active,6)&&
  (originalDetect||(Copy(reinterpret_cast<void*>(0x429EF0),bytes,8)&&!memcmp(bytes,detect,8)))&&
  Copy(reinterpret_cast<void*>(0x409060),bytes,7)&&!memcmp(bytes,heat,7);
}
void Reset();
}
bool MWELHotPursuitActive(){return hot_pursuit_runtime::Active();}
unsigned MWELHotPursuitBudget(){return hot_pursuit_runtime::Budget();}
bool MWELHotPursuitAcquire(unsigned);
bool MWELHotPursuitRetain(unsigned);
bool MWELHotPursuitSpawnAllowed(bool);
