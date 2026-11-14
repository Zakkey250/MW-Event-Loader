#pragma once
#include <cmath>
bool MWELRpdArrestProgress(unsigned* arrested,unsigned* initial);
// Versioned read-only bridge: returns 1 only for a complete live snapshot.
// Outputs are the surviving racers within 300 m, and all surviving racers.
namespace rpd_music_proximity {
#ifdef MWEL_ARMS_FRONTEND_STIMULUS
// Private audio integration stimulus only. No car, pursuit, audio event,
// score, result or save is modified. Absent from the production binary.
float AudioInputFixture(float actual){
 static const bool enabled=[](){const auto p=game/L"MWEventLoader.rpd-music-distance-test";
  return mwel::fs::is_regular_file(p)&&mwel::Read(p,64)=="MW_EVENT_LOADER_RPD_MUSIC_DISTANCE_V1\n";}();
 if(!enabled)return actual;
 static unsigned generation=0,last=~0u;static float start=0;float now=0;
 if(!gauntlet_runtime::Read(0x9885D8,now))return actual;
 if(generation!=combat_health_runtime::generation){generation=combat_health_runtime::generation;start=now;last=~0u;}
 const unsigned phase=unsigned(std::max(0.f,now-start)/24.f)%5;
 constexpr float sequence[]={40.f,150.f,285.f,150.f,40.f};
 if(last!=phase){last=phase;Log("EXPERIMENT_RPD_MUSIC_DISTANCE phase=%u input_m=%.1f actual_m=%.1f audio_input_only=1",phase,sequence[phase],actual);}
 return sequence[phase];
}
#endif
bool Position(unsigned handle,combat_impact::Vector& out){
 const auto car=gauntlet_runtime::Vehicle(handle);combat_impact::Motion motion;
 if(!car||!combat_health_runtime::NativeMotion(car-0x80,motion)||motion.handle!=handle)return false;
 out=motion.position;return std::isfinite(out.x)&&std::isfinite(out.y)&&std::isfinite(out.z);
}
unsigned Read(unsigned* nearbyOut,unsigned* remainingOut,float* distanceOut=nullptr){
 if(nearbyOut)*nearbyOut=0;if(remainingOut)*remainingOut=0;
 if(distanceOut)*distanceOut=300.f;
 unsigned race=0;const auto remaining=rpd_music_session::Remaining(&race);
 if(!remaining||!race||remaining>8)return 0;
 unsigned count=0,player=0;
 if(!gauntlet_runtime::Read(race+0x1958,count)||count<2||count>9||
    !gauntlet_runtime::Read(race+0x18,player))return 0;
 combat_impact::Vector local;if(!Position(player,local))return 0;
 unsigned nearby=0,observed=0;double nearestSquared=1.e30;
 for(unsigned slot=1;slot<count;++slot){
  const auto handle=rpd_music_session::ActiveTarget(race,slot);if(!handle)continue;
  combat_impact::Vector other;if(!Position(handle,other))return 0;
  const double dx=double(other.x)-local.x,dy=double(other.y)-local.y,dz=double(other.z)-local.z;
  const double squared=dx*dx+dy*dy+dz*dz;
  if(squared<=300.0*300.0)++nearby;
  if(squared<nearestSquared)nearestSquared=squared;
  ++observed;
 }
 // A retirement/rebind can precede its native slot update in this callback.
 // Do not misreport an unavailable live body as a distant or missing racer.
 if(observed!=remaining)return 0;
 if(nearbyOut)*nearbyOut=nearby;if(remainingOut)*remainingOut=remaining;
 if(distanceOut)*distanceOut=float(std::sqrt(nearestSquared));
#ifdef MWEL_ARMS_FRONTEND_STIMULUS
 if(distanceOut)*distanceOut=AudioInputFixture(*distanceOut);
#endif
 return 1;
}
}
#pragma comment(linker,"/EXPORT:MWEL_RpdMusicProximityV1=_MWEL_RpdMusicProximityV1")
extern "C" __declspec(dllexport) unsigned __cdecl MWEL_RpdMusicProximityV1(unsigned* nearby,unsigned* remaining){
 return rpd_music_proximity::Read(nearby,remaining);
}
// V1 keeps its published count contract. V2 adds the nearest live suspect's
// actual 3D distance; retired cars and the officer are never music subjects.
#pragma comment(linker,"/EXPORT:MWEL_RpdMusicProximityV2=_MWEL_RpdMusicProximityV2")
extern "C" __declspec(dllexport) unsigned __cdecl MWEL_RpdMusicProximityV2(unsigned* nearby,unsigned* remaining,float* distance){
 return rpd_music_proximity::Read(nearby,remaining,distance);
}
// Versioned together: distance plus actual arrests, never inferred from
// disappeared racers (which would count escapes as successful arrests).
#pragma comment(linker,"/EXPORT:MWEL_RpdMusicProximityV3=_MWEL_RpdMusicProximityV3")
extern "C" __declspec(dllexport) unsigned __cdecl MWEL_RpdMusicProximityV3(unsigned* nearby,unsigned* remaining,float* distance,unsigned* arrested,unsigned* initial){
 if(arrested)*arrested=0;if(initial)*initial=0;
 if(!rpd_music_proximity::Read(nearby,remaining,distance))return 0;
 unsigned a=0,n=0;
 if(!MWELRpdArrestProgress(&a,&n)){
  a=0;
  unsigned race=0,count=0;
  if(!gauntlet_runtime::Read(0x91E000,race)||!race||!gauntlet_runtime::Read(race+0x1958,count)||count<2||count>9)return 0;
  n=count-1; // Pre-bind or single-suspect Interceptor: no completed arrests yet.
 }
 if(!n||n>8||a>n)return 0;
 if(arrested)*arrested=a;if(initial)*initial=n;return 1;
}
