#pragma once
namespace rpd_undercover_strobe {
using combat_health_runtime::Read;
using combat_impact::Vector;
constexpr unsigned Red=0x1E4150B4, Blue=0xE662C161;
constexpr unsigned RedBright=0x41489594, BlueBright=0x6A52A241;
struct Flare {
 Flare* next=nullptr;
 Flare* previous=nullptr;
 unsigned name=0, color=0;
 Vector position{};
 float reflectedZ=0;
 Vector direction{};
 BYTE type=0, flags=0;
 short section=0;
};
static_assert(sizeof(Flare)==0x30 && offsetof(Flare,type)==0x2C);
using RenderFn=void(__cdecl*)(unsigned,int,int);
RenderFn originalRender=nullptr;
unsigned officer=0;
bool attempted=false, reported=false;
bool Anchors(Flare* sentinel,Flare* (&anchors)[4]){
 constexpr unsigned keys[]={0x9DB90133,0xD09091C6,0x31A66786,0xBF700A79};
 Flare* node=nullptr;
 if(!Read(unsigned(sentinel),node))return false;
 for(unsigned i=0;node!=sentinel&&i<128;++i){
  Flare row{};
  if(!Read(unsigned(node),row)||row.next==node)return false;
  if(row.name==Red||row.name==Blue||row.name==RedBright||row.name==BlueBright)return false;
  for(unsigned j=0;j<4;++j){
   if(!anchors[j]&&row.name==keys[j]&&combat_impact::Finite(row.position)&&
      std::abs(row.position.x)<8&&std::abs(row.position.y)<4&&std::abs(row.position.z)<4)
    anchors[j]=node;
  }
  node=row.next;
 }
 return node==sentinel&&anchors[0]&&anchors[1]&&anchors[2]&&anchors[3];
}

unsigned OfficerRenderInfo(){
 unsigned render=0, modelId=0, table=0, data=0, count=0;
 if(!Read(officer+0x5C,render)||render<0x1004C||!Read(render,table)||table!=0x8ADCC8||
    !Read(render+0x10,modelId)||!modelId||!Read(0x9B37CC,data)||!data||
    !Read(0x9B37D4,count)||count>64)return 0;
 const unsigned model=render-0x4C;
 for(unsigned i=0;i<count;++i){
  unsigned conn=0, owner=0, state=0, info=0;
  if(Read(data+4*i,conn)&&Read(conn,table)&&table==0x8B5254&&Read(conn+8,owner)&&owner==model&&
     Read(conn+0x28,state)&&state==3&&Read(conn+0x44,info))return info;
 }
 return 0;
}

void __cdecl Render(unsigned view,int reflection,int flags){
 if(!officer||GetCurrentThreadId()!=combat_health_runtime::gameThread){
  originalRender(view,reflection,flags);
  return;
 }
 const auto info=OfficerRenderInfo();
 auto sentinel=reinterpret_cast<Flare*>(info+0x104);
 Flare* anchors[4]{};
 Flare* tail=nullptr;Flare* tailNext=nullptr;
 unsigned lights=0;
 if(!info||!Read(unsigned(sentinel)+4,tail)||!Read(unsigned(tail),tailNext)||tailNext!=sentinel||
    !Read(info+0x1620,lights)||!Anchors(sentinel,anchors)){
  originalRender(view,reflection,flags);
  return;
 }
 Flare emitters[4]{};
 for(unsigned i=0;i<4;++i){
  auto& flare=emitters[i];
  const bool blue=(i%2)!=0;
  flare.name=blue?Blue:Red;
  flare.type=blue?6:5;
  flare.flags=2;
  flare.position=anchors[i]->position;
  flare.position.x+=(i<2?.18f:-.18f);
  flare.position.z+=.08f;
  flare.direction={i<2?1.f:-1.f,0,0};
  flare.previous=i?&emitters[i-1]:tail;
  flare.next=i==3?sentinel:&emitters[i+1];
 }
 auto& onLights=*reinterpret_cast<unsigned*>(info+0x1620);
 tail->next=emitters;
 sentinel->previous=&emitters[3];
 onLights=lights|0x3000;
 __try{originalRender(view,reflection,flags);}
 __finally{
  onLights=lights;
  tail->next=sentinel;
  sentinel->previous=tail;
 }
 if(worldLogEnabled&&!reported){
  reported=true;
  Log("RPD_UNDERCOVER_STROBE native_flare_types=5,6 front=2 rear=2 render_info=%08X",info);
 }
}
void Set(unsigned vehicle){
 officer=vehicle;
 if(!vehicle||attempted)return;
 attempted=true;
 const BYTE entry[]={0x55,0x8B,0xEC,0x83,0xE4,0xF0,0x83,0xEC,0x74};
 auto site=reinterpret_cast<void*>(0x744E20);
 if(memcmp(site,entry,sizeof(entry))||MH_CreateHook(site,Render,reinterpret_cast<void**>(&originalRender))!=MH_OK)return;
 if(MH_EnableHook(site)!=MH_OK){MH_RemoveHook(site);originalRender=nullptr;return;}

}
}
