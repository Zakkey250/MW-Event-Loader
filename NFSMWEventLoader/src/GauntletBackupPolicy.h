#pragma once
#include "CombatImpactPolicy.h"
namespace gauntlet_backup {
using combat_impact::Vector;
struct Anchor {Vector position,forward;float station=0,speed=0;};
struct Trail {
 std::array<Anchor,64> points{};unsigned count=0;float station=0,lastTime=-1,farSince=-1,readyAt=0;
 void Observe(Vector position,Vector forward,float verticalSpeed,float now,float speed=0){
  using namespace combat_impact;
  if(!Finite(position)||!Finite(forward)||!std::isfinite(verticalSpeed)||!std::isfinite(now)||now<0)return;
  if(lastTime>now){*this={};lastTime=-1;farSince=-1;}
  lastTime=now;if(std::abs(verticalSpeed)>3.5f)return;
  if(count){auto d=position-points[count-1].position;const float length=std::sqrt(Dot(d,d));
   if(length<3)return;if(length>250){count=0;station=0;farSince=-1;}else station+=length;
  }
  if(count==points.size()){for(unsigned i=1;i<count;++i)points[i-1]=points[i];--count;}
  points[count++]={position,forward,station,std::isfinite(speed)?std::clamp(speed,0.f,60.f):0.f};
 }
 bool Due(float distance,float now){
  if(!std::isfinite(distance)||!std::isfinite(now)||distance<650){farSince=-1;return false;}
  if(farSince<0)farSince=now;
  return now>=readyAt&&now-farSince>=3;
 }
 bool Select(Anchor& out)const{
  float best=1e9f;bool found=false;
  for(unsigned i=0;i<count;++i){float back=station-points[i].station;
   if(back<80||back>130)continue;float score=std::abs(back-100);
   if(score<best){out=points[i];best=score;found=true;}
  }return found;
 }
 void Committed(float now){readyAt=now+20;farSince=-1;}
};
}
