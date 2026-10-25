#pragma once
// Opt-in presentation: the native checkpoint callback remains authoritative.
// Borrow the HUD's otherwise-unused checkpoint image; restore it at every
// event boundary. No FE node or native icon array is allocated or enlarged.
namespace checkpoint_display {
using Object=expanded_leaderboard::Object;
using expanded_leaderboard::Read;
struct State {
 unsigned key=0,laps=0;void* map=nullptr;Object package=nullptr,icon=nullptr;
 char name[64]{};unsigned iconKey=0,guid=0,texture=0;bool wasHidden=true;std::array<BYTE,0x54> baseline{};std::vector<std::array<float,2>> points;int shown=-1;
};
State state;unsigned stage=0;
Object Live(){
 auto engine=Read<void*>(reinterpret_cast<void*>(0x91CADC));
 auto p=engine&&state.name[0]?reinterpret_cast<Object(__thiscall*)(void*,const char*)>(0x516AF0)(engine,state.name):nullptr;
 return state.icon&&p&&p==state.package&&expanded_leaderboard::Find(p,state.iconKey)==state.icon&&Read<unsigned>(state.icon,0xC)==state.guid?p:nullptr;
}
void Hide(){if(state.icon&&Live())expanded_leaderboard::Show(state.icon,false);state.shown=-1;}
void Reset(){
 if(state.icon&&Live()){
  auto data=Read<BYTE*>(state.icon,0x2C);
  if(data&&Read<unsigned>(state.icon,0x30)>=state.baseline.size()){
   Copy(state.baseline.data(),data,state.baseline.size());
   reinterpret_cast<void(__cdecl*)(const char*,unsigned,unsigned)>(0x527EE0)(state.name,state.iconKey,state.texture);
   state.icon[7]|=0x400000;expanded_leaderboard::Show(state.icon,!state.wasHidden);
  }
 }state={};
}
void FeaturesChanged(unsigned long long features){
 if(state.map&&Live()&&!(Read<unsigned long long>(state.map,0x18)&features))Hide();
}
void FrameUnsafe(){
 stage=1;
 auto race=Read<BYTE*>(reinterpret_cast<void*>(0x91E000));auto key=MWELNeoCurrentKey();
 if(!key||!race||Read<unsigned>(reinterpret_cast<void*>(0x925E90))!=6||Read<unsigned>(race,0x1960)!=1||!Read<BYTE>(race,0x198A)||!Read<BYTE>(race,0x1978)){Reset();return;}
 if(key!=state.key){
  Reset();state.key=key;auto clone=registration_experiment::Find(key);
  if(clone&&clone->race)for(const auto& p:packages)if(p.nativeNamespace==clone->name&&mwel::Feature(p.authoring,"checkpointsVisible")){
   state.laps=mwel::RaceLaps(p.authoring);
   for(const auto& gate:p.authoring.at("gates"))if(gate.at("kind")=="checkpoint"){
    const auto& v=gate.at("position");state.points.push_back({v.at(0).get<float>(),v.at(1).get<float>()});
   }break;
  }
 }
 stage=2;if(state.points.empty())return;
 if(reinterpret_cast<bool(__cdecl*)()>(0x468390)()){Hide();return;}
 stage=3;using namespace NFSPluginSDK::MW05;
 auto count=Read<unsigned>(reinterpret_cast<void*>(0x92D884));auto players=Read<IPlayer**>(reinterpret_cast<void*>(0x92D87C));
 if(!count||count>8||!event_maps::Readable(players,count*4)||!event_maps::Readable(players[0],4)){Hide();return;}
 stage=4;auto hud=static_cast<FEngHud*>(players[0]->GetHud());
 if(!event_maps::Readable(hud,sizeof(FEngHud))){Hide();return;}
 auto map=static_cast<Minimap*>(hud->pMinimap);
 if(!event_maps::Readable(map,sizeof(Minimap))||!(Read<unsigned long long>(map,0x18)&Read<unsigned long long>(map,0x20))){Hide();return;}
 stage=5;auto racers=Read<unsigned>(race,0x1958);if(!racers||racers>16){Hide();return;}
 auto slot=expanded_leaderboard::PlayerSlot(players[0],race,racers);auto info=race+0x18+slot*0x194;
 // mCheckpointsHitThisLap is reset by the game on lap/retry. Finishing is not
 // a checkpoint; suppress the dot after the last actual CP or a player exit.
 auto next=Read<int>(info,0x2C);
 if(next<0||size_t(next)>=state.points.size()||Read<unsigned>(info,0x28)>=state.laps||Read<BYTE>(info,0x190)){Hide();return;}
 stage=6;if(state.map!=map||!Live()){
  Hide();state.map=map;state.icon=nullptr;state.package=nullptr;state.name[0]=0;
  auto name=Read<const char*>(map,0x10);if(!name)return;
  unsigned n=0;for(;n<sizeof(state.name)-1;++n){state.name[n]=Read<char>(name,n);if(!state.name[n])break;}
  if(n==sizeof(state.name)-1){state.name[0]=0;return;}
  auto engine=Read<void*>(reinterpret_cast<void*>(0x91CADC));if(!engine)return;
  state.package=reinterpret_cast<Object(__thiscall*)(void*,const char*)>(0x516AF0)(engine,state.name);if(!state.package)return;
  stage=7;auto icon=Read<Object>(map,0x110); // native Minimap checkpoint art
  if(!icon||Read<unsigned>(icon,0x18)!=1||Read<unsigned>(icon,0x30)<state.baseline.size())return;
  auto data=Read<BYTE*>(icon,0x2C);if(!data||!Copy(data,state.baseline.data(),state.baseline.size()))return;
  state.iconKey=Read<unsigned>(icon,0x10);state.guid=Read<unsigned>(icon,0xC);
  if(!state.iconKey||expanded_leaderboard::Find(state.package,state.iconKey)!=icon)return;
  const auto texture=expanded_leaderboard::Hash("SMALL_CIRCLE");
  if(!reinterpret_cast<void*(__cdecl*)(unsigned,int,int)>(0x503400)(texture,0,0))return;
  state.texture=Read<unsigned>(icon,0x24);state.wasHidden=(Read<unsigned>(icon,0x1C)&1)!=0;state.icon=icon;
  // This native image is already compiled into the HUD's render tree. A new
  // FE child can otherwise remain outside that cached tree even when shown.
  const float uv[]={0.f,0.f,1.f,1.f};Copy(uv,data+0x44,sizeof(uv));
  reinterpret_cast<void(__cdecl*)(const char*,unsigned,unsigned)>(0x527EE0)(state.name,state.iconKey,texture);
  reinterpret_cast<void(__cdecl*)(Object,float,float)>(0x5155E0)(state.icon,8.f,8.f);
 }
 stage=9;if(!state.icon)return;
 const float heading[]={0.f,1.f};const auto& point=state.points[size_t(next)];
 reinterpret_cast<void(__thiscall*)(void*,const float*,const float*,Object,unsigned)>(0x5675D0)(map,point.data(),heading,state.icon,0);
 const int yellow[]={0,220,255,255}; // native FEColor integer B,G,R,A
 reinterpret_cast<void(__cdecl*)(Object,const int*)>(0x515810)(state.icon,yellow);
 if(worldLogEnabled&&state.shown!=next)Log("EVENT_NEXT_CHECKPOINT_RADAR owner=%08X next=%d position=%.3f,%.3f single_dot=1 native_progress=1",key,next,point[0],point[1]);
 state.shown=next;stage=10;
}
void Audit(){
 if(!worldLogEnabled)return;static ULONGLONG next=0;if(GetTickCount64()<next)return;next=GetTickCount64()+5000;
 float x=0,y=0;if(state.icon&&Live())reinterpret_cast<void(__cdecl*)(Object,float*,float*)>(0x524EE0)(state.icon,&x,&y);
 Log("EVENT_CHECKPOINT_RADAR_STATE stage=%u owner=%08X checkpoints=%u map=%08X package=%08X icon=%08X shown=%d hidden=%u centre=%.3f,%.3f texture=%08X",stage,state.key,unsigned(state.points.size()),unsigned(state.map),unsigned(state.package),unsigned(state.icon),state.shown,Read<unsigned>(state.icon,0x1C)&1,x,y,Read<unsigned>(state.icon,0x24));
}
// Native Minimap::Update hides/rebuilds checkpoint art after the simulation
// frame. Draw only after that update; a simulation-time Show is overwritten.
void(__thiscall* originalUpdate)(void*,void*)=nullptr;
void Tick(){
 if(!state.icon)return;
 auto race=Read<BYTE*>(reinterpret_cast<void*>(0x91E000));
 if(MWELNeoCurrentKey()!=state.key||!race||Read<unsigned>(race,0x1960)!=1||!Read<BYTE>(race,0x198A)||!Read<BYTE>(race,0x1978)){Reset();return;}
 if(reinterpret_cast<bool(__cdecl*)()>(0x468390)()||!(Read<unsigned long long>(state.map,0x18)&Read<unsigned long long>(state.map,0x20)))Hide();
}
void Forget(){state={};}
void Frame(){__try{FrameUnsafe();Audit();}__except(EXCEPTION_EXECUTE_HANDLER){Forget();Log("EVENT_CHECKPOINT_RADAR_UNAVAILABLE fault=%08X",unsigned(GetExceptionCode()));}}
void __fastcall Update(void* self,void*,void* player){originalUpdate(self,player);Frame();}
bool Install(){
 if(originalUpdate)return true;
 if(std::none_of(packages.begin(),packages.end(),[](const auto& p){return mwel::Feature(p.authoring,"checkpointsVisible");}))return true;
 const BYTE guard[]={0x83,0xEC,0x10,0x53,0x56,0x8B,0xF1};auto site=reinterpret_cast<void*>(0x59DB50);
 if(memcmp(site,guard,sizeof(guard))||MH_CreateHook(site,Update,reinterpret_cast<void**>(&originalUpdate))!=MH_OK)return false;
 if(MH_EnableHook(site)!=MH_OK){MH_RemoveHook(site);originalUpdate=nullptr;return false;}
 Log("EVENT_CHECKPOINT_RADAR_ACTIVE native_minimap_post_update=1 arrays_extended=0");return true;
}

}
