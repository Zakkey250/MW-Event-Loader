#pragma once
#include <cstdint>

// Win32 cdecl, fixed-width ABI. No STL objects or process pointers in output.
namespace mcdai {
constexpr std::uint32_t Version=1,Capacity=16,FreshMilliseconds=150;
enum Result : std::uint32_t { Ok=0,NotReady=1,BadRequest=2,NotRacer=3,StaleVehicle=4,Busy=5,ProfileError=6,NotOwned=7,Disabled=8 };
enum Reason : std::uint32_t { Waiting=0,Applied=1,Grip=2,Paused=3,StaleCommand=4,Unsafe=5,Expired=6,IdentityChanged=7,Fault=8 };
enum Scope : std::uint32_t { FreeRoam=0,Race=1,LocalAutodriveRace=2 };
enum Flags : std::uint32_t { AllowCorrection=1,ResetState=2 };
#pragma pack(push,8)
struct CapabilitiesV1 {
 std::uint32_t size=sizeof(CapabilitiesV1),version=Version,ready=0,capacity=Capacity;
 std::uint32_t modeMask=6,minLeaseMs=250,maxLeaseMs=2000,commandFreshMs=FreshMilliseconds;
};
struct BindingV1 {
 std::uint32_t size=sizeof(BindingV1),version=Version,vehicle=0,simHandle=0;
 std::uint32_t mode=0,leaseMs=500;
 std::uint64_t owner=0,generation=0;
 std::uint32_t scope=Race,race=0,reserved[4]{};
};
struct RefreshV1 {
 std::uint32_t size=sizeof(RefreshV1),version=Version;
 std::uint64_t owner=0,token=0;
 std::uint32_t leaseMs=500,flags=0;
};
// Optional race-planner intent. Wheel countersteer must not be interpreted as
// a request to cancel the body's drift direction. Separate from IInput.
struct IntentV1 {
 std::uint32_t size=sizeof(IntentV1),version=Version;
 std::uint64_t owner=0,token=0;
 float pathYaw=0,targetSlip=0;
 std::uint32_t active=0,reserved=0;
};
struct PathAssistV1 {
 std::uint32_t size=sizeof(PathAssistV1),version=Version;
 std::uint64_t owner=0,token=0;
 float strength=1;std::uint32_t reserved=0;
};
struct StateV1 {
 std::uint32_t size=sizeof(StateV1),version=Version,vehicle=0,simHandle=0;
 std::uint32_t mode=0,leased=0,eligible=0,phase=0,reason=Waiting,calibrated=0,steps=0,applied=0;
 std::uint64_t token=0,sampleTick=0;
 float speed=0,beta=0,yaw=0,steer=0,gas=0,brake=0,handbrake=0;
 float deltaYaw=0,deltaDirection=0,deltaSpeed=0,position[3]{};
 std::uint32_t reserved[3]{};
};
#pragma pack(pop)
static_assert(sizeof(CapabilitiesV1)==32&&sizeof(BindingV1)==64&&sizeof(RefreshV1)==32&&sizeof(StateV1)==128,"Fixed V1 ABI");
static_assert(sizeof(IntentV1)==40,"Fixed optional intent ABI");
static_assert(sizeof(PathAssistV1)==32,"Fixed optional path-assist ABI");
using GetCapabilitiesV1Fn=std::uint32_t(__cdecl*)(CapabilitiesV1*);
using AcquireV1Fn=std::uint32_t(__cdecl*)(const BindingV1*,std::uint64_t*);
using RefreshV1Fn=std::uint32_t(__cdecl*)(const RefreshV1*);
using SetIntentV1Fn=std::uint32_t(__cdecl*)(const IntentV1*);
using SetPathAssistV1Fn=std::uint32_t(__cdecl*)(const PathAssistV1*);
using ReleaseV1Fn=std::uint32_t(__cdecl*)(std::uint64_t,std::uint64_t);
using GetStateV1Fn=std::uint32_t(__cdecl*)(std::uint64_t,std::uint64_t,StateV1*);
}
