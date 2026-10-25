#pragma once
#include "EventCore.h"
struct MWELBarrierSegment {float ax,ay,az,bx,by,bz;};
bool MWELPrepareBarriers(const std::vector<mwel::EventPackage>& packages,bool production);
bool MWELCompleteBarriers();
bool MWELBarrierSegments(unsigned key,std::vector<MWELBarrierSegment>& output);
bool MWELBarrierFrame();
