#pragma once
#include <algorithm>
#include <cmath>
#include <string>
namespace rpd_interceptor_policy {
inline bool ArrestHealth(bool arsenalPursuit,bool known,float remaining,float maximum){
 return !arsenalPursuit||(known&&std::isfinite(remaining)&&std::isfinite(maximum)&&
  maximum>0&&remaining>=0&&remaining<=maximum*.25f);
}
inline float Catchup(float metres){
 return std::isfinite(metres)?1.f+.25f*std::clamp((metres-50.f)/250.f,0.f,1.f):1.f;
}
inline bool ArrestContribution(bool localPlayerAlive,bool inSight,float metres){
 return localPlayerAlive&&inSight&&std::isfinite(metres)&&metres>=0&&metres<=15.f;
}
inline float ArrestIncrement(bool eligible,float nativeIncrement,float dt){
 if(!(nativeIncrement>0)||eligible)return nativeIncrement;
 // AI capture may bring an escaped target back to neutral, but must neither
 // build nor hold the local officer's positive arrest progress.
 return std::isfinite(dt)&&dt>0&&dt<=.5f?-.5f*dt:0.f;
}
inline std::string VehicleName(const std::string& preset){
 if(preset=="CS_CAR_COPGTO")return "GTO";
 const std::string prefix="CS_CAR_";auto name=preset.rfind(prefix,0)==0?preset.substr(prefix.size()):preset;
 if(name.rfind("COP",0)==0)name.erase(0,3);
 return name;
}
// Simulation time: a pause cannot advance a handoff or a stuck-car deadline.
struct Stuck {
 float seconds=0;float x=0,y=0,z=0;bool sampled=false;
 bool Step(float dt,float px,float py,float pz){
  if(!std::isfinite(dt)||dt<=0||dt>.5f||!std::isfinite(px)||!std::isfinite(py)||!std::isfinite(pz))return false;
  // Suspension bounce and reversing against an obstacle are not recovery.
  // Require eight metres of horizontal body displacement before resetting
  // the five-second window; wheel speed and vertical movement do not count.
  float dx=px-x,dz=pz-z;
  if(!sampled||dx*dx+dz*dz>=64){sampled=true;x=px;y=py;z=pz;seconds=0;return false;}
  seconds+=dt;return seconds>=4.9999f;
 }
};
}
