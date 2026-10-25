// Local-only ownership experiment. Default mode is zero; both loader test
// eligibility and a second explicit marker are required. Never enable in a
// distribution or main-PC runtime. Values below are native IInput units.
static unsigned neoDiagnosticMode=0,neoDiagnosticHandle=0;
static unsigned long long neoDiagnosticSequence=0;
using NeoControlsGetter=void*(__thiscall*)(void*);
using NeoClearFn=void(__thiscall*)(void*);
static NeoControlsGetter neoOriginalGetter=nullptr;
static NeoClearFn neoOriginalClear=nullptr;
using NeoPacketFn=void(__thiscall*)(void*,float,void*);
static NeoPacketFn neoOriginalPacket=nullptr;
static NeoDrivingFn neoOriginalGoalUpdate=nullptr,neoOriginalRacerUpdate=nullptr;
static ULONGLONG NeoDiagnosticWindow(){return neoDiagnosticMode==7?180000:neoDiagnosticMode==3?120000:24000;}
static bool NeoDiagnosticTarget(NeoRuntimeState& row){
 if(neoDiagnosticMode==7)return false; // Observation must never change driver or jam handling.
 if(!neoDiagnosticMode||!NeoLocalPlayer()||!row.input||!BattleVtable(row.input,neoDiagnosticMode==6?0x8AC670:0x8AB5C8))return false;
 if(!neoDiagnosticHandle)neoDiagnosticHandle=row.handle;
 return row.handle==neoDiagnosticHandle;
}
static bool NeoDiagnosticRead(void* input,float out[3]){
 return AudioRead(static_cast<BYTE*>(input)+0x0C,&out[0])&&
 AudioRead(static_cast<BYTE*>(input)+0x1C,&out[1])&&AudioRead(static_cast<BYTE*>(input)+0x20,&out[2]);
}
static void NeoDiagnosticOrder(NeoRuntimeState& row,const char* stage,std::uintptr_t caller){
 if(row.diagnosticAt&&GetTickCount64()-row.diagnosticAt>=NeoDiagnosticWindow())return;
 void* goal=nullptr;void* action=nullptr;std::uintptr_t goalVT=0,actionVT=0;
 AudioRead(static_cast<BYTE*>(row.ai)-0x4C+0xB8,&goal);
 if(goal){AudioRead(goal,&goalVT);AudioRead(static_cast<BYTE*>(goal)+4,&action);}
 if(action)AudioRead(action,&actionVT);
 Log(LogLevel::Info,"NEO_ORDER seq=%llu tick=%u vehicle=%u stage=%s caller=%08X goal=%08X action=%08X phase=%u",++neoDiagnosticSequence,row.ticks,row.handle,stage,unsigned(caller),unsigned(goalVT),unsigned(actionVT),row.diagnosticPhase);
}
static void NeoDiagnosticControls(NeoRuntimeState& row,const mwneo::Vehicle& v,ULONGLONG now){
 if(!row.diagnosticAt)row.diagnosticAt=now;
 // Response tests wait for race staging to finish. Canary is a value test.
 unsigned phase=unsigned((now-row.diagnosticAt)/3000);row.diagnosticPhase=phase;
 float steer=0,gas=0,brake=1;
 if(neoDiagnosticMode==6){
  if(phase<2||row.brakeProbeFinished){row.controls={};row.controls.brake=1;row.controls.valid=true;return;}
  mwneo::Profile profile;using GetProfile=unsigned(__cdecl*)(mwneo::Profile*,unsigned);
  auto module=GetModuleHandleW(L"MWEventLoader.asi");auto get=reinterpret_cast<GetProfile>(GetProcAddress(module,"MWEL_NeoProfileV1"));if(get)get(&profile,sizeof(profile));
  auto controlled=row.driver.Update(v,row.guidance,profile);row.controls=controlled;
  bool clear=controlled.valid&&row.guidance.status==1&&controlled.recovery==mwneo::Recovery::Normal&&
   row.guidance.obstacleDistance>100&&fabs(controlled.steering)<.12f&&fabs(controlled.headingError)<.2f&&
   mwneo::DotXZ(mwneo::UnitXZ(v.heading),mwneo::UnitXZ(v.velocity))>.99f;
  if(!row.brakeProbeStarted&&clear&&v.speed>=30){
   row.brakeProbeStarted=true;row.brakeProbeInitial=v.speed;
   Log(LogLevel::Info,"NEO_BRAKE_PROBE_BEGIN handle=%u speed=%.6f geometry_checked=1 NOS_disabled=1 local_only=1",row.handle,v.speed);
  }else if(row.brakeProbeStarted){
   float step=std::clamp(v.dt,0.f,.1f);row.brakeProbeTime+=step;row.brakeProbeGrade+=v.heading.y*step;
   row.brakeProbeValid=row.brakeProbeValid&&clear;
   if(v.speed<12||row.brakeProbeTime>2){
    row.brakeProbeFinished=true;
    float decel=row.brakeProbeTime>.01f?(row.brakeProbeInitial-v.speed-9.81f*row.brakeProbeGrade)/row.brakeProbeTime:0;
    Log(LogLevel::Info,"NEO_BRAKE_PROBE_RESULT handle=%u initial_speed=%.6f final_speed=%.6f physics_seconds=%.6f full_brake_decel=%.6f valid=%u local_only=1",row.handle,row.brakeProbeInitial,v.speed,row.brakeProbeTime,decel,unsigned(row.brakeProbeValid));
   }
  }
  if(row.brakeProbeStarted){row.controls.throttle=0;row.controls.brake=1;row.controls.valid=true;}
  return;
 }else if(neoDiagnosticMode==1){
  if(phase==0){gas=.123f;brake=0;}
  if(phase==1){steer=-.789f;gas=.456f;brake=0;}
  if(phase==2){steer=.654f;brake=.321f;}
 }else if(neoDiagnosticMode==2){
  if(phase<2)phase=99;else phase-=2;
  if(phase==0){gas=.5f;brake=0;}
  if(phase==1)brake=.5f;
  if(phase==2){steer=-.5f;gas=.5f;brake=0;}
  if(phase==4){steer=.5f;gas=.5f;brake=0;}
 }else if((neoDiagnosticMode==4||neoDiagnosticMode==5)&&phase>=2){
  // Stage 1: fixed straight points on the initial straight of event 1.2.1.
  // The authored first leg's tangent is used once; no graph, racing line,
  // obstacle, passing, recovery, shortcut or native route planner executes.
  const mwneo::Point direction{-.230877f,0,-.972983f};
  if(!row.testPointsReady){
   row.testPointCount=17;
   for(unsigned i=0;i<row.testPointCount;++i)row.testPoints[i]={v.position.x+direction.x*5*i,v.position.y,v.position.z+direction.z*5*i};
   if(neoDiagnosticMode==5){
    // A fixed local fixture from the first three authored event points. This
    // is a gentle bend only; no intersection, branch or obstacle stage.
    mwneo::Point points[3]={v.position,{v.position.x-20.648803f,v.position.y,v.position.z-87.018310f},{v.position.x-66.142578f,v.position.y,v.position.z-164.353150f}};
    mwneo::Point tangents[3]={{-20.648803f,0,-87.018310f},{-33.071289f,0,-82.176575f},{-45.493775f,0,-77.334840f}};
    row.testPointCount=37;
    for(unsigned i=0;i<row.testPointCount;++i){unsigned leg=std::min(1u,i/18);float t=float(i-leg*18)/18;float t2=t*t,t3=t2*t;
     auto a=points[leg],b=points[leg+1],m=tangents[leg],n=tangents[leg+1];float w0=2*t3-3*t2+1,w1=t3-2*t2+t,w2=-2*t3+3*t2,w3=t3-t2;
     row.testPoints[i]={w0*a.x+w1*m.x+w2*b.x+w3*n.x,w0*a.y+w1*m.y+w2*b.y+w3*n.y,w0*a.z+w1*m.z+w2*b.z+w3*n.z};
    }
   }
   row.testPointsReady=true;
  }
  float station=0,lateral=0,best=1e9f,total=0,selectedT=0;unsigned selected=0;
  for(unsigned i=1;i<row.testPointCount;++i){auto edge=mwneo::Sub(row.testPoints[i],row.testPoints[i-1]);float length=mwneo::LengthXZ(edge);if(length<.01f)continue;
   auto local=mwneo::Sub(v.position,row.testPoints[i-1]);float along=std::clamp(mwneo::DotXZ(local,edge)/(length*length),0.f,1.f);auto closest=row.testPoints[i-1];closest.x+=edge.x*along;closest.z+=edge.z*along;
   float distance=mwneo::LengthXZ(mwneo::Sub(v.position,closest));if(distance<best){best=distance;station=total+along*length;lateral=mwneo::CrossXZ(mwneo::UnitXZ(edge),local);selected=i;selectedT=along;}total+=length;
  }
  float finish=neoDiagnosticMode==5?140.f:60.f;
  if(station<finish&&std::abs(lateral)<5){
   float ahead=12.f;auto aim=row.testPoints[selected-1];auto edge=mwneo::Sub(row.testPoints[selected],aim);aim.x+=edge.x*selectedT;aim.z+=edge.z*selectedT;
   for(unsigned i=selected;i<row.testPointCount;++i){auto delta=mwneo::Sub(row.testPoints[i],aim);float length=mwneo::LengthXZ(delta);if(length>=ahead){aim.x+=delta.x*ahead/length;aim.z+=delta.z*ahead/length;break;}ahead-=length;aim=row.testPoints[i];}
   auto delta=mwneo::Sub(aim,v.position);float d=mwneo::LengthXZ(delta);
   float curve=d>.1f?2*mwneo::CrossXZ(mwneo::UnitXZ(v.heading),delta)/(d*d):0;
   float geometric=std::clamp(std::atan(2.65f*curve)/.55f,-1.f,1.f);
   row.testSteering=mwneo::Approach(row.testSteering,geometric,1.2f*v.dt);steer=-row.testSteering;
   float error=60.f/3.6f-v.speed;row.testIntegral=std::clamp(row.testIntegral+error*v.dt,-5.f,10.f);
   gas=std::clamp(.14f*error+.06f*row.testIntegral,0.f,1.f);brake=error<-1?std::clamp(-.1f*error,0.f,.6f):0;
   if(brake)gas=0;
  }
  Log(LogLevel::Info,"NEO_MINIMAL tick=%u vehicle=%u stage=%u station=%.6f lateral=%.6f target=16.666667 speed=%.6f end=%u",row.ticks,row.handle,neoDiagnosticMode==5?2:1,station,lateral,v.speed,unsigned(station>=finish||std::abs(lateral)>=5));
 }
 row.controls={};row.controls.valid=true;row.controls.steering=-steer;row.controls.throttle=gas;row.controls.brake=brake;
}
static NeoRuntimeState* NeoDiagnosticInput(void* input){
#ifdef MWEL_NEO_OBSERVE_ONLY
 if(neoDiagnosticMode==7&&MWELNeoLoggingEnabled()){
  for(auto& row:neoRuntime)if(row.owned&&row.input==input&&row.diagnosticAt&&row.ticks%15==0&&GetTickCount64()-row.diagnosticAt<NeoDiagnosticWindow())return &row;
  return nullptr;
 }
#endif
 for(auto& row:neoRuntime)if(row.owned&&row.input==input&&NeoDiagnosticTarget(row)&&row.diagnosticAt&&GetTickCount64()-row.diagnosticAt<NeoDiagnosticWindow())return &row;
 return nullptr;
}
static void NeoDiagnosticWriter(void* input,unsigned channel,float value,std::uintptr_t caller){
 auto row=NeoDiagnosticInput(input);if(!row)return;
 Log(LogLevel::Info,"NEO_WRITER seq=%llu tick=%u vehicle=%u channel=%u value=%.6f caller=%08X rejected=%u",++neoDiagnosticSequence,row->ticks,row->handle,channel,value,unsigned(caller),unsigned(neoWriter!=GetCurrentThreadId()));
}
static void __cdecl NeoDiagnosticGetterObserved(void* input,void* result,std::uintptr_t caller){
 auto row=NeoDiagnosticInput(input);if(!row)return;
 float final[3]{};if(!NeoDiagnosticRead(input,final))return;
 NeoDiagnosticOrder(*row,"INPUT_CONSUMER",caller);
 Log(LogLevel::Info,"NEO_CONTROL_TRACE seq=%llu frame=%u vehicle=%u phase=%u caller=%08X requested_steer=%.6f requested_throttle=%.6f requested_brake=%.6f postwrite_steer=%.6f postwrite_throttle=%.6f postwrite_brake=%.6f final_steer=%.6f final_throttle=%.6f final_brake=%.6f",++neoDiagnosticSequence,row->ticks,row->handle,row->diagnosticPhase,unsigned(caller),row->requested[0],row->requested[1],row->requested[2],row->postwrite[0],row->postwrite[1],row->postwrite[2],final[0],final[1],final[2]);
}
// The native getter is a register-preserving leaf. Optimized native callers
// retain volatile registers across it; diagnostic formatting must not leak.
static void __declspec(naked) NeoDiagnosticGetter(){
 __asm {
  call dword ptr [neoOriginalGetter]
  pushfd
  pushad
  mov ebx,esp
  and esp,0FFFFFFF0h
  sub esp,512
  fxsave [esp]
  push dword ptr [ebx+36]
  push dword ptr [ebx+28]
  push dword ptr [ebx+24]
  call NeoDiagnosticGetterObserved
  add esp,12
  fxrstor [esp]
  mov esp,ebx
  popad
  popfd
  ret
 }
}
static void __fastcall NeoDiagnosticClear(void* input,void*){
 auto row=NeoDiagnosticInput(input);if(row)NeoDiagnosticOrder(*row,"CLEAR_INPUT_BEGIN",reinterpret_cast<std::uintptr_t>(_ReturnAddress()));
 neoOriginalClear(input);
 if(row)NeoDiagnosticOrder(*row,"CLEAR_INPUT_END",reinterpret_cast<std::uintptr_t>(_ReturnAddress()));
}
static NeoRuntimeState* NeoDiagnosticObject(void* object,bool goal){
 for(auto& row:neoRuntime)if(row.owned&&NeoDiagnosticTarget(row)&&row.diagnosticAt&&GetTickCount64()-row.diagnosticAt<NeoDiagnosticWindow()){
  void* current=static_cast<BYTE*>(row.ai)-0x4C;
  if(goal)AudioRead(static_cast<BYTE*>(current)+0xB8,&current);
  if(current==object)return &row;
 }return nullptr;
}
static void __fastcall NeoDiagnosticGoalUpdate(void* goal,void*,float dt){
 auto row=NeoDiagnosticObject(goal,true);if(row)NeoDiagnosticOrder(*row,"GOAL_ACTION_BEGIN",reinterpret_cast<std::uintptr_t>(_ReturnAddress()));
 neoOriginalGoalUpdate(goal,dt);
 if(row)NeoDiagnosticOrder(*row,"GOAL_ACTION_END",reinterpret_cast<std::uintptr_t>(_ReturnAddress()));
}
static void __fastcall NeoRacerUpdate(void* primary,void*,float dt){
 auto row=NeoDiagnosticObject(primary,false);if(row)NeoDiagnosticOrder(*row,"AI_MECHANIC_BEGIN",reinterpret_cast<std::uintptr_t>(_ReturnAddress()));
 auto previous=neoUpdatingPrimary;bool priorDrove=neoUpdateDrove;neoUpdatingPrimary=primary;neoUpdateDrove=false;
 neoOriginalRacerUpdate(primary,dt);
 if(!neoUpdateDrove)NeoResumeAfterNativeAction(primary,dt);
 neoUpdatingPrimary=previous;neoUpdateDrove=priorDrove;
 if(row)NeoDiagnosticOrder(*row,"AI_MECHANIC_END",reinterpret_cast<std::uintptr_t>(_ReturnAddress()));
}
static void __cdecl NeoDiagnosticPacketObserved(void* primary,void* packet,std::uintptr_t caller){
 void* input=nullptr;AudioRead(static_cast<BYTE*>(primary)+0x58,&input);auto row=NeoDiagnosticInput(input);
 if(row){float steer=0,gas=0,brake=0,hand=0;AudioRead(static_cast<BYTE*>(packet)+0x8C,&steer);AudioRead(static_cast<BYTE*>(packet)+0x4C,&gas);AudioRead(static_cast<BYTE*>(packet)+0x5C,&brake);AudioRead(static_cast<BYTE*>(packet)+0x7C,&hand);
  Log(LogLevel::Info,"NEO_PACKET seq=%llu frame=%u vehicle=%u phase=%u caller=%08X requested_steer=%.6f requested_throttle=%.6f requested_brake=%.6f final_steer=%.6f final_throttle=%.6f final_brake=%.6f final_handbrake=%.6f",++neoDiagnosticSequence,row->ticks,row->handle,row->diagnosticPhase,unsigned(caller),row->requested[0],row->requested[1],row->requested[2],steer,gas,brake,hand);
 }
}
static void __declspec(naked) NeoDiagnosticPacket(){
 __asm {
  push ecx
  push dword ptr [esp+12]
  push dword ptr [esp+12]
  call dword ptr [neoOriginalPacket]
  pushfd
  pushad
  mov ebx,esp
  and esp,0FFFFFFF0h
  sub esp,512
  fxsave [esp]
  push dword ptr [ebx+40]
  push dword ptr [ebx+48]
  push dword ptr [ebx+36]
  call NeoDiagnosticPacketObserved
  add esp,12
  fxrstor [esp]
  mov esp,ebx
  popad
  popfd
  lea esp,[esp+4]
  ret 8
 }
}
static void InstallNeoOwnershipDiagnostic(){
 if(!MWELNeoLoggingEnabled())return;
 wchar_t executable[MAX_PATH]{};if(!GetModuleFileNameW(nullptr,executable,MAX_PATH))return;
 std::wstring root=executable;auto slash=root.find_last_of(L"\\/");if(slash==std::wstring::npos)return;root.resize(slash+1);
 auto ini=root+L"scripts\\EventLoader\\EventLoader.ini";
 unsigned mode=GetPrivateProfileIntW(L"NeoAI",L"OwnershipDiagnostic",0,ini.c_str());
#ifdef MWEL_NEO_OBSERVE_ONLY
 if(mode==7&&!MWELNeoLoggingEnabled())return;
 if(mode<1||mode>7)return;
#else
 if(mode<1||mode>6)return;
#endif
 // Marker is separate from the existing race test marker. Never auto-create.
 auto marker=root+L"MWEventLoader.ownership-test";
 HANDLE file=CreateFileW(marker.c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
 if(file==INVALID_HANDLE_VALUE)return;char bytes[64]{};DWORD read=0;ReadFile(file,bytes,63,&read,nullptr);CloseHandle(file);
 if(strcmp(bytes,"MW_NEOAI_OWNERSHIP_LOCAL_ONLY_V1\n"))return;
 const BYTE getter[]={0x8D,0x41,0x08,0xC3,0xCC,0xCC,0xCC,0xCC};
 const BYTE clear[]={0x83,0xEC,0x24,0x56,0x57,0x32,0xC0};
 const BYTE packet[]={0x83,0xEC,0x10,0x53,0x55,0x8B,0xD9};
 const BYTE goal[]={0x56,0x57,0x8B,0x7C,0x24,0x0C};
 if(memcmp(reinterpret_cast<void*>(Address(0x69CBB0)),getter,sizeof(getter))||memcmp(reinterpret_cast<void*>(Address(0x68CEE0)),clear,sizeof(clear))||memcmp(reinterpret_cast<void*>(Address(0x69D4D0)),packet,sizeof(packet))||memcmp(reinterpret_cast<void*>(Address(0x40A6D0)),goal,sizeof(goal))){Log(LogLevel::Warning,"NEO_DIAGNOSTIC_REFUSED ABI=1");return;}
 // The mechanic tick is always hooked by the production owner-recovery path;
 // diagnostics share that hook instead of trying to attach a second one.
 const unsigned addresses[]={0x69CBB0,0x68CEE0,0x69D4D0,0x40A6D0};void* hooks[]={&NeoDiagnosticGetter,&NeoDiagnosticClear,&NeoDiagnosticPacket,&NeoDiagnosticGoalUpdate};void** originals[]={reinterpret_cast<void**>(&neoOriginalGetter),reinterpret_cast<void**>(&neoOriginalClear),reinterpret_cast<void**>(&neoOriginalPacket),reinterpret_cast<void**>(&neoOriginalGoalUpdate)};
 bool created[4]{};bool ok=true;
 for(unsigned i=0;i<std::size(addresses);++i){if(mode==7&&i!=0&&i!=2)continue;if(MH_CreateHook(reinterpret_cast<void*>(Address(addresses[i])),hooks[i],originals[i])!=MH_OK){ok=false;break;}created[i]=true;}
 if(ok)for(unsigned i=0;i<std::size(addresses);++i)if(created[i])ok=MH_EnableHook(reinterpret_cast<void*>(Address(addresses[i])))==MH_OK&&ok;
 if(!ok)for(unsigned i=0;i<std::size(addresses);++i)if(created[i]){MH_DisableHook(reinterpret_cast<void*>(Address(addresses[i])));MH_RemoveHook(reinterpret_cast<void*>(Address(addresses[i])));}
 if(ok)neoDiagnosticMode=mode;
 Log(LogLevel::Info,"NEO_DIAGNOSTIC mode=%u hooks=%u local_eligibility_required=1 single_opponent=1",mode,unsigned(ok));
}
