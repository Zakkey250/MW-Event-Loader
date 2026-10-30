// NeoAI actuators and state live in the event ASI. NFRR remains the native
// vehicle-scope integration dependency; its free-roam controller is separate.
#include "NeoDriverBridge.h"
#include "NeoProviderBridge.h"
#include <MinHook.h>
#include <cstdio>
#include <cstdarg>
#include <cstring>
#include <string>
#include <array>
#include <algorithm>
#include <cmath>
#include <intrin.h>
#include <NFSPluginSDK/Game.MW05/Types/FECustomizationRecord.h>
#include <NFSPluginSDK/Game.MW05/Types/AIVehicle.h>
#include <NFSPluginSDK/Game.MW05/Types/Attrib/Gen/pvehicle.h>
#include <NFSPluginSDK/Game.MW05/Types/Physics.h>
#include "DifficultySettings.h"
#include "DriftAIClient.h"
#include "DriftCurvePolicy.h"
void MWELNeoLog(const char* text);
bool MWELNeoDetailedLoggingEnabled();
namespace event_neo_driver {
enum class LogLevel{Info,Warning,Error};
void Log(LogLevel,const char* format,...){if(!MWELNeoDetailedLoggingEnabled())return;char text[8192];va_list args;va_start(args,format);vsnprintf_s(text,sizeof(text),_TRUNCATE,format,args);va_end(args);MWELNeoLog(text);}
#define Log(...) do { if(MWELNeoDetailedLoggingEnabled()) (Log)(__VA_ARGS__); } while(false)
uintptr_t Address(uintptr_t address){return address;}
template<class T>bool AudioRead(const void* from,T* out){if(!from)return false;MEMORY_BASIC_INFORMATION region{};if(!VirtualQuery(from,&region,sizeof(region))||region.State!=MEM_COMMIT||(region.Protect&(PAGE_NOACCESS|PAGE_GUARD)))return false;uintptr_t begin=(uintptr_t)from,end=(uintptr_t)region.BaseAddress+region.RegionSize;if(begin>end||sizeof(T)>end-begin)return false;__try{memcpy(out,from,sizeof(T));return true;}__except(EXCEPTION_EXECUTE_HANDLER){return false;}}
bool BattleVtable(const void* object,unsigned table){unsigned actual=0;return AudioRead(object,&actual)&&actual==table;}
constexpr unsigned kRacecarPrimaryVtable=0x892720,kDriverHuman=0;
struct Vector {float x=0,y=0,z=0;};
struct VehicleSnapshot {void* pointer=nullptr;unsigned handle=0,driverClass=~0u;Vector heading{};float speed=0;};
unsigned W2CRaceHandle(const VehicleSnapshot& racer){return racer.handle;}
bool W2CRaceVehicle(void* ai,VehicleSnapshot& racer,VehicleSnapshot&,bool local,bool action,unsigned* failure){
 auto module=GetModuleHandleW(L"NFSMWNativeFreeRoamRacers.asi");using Scope=unsigned(__cdecl*)(void*,unsigned,unsigned,MWELNeoScopeV1*,unsigned);
 static Scope scope=nullptr;if(!scope&&module)scope=reinterpret_cast<Scope>(GetProcAddress(module,"NFRR_NeoScopeV1"));MWELNeoScopeV1 out{};
 if(!scope||!scope(ai,local,action,&out,sizeof(out))){if(failure)*failure=out.failure;return false;}racer.pointer=out.vehicle;racer.handle=out.handle;racer.driverClass=out.driverClass;racer.heading={out.heading[0],out.heading[1],out.heading[2]};racer.speed=out.speed;return true;
}
bool W2CReleaseNativeQuery(void* ai){void* nav=nullptr;if(!AudioRead(static_cast<BYTE*>(ai)+0x24,&nav)||!nav)return false;const BYTE guard[]={0x56,0x8B,0xF1};if(memcmp(reinterpret_cast<void*>(0x770060),guard,sizeof(guard)))return false;reinterpret_cast<void(__thiscall*)(void*)>(0x770060)(nav);return true;}

#include "NeoActuators.inl"
}
bool MWELInstallNeoDriver(){event_neo_driver::InstallNeoAI();return event_neo_driver::neoInstalled;}
bool MWELNeoDriverInstalled(){return event_neo_driver::neoInstalled;}
bool MWELNeoGroundManagerOwned(const void* manager){
 using namespace event_neo_driver;
 if(!manager||!neoInstalled||*reinterpret_cast<unsigned*>(0x925E90)!=6||!MWELNeoCourseEnabled())return false;
 unsigned worldId=0;if(!AudioRead(static_cast<const BYTE*>(manager)+0x90,&worldId)||!worldId)return false;
 auto race=*reinterpret_cast<void**>(0x91E000);auto now=GetTickCount64();
 for(const auto& row:neoRuntime)if(row.owned&&row.race==race&&now-row.activeAt<250){
  BYTE* vehicle=nullptr;unsigned actual=0;
  if(AudioRead(static_cast<const BYTE*>(row.ai)-0x4C+0x48,&vehicle)&&BattleVtable(vehicle,0x8AA828)&&AudioRead(vehicle-0x20,&actual)&&actual==worldId)return true;
 }
 return false;
}
bool MWELNeoGroundQueryAllowed(const float* point){
 using namespace event_neo_driver;
 if(!point||!neoInstalled||*reinterpret_cast<unsigned*>(0x925E90)!=6||!MWELNeoCourseEnabled())return false;
 mwneo::Point p{point[0],point[1],point[2]};if(!mwneo::Finite(p))return false;
 if(neoPlacementPoint){mwneo::Point target{neoPlacementPoint[0],neoPlacementPoint[1],neoPlacementPoint[2]};return mwneo::LengthXZ(mwneo::Sub(p,target))<8&&std::abs(p.y-target.y)<6;}
 auto now=GetTickCount64();auto race=*reinterpret_cast<void**>(0x91E000);
 for(const auto& row:neoRuntime)if(row.owned&&row.race==race&&now-row.activeAt<250&&row.groundExtent>0&&
   std::abs(p.y-row.groundPosition.y)<6&&mwneo::LengthXZ(mwneo::Sub(p,row.groundPosition))<row.groundExtent)return true;
 return false;
}
bool MWELNeoReadMotion(unsigned handle,MWELNeoMotionState& out){
 out={};using namespace event_neo_driver;
 for(const auto& row:neoRuntime)if(row.owned&&row.handle==handle&&row.driver.initialized&&row.controls.valid){
  out.acceleration=row.driver.acceleration;out.throttle=row.controls.throttle;out.brake=row.controls.brake;out.steering=row.controls.steering;out.powerScale=row.powerScale;
  out.nosRead=row.nosRead;out.nosEquipped=row.nosEquipped;out.nosRequested=row.nitrous.pressed;out.nosEngaged=row.nosEngaged;out.nosTank=row.nosTank;
  out.nosAllowed=neoDifficulty.nitrous;
  out.known=std::isfinite(out.acceleration)&&std::isfinite(out.powerScale)&&out.powerScale>0;return out.known;
 }return false;
}
void MWELResetNeoDriverRace(){
 // Retry can change the order in which human/AI rows are allocated while
 // reusing native vehicle identities. Restore ALL old suspension leases
 // before any row is cleared/rebound. Otherwise an old row can restore the
 // human's Simple suspension after a different row has just acquired Racer.
 for(auto& row:event_neo_driver::neoRuntime)event_neo_driver::NeoDriftRelease(row);
 if(!++event_neo_driver::neoDriftGeneration)++event_neo_driver::neoDriftGeneration;
 for(auto& row:event_neo_driver::neoRuntime)row={};event_neo_driver::neoDiagnosticHandle=0;
}
void MWELNeoReleaseObservedGoal(unsigned handle){
 if(!MWELNeoHoldingAtGenuineGoal(handle))return;
 for(auto& row:event_neo_driver::neoRuntime)if(row.handle==handle){
  event_neo_driver::NeoDriftRelease(row);row.owned=false;row.genuineGoalHeld=true;
 }
}
bool MWELInstallGauntletPower(){
 if(event_neo_driver::neoEngineOutputInstalled)return true;
 event_neo_driver::neoEngineOutputInstalled=event_neo_driver::InstallNeoEngineOutput();
 return event_neo_driver::neoEngineOutputInstalled;
}
#pragma comment(linker,"/EXPORT:MWEL_NeoRoadBlockedV1=_MWEL_NeoRoadBlockedV1")
extern "C" __declspec(dllexport) unsigned __cdecl MWEL_NeoRoadBlockedV1(void* nav){
 __try{return event_neo_driver::NeoRoadBlocked(nav);}
 __except(EXCEPTION_EXECUTE_HANDLER){return 0;}
}
#pragma comment(linker,"/EXPORT:MWEL_NeoPowerOwnedV1=_MWEL_NeoPowerOwnedV1")
extern "C" __declspec(dllexport) unsigned __cdecl MWEL_NeoPowerOwnedV1(void* ai){
 __try{return event_neo_driver::neoEngineOutputInstalled&&event_neo_driver::NeoOwned(ai)!=nullptr;}
 __except(EXCEPTION_EXECUTE_HANDLER){return 0;}
}
