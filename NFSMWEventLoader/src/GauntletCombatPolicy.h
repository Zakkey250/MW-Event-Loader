#pragma once
#include "CombatImpactPolicy.h"
namespace gauntlet_combat {
inline float EvadeMeter(float native){return std::isfinite(native)?std::max(-.95f,native):native;}
inline bool Ram(const combat_impact::Motion& player,const combat_impact::Motion& cop){
 using namespace combat_impact;
 if(!Finite(player.position)||!Finite(cop.position)||!Finite(player.forward)||!Finite(cop.forward))return false;
 const auto delta=player.position-cop.position;
 const float distance=std::hypot(delta.x,delta.z);
 return distance>=6&&distance<=70&&std::abs(delta.y)<3.5f&&
  std::abs(Dot(player.forward,cop.forward))>.4f&&Dot(delta,cop.forward)>0&&
  std::hypot(cop.velocity.x,cop.velocity.z)>8;
}
enum class Front {None,Brake,Pit};
inline Front FrontBlock(const combat_impact::Motion& player,const combat_impact::Motion& cop){
 using namespace combat_impact;
 if(!Finite(player.position)||!Finite(cop.position)||!Finite(player.forward)||!Finite(cop.forward)||
    !Finite(player.velocity)||!Finite(cop.velocity)||Dot(player.forward,cop.forward)<.75f)return Front::None;
 const auto delta=cop.position-player.position;
 const float ahead=Dot(delta,player.forward),side=std::abs(delta.x*player.forward.z-delta.z*player.forward.x);
 const float speed=Dot(player.velocity,player.forward),closing=Dot(player.velocity-cop.velocity,player.forward);
 if(std::abs(delta.y)>=3.5f||speed<8||ahead<2||ahead>40||side>9)return Front::None;
 // Brake only while already occupying the player's lane. Offset cars use
 // the native PIT maneuver to converge instead of braking off the line.
 if(side<=2.7f&&ahead>=6&&ahead<=35&&closing>=-2)return Front::Brake;
 return side>2.7f&&ahead<=25?Front::Pit:Front::None;
}
}
