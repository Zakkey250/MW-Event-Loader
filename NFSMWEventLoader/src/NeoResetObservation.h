#pragma once
// Private, read-only EN1.3 diagnosis. Never used by the driving policy.
#ifdef MWEL_NEO_OBSERVE_ONLY
struct NeoResetObservation {
 unsigned primary=0,driver=~0u,cookieCount=0,cookieLast=~0u;
 unsigned worldSurface=0,nullSurface=0,defaultSurface=0,worldFlags=0;
 unsigned wheelSurface[4]{};int wheels=-1,wheelContact[4]{};
 float up[3]{},normal[4]{},angular[3]{},compression[4]{};
 float flipped=0,cookie[3]{},cookieTime=0;
};
static bool NeoReadResetObservation(BYTE* vehicle,const NeoNativeDynamics& body,NeoResetObservation& out){
 __try {
  if(!vehicle||*reinterpret_cast<unsigned*>(vehicle)!=0x8AA828||!body.body||!body.positionPointer)return false;
  auto object=*reinterpret_cast<BYTE**>(body.body+4);if(!object)return false;
  auto begin=*reinterpret_cast<BYTE**>(object+4),end=*reinterpret_cast<BYTE**>(object+8);
  if(!begin||end<begin||end-begin>8192||(end-begin)%8)return false;
  BYTE* reset=nullptr;for(auto pair=begin;pair<end;pair+=8)if(*reinterpret_cast<unsigned*>(pair)==0x620250){reset=*reinterpret_cast<BYTE**>(pair+4);break;}
  if(!reset||*reinterpret_cast<unsigned*>(reset)!=0x8ABA10||*reinterpret_cast<BYTE**>(reset+4)!=object)return false;
  auto primary=reset-0x4C;
  if(*reinterpret_cast<unsigned*>(primary)!=0x8ABA2C||*reinterpret_cast<BYTE**>(primary+0x48)!=vehicle||*reinterpret_cast<unsigned*>(primary+0x60)!=4)return false;
  auto collisionBody=*reinterpret_cast<BYTE**>(primary+0xE8),suspension=*reinterpret_cast<BYTE**>(primary+0xEC),sim=*reinterpret_cast<BYTE**>(primary+0x34);
  if(!collisionBody||!suspension||!sim||*reinterpret_cast<unsigned*>(collisionBody)!=0x8AC7D8||*reinterpret_cast<unsigned*>(suspension)!=0x8ABC30||*reinterpret_cast<unsigned*>(sim)!=0x8AA940)return false;
  // Verified SDK getters: ICollisionBody.up -> volatile+80, normal -> this+80;
  // ISuspension.wheel count -> 4, grounded count -> this+C8, wheel -> E0+i*4.
  auto ct=*reinterpret_cast<unsigned**>(collisionBody),st=*reinterpret_cast<unsigned**>(suspension),vt=*reinterpret_cast<unsigned**>(sim);
  if(ct[40]!=0x6710B0||ct[32]!=0x670DC0||st[2]!=0x6AC2A0||st[19]!=0x6AC3D0||st[9]!=0x6AC360||st[17]!=0x6AC390||vt[19]!=0x686A80)return false;
  auto volatileState=**reinterpret_cast<BYTE***>(collisionBody+0x28);
  if(!volatileState||unsigned(volatileState+0x10)!=body.positionPointer)return false;
  out.primary=unsigned(primary);out.driver=*reinterpret_cast<unsigned*>(vehicle+0x94);
  memcpy(out.up,volatileState+0x80,12);memcpy(out.angular,volatileState+0x30,12);memcpy(out.normal,collisionBody+0x80,16);
  out.flipped=*reinterpret_cast<float*>(primary+0x54);out.cookieCount=*reinterpret_cast<unsigned*>(primary+0x58);out.cookieLast=*reinterpret_cast<unsigned*>(primary+0x5C);
  if(out.cookieCount>4||(out.cookieCount&&out.cookieLast>=4))return false;
  if(out.cookieCount){auto cookie=primary+0x68+32*out.cookieLast;memcpy(out.cookie,cookie,12);out.cookieTime=*reinterpret_cast<float*>(cookie+28);}
  out.wheels=*reinterpret_cast<int*>(suspension+0xC8);if(out.wheels<0||out.wheels>4)return false;
  for(unsigned i=0;i<4;++i){auto wheel=*reinterpret_cast<BYTE**>(suspension+0xE0+4*i);if(!wheel)return false;out.compression[i]=*reinterpret_cast<float*>(wheel+0x78);out.wheelContact[i]=std::isfinite(out.compression[i])&&out.compression[i]>0;out.wheelSurface[i]=*reinterpret_cast<unsigned*>(wheel+0xA0);}
  auto wpos=*reinterpret_cast<BYTE**>(sim+0x2C);if(!wpos)return false;
  out.worldFlags=*reinterpret_cast<unsigned*>(wpos+0x30);out.worldSurface=*reinterpret_cast<unsigned*>(wpos+0x38);out.nullSurface=*reinterpret_cast<unsigned*>(0x988E84);out.defaultSurface=*reinterpret_cast<unsigned*>(0x988E18);
  for(float x:out.up)if(!std::isfinite(x))return false;
  for(float x:out.normal)if(!std::isfinite(x))return false;
  return std::isfinite(out.flipped);
 }__except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
#endif
