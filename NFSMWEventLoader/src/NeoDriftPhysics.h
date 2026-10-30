#pragma once
#include <windows.h>
#include <cstdint>
#include <cmath>
namespace mwel_drift {
// Event-owned suspension override. Native simulated-body mode alone does not
// change SuspensionSimple into SuspensionRacer. Call only from OnDriving,
// never while iterating suspension/physics or while holding the AI API lock.
struct SuspensionLease {
 using U=std::uint32_t;
 U vehicle=0,handle=0,key=0,desired=0,previousValue=0,previousVtable=0;
 bool previousPresent=false,active=false,failed=false;
 template<class T>static bool Read(U a,T& v){SIZE_T n=0;return a>=0x10000&&a<=UINT32_MAX-sizeof(T)&&ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(a),&v,sizeof(v),&n)&&n==sizeof(v);}
 static bool Identity(U v,U h){U vt=0,sim=0,actual=0;return Read(v,vt)&&vt==0x8aa828&&Read(v-0x80,sim)&&sim==0x8aa940&&Read(v-0x78,actual)&&actual==h;}
 static bool Member(U v){U list=0,count=0;if(!Read(0x92cd1c,list)||!Read(0x92cd24,count)||!list||count>256)return false;for(U i=0;i<count;++i){U current=0;if(Read(list+i*4,current)&&current==v)return true;}return false;}
 static U Vtable(U v){U s=0,vt=0;return Read(v+0x44,s)&&Read(s,vt)?vt:0;}
 static bool SteeringLock(U v,float& radians){
  U suspension=0,vt=0,getter=0;
  if(!Read(v+0x44,suspension)||!Read(suspension,vt)||!Read(vt+27*4,getter)||
     !((vt==0x8abac8&&getter==0x6a9700)||(vt==0x8abc30&&getter==0x6ac410)))return false;
  // Both verified getters return turns using the same 0089095C factor;
  // Player reads +d0, Racer +dc. Keep the class-specific getters intact.
  const float measured=reinterpret_cast<float(__thiscall*)(void*)>(getter)(reinterpret_cast<void*>(suspension))*6.28318530718f;
  if(!std::isfinite(measured)||measured<=.1f||measured>=1.5f)return false;
  radians=measured;return true;
 }
 static bool Methods(U v){U vt=0,a=0,b=0,c=0;return Read(v,vt)&&Read(vt+16,a)&&a==0x6857a0&&Read(vt+20,b)&&b==0x683500&&Read(vt+24,c)&&c==0x688370;}
 // Exact read-only tree layout verified against native find 0067D350 and
 // mapped-value accessor 006847D0. Preserve absence as well as the old value.
 static bool Override(U v,U key,bool& present,U& value){
  present=false;value=0;U sentinel=0,node=0;if(!Read(v+0xc0,sentinel)||!Read(sentinel+4,node))return false;
  for(unsigned depth=0;depth<64;++depth){
   unsigned char nil=0;U current=0;if(!Read(node+0x15,nil))return false;
   if(nil||node==sentinel)return true;
   if(!Read(node+0xc,current))return false;
   if(current==key){present=true;return Read(node+0x10,value);}
   U next=0;if(!Read(node+(key<current?0:8),next)||next==node)return false;node=next;
  }return false;
 }
 static void Set(U v,U key,U value){reinterpret_cast<void(__thiscall*)(void*,U,U)>(0x6857a0)(reinterpret_cast<void*>(v),key,value);}
 static void Remove(U v,U key){reinterpret_cast<void(__thiscall*)(void*,U)>(0x683500)(reinterpret_cast<void*>(v),key);}
 static void Commit(U v){reinterpret_cast<void(__thiscall*)(void*)>(0x688370)(reinterpret_cast<void*>(v));}
 // 0 no lease / vehicle retired, 1 exact restoration, 2 another owner changed
 // the override (left intact), 3 native restoration could not be verified.
 unsigned Restore(){
  unsigned result=0;
  if(active&&Member(vehicle)&&Identity(vehicle,handle)){
   if(!Methods(vehicle))return 3;
   bool present=false;U current=0;
   if(!Override(vehicle,key,present,current))result=3;
   else if(!present||current!=desired)result=2;
   else {
    if(previousPresent)Set(vehicle,key,previousValue);else Remove(vehicle,key);
    Commit(vehicle);bool restored=false;U value=0;
    result=Override(vehicle,key,restored,value)&&restored==previousPresent&&(!restored||value==previousValue)&&Vtable(vehicle)==previousVtable?1:3;
   }
  }
  if(result!=3)*this={};return result;
 }
 // 0 ready without a change, 1 native suspension replaced (resample all
 // interfaces next tick), 2 refused. Refusals latch for this vehicle identity.
 unsigned Ensure(U v,U h){
  if(vehicle!=v||handle!=h){if(Restore()==3)return 2;vehicle=v;handle=h;}
  if(failed||!Member(v)||!Identity(v,h)||!Methods(v)){failed=true;return 2;}
  if(active){bool p=false;U value=0;if(Override(v,key,p,value)&&p&&value==desired&&Vtable(v)==0x8abac8)return 0;failed=true;return 2;}
  previousVtable=Vtable(v);if(previousVtable==0x8abac8)return 0;
  if(previousVtable!=0x8abc30||!Read(0x92c658,key)||!key||!Read(0x93823c,desired)||!desired||!Override(v,key,previousPresent,previousValue)){failed=true;return 2;}
  U beforeBody=0,afterBody=0,beforeAI=0,afterAI=0;
  if(!Read(v-0x34,beforeBody)||!Read(v+0x54,beforeAI)){failed=true;return 2;}
  active=true;Set(v,key,desired);Commit(v);
  bool p=false;U value=0;
  if(Identity(v,h)&&Vtable(v)==0x8abac8&&Read(v-0x34,afterBody)&&afterBody==beforeBody&&Read(v+0x54,afterAI)&&afterAI==beforeAI&&Override(v,key,p,value)&&p&&value==desired)return 1;
  Restore();vehicle=v;handle=h;failed=true;return 2;
 }
};
}
