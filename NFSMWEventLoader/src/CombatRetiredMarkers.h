#pragma once
namespace combat_markers {
// FEng FEColor is four signed INTEGER channels (0..255), not RGBA floats.
// Verified against the native packed-color decoder at 005B2D50.
using Channels=std::array<int,4>;
struct Tint {unsigned icon=0,guid=0,name=0;Channels baseline{},written{};};
std::array<Tint,16> tints{};thread_local unsigned pending=0;bool installed=false,attempted=false,enabled=false;
using ColorSetter=void(__cdecl*)(unsigned,const int*);
ColorSetter setColor=reinterpret_cast<ColorSetter>(0x515810);
using NativeDraw=void(__thiscall*)(unsigned,const void*,const void*,unsigned,unsigned);
NativeDraw nativeDraw=reinterpret_cast<NativeDraw>(0x5675D0);
int RetiredSlot(unsigned vehicle){
 using namespace combat_health_runtime;unsigned table=0,handle=0;
 if(!enabled||gameThread!=GetCurrentThreadId()||!Read(vehicle,table)||table!=0x8AA828||vehicle<0x10080||!Read(vehicle-0x80+8,handle))return -1;
 const unsigned sim=vehicle-0x80;
 for(unsigned i=0;i<bindings.size();++i){unsigned boundSim=0;const auto& b=bindings[i];
  if(b.driver==3&&b.handle==handle&&ledger.racers[i].retired&&b.retirementSent&&Participant(i,handle)&&Native(b.damage,handle)&&Read(b.damage-0x18,boundSim)&&boundSim==sim)return int(i);
 }return -1;
}
int __cdecl ShowRetired(unsigned vehicle){return RetiredSlot(vehicle)>=0;}
// Preserve the original IsActive call and its return address: NFRR's optional
// encounter hook inspects that exact caller. Only the following branch changes.
__declspec(naked) void ActiveGate(){__asm {
 test al,al
 jnz show
 pushad
 push dword ptr [esi]
 call ShowRetired
 add esp,4
 test eax,eax
 jz hide
 popad
 jmp show
 hide:
 popad
 mov dword ptr [esp],057A1A5h
 ret
 show:
 mov ecx,dword ptr [esi]
 ret
}}
const void* __fastcall Capture(unsigned vehicle,void*){
 pending=vehicle;const auto table=*reinterpret_cast<unsigned**>(vehicle);
 return reinterpret_cast<const void*(__thiscall*)(unsigned)>(table[1])(vehicle);
}
bool Identity(const Tint& t){unsigned guid=0,name=0;return t.icon&&combat_health_runtime::Read(t.icon+8,guid)&&combat_health_runtime::Read(t.icon+12,name)&&guid==t.guid&&name==t.name;}
bool Readable(unsigned address,unsigned size){
 if(address<0x10000||!size||address+size<address)return false;
 auto at=uintptr_t(address);const auto end=at+size;
 while(at<end){MEMORY_BASIC_INFORMATION page{};
  if(!VirtualQuery(reinterpret_cast<const void*>(at),&page,sizeof(page))||page.State!=MEM_COMMIT||(page.Protect&(PAGE_NOACCESS|PAGE_GUARD)))return false;
  const auto access=page.Protect&0xff;
  if(access!=PAGE_READONLY&&access!=PAGE_READWRITE&&access!=PAGE_WRITECOPY&&access!=PAGE_EXECUTE_READ&&access!=PAGE_EXECUTE_READWRITE&&access!=PAGE_EXECUTE_WRITECOPY)return false;
  const uintptr_t next=reinterpret_cast<uintptr_t>(page.BaseAddress)+static_cast<uintptr_t>(page.RegionSize);if(next<=at)return false;at=std::min(end,next);
 }return true;
}
bool Color(unsigned icon,Channels& color){
 unsigned data=0;if(!combat_health_runtime::Read(icon+0x2C,data)||!Readable(data,16)||!Copy(reinterpret_cast<void*>(data),color.data(),16))return false;
 for(int value:color)if(value<0||value>255)return false;return true;
}
void Restore(Tint& t){
 // Native results/retry can release the HUD package before staging resets our
 // borrowed tint records. An unmapped former icon is no longer our property;
 // never probe it through a deliberate AV/SEH just to discover that fact.
 // Live/reused objects still require their original GUID/name and exact tint.
 Channels color{};if(Readable(t.icon,0x30)&&Identity(t)&&Color(t.icon,color)&&color==t.written)setColor(t.icon,t.baseline.data());t={};
}
void Reset(){for(auto& t:tints)Restore(t);pending=0;enabled=false;}
void __fastcall Draw(unsigned map,void*,const void* position,const void* heading,unsigned icon,unsigned flag){
 const unsigned vehicle=pending;pending=0;
 const int slot=RetiredSlot(vehicle);
 for(auto& t:tints)if(t.icon==icon&&(slot<0||!Identity(t)))Restore(t);
 nativeDraw(map,position,heading,icon,flag);
 if(slot<0)return;
 auto& tint=tints[unsigned(slot)];if(tint.icon!=icon)Restore(tint);
 if(!tint.icon){Channels color{};unsigned guid=0,name=0;
  if(!Color(icon,color)||!combat_health_runtime::Read(icon+8,guid)||!combat_health_runtime::Read(icon+12,name))return;
  tint={icon,guid,name,color,color};tint.written[0]=tint.written[1]=tint.written[2]=255;
  if(worldLogEnabled)Log("COMBAT_RETIRED_RADAR_WHITE slot=%u handle=%08X icon=%08X native_visibility_preserved=1",unsigned(slot),combat_health_runtime::bindings[slot].handle,icon);
 }
 setColor(icon,tint.written.data());
}
bool Install(){
 const BYTE gate[]={0x84,0xC0,0x74,0x37,0x8B,0x0E},capture[]={0x8B,0x01,0xFF,0x50,0x04};
 BYTE observed[6]{},oldDraw[5]{};int displacement=0;
 if(!Copy(reinterpret_cast<void*>(0x57A16A),observed,6)||memcmp(observed,gate,6)||!Copy(reinterpret_cast<void*>(0x57A170),observed,5)||memcmp(observed,capture,5)||!Copy(reinterpret_cast<void*>(0x57A19C),oldDraw,5)||oldDraw[0]!=0xE8)return false;
 memcpy(&displacement,oldDraw+1,4);if(0x57A1A1+displacement!=0x5675D0)return false;
 BYTE color[4]{};
 // 00515810 is the native integer-channel color setter, not the SDK's misleading
 // five-component declaration of the packed-color convenience wrapper.
 const BYTE colorSetter[]={0x8B,0x4C,0x24,0x04};
 if(!Copy(reinterpret_cast<void*>(0x515810),color,4)||memcmp(color,colorSetter,4))return false;
 struct Patch {unsigned address,length;void* target;BYTE old[6];};
 Patch patches[]={{0x57A16A,6,ActiveGate,{}},{0x57A170,5,Capture,{}},{0x57A19C,5,Draw,{}}};unsigned changed=0;
 for(auto& p:patches){DWORD protect=0;if(!Copy(reinterpret_cast<void*>(p.address),p.old,p.length)||!VirtualProtect(reinterpret_cast<void*>(p.address),p.length,PAGE_EXECUTE_READWRITE,&protect))break;
  BYTE code[6]={0xE8,0,0,0,0,0x90};const int offset=int(unsigned(p.target)-p.address-5);memcpy(code+1,&offset,4);memcpy(reinterpret_cast<void*>(p.address),code,p.length);DWORD ignored=0;VirtualProtect(reinterpret_cast<void*>(p.address),p.length,protect,&ignored);FlushInstructionCache(GetCurrentProcess(),reinterpret_cast<void*>(p.address),p.length);++changed;
 }
 if(changed==std::size(patches))return true;
 for(unsigned i=0;i<changed;++i){auto& p=patches[i];DWORD protect=0;if(VirtualProtect(reinterpret_cast<void*>(p.address),p.length,PAGE_EXECUTE_READWRITE,&protect)){memcpy(reinterpret_cast<void*>(p.address),p.old,p.length);DWORD ignored=0;VirtualProtect(reinterpret_cast<void*>(p.address),p.length,protect,&ignored);FlushInstructionCache(GetCurrentProcess(),reinterpret_cast<void*>(p.address),p.length);}}
 return false;
}
void Frame(){
 const bool active=combat_health_runtime::ledger.event&&combat_health_runtime::Active();
 if(!active){if(enabled)Reset();return;}
 if(!attempted){attempted=true;installed=Install();if(worldLogEnabled)Log("COMBAT_RETIRED_RADAR installed=%u encounter_IsActive_call_untouched=1",unsigned(installed));}
 enabled=installed;
}
}
