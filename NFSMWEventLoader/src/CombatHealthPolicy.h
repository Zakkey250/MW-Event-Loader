#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
namespace combat_health {
enum class Hit:unsigned {Collision=0,Emp=1,Spike=2,Shockwave=3};
constexpr float PoliceReference=100.f,RacerCapacity=300.f;
inline float PoliceFraction(Hit kind){switch(kind){case Hit::Emp:return .25f;case Hit::Spike:return .5f;case Hit::Shockwave:return .15f;default:return 0;}}
// Weapon damage is measured against the police reference, never victim max HP.
// A racer withstands six 50-unit spikes, twelve 25-unit EMPs or twenty shocks.
inline float WeaponUnits(Hit kind){return PoliceReference*PoliceFraction(kind);}
struct Car {
 unsigned handle=0;float maximum=0,remaining=0;bool occupied=false,invincible=false,retired=false;
 // All values are transient. A garage/save record is never a health owner.
 void Begin(unsigned instance,bool police,bool immortal=false){handle=instance;maximum=police?PoliceReference:RacerCapacity;remaining=maximum;occupied=true;invincible=immortal;retired=false;}
 bool Damage(Hit kind,float collisionUnits=0,float weaponScale=1){
  if(!occupied||invincible||retired)return false;
  const float amount=kind==Hit::Collision?collisionUnits:WeaponUnits(kind)*weaponScale;
  if(!std::isfinite(amount)||amount<=0)return false;
  remaining=std::max(0.f,remaining-amount);retired=remaining<=0;return true;
 }
 float Normalized()const{return occupied&&maximum>0?remaining/maximum:0;}
 float Heal(float fraction){
  if(!occupied||retired||invincible||!std::isfinite(fraction)||fraction<=0)return 0;
  float prior=remaining;remaining=std::min(maximum,remaining+maximum*fraction);return remaining-prior;
 }
};
struct Ledger {
 unsigned event=0,epoch=0;std::array<Car,16> racers{};
 bool Observe(unsigned key,unsigned generation,bool enabled){
  if(!enabled||!key){bool changed=event!=0;*this={};return changed;}
  if(event==key&&epoch==generation)return false;
  *this={};event=key;epoch=generation;return true;
 }
 Car* Join(unsigned slot,unsigned handle,bool police=false,bool immortal=false){
  if(!event||slot>=racers.size()||!handle)return nullptr;
  auto& c=racers[slot];
  if(!c.occupied)c.Begin(handle,police,immortal);
  else c.handle=handle; // Respawning must not refill health.
  return &c;
 }
};
}
