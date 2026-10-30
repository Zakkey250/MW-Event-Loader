#pragma once
#include <array>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include "LeaderboardPhase.h"
// Clone the installation's native FEng objects; never enlarge or index past
// the native LeaderBoard's four-entry arrays. The FE package owns all clones.
namespace expanded_leaderboard {
inline bool(*combatActive)()=nullptr;
inline bool(*combatHealth)(unsigned,unsigned,float&,float&)=nullptr;
inline bool(*driftActive)()=nullptr;
inline unsigned(*driftScore)(unsigned)=nullptr;
using Object=unsigned*;
inline unsigned Hash(const char* p){unsigned h=~0u;while(*p)h=h*33+static_cast<unsigned char>(*p++);return h;}
template<class T> T Read(const void* p,unsigned at=0){T value{};__try{if(p)memcpy(&value,static_cast<const BYTE*>(p)+at,sizeof(T));}__except(EXCEPTION_EXECUTE_HANDLER){}return value;}
inline Object Find(Object package,unsigned hash){return reinterpret_cast<Object(__thiscall*)(void*,unsigned)>(0x5B8550)(package,hash);}
inline void Show(Object o,bool visible){if(o&&bool(o[7]&1)==visible)reinterpret_cast<void(__cdecl*)(void*)>(visible?0x514CC0:0x514C70)(o);}
inline unsigned* ParentList(unsigned* list,Object target,unsigned depth=0){
 if(!list||depth>12||list[0]>4096)return nullptr;auto p=reinterpret_cast<Object>(list[1]);
 for(unsigned n=0;p&&n<list[0];++n,p=reinterpret_cast<Object>(p[1])){
  if(p==target)return list;
  if(p[6]==5)if(auto found=ParentList(p+24,target,depth+1))return found;
 }return nullptr;
}
inline void ClearCache(Object object,unsigned depth=0){
 object[22]=0;if(object[6]!=5||depth>12)return;
 auto p=reinterpret_cast<Object>(object[25]);for(unsigned n=0;p&&n<object[24]&&n<4096;++n,p=reinterpret_cast<Object>(p[1]))ClearCache(p,depth+1);
}
inline Object Clone(Object package,Object source,unsigned key){
 if(auto existing=Find(package,key))return existing;
 auto list=ParentList(package+18,source);if(!list)return nullptr;
 auto copy=(*reinterpret_cast<Object(__thiscall**)(Object,bool)>(*source+4))(source,false);if(!copy)return nullptr;
 copy[4]=key;ClearCache(copy);
 // Clone(false) leaves the evaluated FE render state zeroed. Preserve the
 // source's colour, scale and layer before changing only the row translation.
 if(source[11]&&copy[11]&&source[12]>=68&&copy[12]>=68)memcpy(reinterpret_cast<void*>(copy[11]),reinterpret_cast<void*>(source[11]),68);
 auto tail=reinterpret_cast<Object>(list[2]);copy[1]=0;copy[2]=unsigned(tail);
 if(tail)tail[1]=unsigned(copy);else list[1]=unsigned(copy);
 list[2]=unsigned(copy);++list[0];
 // Connect only the new object. Reconnecting the live HUD package overwrites
 // the tachometer's runtime texture handles with its startup placeholders.
 unsigned connector[]={0x8A2CD0,unsigned(package),unsigned(package+26)};
 reinterpret_cast<bool(__thiscall*)(void*,void*)>(0x5BC670)(connector,copy);
 return copy;
}
struct Row {Object name=nullptr,rank=nullptr,back=nullptr;char text[96]{};bool rankWritten=false;};
struct State {Object package=nullptr;void* board=nullptr;std::array<Row,16> rows{};char packageName[64]{};bool ready=false;unsigned error=0;float step=0;unsigned attempts=0,phase=0,mode=0;LeaderboardPhase display;};
inline State state;
inline unsigned RowKey(unsigned row,unsigned part){char key[48];sprintf_s(key,"MWEL_Leaderboard_%u_%u",row,part);return Hash(key);}
inline bool FeaturesVisible(void* board,unsigned long long features){return (Read<unsigned long long>(board,0x18)&features)!=0;}
inline void HideExtra(){if(state.ready)for(unsigned i=4;i<16;++i){Show(state.rows[i].name,false);Show(state.rows[i].rank,false);Show(state.rows[i].back,false);}}
inline void FeaturesChanged(void* board,unsigned long long features){
 // Native HudElement owns only its original objects. Hide borrowed clones
 // synchronously too, even when pause/NIS stops LeaderBoard::Update entirely.
 if(board!=state.board||!state.ready)return;
 auto engine=Read<void*>(reinterpret_cast<void*>(0x91CADC));
 auto live=engine&&state.packageName[0]?reinterpret_cast<Object(__thiscall*)(void*,const char*)>(0x516AF0)(engine,state.packageName):nullptr;
 if(live!=state.package){state={};return;}
 if(!FeaturesVisible(board,features))HideExtra();
}
inline unsigned Reset(){
 unsigned hidden=0;auto engine=Read<void*>(reinterpret_cast<void*>(0x91CADC));
 auto live=engine&&state.packageName[0]?reinterpret_cast<Object(__thiscall*)(void*,const char*)>(0x516AF0)(engine,state.packageName):nullptr;
 if(live&&live==state.package){
  for(unsigned i=4;i<16;++i)for(unsigned part=0;part<3;++part){
   auto object=Find(live,RowKey(i,part));auto expected=part==0?state.rows[i].name:part==1?state.rows[i].rank:state.rows[i].back;
   if(!object||object!=expected)continue;
   // The package owns these clones. Stop/roaming can run while the current
   // FE render traversal still refers to them; synchronous deletion here
   // invalidates those references. Hide now, detach our borrowed pointers,
   // and let the native package destroy its own objects at package unload.
   // A subsequent race reuses the same fixed keys within a still-live package.
   Show(object,false);++hidden;
  }
 }
 state={};return hidden;
}
inline bool Prepare(void* board,Object package){
 auto attempts=state.attempts+1;state={};state.attempts=attempts;state.package=package;state.board=board;state.error=1;
 auto packageName=Read<const char*>(board,0x10);for(unsigned i=0;i<sizeof(state.packageName)-1&&packageName;++i){state.packageName[i]=Read<char>(packageName,i);if(!state.packageName[i])break;}
 const unsigned offsets[]={0x234,0x244,0x264};
 for(unsigned i=0;i<4;++i){state.rows[i]={Read<Object>(board,offsets[0]+i*4),Read<Object>(board,offsets[1]+i*4),Read<Object>(board,offsets[2]+i*4)};if(!state.rows[i].name||!state.rows[i].rank||!state.rows[i].back)return false;}
 float x=0,y=0,x4=0,y4=0;
 auto pos=reinterpret_cast<void(__cdecl*)(void*,float*,float*)>(0x514FD0);
 auto move=reinterpret_cast<void(__cdecl*)(void*,float,float)>(0x524CC0);
 pos(state.rows[0].rank,&x,&y);pos(state.rows[3].rank,&x4,&y4);
 float step=(y4-y)/3;state.step=step;state.error=2;if(!std::isfinite(step)||fabs(step)<5||fabs(step)>80)return false;
 state.error=3;
 for(unsigned i=4;i<16;++i){Object* dst[]={&state.rows[i].name,&state.rows[i].rank,&state.rows[i].back};Object src[]={state.rows[3].name,state.rows[3].rank,state.rows[3].back};
  for(unsigned part=0;part<3;++part){*dst[part]=Clone(package,src[part],RowKey(i,part));if(!*dst[part])return false;
   float sx,sy;pos(src[part],&sx,&sy);
   // The second column starts one row lower, beneath the mirror. The accepted
   // eight-car column keeps the installation's original font and position.
   move(*dst[part],sx-(i>=8?180.f:0.f),sy+(int(i%8)-3+(i>=8?1:0))*step);Show(*dst[part],false);
  }
 }
 state.ready=true;state.error=0;return true;
}
inline const char* Localized(unsigned key,const char* fallback){
 auto text=reinterpret_cast<const char*(__cdecl*)(unsigned)>(0x56BC60)(key);return text?text:fallback;
}
inline unsigned PlayerSlot(void* player,BYTE* race,unsigned count){
 auto vtable=Read<unsigned*>(player);auto get=Read<unsigned>(vtable,0x20);
 auto sim=get?reinterpret_cast<void*(__thiscall*)(void*)>(get)(player):nullptr;auto handle=Read<unsigned>(sim,4);
 for(unsigned i=0;handle&&i<count;++i)if(Read<unsigned>(race,0x18+i*0x194)==handle)return i;
 return 0;
}
inline void Text(Object object,const char* text){reinterpret_cast<void(__cdecl*)(void*,const char*,...)>(0x515D70)(object,"%s",text);}
inline void Format(char(&text)[96],BYTE* race,BYTE* info,unsigned slot,unsigned playerSlot,unsigned mode,bool metric,unsigned phase){
 if(!phase){auto name=Read<const char*>(info,8);for(unsigned j=0;name&&j<sizeof(text)-1;++j){text[j]=Read<char>(name,j);if(!text[j])break;}if(!text[0])sprintf_s(text,"Racer %u",slot+1);return;}
 if(driftActive&&driftActive()&&driftScore){sprintf_s(text,"%u PTS",driftScore(slot));return;}
 if(phase==1&&combatActive&&combatActive()){
  float hp=0,maximum=0;
  if(combatHealth&&combatHealth(slot,Read<unsigned>(info),hp,maximum)&&maximum>0)sprintf_s(text,"HP %.0f%%",double(std::clamp(hp/maximum*100.f,0.f,100.f)));
  else strcpy_s(text,"HP --%");return;
 }
 // Native score includes end-of-race deductions; summing checkpoint speeds
 // would incorrectly omit these. Keep the game's authoritative PointTotal.
 if(Read<BYTE>(info,0x1F)){sprintf_s(text,"%s",Localized(0x532B5186,"Busted"));return;}
 if(Read<BYTE>(info,0x1C)){sprintf_s(text,"%s",Localized(0x5D82DBA2,"K.O."));return;}
 if(Read<BYTE>(info,0x1D)||Read<BYTE>(info,0x1E)||Read<BYTE>(info,0x190)){strcpy_s(text,"DNF");return;}
 if(mode==5){
  float score=Read<float>(info,0x120);if(!std::isfinite(score))score=0;
  if(!metric)score*=Read<float>(reinterpret_cast<void*>(0x8A0294))*Read<float>(reinterpret_cast<void*>(0x8967E0));
  sprintf_s(text,"%.0f %s",double(score),Localized(metric?0x8569A25F:0x8569AB44,metric?"km/h":"mph"));return;
 }
 if(slot==playerSlot){strcpy_s(text,"----");return;}
 float delta=Read<float>(info,0x18)-Read<float>(race,0x18+playerSlot*0x194+0x18);
 float distance=std::abs(delta)*.01f*Read<float>(race,0x4484);if(!metric)distance*=Read<float>(reinterpret_cast<void*>(0x8A029C));
 if(!std::isfinite(distance))distance=0;
 sprintf_s(text,"%c%.0f %s",delta>=0?'+':'-',double(distance),Localized(metric?0xE2078322:0x2E8496B1,metric?"m":"ft"));
}
inline void Update(void* board,void* player){
 auto packageName=Read<const char*>(board,0x10);if(!packageName)return;
 auto engine=Read<void*>(reinterpret_cast<void*>(0x91CADC));if(!engine)return;
 auto package=reinterpret_cast<Object(__thiscall*)(void*,const char*)>(0x516AF0)(engine,packageName);if(!package)return;
 auto race=Read<BYTE*>(reinterpret_cast<void*>(0x91E000));auto count=Read<unsigned>(race,0x1958);
 if(package!=state.package||board!=state.board)state={};
 if(state.ready&&Find(package,RowKey(4,0))!=state.rows[4].name)state={};
 bool active=race&&count>=1&&(count>4||(combatActive&&combatActive())||(driftActive&&driftActive()))&&count<=16&&Read<unsigned>(race,0x1960)==1;
 if(!active){HideExtra();return;}
 if(!FeaturesVisible(board,Read<unsigned long long>(board,0x20))||reinterpret_cast<bool(__cdecl*)()>(0x64B680)()){HideExtra();return;}
 if(!state.ready&&(state.attempts>=120||!Prepare(board,package)))return;
 // The native mask has been checked before touching either original or
 // cloned rows; do not resurrect them during an action camera or pause.
 std::array<unsigned,16> order{};for(unsigned i=0;i<count;++i)order[i]=i;
 auto rank=[&](unsigned slot){auto value=Read<unsigned>(race+0x18+slot*0x194,0x10);return value&&value<=count?value:count+slot+1;};
 std::sort(order.begin(),order.begin()+count,[&](unsigned a,unsigned b){auto x=rank(a),y=rank(b);return x!=y?x<y:a<b;});
 auto params=Read<void*>(race,0x1968);unsigned mode=params?reinterpret_cast<unsigned(__thiscall*)(void*)>(0x5FAA20)(params):0;
 bool values=Read<BYTE>(board,0x22C)!=0;state.mode=mode;
 state.phase=state.display.Observe(mode==5&&combatActive&&combatActive(),values);
 auto profile=Read<void*>(reinterpret_cast<void*>(0x91CF90));auto settings=Read<void*>(profile,0x10);bool metric=Read<BYTE>(settings,0x3B)!=0;
 unsigned playerSlot=PlayerSlot(player,race,count);
 // Reuse the evaluated native fade/flip, retaining each clone's translation.
 // Set the same render-state dirty bit as FEObject_SetSize; colour alone leaves
 // extra names at full height while the original four collapse during a flip.
 for(unsigned i=4;i<count;++i){auto dst=state.rows[i].name,src=state.rows[0].name;
  if(!dst[11]||!src[11]||dst[12]<68||src[12]<68)continue;
  auto d=reinterpret_cast<BYTE*>(dst[11]);auto s=reinterpret_cast<const BYTE*>(src[11]);
  if(memcmp(d,s,28)||memcmp(d+40,s+40,28)){
   memcpy(d,s,28);memcpy(d+40,s+40,28);dst[7]|=0x400000;
  }
 }
 for(unsigned i=0;i<16;++i){auto row=state.rows[i];bool visible=i<count;
  Show(row.name,visible);Show(row.rank,visible);Show(row.back,visible);if(!visible)continue;
  unsigned slot=order[i];auto info=race+0x18+slot*0x194;char text[96]{};
  Format(text,race,info,slot,playerSlot,mode,metric,state.phase);
  // Native Update rewrites its four rows. Extra rows are owned by this
  // adapter, so identical text need not allocate/rebuild every render tick.
  auto& cached=state.rows[i];
  if(i<4||strcmp(text,cached.text)){Text(row.name,text);strcpy_s(cached.text,text);}
  if(i<4||!cached.rankWritten){reinterpret_cast<void(__cdecl*)(void*,const char*,...)>(0x515D70)(row.rank,"%u",i+1);cached.rankWritten=true;}
  auto script=slot==playerSlot?0x249DB7B7u:0x1744B3u;
  if(!reinterpret_cast<bool(__cdecl*)(void*,unsigned)>(0x514DA0)(row.back,script))reinterpret_cast<void(__cdecl*)(void*,unsigned,bool)>(0x514D10)(row.back,script,true);
  if(i<4)Show(Read<Object>(board,0x254+i*4),false);
 }
}
}
