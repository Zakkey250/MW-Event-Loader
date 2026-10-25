#pragma once
#include <windows.h>
#include "DifficultyPolicy.h"
bool MWELNeoDifficultySample(unsigned handle,mwel_difficulty::RaceSample& sample);
struct MWELNeoScopeV1 {void* vehicle;unsigned handle,driverClass,failure;float heading[3],speed;};
bool MWELInstallNeoDriver();
bool MWELNeoDriverInstalled();
bool MWELNeoGroundQueryAllowed(const float* point);
bool MWELInstallNeoGroundFallback();
void MWELNeoGroundFrame();
bool MWELNeoGroundManagerOwned(const void* manager);
void MWELNeoCompleteGroundLists(void* manager,unsigned flags);
bool MWELInstallGauntletPower();
bool MWELGauntletBoostOutput(void* vehicle,float* output);
void MWELResetNeoDriverRace();
bool MWELNeoHoldingAtGenuineGoal(unsigned handle);
// Private, same-ASI observation surface; does not change the public NeoAI ABI.
struct MWELNeoMotionState {float acceleration=0,throttle=0,brake=0,steering=0,powerScale=0,nosTank=0;bool known=false,nosRead=false,nosEquipped=false,nosRequested=false,nosEngaged=false,nosAllowed=false;};
bool MWELNeoReadMotion(unsigned handle,MWELNeoMotionState& out);
#ifdef MWEL_NEO_OBSERVE_ONLY
struct MWELNeoFrameState {unsigned handle;float position[3],velocity[3],forward[3],priorInputs[3];};
void MWELNeoObservePhysicsFrame(void* vehicle,const MWELNeoFrameState& state,float steering,float throttle,float brake);
int MWELNeoDiagnosticWheelCount(void* vehicle,const MWELNeoFrameState& state);
#endif
#if defined(MWEL_NEO_AIR_COAST_EXPERIMENT) && !defined(MWEL_NEO_OBSERVE_ONLY)
#error Air-coast experiment must never enter an ordinary handoff build.
#endif
