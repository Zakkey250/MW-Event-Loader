#pragma once
#include "NeoDepartureLearning.h"
#include "NeoDepartureBounds.h"
struct NeoDeparturePrediction {
 neo_departure::Arrival arrival{};float accelerationLow=0,accelerationHigh=0;unsigned samples=0,reason=0,missingMode=0;
};
// Hypothetical full-throttle prediction. The caller supplies all NOS modes
// that may be selected before departure; an unobserved one keeps a-policy.
static bool NeoPredictDeparture(const NeoDepartureLearning& learner,const NeoDepartureLearning::Observation& current,
 float speed,float distance,bool nitrousPossible,NeoDeparturePrediction& result){
 result={};result.reason=1;if(!std::isfinite(speed)||speed<=15||!std::isfinite(distance)||distance<0)return false;
 float speedLow=speed,speedHigh=speed;
 for(unsigned attempt=0;attempt<8;++attempt){
  float low=1e30f,high=-1e30f;unsigned total=0;
  for(unsigned mode:{0u,1u,7u}){
   if((current.mode&1)?(mode==0||(!nitrousPossible&&mode!=1)):mode!=0)continue;
   auto hypothesis=current;hypothesis.mode=mode;float a=0,b=0;unsigned n=0,reason=0;
   if(!learner.Range(hypothesis,std::min(speed,speedLow),std::max(speed,speedHigh),a,b,&n,&reason)){result.reason=10+reason;result.missingMode=mode;result.samples=n;return false;}
   low=std::min(low,a);high=std::max(high,b);total+=n;
  }
  neo_departure::Arrival next;
  if(!neo_departure::Propagate(speed,distance,low,high,next)){result.reason=2;return false;}
  result.arrival=next;result.accelerationLow=low;result.accelerationHigh=high;result.samples=total;
  if(next.latestSeconds>.75){result.reason=3;return false;}
  bool covered=next.minimumSpeed>=speedLow-.001&&next.maximumSpeed<=speedHigh+.001;
  speedLow=std::min(speedLow,float(next.minimumSpeed));speedHigh=std::max(speedHigh,float(next.maximumSpeed));
  if(covered){result={next,low,high,total,0,0};return true;}
 }
 result.reason=4;return false;
}
