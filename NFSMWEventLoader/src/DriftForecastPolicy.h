#pragma once
#include "DriftScorePolicy.h"
#include "DriftCurvePolicy.h"
#include "DriftSpeedPolicy.h"
#include <vector>
namespace mwel_drift_forecast {
struct Knot {float station=0,curve=0;};
struct Route {std::vector<Knot> knots;float length=0,station=0;unsigned lap=0,laps=1;};
struct Performance {
 double seconds=0,speed=0,angle=0;
 float lastSpeed=0,lastAngle=0;
 void Observe(float dt,float v,float degrees,bool sliding){lastSpeed=v;lastAngle=degrees;if(sliding&&dt>0&&dt<=.25f){seconds+=dt;speed+=v*dt;angle+=std::abs(degrees)*dt;}}
};
struct Result {unsigned total=0,actual=0,added=0,steps=0;float remaining=0,efficiency=1;bool geometry=false,complete=false;};
inline bool Valid(const Route& r){
 if(r.knots.size()<2||r.knots.size()>32768||!std::isfinite(r.length)||r.length<1||!std::isfinite(r.station)||r.station<0||r.station>r.length+5||!r.laps||r.laps>100||r.lap>=r.laps)return false;
 float last=-1;for(auto p:r.knots){if(!std::isfinite(p.station)||!std::isfinite(p.curve)||p.station<=last||p.station<0||p.station>r.length+1||std::abs(p.curve)>2)return false;last=p.station;}
 return r.knots.front().station<=.01f&&r.knots.back().station>=r.length-.01f;
}
inline float Curve(const Route& r,float absolute){
 float station=std::fmod(std::max(0.f,absolute),r.length);
 auto it=std::upper_bound(r.knots.begin(),r.knots.end(),station,[](float v,Knot p){return v<p.station;});
 if(it==r.knots.begin())return it->curve;if(it==r.knots.end())return r.knots.back().curve;
 auto a=it[-1],b=*it;float t=(station-a.station)/(b.station-a.station);return a.curve+(b.curve-a.curve)*t;
}
// Pure shadow calculation. It receives copied geometry/score samples only:
// no vehicle pointers, game callbacks, input setters or native physics calls.
inline mwel_drift::Score Simulate(const Route& r,float begin,float end,mwel_drift::Score score,
                                 float initialSpeed,float initialAngle,float meanSpeed,float meanAngle,unsigned& steps){
 float position=begin,travel=0,speed=std::clamp(initialSpeed,12.f,60.f),angle=initialAngle;
 double time=std::max(0.,score.lastTime);score.primed=true;score.lastTime=score.progressTime=time;score.x=score.z=0;score.speed=speed;
 const float initialProgress=score.bestProgress;
 while(position<end-.001f&&score.standing()<mwel_drift::Maximum&&steps<20000){
  float curve=Curve(r,position),peak=std::abs(curve);
  for(float offset:{10.f,25.f,45.f})if(position+offset<end)peak=std::max(peak,std::abs(Curve(r,position+offset)));
  const float target=peak>mwel_drift_curve::Minimum?std::min(meanSpeed,mwel_drift_speed::Corner(peak)):mwel_drift_speed::Maximum;
  float dt=.1f;speed+=std::clamp(target-speed,-10.f*dt,7.f*dt);
  dt=std::min(dt,(end-position)/std::max(1.f,speed));if(dt<=0)break;
  const bool bend=std::abs(curve)>mwel_drift_curve::Minimum&&std::abs(curve)<.09f;
  const float requested=bend?-std::copysign(meanAngle,curve):0;
  angle+=std::clamp(requested-angle,-45.8366f*dt,45.8366f*dt); // same .8 rad/s AI intent slew
  const float moved=speed*dt;position+=moved;travel+=moved;time+=dt;
  score.update({time,travel,0,speed,angle,initialProgress+travel*.001f,true,false,false,false});++steps;
 }
 score.finish(time);return score;
}
inline Result Predict(const Route& route,const mwel_drift::Score& actual,const Performance& performance){
 Result result;result.actual=result.total=actual.standing();
 if(actual.finished){result.complete=true;return result;}if(!Valid(route))return result;
 result.geometry=true;const float begin=route.lap*route.length+std::min(route.station,route.length),end=route.laps*route.length;
 result.remaining=std::max(0.f,end-begin);if(result.remaining<.01f){result.complete=true;return result;}
 const float meanSpeed=performance.seconds>=2?std::clamp(float(performance.speed/performance.seconds),20.f,mwel_drift_speed::Maximum):30.f;
 const float meanAngle=performance.seconds>=2?std::clamp(float(performance.angle/performance.seconds),15.f,37.2423f):37.2423f;
 // Calibrate against this racer's observed score over its completed route.
 // This includes missed turns and lost chains; future proximity bonuses are
 // not invented. A very short/low-scoring history does not drive calibration.
 if(begin>=200){unsigned pastSteps=0;mwel_drift::Score initial;initial.awardScale=actual.awardScale;auto past=Simulate(route,0,begin,initial,30,0,meanSpeed,meanAngle,pastSteps);
  if(past.total>=500)result.efficiency=std::clamp(float(result.actual)/past.total,.35f,1.25f);}
 auto shadow=Simulate(route,begin,end,actual,performance.lastSpeed>5?performance.lastSpeed:meanSpeed,performance.lastAngle,meanSpeed,meanAngle,result.steps);
 const auto extra=shadow.total>result.actual?shadow.total-result.actual:0;
 result.added=unsigned(std::min(double(mwel_drift::Maximum-result.actual),std::round(extra*double(result.efficiency))));
 result.total=result.actual+result.added;result.complete=result.steps<20000||result.total==mwel_drift::Maximum;return result;
}
}
