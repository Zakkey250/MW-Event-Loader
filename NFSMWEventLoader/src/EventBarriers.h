#pragma once
#include "EventCore.h"
struct MWELBarrierSegment {float ax,ay,az,bx,by,bz;};
bool MWELPrepareBarriers(const std::vector<mwel::EventPackage>& packages,bool production);
bool MWELCompleteBarriers();
void MWELPrepareStockBarrierAppearance();
void MWELStockBarrierAppearanceFrame();
bool MWELBarrierSegments(unsigned key,std::vector<MWELBarrierSegment>& output);
bool MWELBarrierFrame();
bool MWELRpdOfficerIgnoresGuards(unsigned worldId);
