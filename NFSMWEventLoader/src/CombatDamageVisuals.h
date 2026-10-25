#pragma once
// Native world particles only: no D3D overlay, additional source-game asset,
// vehicle-state mutation, or shared particle/attribute collection edits.
namespace combat_visuals {
using combat_impact::Vector;
// Stock police wreck smoke: two smoke emitters, no flame/explosion or damage call.
constexpr unsigned PoliceSmokeKey=0x3AEB075D;
struct Anchor {alignas(16) std::array<BYTE,80> record{};unsigned handle=0;float began=-100,retry=0,diagnosticAt=0;bool observed=false;};
struct Car {Anchor spark,smoke;};
std::array<Car,16> cars{};bool guardsReady=false,attempted=false;unsigned epoch=0;float last=-100;
// IRenderable belongs to VehicleFX at +4C. CarRenderConn+8 points to that
// VehicleFX object; IRenderable+10 is a separate integer HMODEL generation.
// Observe completed loads once; never reinterpret that integer as a pointer.
struct Connection {unsigned model=0,conn=0,info=0,id=0;Vector local;bool markers=false;};
std::array<Connection,128> connections{};unsigned connectionCursor=0;
SRWLOCK connectionLock=SRWLOCK_INIT;
using LoadFn=void(__thiscall*)(unsigned,unsigned);LoadFn originalLoad=nullptr;
bool loadInstalled=false;
bool LightPair(unsigned info,Vector& local);
void __fastcall Loaded(unsigned conn,void*,unsigned info){
 originalLoad(conn,info);unsigned table=0,model=0,actual=0,id=0;
 if(!combat_health_runtime::Read(conn,table)||table!=0x8B5254||!combat_health_runtime::Read(conn+8,model)||!model||!combat_health_runtime::Read(conn+0x44,actual)||actual!=info)return;
 if(!combat_health_runtime::Read(model+0x5C,id)||!id)return;
 Connection loaded{model,conn,info,id};loaded.markers=LightPair(info,loaded.local);
 AcquireSRWLockExclusive(&connectionLock);
 auto target=&connections[connectionCursor++%connections.size()];
 for(auto& entry:connections)if(entry.model==model){target=&entry;break;}
 *target=loaded;ReleaseSRWLockExclusive(&connectionLock);
 if(worldLogEnabled)Log("COMBAT_HEADLIGHT_MODEL model=%08X id=%u pair=%u local=%.3f,%.3f,%.3f",model,id,unsigned(loaded.markers),loaded.local.x,loaded.local.y,loaded.local.z);
}
bool Bytes(unsigned p,const BYTE* expected,unsigned size){BYTE bytes[16]{};return size<=sizeof(bytes)&&Copy(reinterpret_cast<void*>(p),bytes,size)&&!memcmp(bytes,expected,size);}
bool Install(){
 if(loadInstalled)return true;
 const BYTE expected[]={0x55,0x8B,0xEC,0x83,0xE4,0xF0,0x83,0xEC,0x18,0x56,0x57};
 if(!Bytes(0x738A10,expected,sizeof(expected)))return false;
 auto address=reinterpret_cast<void*>(0x738A10);
 if(MH_CreateHook(address,Loaded,reinterpret_cast<void**>(&originalLoad))!=MH_OK)return false;
 if(MH_EnableHook(address)!=MH_OK){MH_RemoveHook(address);originalLoad=nullptr;return false;}
 loadInstalled=true;return true;
}
bool HeadlightLocal(Vector left,Vector right,Vector& out){
 if(!combat_impact::Finite(left)||!combat_impact::Finite(right))return false;
 const auto separation=left-right;const float span=combat_impact::Dot(separation,separation);
 auto center=(left+right)*.5f;const float length=std::sqrt(center.x*center.x+center.y*center.y);
 if(span<.04f||span>25||std::abs(left.x-right.x)>1.5f||std::abs(left.z-right.z)>1||length<.5f||length>12||std::abs(center.z)>3)return false;
 // Move from the lights toward the body center; this handles either model-X
 // convention and asymmetric kits without assuming which sign means forward.
 out=center-Vector{center.x,center.y,0}*(.35f/length)+Vector{0,0,.05f};return true;
}
bool LightPair(unsigned info,Vector& local){
 using combat_health_runtime::Read;const unsigned sentinel=info+0x104;unsigned node=0,prior=sentinel;
 Vector left{},right{};bool haveLeft=false,haveRight=false;
 if(!Read(sentinel,node))return false;
 for(unsigned count=0;node!=sentinel&&count<128;++count){unsigned next=0,prev=0,key=0;
  if(!Read(node,next)||!Read(node+4,prev)||prev!=prior||next==node||!Read(node+8,key))return false;
  if(key==0x9DB90133||key==0xD09091C6){Vector p;if(!Read(node+0x10,p)||!combat_impact::Finite(p))return false;
   if(key==0x9DB90133&&!haveLeft){left=p;haveLeft=true;}else if(key==0xD09091C6&&!haveRight){right=p;haveRight=true;}}
  if(haveLeft&&haveRight)return HeadlightLocal(left,right,local);
  prior=node;node=next;
 }return false;
}
bool HeadlightAnchor(unsigned sim,const combat_impact::Motion& motion,Vector& world){
 using combat_health_runtime::Read;unsigned render=0,table=0,model=0;
 unsigned id=0;if(!Read(sim+0xDC,render)||!Read(render,table)||table!=0x8ADCC8||render<0x1004C||!Read(render+0x10,id)||!id)return false;model=render-0x4C;
 Connection found{};if(!TryAcquireSRWLockShared(&connectionLock))return false;
 for(const auto& entry:connections)if(entry.model==model){found=entry;break;}
 ReleaseSRWLockShared(&connectionLock);
 unsigned actual=0,identity=0;if(!found.conn||found.id!=id||!found.markers||!Read(found.conn,table)||table!=0x8B5254||!Read(found.conn+8,identity)||identity!=model||!Read(found.conn+0x44,actual)||actual!=found.info)return false;
 const auto local=found.local;
 std::array<float,16> matrix{};if(!Read(found.conn+0x330,matrix))return false;
 for(float value:matrix)if(!std::isfinite(value))return false;
 const Vector origin{matrix[12],matrix[13],matrix[14]},physical{motion.position.z,-motion.position.x,motion.position.y};
 if(combat_impact::Dot(origin-physical,origin-physical)>100||std::abs(matrix[15]-1)>.05f)return false;
 for(unsigned row=0;row<3;++row){const Vector axis{matrix[row*4],matrix[row*4+1],matrix[row*4+2]};const float length=combat_impact::Dot(axis,axis);if(length<.5f||length>1.5f)return false;}
 world=origin+Vector{matrix[0],matrix[1],matrix[2]}*local.x+Vector{matrix[4],matrix[5],matrix[6]}*local.y+Vector{matrix[8],matrix[9],matrix[10]}*local.z;
 return combat_impact::Finite(world);
}
bool Guards(){
 const BYTE create[]={0xA0,0xEC,0x86,0x8F,0,0x84,0xC0,0x56,0x8B,0xF1};
 const BYTE matrix[]={0x53,0x8B,0x5C,0x24,8,0x56,0x57,0x8B,0xF1};
 const BYTE destroy[]={0x6A,0xFF,0x68,0x2B,0xDF,0x86,0};
 const BYTE freeGroup[]={0x8B,0x44,0x24,4,0x8B,0x48,0x18,0x8B,0x54,0x24,8,0x56};
 const BYTE clear[]={0x8B,0x44,0x24,4,0xC7,0x40,0xC,0,0,0,0,0xC3};
 const BYTE velocity[]={0x8B,0x41,8,0x8D,0x51,8,0x3B,0xC2};
 const BYTE restart[]={0x8B,0x41,8,0x8D,0x51,8,0x53,0x32,0xDB};
 return Bytes(0x50CC20,create,sizeof(create))&&Bytes(0x503BC0,matrix,sizeof(matrix))&&Bytes(0x5094C0,destroy,sizeof(destroy))&&Bytes(0x463CA0,freeGroup,sizeof(freeGroup))&&Bytes(0x4FDFD0,clear,sizeof(clear))&&Bytes(0x503C00,velocity,sizeof(velocity))&&Bytes(0x503B60,restart,sizeof(restart));
}
unsigned Group(const Anchor& a){unsigned result=0;memcpy(&result,a.record.data()+12,4);return result;}
bool Owned(const Anchor& a,unsigned group){unsigned owner=0,callback=0;return group&&combat_health_runtime::Read(group+0x60,owner)&&owner==unsigned(a.record.data())&&combat_health_runtime::Read(group+0x6C,callback)&&callback==0x4FDFD0;}
void Release(Anchor& a){
 const auto group=Group(a);if(!group){a.handle=0;return;}
 unsigned pool=0;if(!guardsReady||!Owned(a,group)||!combat_health_runtime::Read(0x916068,pool)||!pool){a.record.fill(0);a.handle=0;return;}
 reinterpret_cast<void(__thiscall*)(unsigned)>(0x5094C0)(group);
 if(!Group(a))reinterpret_cast<void(__cdecl*)(unsigned,unsigned)>(0x463CA0)(pool,group);
 a.handle=0;
}
void Reset(){
 for(auto& car:cars){
  Release(car.spark);Release(car.smoke);
  // Release native owners before clearing their callback records. Simulation
  // time restarts between events: an old retry deadline must not suppress the
  // next event's sparks/smoke until the previous race duration has elapsed.
  car={};
 }
 epoch=0;last=-100;
}
Vector RenderPoint(Vector v){return {v.z,-v.x,v.y};}
void Matrix(const combat_impact::Motion& motion,float* out){
 const auto forward=motion.forward;const Vector right{forward.z,0,-forward.x};
 const auto p=RenderPoint(motion.position+forward*(motion.halfLength*.8f)+Vector{0,std::clamp(motion.halfHeight*.25f,.1f,.35f),0});
 const auto r=RenderPoint(right),f=RenderPoint(forward);
 // Native 45E1A0 maps render -> physics as (-Y,Z,X). Particle matrices
 // use the inverse (Z,-X,Y), including translation, not physics coordinates.
 const float matrix[16]={r.x,r.y,r.z,0,f.x,f.y,f.z,0,0,0,1,0,p.x,p.y,p.z,1};memcpy(out,matrix,sizeof(matrix));
}
bool EngineMatrix(unsigned sim,const combat_impact::Motion& motion,float* out){
 Matrix(motion,out);Vector anchor{};
 if(!sim||!HeadlightAnchor(sim,motion,anchor))return false;
 out[12]=anchor.x;out[13]=anchor.y;out[14]=anchor.z;return true;
}
bool Update(Anchor& a,unsigned handle,unsigned key,const float* matrix,const Vector& velocity,float now,bool allowCreate){
 if(a.handle&&a.handle!=handle)Release(a);
 if(auto group=Group(a)){
  if(!Owned(a,group)){a.record.fill(0);a.handle=0;return false;}
  reinterpret_cast<void(__thiscall*)(unsigned,const float*)>(0x503BC0)(group,matrix);
  reinterpret_cast<void(__thiscall*)(unsigned,const Vector*)>(0x503C00)(group,&velocity);
  if(worldLogEnabled&&(!a.observed||(key==PoliceSmokeKey&&now>=a.diagnosticAt))&&now-a.began>(key==PoliceSmokeKey?1.5f:.3f)){
   a.observed=true;a.diagnosticAt=now+3;unsigned node=0;unsigned particles=0,seen=0;combat_health_runtime::Read(group+8,node);
   for(;node&&node!=group+8&&seen<16;++seen){unsigned short count=0;unsigned next=0;if(!combat_health_runtime::Read(node+0x1C,count)||!combat_health_runtime::Read(node,next))break;particles+=count;node=next;}
   Log("COMBAT_DAMAGE_FX_PARTICLES handle=%08X key=%08X particles=%u emitters=%u render=%.2f,%.2f,%.2f",handle,key,particles,seen,matrix[12],matrix[13],matrix[14]);
  }return true;
 }
 if(!allowCreate||now<a.retry)return false;a.retry=now+.75f;
 unsigned pool=0,groups=0,emitters=0;BYTE enabled=0;
 if(!combat_health_runtime::Read(0x8F86EC,enabled)||!enabled||!combat_health_runtime::Read(0x916068,pool)||!pool||!combat_health_runtime::Read(0x916A50,groups)||groups>120||!combat_health_runtime::Read(0x916A54,emitters)||emitters>360)return false;
 a.record.fill(0);memcpy(a.record.data(),&key,4);const unsigned short section=2600;memcpy(a.record.data()+8,&section,2);memcpy(a.record.data()+16,matrix,64);
 const auto group=reinterpret_cast<unsigned(__thiscall*)(unsigned,void*,unsigned)>(0x50CC20)(0x9166C0,a.record.data(),0x08000000);
 if(!group)return false;
 const unsigned owner=unsigned(a.record.data()),callback=0x4FDFD0;memcpy(reinterpret_cast<void*>(group+0x60),&owner,4);memcpy(reinterpret_cast<void*>(group+0x6C),&callback,4);memcpy(a.record.data()+12,&group,4);
 reinterpret_cast<void(__thiscall*)(unsigned,const float*)>(0x503BC0)(group,matrix);unsigned flags=0;combat_health_runtime::Read(group+0x18,flags);flags|=1;memcpy(reinterpret_cast<void*>(group+0x18),&flags,4);
 // Native 50AF90 emits only while bit 0x10 is SET. Keep the constructor's
 // enabled state; 503C50 clears it and would silently stop emission.
 reinterpret_cast<void(__thiscall*)(unsigned,const Vector*)>(0x503C00)(group,&velocity);
 reinterpret_cast<bool(__thiscall*)(unsigned,bool)>(0x503B60)(group,key!=PoliceSmokeKey);
 a.handle=handle;a.began=now;a.observed=false;
 if(worldLogEnabled){unsigned short emitters=0;combat_health_runtime::Read(group+0x1C,emitters);Log("COMBAT_DAMAGE_FX_CREATE handle=%08X group=%08X key=%08X emitters=%u emitting=%u render=%.2f,%.2f,%.2f native_world_depth=1",handle,group,key,unsigned(emitters),unsigned((flags&0x10)!=0),matrix[12],matrix[13],matrix[14]);}
 return true;
}
void Frame(){
 using namespace combat_health_runtime;
 if(!ledger.event||!Active()||!Racing()){if(epoch)Reset();return;}
 if(!attempted){attempted=true;guardsReady=Guards();if(worldLogEnabled)Log("COMBAT_DAMAGE_FX_GUARDS ready=%u native_only=1",unsigned(guardsReady));}
 if(!guardsReady)return;
 if(epoch!=generation){Reset();epoch=generation;}
 float now=0;if(!Read(0x9885D8,now)||!std::isfinite(now)||now==last)return;last=now;
 combat_impact::Motion player{};for(unsigned i=0;i<bindings.size();++i)if(bindings[i].driver==0&&motions[i].valid){player=motions[i];break;}
 unsigned created=0;
 for(unsigned i=0;i<cars.size();++i){auto& car=cars[i];const auto& binding=bindings[i];const auto& hp=ledger.racers[i];const auto& motion=motions[i];
  const unsigned zone=combat_impact::VisualZone(hp.remaining,hp.maximum);
  const bool nearby=player.valid&&motion.valid&&combat_impact::Dot(motion.position-player.position,motion.position-player.position)<160.f*160.f;
  if(!binding.damage||!zone||!nearby||!Participant(i,binding.handle)||!Native(binding.damage,binding.handle)||motion.handle!=binding.handle){Release(car.spark);Release(car.smoke);continue;}
  // Resolve once per car/frame: yellow sparks and red smoke use exactly the
  // same hood position, including the dimension fallback without light markers.
  alignas(16) float matrix[16]{};unsigned sim=0;Read(binding.damage-0x18,sim);
  const bool marker=EngineMatrix(sim,motion,matrix);const auto velocity=RenderPoint(motion.velocity);
  if(hp.retired){Release(car.spark);}else{
   if(Group(car.spark)&&now-car.spark.began>.6f){Release(car.spark);car.spark.retry=now+.75f;}
   const bool absent=!Group(car.spark);if(Update(car.spark,binding.handle,0xF991F8E8,matrix,velocity,now,created<1)&&absent)++created;
  }
  if(zone>=2){
   const bool absent=!Group(car.smoke);if(Update(car.smoke,binding.handle,PoliceSmokeKey,matrix,velocity,now,created<1)&&absent){++created;if(worldLogEnabled)Log("COMBAT_SMOKE_ANCHOR handle=%08X headlights=%u model_load_hook=%u",binding.handle,unsigned(marker),unsigned(loadInstalled));}}
  else Release(car.smoke);
 }
}
}
