#pragma once
namespace mwel_rpd_weapons {
enum class Weapon {Emp,Spike,Shock,Jammer};
constexpr float RacerCooldown(bool policeSprint,float seconds){return policeSprint?seconds*1.5f:seconds;}
inline bool Police(unsigned driver){return driver==0||driver==2;}
inline bool Hostile(bool policeSide,unsigned source,unsigned target){
 if(!policeSide)return true;
 return Police(source)?target==3:source==3&&Police(target);
}
inline bool Allowed(bool policeSide,unsigned source,Weapon weapon){
 return !(policeSide&&Police(source)&&weapon==Weapon::Jammer);
}
inline void RestrictAmmo(bool policeSide,unsigned& mask,unsigned* counts){
 if(policeSide){mask|=1u<<4;counts[4]=0;}
}
}
