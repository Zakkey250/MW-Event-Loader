#pragma once
#include "../../integration/MWNeoObstacles.h"
// This optional authored-module interface is absent in vanilla MW. Preserve
// native props and their ownership; consume copied avoidance volumes only.
static unsigned NeoLiveObstacles(const float* origin,float range,YellowTrafficPoint* output,unsigned capacity){
 auto world=GetModuleHandleW(L"World2MWNativeLoader.asi");
 auto query=world?reinterpret_cast<mwneo_obstacles::Query>(GetProcAddress(world,"W2MW_NeoObstaclesV1")):nullptr;
 if(!query||!output||!capacity)return 0;
 mwneo_obstacles::Volume volumes[64]{};unsigned count=query(mwneo_obstacles::Version,origin,std::clamp(range,1.f,300.f),volumes,std::min(capacity,64u));
 if(count>std::min(capacity,64u))return 0;
 unsigned accepted=0;
 for(unsigned i=0;i<count;++i){const auto& volume=volumes[i];
  if(volume.size!=sizeof(volume)||!std::isfinite(volume.halfWidth)||!std::isfinite(volume.halfLength)||!std::isfinite(volume.halfHeight)||
     volume.halfWidth<=0||volume.halfLength<=0||volume.halfHeight<=0||volume.halfWidth>100||volume.halfLength>100||volume.halfHeight>100)continue;
  bool finite=true;for(unsigned n=0;n<3;++n)finite&=std::isfinite(volume.position[n])&&std::isfinite(volume.heading[n]);if(!finite)continue;
  auto& target=output[accepted++];target={};memcpy(target.p,volume.position,12);memcpy(target.heading,volume.heading,12);
  target.halfWidth=volume.halfWidth;target.halfLength=volume.halfLength;target.halfHeight=volume.halfHeight;
  target.handle=0xE0000000u|(volume.owner&0x0fffffffu);target.indestructible=true;
 }
 return accepted;
}
