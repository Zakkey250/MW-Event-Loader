#pragma once
// Internal bridge: the owned recovery planner alone may use native placement.
namespace event_neo_driver { bool MWELNeoPlaceVehicle(void* ai,float* position,float* heading); }
bool MWELNeoCourseFacing(unsigned handle,const float* position,const float* heading,bool nativeFacing);
#include "GridPolicy.h"
bool MWELFitGrid(unsigned key,unsigned count,const mwel_grid::Position& origin,float rotation,mwel_grid::Layout& layout);
bool MWELPrepareNeoCourse(unsigned key);
unsigned MWELNeoCurrentKey();
bool MWELNeoCourseEnabled();
bool MWELNeoCourseFile(unsigned key,const char* suffix,char* output,unsigned size);
bool MWELNeoSettingsFile(char* output,unsigned size);
bool MWELDriftWallClearance(const float* position,const float* heading,float halfWidth,float halfLength,float* clearance);
bool MWELNeoGate(unsigned key,unsigned leg,float* center,float* dimensions,float* rotation);
void MWELNeoLog(const char* text);
bool MWELNeoLoggingEnabled();
void MWELResetNeoProviderRace();
bool MWELNeoPendingGateEnd(unsigned handle,unsigned edge);
bool MWELNeoRouteProgress(unsigned handle,unsigned edge,float* station,unsigned* revision=nullptr);
bool MWELNeoRecoveryResumed(unsigned handle,unsigned edge);

bool MWELCourseAhead(unsigned key,const float* position,unsigned leg,float metres,float* point,float* forward);
bool MWELPrepareGauntletRoad(unsigned key);
