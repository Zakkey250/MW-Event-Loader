#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
namespace combat_impact {
// Apply once at the contact boundary, including native fallback health deltas.
// Weapon HP is specified separately and must never pass through this scale.
inline float Scale(float units){return std::isfinite(units)&&units>0?units*.5f:0;}
// MW contact impulse is normalized by the body's actual mass. This is an
// MW-specific conversion to the 100-unit police reference, not an unverified
// claim that HPR's hashed tuning coefficients use the same units.
inline float Units(float impulse,float mass,float normalY){
 if(!std::isfinite(impulse)||!std::isfinite(mass)||!std::isfinite(normalY)||impulse<=0||mass<100||mass>20000||std::abs(normalY)>1.1f)return 0;
 const float severity=impulse/mass;
 const float threshold=std::abs(normalY)>.7f?8.f:3.f;
 if(!std::isfinite(severity)||severity<=threshold)return 0;
 return Scale(std::min(75.f,(severity-threshold)*(severity-threshold)*.04f));
}
struct Vector {float x=0,y=0,z=0;};
inline Vector operator-(Vector a,Vector b){return {a.x-b.x,a.y-b.y,a.z-b.z};}
inline Vector operator+(Vector a,Vector b){return {a.x+b.x,a.y+b.y,a.z+b.z};}
inline Vector operator*(Vector a,float b){return {a.x*b,a.y*b,a.z*b};}
inline float Dot(Vector a,Vector b){return a.x*b.x+a.y*b.y+a.z*b.z;}
inline bool Finite(Vector v){return std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z);}
struct Motion {Vector position,velocity,forward;unsigned handle=0;float sampledAt=-100;bool valid=false;float halfWidth=1,halfHeight=.6f,halfLength=2.3f;};
struct Ram {float factor=1,closing=0;bool attacker=false;};
inline bool EnteringRoadblock(const Motion& self,const Motion& other){
 if(!self.valid||!other.valid||!Finite(self.position)||!Finite(other.position)||!Finite(self.velocity)||!Finite(other.velocity))return false;
 auto direction=other.position-self.position;
 if(std::abs(direction.y)>3.5f)return false;
 direction.y=0;const float length=std::sqrt(Dot(direction,direction));
 if(length<.25f||length>15)return false;
 direction=direction*(1/length);
 const float own=Dot(self.velocity,direction),incoming=-Dot(other.velocity,direction);
 // Parked roadblock cars may face across or against the road. Their heading
 // cannot decide who initiated the collision; actual approach speed can.
 return own>3&&own>incoming+3&&own+incoming>2;
}
inline Ram RamContact(const Motion& self,const Motion& other,Vector point){
 if(!self.valid||!other.valid||!Finite(self.position)||!Finite(other.position)||!Finite(self.velocity)||!Finite(other.velocity)||!Finite(self.forward)||!Finite(other.forward)||!Finite(point))return {};
 auto direction=other.position-self.position;direction.y=0;float length=std::sqrt(Dot(direction,direction));
 if(length<.25f||length>15)return {};direction=direction*(1/length);
 const float own=Dot(self.velocity,direction),opponent=-Dot(other.velocity,direction),closing=std::max(0.f,own+opponent);
 if(!std::isfinite(closing)||closing>350)return {};
 auto contact=point-self.position;contact.y=0;const float contactLength=std::sqrt(Dot(contact,contact));
 const bool frontal=contactLength>.15f&&Dot(self.forward,contact*(1/contactLength))>.35f&&Dot(self.forward,direction)>.35f;
 // A deliberate front/quarter or rear-end ram must have a clear initiator.
 // Head-on mutual contacts and ambiguous low-speed pushes get no immunity.
 const bool attacker=frontal&&Dot(self.forward,other.forward)>-.5f&&own>3&&closing>2&&own>opponent+3;
 return {attacker?.1f:std::clamp(.65f+closing/55.f,.65f,1.25f),closing,attacker};
}
inline unsigned VisualZone(float remaining,float maximum){
 if(!std::isfinite(remaining)||!std::isfinite(maximum)||maximum<=0||remaining>maximum*.6f)return 0;
 return remaining<=maximum*.3f?2u:1u;
}
struct Contacts {
 struct Entry {std::uintptr_t other=0;unsigned handle=0;float last=-100,peak=0;bool occupied=false;float factor=1;};
 std::array<Entry,8> entries{};
 float Charge(std::uintptr_t other,unsigned handle,float now,float units,float factor=1){
  if(!std::isfinite(now)||!std::isfinite(units)||units<0||!std::isfinite(factor)||factor<0||factor>2)return 0;
  Entry* slot=nullptr;Entry* oldest=&entries[0];
  for(auto& e:entries){
   if(e.occupied&&e.other==other&&e.handle==handle){slot=&e;break;}
   if(!e.occupied)oldest=&e;else if(oldest->occupied&&e.last<oldest->last)oldest=&e;
  }
  if(!slot)slot=oldest;
  const bool separate=!slot->occupied||slot->other!=other||slot->handle!=handle||now<slot->last||now-slot->last>.4f;
  if(separate)*slot={other,handle,now,0,true,factor};
  units*=slot->factor; // Keep attribution stable throughout one contact.
  const float charge=std::max(0.f,units-slot->peak);
  slot->last=now;slot->peak=std::max(slot->peak,units);return charge;
 }
};
}
