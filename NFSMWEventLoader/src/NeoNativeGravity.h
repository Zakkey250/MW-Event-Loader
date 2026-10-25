#pragma once
// Read-only EN1.3 SDK surface: IRigidBody -> IUnknown::_mCOMObject ->
// ICollisionBody (GetIHandle=0x4039C0), virtual GetGravity slot 37.
// The raw native value is diagnostic until its units are confirmed.
struct NeoNativeDynamics {float gravity=0,mass=0,force[3]{},halfHeight=0,position[3]{};unsigned getter=0,forceGetter=0,positionGetter=0,body=0,positionPointer=0;int contacts=-1,physicsMode=-1;unsigned collisionFlags=0;bool forceRead=false;};
static bool NeoReadNativeGravity(BYTE* vehicle,NeoNativeDynamics& out){
 auto& raw=out.gravity;auto& getter=out.getter;
 raw=0;getter=0;
 __try {
  const BYTE handleCode[]={0xB8,0xC0,0x39,0x40,0x00,0xC3};
  if(memcmp(reinterpret_cast<void*>(0x4039C0),handleCode,sizeof(handleCode))||!vehicle||*reinterpret_cast<unsigned*>(vehicle)!=0x8AA828)return false;
  const BYTE modeGetter[]={0x8B,0x81,0xAC,0,0,0,0xC3};auto vehicleTable=*reinterpret_cast<void***>(vehicle);
  if(unsigned(vehicleTable[14])==0x688210&&!memcmp(vehicleTable[14],modeGetter,sizeof(modeGetter)))out.physicsMode=reinterpret_cast<int(__thiscall*)(void*)>(vehicleTable[14])(vehicle);
  BYTE* entity=vehicle-0x80;auto et=*reinterpret_cast<void***>(entity);
  auto body=reinterpret_cast<BYTE*(__thiscall*)(void*)>(et[21])(entity);if(!body)return false;
  auto object=*reinterpret_cast<BYTE**>(body+4);if(!object)return false;
  auto begin=*reinterpret_cast<BYTE**>(object+4),end=*reinterpret_cast<BYTE**>(object+8);
  if(!begin||end<begin||end-begin>8192||(end-begin)%8)return false;
  for(auto pair=begin;pair<end;pair+=8)if(*reinterpret_cast<unsigned*>(pair)==0x4039C0){
   auto collisionBody=*reinterpret_cast<BYTE**>(pair+4);if(!collisionBody)return false;
   auto table=*reinterpret_cast<void***>(collisionBody);getter=unsigned(table[37]);
   if(getter<0x401000||getter>=0x870000)return false;
   raw=reinterpret_cast<float(__thiscall*)(void*)>(table[37])(collisionBody);
   auto bt=*reinterpret_cast<void***>(body);out.body=unsigned(body);out.positionGetter=unsigned(bt[8]);
   if(out.positionGetter<0x401000||out.positionGetter>=0x870000)return false;
   auto position=reinterpret_cast<const float*(__thiscall*)(void*)>(bt[8])(body);if(!position)return false;out.positionPointer=unsigned(position);memcpy(out.position,position,12);
   float dimensions[3]{};reinterpret_cast<void(__thiscall*)(void*,float*)>(bt[18])(body,dimensions);if(std::isfinite(dimensions[1])&&dimensions[1]>.1f&&dimensions[1]<5.f)out.halfHeight=dimensions[1];out.mass=reinterpret_cast<float(__thiscall*)(void*)>(bt[6])(body);
   out.forceGetter=unsigned(table[35]);
   if(out.forceGetter<0x401000||out.forceGetter>=0x870000||unsigned(table[31])<0x401000||unsigned(table[31])>=0x870000)return false;
   auto force=reinterpret_cast<const float*(__thiscall*)(void*)>(table[35])(collisionBody);
   if(force){out.collisionFlags=*reinterpret_cast<const unsigned*>(reinterpret_cast<const BYTE*>(force)-4)|*reinterpret_cast<const unsigned*>(reinterpret_cast<const BYTE*>(force)-52);memcpy(out.force,force,12);out.forceRead=std::isfinite(out.mass)&&out.mass>0;for(float v:out.force)out.forceRead=out.forceRead&&std::isfinite(v);}
   out.contacts=reinterpret_cast<int(__thiscall*)(void*)>(table[31])(collisionBody);
   return std::isfinite(raw)&&std::abs(raw)<1000;
  }
 }__except(EXCEPTION_EXECUTE_HANDLER){return false;}
 return false;
}
