#pragma once
// EN1.3 AIActionRace has both course and native road-network operation.
// Only the owned start-only Interceptor suspect uses the latter.
namespace rpd_road_navigation {
void(__thiscall* originalBegin)(unsigned,float)=nullptr;
bool(__thiscall* originalCanRace)(unsigned,float)=nullptr;
void* originalCourseTarget=nullptr;

// A pursuit can still have a native target object after its IRacer has
// detached. AIActionRace has a position-only branch for that same object.
// Use it when the course target is missing. If the position is also gone,
// leave the current navigation untouched until the next native update.
__declspec(naked) void CourseTarget(){
 __asm {
  mov ecx,[esp+34h]
  pushfd
  test ecx,ecx
  jz missingTarget
  popfd
  jmp dword ptr [originalCourseTarget]
 missingTarget:
  test ebp,ebp
  jz skipUpdate
  cmp byte ptr [ebp+2Ch],0
  je skipUpdate
  popfd
  push 0428A13h
  ret
 skipUpdate:
  popfd
  push 0428BE7h
  ret
 }
}

bool InstallCourseTargetGuard(){
 if(originalCourseTarget)return true;
 constexpr BYTE lookup[]={0x8B,0x4C,0x24,0x34,0x8B,0x11,0xFF,0x92,0xA8,0,0,0};
 constexpr BYTE position[]={0x83,0xC5,0x10,0x8B,0x55,0,0x89,0x54,0x24,0x3C};
 constexpr BYTE next[]={0x84,0xDB,0xB9,0x48,0xB4,0x8E,0,0x75};
 auto address=reinterpret_cast<void*>(0x428A2D);
 if(memcmp(address,lookup,sizeof(lookup))||memcmp(reinterpret_cast<void*>(0x428A13),position,sizeof(position))||
    memcmp(reinterpret_cast<void*>(0x428BE7),next,sizeof(next)))return false;
 if(MH_CreateHook(address,CourseTarget,&originalCourseTarget)!=MH_OK)return false;
 if(MH_EnableHook(address)==MH_OK)return true;
 MH_RemoveHook(address);originalCourseTarget=nullptr;return false;
}
#ifdef MWEL_ARMS_FRONTEND_STIMULUS
bool testRejectRoad=false,testBypassResume=false;
#endif
bool SuspectAction(unsigned action){
 using combat_health_runtime::Read;
 const auto p=event_race_policies::Current();if(!p||!mwel::RpdStartOnly(p->authoring))return false;
 unsigned race=0,count=0,handle=0,car=0,ai=0,bound=0,table=0;
 return Read(0x91E000,race)&&Read(race+0x1958,count)&&count==2&&Read(race+0x18+0x194,handle)&&
  (car=gauntlet_runtime::Vehicle(handle))&&Read(car+0x54,ai)&&Read(action+0x40,bound)&&ai==bound&&Read(action,table)&&table==0x891090;
}
bool __fastcall CanRace(unsigned action,void*,float dt){
 using combat_health_runtime::Read;
 const bool native=originalCanRace(action,dt);
 if(!SuspectAction(action))return native;
 unsigned ai=0,goal=0,current=0;float reverse=0;
 if(!Read(action+0x40,ai)||ai<0x4C||!Read(ai-0x4C+0xB8,goal)||!gauntlet_runtime::Table(goal,0x892D30)||
    !Read(goal+4,current)||!gauntlet_runtime::Table(current,0x8912C0)||!Read(ai-0x4C+0x94,reverse)||!std::isfinite(reverse)||reverse>0)return native;
 bool roadAccepted=native;
#ifdef MWEL_ARMS_FRONTEND_STIMULUS
 if(testRejectRoad)roadAccepted=false;
 if(testRejectRoad&&testBypassResume)return false;
#endif
 if(roadAccepted)return true;
 // Preserve all five native component prerequisites. Only the temporary
 // native-road test may be replaced by this suspect's checked custom route.
 for(unsigned offset:{0x3Cu,0x40u,0x48u,0x4Cu,0x50u}){unsigned component=0;if(!Read(action+offset,component)||!component)return false;}
 if(!MWELRpdResumeTarget(action))return false;
 Log("RPD_AI_DRIVE_RESUME action=%08X prior_action=%08X native_road=%u custom_corridor=1 reverse_finished=1 native_goal_selection=1 vehicle_reposition=0",action,current,unsigned(native));
 return true;
}
void __fastcall Begin(unsigned self,void*,float delta){
 originalBegin(self,delta);
 if(!SuspectAction(self))return;
 // 0042F018..0042F068 derives +64 from IRacer's active race binding.
 // 0042880A selects the course branch with this byte. The false branch at
 // 00428AD7 preserves Direction/Racer navigation along native road choices.
 unsigned ai=0,nav=0;combat_health_runtime::Read(self+0x40,ai);combat_health_runtime::Read(ai+0x24,nav);
 if(!nav)return;
 *reinterpret_cast<BYTE*>(self+0x64)=0;
 reinterpret_cast<void(__thiscall*)(unsigned,bool)>(0x404570)(nav,false);
 Log("RPD_SUSPECT_FREE_ROAD action=%08X nav=%08X native_direction_navigation=1 course_binding=0 pose_writes=0",self,nav);
}
void PreparePursuit(){
 using namespace hot_pursuit_runtime;
 const auto p=event_race_policies::Current();if(!p||!mwel::RpdStartOnly(p->authoring)||!Active())return;
 unsigned race=0,count=0,handle=0,manager=0;BYTE racing=0,timer=0;
 if(!Read(0x91E000,race)||!Read(race+0x1958,count)||count!=2||!Read(race+0x198A,racing)||!racing||
  !Read(race+0x1978,timer)||!timer||
  !Read(race+0x18+0x194,handle)||!Vehicle(handle)||!Read(0x92C67C,manager)||!Table(manager,0x891900))return;
 if(hot_pursuit_runtime::owner!=MWELNeoCurrentKey()){hot_pursuit_runtime::Reset();hot_pursuit_runtime::owner=MWELNeoCurrentKey();}
 if(!hot_pursuit_runtime::requested){ApplyDeployment(manager,Budget());reinterpret_cast<void(__thiscall*)(unsigned,bool)>(0x411410)(manager,false);hot_pursuit_runtime::requested=true;}
 UpdateEligibility(race,count);
 const auto existed=FindPursuit(handle);
 rpd_native_pursuit::Start(race,manager,handle,Heat(Vehicle(*reinterpret_cast<unsigned*>(race+0x18))));
 const auto group=FindPursuit(handle);
 if(!existed&&group){Read(race+0x1978,timer);Log("RPD_PURSUIT_READY suspect=%u player_target=0 before_go=%u group=%08X",handle,unsigned(!timer),group);}
}
bool Install(){
 if(!native_runtime_diagnostics::InstallRoadPlacementGuard()||!native_runtime_diagnostics::InstallVehiclePlacementGuard())return false;
 if(!InstallCourseTargetGuard()||!rpd_pursuit_action_guard::Install()||!rpd_police_autodrive::InstallSteering())return false;
 if(originalBegin)return true;
 const BYTE begin[]={0x51,0x53,0x55,0x56,0x8B,0xF1,0x8B,0x4E,0x40};
 const BYTE filter[]={0x8A,0x44,0x24,0x04,0x84,0xC0,0x74,0x14};
 const BYTE eligible[]={0x64,0xA1,0,0,0,0,0x6A,0xFF,0x68,0xCB,0x78,0x86,0};
 const BYTE unstuck[]={0x60,0x68,0x40,0};
 if(memcmp(reinterpret_cast<void*>(0x42EF90),begin,sizeof(begin))||memcmp(reinterpret_cast<void*>(0x404570),filter,sizeof(filter))||
    memcmp(reinterpret_cast<void*>(0x4076B0),eligible,sizeof(eligible))||memcmp(reinterpret_cast<void*>(0x8912CC),unstuck,sizeof(unstuck)))return false;
 auto eligibility=reinterpret_cast<void*>(0x4076B0);
 if(MH_CreateHook(eligibility,CanRace,reinterpret_cast<void**>(&originalCanRace))!=MH_OK)return false;
 if(MH_EnableHook(eligibility)!=MH_OK){MH_RemoveHook(eligibility);originalCanRace=nullptr;return false;}
 auto address=reinterpret_cast<void*>(0x42EF90);
 if(MH_CreateHook(address,Begin,reinterpret_cast<void**>(&originalBegin))!=MH_OK)return false;
 if(MH_EnableHook(address)!=MH_OK){MH_RemoveHook(address);originalBegin=nullptr;return false;}return true;
}
}
