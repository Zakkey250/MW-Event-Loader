#pragma once
#include <algorithm>
#include <cmath>
#include <cstdlib>
namespace mwel_difficulty {
struct Settings {
 bool enabled=true,catchUp=true,nitrous=true;
 float engineScale=1.75f,catchUpMaxScale=2,startMetres=50,fullMetres=600;
 // Explicit opt-in keeps existing EnginePowerScale-only INIs compatible.
 bool blacklistScaling=false,farCatchUp=false;
 float blacklist15Scale=1.4f,blacklist1Scale=1.75f,farMetres=301,farScale=2.5f;
};
struct RaceSample {unsigned rank=0;float playerGap=0;bool eligible=false;unsigned blacklist=0;};
inline float Number(const char* text,float fallback,float low,float high){
 if(!text||!*text)return fallback;char* end=nullptr;float value=std::strtof(text,&end);
 while(end&&(*end==' '||*end=='\t'))++end;
 return end!=text&&end&&!*end&&std::isfinite(value)?std::clamp(value,low,high):fallback;
}
inline float BaseScale(const Settings& s,const RaceSample& race){
 if(!s.blacklistScaling||!race.eligible||race.blacklist<1||race.blacklist>15)return s.engineScale;
 return s.blacklist15Scale+(s.blacklist1Scale-s.blacklist15Scale)*(15-race.blacklist)/14.f;
}
inline float Multiplier(const Settings& s,const RaceSample& race){
 if(!s.enabled)return 1;
 float extra=1;
 if(s.catchUp&&race.eligible&&race.rank>=2&&race.rank<=16&&std::isfinite(race.playerGap)&&race.playerGap>s.startMetres){
  const float t=std::clamp((race.playerGap-s.startMetres)/std::max(1.f,s.fullMetres-s.startMetres),0.f,1.f);
  // Continuous at both ends, avoiding a power step on every overtake.
  extra=1+(s.catchUpMaxScale-1)*t*t*(3-2*t);
 }
 if(s.catchUp&&s.farCatchUp&&race.eligible&&race.rank>=2&&race.rank<=16&&std::isfinite(race.playerGap)&&race.playerGap>=s.farMetres)
  extra=std::max(extra,s.farScale);
 return std::clamp(BaseScale(s,race)*extra,1.f,8.f);
}
inline float Output(float native,float scale){
 return std::isfinite(native)&&native>0&&native<1e8f&&std::isfinite(scale)&&scale>=1&&scale<=8?native*scale:native;
}
inline unsigned FieldMask(unsigned count){return count&&count<=16?(1u<<count)-1:0;}
}
