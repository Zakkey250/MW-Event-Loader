#pragma once
#include "RpdArrestNisPolicy.h"
namespace rpd_native_arrest_nis {
unsigned owned=0;ULONGLONG began=0;bool terminal=false;
bool victory=false;unsigned videoPackage=0;
std::vector<std::pair<unsigned,unsigned>> videoRoots;
#ifdef MWEL_ARMS_FRONTEND_STIMULUS
unsigned PrivatePattern(){
 static const auto value=[](){const auto path=game/L"MWEventLoader.arrest-pattern-test";
  if(!mwel::fs::exists(path))return ~0u;const auto text=mwel::Read(path,128);
  for(unsigned i=0;i<rpd_arrest_nis::scenes.size();++i)if(text=="MW_EVENT_LOADER_ARREST_PATTERN_V1\n"+std::string(rpd_arrest_nis::scenes[i])+"\n")return i;
  return ~0u;}();return value;
}
#endif
void Video(bool hide){
 using namespace expanded_leaderboard;
 const auto engine=Read<void*>(reinterpret_cast<void*>(0x91CADC));
 auto package=engine?reinterpret_cast<Object(__thiscall*)(void*,const char*)>(0x516AF0)(engine,"BUSTED_OVERLAY.fng"):nullptr;
 if(!package)return;
 if(hide&&videoPackage!=unsigned(package)){videoRoots.clear();videoPackage=unsigned(package);}
 const auto count=package[18];auto root=reinterpret_cast<Object>(package[19]);
 for(unsigned i=0;root&&i<count&&i<4096;++i,root=reinterpret_cast<Object>(root[1])){
  if(hide){
   if(std::none_of(videoRoots.begin(),videoRoots.end(),[&](const auto& r){return r.first==unsigned(root);}))videoRoots.emplace_back(unsigned(root),root[7]&1u);
   Show(root,false);
  }else if(videoPackage==unsigned(package)){
   auto saved=std::find_if(videoRoots.begin(),videoRoots.end(),[&](const auto& r){return r.first==unsigned(root);});
   if(saved!=videoRoots.end())Show(root,saved->second==0);
  }
 }
}
void Victory(){victory=true;Video(true);Log("RPD_NATIVE_ARREST_VIDEO_HIDDEN winner_foreground=1 native_characters_retained=1 overlay_only=1");}
unsigned requestRace=0,requestKey=0,requestEpoch=0,requestSuspect=0;
void(__thiscall* originalRequest)(void*,void*)=nullptr;
void(__thiscall* originalAddCar)(unsigned,unsigned,unsigned)=nullptr;
void(__cdecl* originalSelect)(unsigned*,char*,unsigned)=nullptr;
bool(__cdecl* originalPose)(float*,float*)=nullptr;
bool nativeSetup=false;unsigned setupSuspect=0;std::string selectedScene;
unsigned retiringActivity=0;void* completeTrampoline=nullptr;
bool __cdecl SuppressCompletion(unsigned activity){
 if(!retiringActivity||activity!=retiringActivity)return false;
 retiringActivity=0;Log("RPD_NATIVE_ARREST_COMPLETION_SUPPRESSED own_nis_cleanup=1 native_defeat_navigation_disabled=1");return true;
}
void __declspec(naked) Completion(){__asm{
 pushfd
 pushad
 push edi
 call SuppressCompletion
 add esp,4
 test al,al
 jnz suppress
 popad
 popfd
 jmp completeTrampoline
suppress:
 popad
 popfd
 // The skipped native __thiscall consumes its by-value message argument.
 // Preserve that ret 4 stack effect before continuing the destructor.
 lea esp,[esp+4]
 push 0x6FD0C7
 ret
}}
bool SetupNative(void*,void*,unsigned);
std::string PickScene();
void __fastcall AddCar(unsigned nis,void*,unsigned channel,unsigned vehicle){
 if(nativeSetup&&channel==reinterpret_cast<unsigned(__cdecl*)(const char*)>(0x5CC240)("car1"))vehicle=gauntlet_runtime::Vehicle(setupSuspect);
 originalAddCar(nis,channel,vehicle);
}
void __cdecl Select(unsigned* track,char* name,unsigned capacity){
 originalSelect(track,name,capacity);if(!nativeSetup)return;
 selectedScene=PickScene();if(!selectedScene.empty()&&selectedScene.size()+1<=capacity){strcpy_s(name,capacity,selectedScene.c_str());*track=0;}
}
bool __cdecl Pose(float* position,float* heading){
 if(nativeSetup){combat_impact::Motion pose;const auto car=gauntlet_runtime::Vehicle(setupSuspect);
  if(!car||!combat_health_runtime::NativeMotion(car-0x80,pose))return false;
  position[0]=pose.position.x;position[1]=pose.position.y;position[2]=pose.position.z;*heading=std::atan2(pose.forward.x,pose.forward.z);return true;
 }
 return originalPose(position,heading);
}
bool Requested(unsigned race,unsigned key,unsigned epoch,unsigned suspect){return race&&race==requestRace&&key==requestKey&&epoch==requestEpoch&&suspect==requestSuspect;}
void __fastcall NativeRequest(void* self,void*,void* message){
 if(MWELRpdHotPursuitScope())return; // The sprint contact gate requests only its final scene.
 const auto package=event_race_policies::Current();unsigned race=0,target=0,payload=0;
 if(package&&mwel_rpd::Armed(package->authoring)&&combat_health_runtime::Read(0x91E000,race)&&
    combat_health_runtime::Read(race+0x18+0x194,target)&&combat_health_runtime::Read(unsigned(message)+0x10,payload)&&payload==target){
  BYTE totalled=0;
  if(combat_health_runtime::Read(race+0x18+0x194+0x1D,totalled)&&totalled){
   Log("RPD_NATIVE_ARREST_NIS_SKIPPED suspect=%u reason=already_totalled wreck_presentation_retained=1");return;
  }
  requestRace=race;requestKey=MWELNeoCurrentKey();requestEpoch=combat_health_runtime::generation;requestSuspect=target;
  Log("RPD_NATIVE_ARREST_NIS_REQUEST suspect=%u payload=%u native_start_callback=0044DC70 gauge_trigger_used=0",target,payload);
  SetupNative(self,message,target);return; // Keep the entire native request path; override its actor binding only.
 }
 originalRequest(self,message);
}
bool Install(){
 if(originalRequest)return true;
 const BYTE request[]={0x64,0xA1,0,0,0,0,0x6A,0xFF,0x68,0xBE,0x9A,0x86,0};
 const BYTE select[]={0xA1,0x84,0xCD,0x92,0,0x53,0x55,0x56};
 const BYTE bind[]={0x53,0x56,0x8B,0x74,0x24,0x10,0x57,0x8B,0xF9};
 const BYTE pose[]={0x64,0xA1,0,0,0,0,0x6A,0xFF,0x68,0x8B,0x9A,0x86,0};
 const BYTE completion[]={0xE8,0x29,0x7A,0xF5,0xFF};
 const BYTE completedLoading[]={0xC6,0x05,0x54,0x58,0x92,0,0};
 if(memcmp(reinterpret_cast<void*>(0x6F5919),completedLoading,sizeof(completedLoading)))return false;
 if(memcmp(reinterpret_cast<void*>(0x44DC70),request,sizeof(request))||memcmp(reinterpret_cast<void*>(0x44D770),select,sizeof(select))||memcmp(reinterpret_cast<void*>(0x6FC920),bind,sizeof(bind))||memcmp(reinterpret_cast<void*>(0x44DA10),pose,sizeof(pose))||memcmp(reinterpret_cast<void*>(0x6FD0C2),completion,sizeof(completion)))return false;
 struct Hook{unsigned site;void* hook;void** original;};Hook hooks[]={{0x44DC70,NativeRequest,reinterpret_cast<void**>(&originalRequest)},{0x44D770,Select,reinterpret_cast<void**>(&originalSelect)},{0x6FC920,AddCar,reinterpret_cast<void**>(&originalAddCar)},{0x44DA10,Pose,reinterpret_cast<void**>(&originalPose)},{0x6FD0C2,Completion,&completeTrampoline}};
 unsigned made=0;bool ok=true;
 for(auto& h:hooks){if(MH_CreateHook(reinterpret_cast<void*>(h.site),h.hook,h.original)!=MH_OK){ok=false;break;}++made;}
 if(ok)for(auto& h:hooks)if(MH_EnableHook(reinterpret_cast<void*>(h.site))!=MH_OK){ok=false;break;}
 if(!ok)for(unsigned i=0;i<made;++i){MH_DisableHook(reinterpret_cast<void*>(hooks[i].site));MH_RemoveHook(reinterpret_cast<void*>(hooks[i].site));*hooks[i].original=nullptr;}
 return ok;
}
unsigned Current(){return *reinterpret_cast<unsigned*>(0x9885C8);}
bool Owns(){return owned&&Current()==owned&&*reinterpret_cast<unsigned*>(owned)==0x8B0D78;}
void Release(){
 Video(false);victory=false;videoPackage=0;videoRoots.clear();
 if(Owns()){
  const auto activity=owned-0x1C;
  retiringActivity=owned-0x48;
  // OnTask(COMPLETE), 006F5916..20, clears the native loading fast loop
  // before releasing IActivity. Our held scene never reaches that case.
  const auto loadingFast=*reinterpret_cast<BYTE*>(0x925854);
  *reinterpret_cast<BYTE*>(0x925854)=0;
  Log("RPD_NATIVE_ARREST_RELEASE loading_fast_before=%u native_complete_cleanup=1",unsigned(loadingFast));
  // Finish the native sequencer's cleanup actions (including resume/unload),
  // as SkipOverNIS does, without dispatching the arrest's losing end tags.
  const auto sequencer=*reinterpret_cast<unsigned*>(owned+0x370);
  if(sequencer){auto table=*reinterpret_cast<unsigned**>(sequencer);
   reinterpret_cast<void(__thiscall*)(unsigned,float,bool,void*)>(table[19])(sequencer,*reinterpret_cast<float*>(0x9885D8),true,nullptr);
  }
  if(!Owns()){owned=0;terminal=false;requestRace=requestKey=requestEpoch=requestSuspect=0;return;}
  auto release=reinterpret_cast<void(__thiscall*)(unsigned)>((*reinterpret_cast<unsigned**>(activity))[1]);
  owned=0;terminal=false;release(activity);
 }else {owned=0;terminal=false;}
 requestRace=requestKey=requestEpoch=requestSuspect=0;
}
std::string PickScene(){
 std::vector<std::string> available;
 for(const auto* name:rpd_arrest_nis::scenes)if(mwel::fs::is_regular_file(game/L"NIS"/("Scene_"+std::string(name)+"_BundleB.bun")))available.emplace_back(name);
 if(available.empty())return {};
 static std::vector<std::string> bag,previousAvailable;static std::string previous;
 static std::mt19937 random(GetTickCount()^GetCurrentProcessId());
 if(previousAvailable!=available){bag.clear();previousAvailable=available;}
 if(bag.empty()){bag=available;std::shuffle(bag.begin(),bag.end(),random);if(bag.size()>1&&bag.back()==previous)std::swap(bag.front(),bag.back());}
 std::string selected=bag.back();bag.pop_back();previous=selected;
#ifdef MWEL_ARMS_FRONTEND_STIMULUS
 if(PrivatePattern()<rpd_arrest_nis::scenes.size()){const auto expected=rpd_arrest_nis::scenes[PrivatePattern()];if(std::find(available.begin(),available.end(),expected)!=available.end())selected=expected;}
#endif
 return selected;
}
bool SetupNative(void* self,void* message,unsigned suspect){
 if(Current()||!gauntlet_runtime::Vehicle(suspect))return false;
 setupSuspect=suspect;nativeSetup=true;
 __try{originalRequest(self,message);}__finally{nativeSetup=false;setupSuspect=0;}
 owned=Current();began=GetTickCount64();terminal=false;
 if(!Owns()){owned=0;Log("RPD_NATIVE_ARREST_NIS_FAILED reason=native_request_did_not_create_scene");return false;}
 const auto bound=reinterpret_cast<unsigned(__thiscall*)(unsigned,unsigned)>(0x6F70E0)(owned,reinterpret_cast<unsigned(__cdecl*)(const char*)>(0x5CC240)("car1"));
 const auto expected=gauntlet_runtime::Vehicle(suspect);
 Log("RPD_NATIVE_ARREST_NIS_BEGIN suspect=%u car1=%08X expected=%08X scene=%s pool=26 native_characters=1 native_audio=1 native_start_setup=1 player_ownership_changed=0",suspect,bound,expected,selectedScene.c_str());
 if(bound!=expected){Release();return false;}
 return true;
}
bool Sprint(unsigned race,unsigned suspect){
 // EN1.3 0044DC70 does not read this or the incoming message; the hook
 // normally uses the message only to resolve the actor. We have its handle.
 if(!originalRequest||std::none_of(rpd_arrest_nis::scenes.begin(),rpd_arrest_nis::scenes.end(),[](const char* name){return mwel::fs::is_regular_file(game/L"NIS"/("Scene_"+std::string(name)+"_BundleB.bun"));}))return false;
 requestRace=race;requestKey=MWELNeoCurrentKey();requestEpoch=combat_health_runtime::generation;requestSuspect=suspect;
 return SetupNative(nullptr,nullptr,suspect);
}
bool Begin(unsigned suspect,const combat_impact::Motion&){
 return Owns()&&suspect==requestSuspect;
}
bool Tick(){
 if(!Owns())return false;
 if(victory)Video(true);
 const auto activity=owned-0x48;const auto state=*reinterpret_cast<unsigned*>(activity+0x2F4);
 if(state==5){
  const auto scene=*reinterpret_cast<unsigned*>(activity+0x39C);
  if(scene){auto table=*reinterpret_cast<unsigned**>(scene);
   const float elapsed=reinterpret_cast<float(__thiscall*)(unsigned)>(table[10])(scene);
   const float end=reinterpret_cast<float(__thiscall*)(unsigned)>(table[9])(scene);
   if(!terminal&&rpd_arrest_nis::HoldBeforeEnd(elapsed,end)){
    reinterpret_cast<void(__thiscall*)(unsigned)>(0x6EA730)(owned);terminal=true;
    Log("RPD_NATIVE_ARREST_NIS_TERMINAL elapsed=%.4f end=%.4f paused_before_end=1 native_busted_menu_dispatched=0",elapsed,end);
   }
  }
 }
 if(worldLogEnabled){static ULONGLONG next=0;if(GetTickCount64()>=next){next=GetTickCount64()+1000;Log("RPD_NATIVE_ARREST_NIS_STATE state=%u activity=%08X scene=%08X held=%u start=%u settle=%u paused=%u",state,activity,*reinterpret_cast<unsigned*>(activity+0x39C),unsigned(terminal),unsigned(*reinterpret_cast<BYTE*>(activity+0x3D4)),*reinterpret_cast<unsigned*>(activity+0x3DC),unsigned(*reinterpret_cast<BYTE*>(activity+0x404)));}}
 if(state==7||(!terminal&&GetTickCount64()-began>45000)){Log("RPD_NATIVE_ARREST_NIS_FAILED state=%u timeout=%u",state,unsigned(GetTickCount64()-began>45000));Release();return false;}
 return true;
}

}
