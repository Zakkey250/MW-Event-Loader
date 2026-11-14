#pragma once
#include <NFSPluginSDK/Game.MW05/Types/IDamageable.h>
namespace rpd_officer_handoff {
using combat_health_runtime::Read;
unsigned disabled=0,disabledVehicle=0,epoch=0,stopOwned=0;float waiting=0,lastTime=-1,blend=0,fade=0;
bool switched=false,nativeDefeat=false,distant=false,cameraReady=false;
std::array<float,16> lastCamera{},fromCamera{};
struct AudioLease{unsigned handle=0,mechanic=0;};
std::vector<AudioLease> playerAudio;
std::vector<unsigned> wrecks;
std::vector<combat_health_runtime::Binding> retiredHealth;
struct PlayerVehicle {unsigned handle=0,vehicle=0;std::unique_ptr<NFSPluginSDK::MW05::FECustomizationRecord> customization;};
std::vector<PlayerVehicle> playerVehicles;
unsigned pendingPlayer=0,pendingAlly=0,reservedAlly=0,reservedHandle=0;
unsigned homeHandle=0,homeVehicle=0,currentHandle=0,homeCharacter=0;
NFSPluginSDK::MW05::Sim::IEntity* localEntity=nullptr;
NFSPluginSDK::MW05::IPlayer* localPlayer=nullptr;
bool returning=false;
bool testNoAlly=false,cameraReported=false;
NFSPluginSDK::MW05::IVehicle* Vehicle(unsigned v){return reinterpret_cast<NFSPluginSDK::MW05::IVehicle*>(v);}
NFSPluginSDK::MW05::ISimable* Sim(unsigned v){return reinterpret_cast<NFSPluginSDK::MW05::ISimable*>(v-0x80);}
bool BodyMatrix(NFSPluginSDK::MW05::IRigidBody* body,float* matrix,bool write){
 // The SDK reverses the native Matrix4/Quaternion virtual overload slots.
 // EN1.3 slot 28 consumes 64 matrix bytes; slot 29 consumes a quaternion.
 // Calling the declared Matrix4 overload selected slot 29 and rolled the car.
 unsigned table=0,get=0,set=0;if(!body||!matrix||!Read(unsigned(body),table)||
  !Read(table+0x40,get)||get!=0x6712A0||!Read(table+0x70,set)||set!=0x696D90)return false;
 reinterpret_cast<void(__thiscall*)(void*,float*)>(write?set:get)(body,matrix);return true;
}
bool BindPlayerAudio(unsigned v){
 unsigned manager=0,com=0,voice=0,state=0,context=~0u;
 if(!Read(0x911F68,manager)||!manager||!Read(v+4,com)||!com)return false;
 const auto audible=reinterpret_cast<unsigned(__thiscall*)(unsigned,unsigned)>(0x5D59F0)(com,0x694080);
 if(audible<0x4C||
    !gauntlet_runtime::Table(audible-0x4C,0x8AC0FC)||!Read(audible-0x4C+0x5C,voice)||
    !Read(voice+0x14,state)||!Read(state+0x228,context)||context!=0)return false;
 const BYTE expected[]={0xA1,0xBC,0x3D,0x91,0,0x56,0x57,0x33,0xFF,0x33,0xF6};
 if(memcmp(reinterpret_cast<void*>(0x4C2100),expected,sizeof(expected)))return false;
 // EAXSound::AttachPlayerCars is normally called only by MixMapReady.
 // A retained wreck otherwise leaves the replacement orphaned for the rest
 // of the race. Use that native attachment, including all player SFX objects.
 reinterpret_cast<void(__cdecl*)()>(0x4C2100)();
 const auto bound=reinterpret_cast<unsigned(__thiscall*)(unsigned,unsigned)>(0x4D96B0)(manager,state);
 Log("RPD_PLAYER_SFX_BIND handle=%u state=%08X native_player_mixer=%08X native_player_state_attached=%u",hot_pursuit_runtime::Handle(v),state,bound,unsigned(bound!=0));return bound!=0;
}
bool RetirePlayerAudio(unsigned v){
 unsigned com=0;Read(v+4,com);if(!com)return false;
 const auto audible=reinterpret_cast<unsigned(__thiscall*)(unsigned,unsigned)>(0x5D59F0)(com,0x694080);
 const auto sound=audible>=0x4C?audible-0x4C:0;unsigned mechanic=0;
 if(!gauntlet_runtime::Table(sound,0x8AC0FC)||!Read(sound+0x38,mechanic)||!mechanic)return false;
 const auto signature=reinterpret_cast<unsigned(__cdecl*)(const char*)>(0x5CC240)("SoundTraffic");
 const auto factory=rpd_police_autodrive::Factory(signature);if(factory<0x401000||factory>=0x889000)return false;
 reinterpret_cast<void(__thiscall*)(unsigned,unsigned,unsigned)>(0x6857A0)(v,mechanic,signature);
 reinterpret_cast<void(__thiscall*)(unsigned)>(0x688370)(v);
 playerAudio.push_back({hot_pursuit_runtime::Handle(v),mechanic});
 Log("RPD_PLAYER_AUDIO_RETIRED handle=%u native_traffic_factory=%08X full_player_sfx_slot_released=1",hot_pursuit_runtime::Handle(v),factory);return true;
}
void RepairRetainedPlayer(unsigned v){
 unsigned com=0;if(!Read(v-0x80+4,com)||!com)return;
 const auto damage=reinterpret_cast<unsigned(__thiscall*)(unsigned,unsigned)>(0x5D59F0)(com,0x405000);
 unsigned table=0,reset=0;if(!damage||!Read(damage,table)||!Read(table+0x10,reset)||reset<0x401000||reset>=0x889000)return;
 reinterpret_cast<NFSPluginSDK::MW05::IDamageable*>(damage)->ResetDamage();
 Log("RPD_RETAINED_PLAYER_REPAIRED handle=%u native_damage_reset=1",hot_pursuit_runtime::Handle(v));
}
void ReturnOriginal(){
 if(returning||!homeHandle||!localEntity||gauntlet_runtime::Vehicle(homeHandle)!=homeVehicle)return;
 unsigned list=0,count=0;bool live=false;
 if(Read(0x92D87C,list)&&Read(0x92D884,count)&&count<=4)for(unsigned i=0;i<count;++i){unsigned player=0;if(Read(list+i*4,player)&&player==unsigned(localPlayer))live=true;}
 if(!live)return;
 const auto current=gauntlet_runtime::Vehicle(currentHandle);
 returning=true;
 rpd_controls::BeginTransition();
 // The loading owner must be retained, but it must not drag either the player
 // or an exit camera back to the crash site. Prepare it at the current body's
 // exact pose before reattachment; never ray-snap to a neighbouring road level.
 if(current&&current!=homeVehicle){
  auto from=Sim(current)->GetRigidBody();
  if(!from){returning=false;return;}
  std::array<float,16> matrix{};if(!BodyMatrix(from,matrix.data(),false)){returning=false;return;}
  const auto position=from->GetPosition(),velocity=from->GetLinearVelocity(),angular=from->GetAngularVelocity();
  Vehicle(homeVehicle)->SetDriverClass(NFSPluginSDK::MW05::DriverClass::Human);Vehicle(homeVehicle)->Activate();
  Vehicle(homeVehicle)->SetStaging(false);RepairRetainedPlayer(homeVehicle);
  auto to=Sim(homeVehicle)->GetRigidBody();if(!to){returning=false;return;}
  if(!BodyMatrix(to,matrix.data(),true)){returning=false;return;}
  to->SetPosition(position);to->SetLinearVelocity(velocity);to->SetAngularVelocity(angular);
  std::array<float,16> verified{};if(!BodyMatrix(to,verified.data(),false)){returning=false;return;}
  float error=0;for(unsigned i=0;i<12;++i)error=std::max(error,std::abs(matrix[i]-verified[i]));
  if(error>.001f){Log("RPD_HANDOFF_RETURN_REFUSED matrix_error=%.5f",error);returning=false;return;}
  Log("RPD_HANDOFF_RETURN_POSE current_handle=%u retained_owner=%u exact_body_pose=1 road_snap=0 matrix_error=%.5f",currentHandle,homeHandle,error);
 }
 rpd_police_autodrive::Reset();
 if(current&&current!=homeVehicle)RetirePlayerAudio(current);
 for(auto it=playerAudio.begin();it!=playerAudio.end();)if(it->handle==homeHandle){
  reinterpret_cast<void(__thiscall*)(unsigned,unsigned)>(0x683500)(homeVehicle,it->mechanic);
  reinterpret_cast<void(__thiscall*)(unsigned)>(0x688370)(homeVehicle);it=playerAudio.erase(it);
 }else ++it;
 // GRacerInfo must again name the original race-owned loan car before the
 // native retry tears down simulation. A pooled cop is not its replacement
 // loading owner; leaving it here makes LocalPlayer::GetSimable null.
 if(current&&current!=homeVehicle&&Sim(current)->GetEntity()==localEntity)Sim(current)->DetachEntity();
 Vehicle(homeVehicle)->SetDriverClass(NFSPluginSDK::MW05::DriverClass::Human);
 Sim(homeVehicle)->AttachEntity(localEntity);
 BindPlayerAudio(homeVehicle);
 rpd_controls::Restore("return_original");rpd_controls::Forget();
 if(homeCharacter){
  auto character=reinterpret_cast<NFSPluginSDK::MW05::GCharacter*>(homeCharacter);
  auto attachment=static_cast<NFSPluginSDK::MW05::IAttachable*>(character);
  if(current&&character->mVehicle==Vehicle(current))Sim(current)->Detach(attachment);
  if(character->mVehicle!=Vehicle(homeVehicle))Sim(homeVehicle)->Attach(attachment);
 }
 unsigned race=0,slot=0;if(Read(0x91E000,race)&&Read(race+0x18,slot)&&slot==currentHandle)*reinterpret_cast<unsigned*>(race+0x18)=homeHandle;
 if(stopOwned)Vehicle(homeVehicle)->ForceStopOff(static_cast<NFSPluginSDK::MW05::IVehicle::ForceStopType>(stopOwned));
 stopOwned=0;
 const bool attached=Sim(homeVehicle)->IsPlayer()&&localEntity->GetSimable()==Sim(homeVehicle);
 if(attached){
  if(current&&current!=homeVehicle){Vehicle(current)->SetDriverClass(NFSPluginSDK::MW05::DriverClass::None);Vehicle(current)->Deactivate();}
  wrecks.erase(std::remove(wrecks.begin(),wrecks.end(),homeHandle),wrecks.end());
  Log("RPD_HANDOFF_RETURN_ORIGINAL handle=%u local_player_sim_verified=1 native_retry_loading_owner=1",homeHandle);
  homeHandle=homeVehicle=currentHandle=homeCharacter=0;localEntity=nullptr;localPlayer=nullptr;
 }else Log("RPD_HANDOFF_RETURN_REFUSED native_player_binding_not_restored=1");
 returning=false;
}
bool PrepareResult(){
 if(!homeHandle)return true;
 ReturnOriginal();
 // Native result cameras retain the race-owned loading vehicle. Reattach it
 // at the current officer's repaired physical pose before they acquire it.
 return homeHandle==0;
}
void RetireDistantWrecks(){
 const auto current=gauntlet_runtime::Vehicle(currentHandle);combat_impact::Motion own;
 if(!current||!Sim(current)->IsPlayer()||!combat_health_runtime::NativeMotion(current-0x80,own))return;
 for(auto it=wrecks.begin();it!=wrecks.end();){const auto handle=*it,v=gauntlet_runtime::Vehicle(handle);unsigned driver=~0u;
  if(!v){it=wrecks.erase(it);continue;}
  combat_impact::Motion old;
  if(v==current||Sim(v)->IsPlayer()||!Read(v+0x94,driver)||driver!=4||!combat_health_runtime::NativeMotion(v-0x80,old)||
     combat_impact::Dot(old.position-own.position,old.position-own.position)<250.f*250.f||Vehicle(v)->GetOffScreenTime()<2.f){++it;continue;}
  if(handle==homeHandle){
   // GRaceStatus owns this one loading/cache object until retry/exit. Remove
   // it from active simulation; deleting it would break native retry ownership.
   if(Vehicle(v)->IsActive()){Vehicle(v)->Deactivate();Log("RPD_WRECK_RETIRED handle=%u retained_loading_owner=1 native_deactivate=1 offscreen=1",handle);}
   ++it;continue;
  }
  if(!gauntlet_runtime::Table(v-0x80,0x8AA940)){++it;continue;}
  playerAudio.erase(std::remove_if(playerAudio.begin(),playerAudio.end(),[&](const auto& r){return r.handle==handle;}),playerAudio.end());
  retiredHealth.erase(std::remove_if(retiredHealth.begin(),retiredHealth.end(),[&](const auto& r){return r.handle==handle;}),retiredHealth.end());
  Sim(v)->Kill();Log("RPD_WRECK_RETIRED handle=%u retained_loading_owner=0 native_kill=1 offscreen=1",handle);it=wrecks.erase(it);
 }
}
void Reset(){
 ReturnOriginal();
 // Slot zero is rebound at possession; retain ownership of the former
 // native damage write until race teardown, just like the active ledger.
 for(const auto& binding:retiredHealth){float value=0;
  if(combat_health_runtime::Native(binding.damage,binding.handle)&&Read(binding.damage+0x3C,value)&&std::abs(value-binding.written)<.00001f)
   *reinterpret_cast<float*>(binding.damage+0x3C)=binding.original;
 }
 retiredHealth.clear();
 if(stopOwned&&disabledVehicle&&gauntlet_runtime::Vehicle(disabled)==disabledVehicle)
  Vehicle(disabledVehicle)->ForceStopOff(static_cast<NFSPluginSDK::MW05::IVehicle::ForceStopType>(stopOwned));
 stopOwned=0;
 for(const auto& row:playerAudio){const auto v=gauntlet_runtime::Vehicle(row.handle);
  if(v){reinterpret_cast<void(__thiscall*)(unsigned,unsigned)>(0x683500)(v,row.mechanic);reinterpret_cast<void(__thiscall*)(unsigned)>(0x688370)(v);}
 }
 playerAudio.clear();
 for(auto& row:playerVehicles)if(gauntlet_runtime::Vehicle(row.handle)==row.vehicle&&!Sim(row.vehicle)->IsPlayer())Sim(row.vehicle)->Kill();
 playerVehicles.erase(std::remove_if(playerVehicles.begin(),playerVehicles.end(),[](const auto& row){return gauntlet_runtime::Vehicle(row.handle)!=row.vehicle;}),playerVehicles.end());
 pendingPlayer=pendingAlly=reservedAlly=reservedHandle=0;
 for(auto handle:wrecks){const auto v=gauntlet_runtime::Vehicle(handle);unsigned driver=~0u;
  if(v&&Read(v+0x94,driver)&&driver==4&&gauntlet_runtime::Table(v-0x80,0x8AA940)&&!Sim(v)->IsPlayer())Sim(v)->Kill();
 }
 testNoAlly=cameraReported=false;wrecks.clear();disabled=disabledVehicle=epoch=0;waiting=blend=fade=0;lastTime=-1;switched=nativeDefeat=distant=cameraReady=false;
 homeHandle=homeVehicle=currentHandle=homeCharacter=0;localEntity=nullptr;localPlayer=nullptr;
}
unsigned Ally(float radius=150.f);
void Message(){
 unsigned count=0,list=0,player=0,hud=0,com=0;
 if(!Read(0x92D884,count)||!count||!Read(0x92D87C,list)||!Read(list,player)||!player)return;
 hud=reinterpret_cast<unsigned(__thiscall*)(unsigned)>(0x6F8F10)(player);
 if(!Read(hud+4,com)||!com)return;
 const auto generic=reinterpret_cast<unsigned(__thiscall*)(unsigned,unsigned)>(0x5D59F0)(com,0x5650B0);
 if(!generic)return;
 const auto key=event_presentation::Hash("MWEL_RPD_OFFICER_DISABLED");
 event_titles::titles.emplace(key,event_titles::Title{L"VEHICLE DISABLED", "MWEL_RPD_OFFICER_DISABLED",{},L"\u8eca\u4e21\u5927\u7834"});
 const char* text=event_titles::Lookup(nullptr,key);
 reinterpret_cast<bool(__thiscall*)(unsigned,const char*,bool,unsigned,unsigned,unsigned,unsigned)>(0x568030)(generic,text,false,0x8AB83EDB,0,0,2);
}
bool Begin(unsigned handle){
 const auto policy=event_race_policies::Current();
 if(!policy||!mwel_rpd::Armed(policy->authoring)||!mwel_rpd::OfficerHandoff(policy->authoring))return false;
 if(nativeDefeat)return disabled==handle;
 if(disabled==handle)return true;
 const auto v=gauntlet_runtime::Vehicle(handle);
 if(!v||!gauntlet_runtime::Table(v-0x80,0x8AA940)||!Sim(v)->IsPlayer())return false;
 disabled=handle;disabledVehicle=v;epoch=combat_health_runtime::generation;waiting=0;switched=false;
 Read(0x9885D8,lastTime);
 stopOwned=3u&~unsigned(Vehicle(v)->GetForceStop());
 Vehicle(v)->ForceStopOn(static_cast<NFSPluginSDK::MW05::IVehicle::ForceStopType>(3));Message();
 reservedAlly=Ally();reservedHandle=reservedAlly?hot_pursuit_runtime::Handle(reservedAlly):0;
 nativeDefeat=!reservedAlly;
 Log("RPD_OFFICER_DISABLED handle=%u pseudo_destroy=1 native_destroy_event=0 wait_seconds=3 nearby_limit_metres=150 immediate_failure=%u",handle,unsigned(nativeDefeat));return true;
}
unsigned Ally(float radius){
 if(testNoAlly)return 0;
 unsigned race=0,suspect=0;Read(0x91E000,race);Read(race+0x18+0x194,suspect);
 if(MWELRpdHotPursuitScope())suspect=MWELRpdSprintTarget(disabled);
 const auto group=hot_pursuit_runtime::FindPursuit(suspect);if(!group)return 0;
 combat_impact::Motion own{};if(!combat_health_runtime::NativeMotion(disabledVehicle-0x80,own))return 0;
 std::vector<unsigned> cars;if(!hot_pursuit_runtime::List(cars))return 0;
 unsigned best=0;float bestDistance=radius*radius;
 for(auto v:cars){unsigned driver=~0u,ai=0,activity=0,roadblock=0;
  if(v==disabledVehicle||!Read(v+0x94,driver)||driver!=2||!gauntlet_runtime::Table(v-0x80,0x8AA940)||
     !Read(v+0x54,ai)||!gauntlet_runtime::Table(ai,0x892480)||!Read(ai+0x70,activity)||(activity&&hot_pursuit_runtime::TargetHandle(activity)!=suspect)||
     !Read(ai+0x74,roadblock)||roadblock||Vehicle(v)->IsDestroyed()||!Vehicle(v)->IsActive()||Sim(v)->IsPlayer())continue;
  combat_impact::Motion m;if(!combat_health_runtime::NativeMotion(v-0x80,m))continue;
  const auto delta=m.position-own.position;float d=combat_impact::Dot(delta,delta);
  if(d<bestDistance){best=v;bestDistance=d;}
 }
 distant=best&&bestDistance>150.f*150.f;return best;
}
unsigned EstablishPlayerVehicle(unsigned ally,unsigned race){
 using namespace NFSPluginSDK::MW05;
 if(pendingPlayer){
  const auto v=gauntlet_runtime::Vehicle(pendingPlayer);
  if(!v){pendingPlayer=pendingAlly=0;return 0;}
  if(ally!=pendingAlly||Vehicle(v)->IsLoading()){
   if(worldLogEnabled){static ULONGLONG report=0;const auto now=GetTickCount64();if(now>=report){report=now+1000;
    unsigned render=0,audio=0;Read(v+0x5C,render);Read(v+0x60,audio);
    auto ready=[](unsigned object,unsigned slot){unsigned table=0,fn=0;if(!Read(object,table)||!Read(table+slot*4,fn)||fn<0x401000||fn>=0x889000)return ~0u;return unsigned(reinterpret_cast<bool(__thiscall*)(unsigned)>(fn)(object));};
    Log("RPD_PLAYER_LOADING_WAIT handle=%u render_ready=%u audio_ready=%u native_loading=%u",pendingPlayer,ready(render,2),ready(audio,1),unsigned(Vehicle(v)->IsLoading()));
   }}return 0;
  }
  Log("RPD_PLAYER_LOADING_COMPLETE handle=%u native_loading=0 before_local_attachment=1",pendingPlayer);
  pendingPlayer=pendingAlly=0;return v;
 }
 auto source=Vehicle(ally);auto cache=static_cast<IVehicleCache*>(reinterpret_cast<GRaceStatus*>(race));
 if(!gauntlet_runtime::Table(unsigned(cache),0x8A3A60))return 0;
 auto prototype=source->GetCustomizations();auto original=Vehicle(homeVehicle?homeVehicle:disabledVehicle)->GetCustomizations();
 if(!prototype)prototype=original;if(!prototype||!original)return 0;
 auto customization=std::make_unique<FECustomizationRecord>(*original);
 unsigned nativeKey=0;if(!MWELRpdPlayerSpecification(source->GetVehicleKey(),customization.get(),&nativeKey))return 0;
 // Preserve the actual ally's vehicle/appearance. Player NOS equipment uses
 // the mission's genuine loan-car package, never a fabricated tank/boost.
 customization->mInstalledPhysics.mNOS=original->mInstalledPhysics.mNOS;
 combat_impact::Motion pose;if(!combat_health_runtime::NativeMotion(ally-0x80,pose))return 0;
 // The SDK exposes its legacy vector fields as y,z,x. Native vehicle
 // construction consumes the physical XYZ byte layout, like the body API.
 UMath::Vector3 p{},h{};memcpy(&p,&pose.position,12);memcpy(&h,&pose.forward,12);
 // These are the native player spawn flags: use its already resident
 // model and native NOS installation, not the NPC/default spawn flags.
 Log("RPD_PLAYER_ARCHETYPE ally_key=%08X native_player_key=%08X",source->GetVehicleKey(),nativeKey);
 VehicleParams params(DriverClass::Human,nativeKey,h,p,customization.get(),eVehicleParamFlags::CalcPerformance|eVehicleParamFlags::NosAdded,cache,nullptr);
 auto created=PVehicle::Construct(params);if(!created)return 0;
 const auto next=unsigned(static_cast<IVehicle*>(created)),handle=hot_pursuit_runtime::Handle(next);
 Vehicle(next)->Activate();Vehicle(next)->SetStaging(false);
 if(!handle||Vehicle(next)->GetDriverClass()!=DriverClass::Human){
  Log("RPD_PLAYER_CREATION_REFUSED handle=%u spooled=%u driver=%u native_full_player_not_ready=1",handle,unsigned(Vehicle(next)->IsSpooled()),unsigned(Vehicle(next)->GetDriverClass()));Sim(next)->Kill();return 0;
 }
 playerVehicles.push_back({handle,next,std::move(customization)});
 // Start all native player mechanics while the former local sim remains
 // connected. Attaching a loading player freezes simulation and prevents
 // those same native resources from finishing their initialization.
 unsigned audioCom=0;Read(next+4,audioCom);
 const auto audible=audioCom?reinterpret_cast<unsigned(__thiscall*)(unsigned,unsigned)>(0x5D59F0)(audioCom,0x694080):0;
 const auto sound=audible>=0x4C?audible-0x4C:0;unsigned mechanic=0;
 if(gauntlet_runtime::Table(sound,0x8AC0B4)&&Read(sound+0x38,mechanic)&&mechanic){
  const auto signature=reinterpret_cast<unsigned(__cdecl*)(const char*)>(0x5CC240)("SoundRacer");
  if(rpd_police_autodrive::Factory(signature)!=0x6B4C20){Sim(next)->Kill();return 0;}
  reinterpret_cast<void(__thiscall*)(unsigned,unsigned,unsigned)>(0x6857A0)(next,mechanic,signature);
  reinterpret_cast<void(__thiscall*)(unsigned)>(0x688370)(next);
 }
 Log("RPD_PLAYER_NATIVE_READY handle=%u loading=%u active=%u physics=%u",handle,unsigned(Vehicle(next)->IsLoading()),unsigned(Vehicle(next)->IsActive()),unsigned(Vehicle(next)->GetPhysicsMode()));
 Log("RPD_PLAYER_CREATED ally=%u player=%u native_human_constructor=00689820 cache=GRaceStatus spooled=%u",hot_pursuit_runtime::Handle(ally),handle,unsigned(Vehicle(next)->IsSpooled()));
 pendingPlayer=handle;pendingAlly=ally;return 0;
}
bool Transfer(unsigned next){
 const auto old=disabledVehicle;unsigned race=0,handle=hot_pursuit_runtime::Handle(next),current=0;
 if(!old||gauntlet_runtime::Vehicle(disabled)!=old||!next||!handle||!Read(0x91E000,race)||!Read(race+0x18,current)||current!=disabled)return false;
 auto entity=Sim(old)->GetEntity();if(!entity||!Sim(old)->IsPlayer())return false;
 unsigned characterAddress=0;Read(race+0x18+4,characterAddress);
 auto character=reinterpret_cast<NFSPluginSDK::MW05::GCharacter*>(characterAddress);
 auto attachment=character?static_cast<NFSPluginSDK::MW05::IAttachable*>(character):nullptr;
 NFSPluginSDK::MW05::IVehicle* characterVehicle=nullptr;
 if(character&&(!Copy(&character->mVehicle,&characterVehicle,sizeof(characterVehicle))||characterVehicle!=Vehicle(old))){
  Log("RPD_HANDOFF_REFUSED character_binding_mismatch=1 character=%08X expected_vehicle=%08X observed_vehicle=%08X",characterAddress,old,unsigned(characterVehicle));return false;
 }
 if(!homeHandle){homeHandle=disabled;homeVehicle=old;homeCharacter=characterAddress;localEntity=entity;localPlayer=Sim(old)->GetPlayer();}
 rpd_controls::BeginTransition();
 const auto ally=next;next=EstablishPlayerVehicle(ally,race);if(!next)return false;
 handle=hot_pursuit_runtime::Handle(next);

 // Remove both ICop memberships before SetDriverClass/behaviour commits.
 rpd_police_autodrive::Reset();
 ++hot_pursuit_runtime::ownershipDepth;
 for(auto group:hot_pursuit_runtime::NativeGroups())if(gauntlet_runtime::Table(group,0x892770))
  reinterpret_cast<bool(__thiscall*)(unsigned,unsigned)>(0x4338B0)(group,ally);
 --hot_pursuit_runtime::ownershipDepth;
 // Follow the native ChangePlayerVehicle ordering: Attach the new sim first.
 // LocalPlayer's attachment callback detaches its former physics itself.
 const bool playerAttached=Sim(next)->Attach(localPlayer);
 if(!playerAttached||!Sim(next)->IsPlayer()||Sim(old)->IsPlayer()||localPlayer->GetSimable()!=Sim(next)){
  Sim(old)->AttachEntity(entity);Vehicle(next)->SetDriverClass(NFSPluginSDK::MW05::DriverClass::Cop);return false;
 }
 // Move the original native race-character attachment too. Its interfaces
 // own racer statistics/preparation and its native vehicle lifecycle; changing
 // only GRacerInfo's handle leaves SoundRacer and race cleanup without them.
 *reinterpret_cast<unsigned*>(race+0x18)=handle;
 const bool detached=!attachment||Sim(old)->Detach(attachment),attached=detached&&(!attachment||Sim(next)->Attach(attachment));
 if(!attached||(character&&character->mVehicle!=Vehicle(next))){
  if(detached&&attachment)Sim(old)->Attach(attachment);*reinterpret_cast<unsigned*>(race+0x18)=current;
  Sim(next)->DetachEntity();Sim(old)->AttachEntity(entity);Vehicle(next)->SetDriverClass(NFSPluginSDK::MW05::DriverClass::Cop);
  Log("RPD_HANDOFF_REFUSED native_character_attachment=1");return false;
 }
 unsigned raceCom=0;Read(next-0x80+4,raceCom);
 const auto raceInterface=raceCom?reinterpret_cast<unsigned(__thiscall*)(unsigned,unsigned)>(0x5D59F0)(raceCom,0x604250):0;
 Log("RPD_HANDOFF_RACE_CHARACTER native_detach=%u native_attach=%u original_character=%08X racer_interface=%08X",unsigned(attachment&&detached),unsigned(attachment&&attached),characterAddress,raceInterface);
 *reinterpret_cast<unsigned*>(race+0x18)=handle;
 Vehicle(old)->SetDriverClass(NFSPluginSDK::MW05::DriverClass::None);
 wrecks.push_back(disabled);
 // The racer slot/timer survives. Only its vehicle and transient HP binding change.
 retiredHealth.push_back(combat_health_runtime::bindings[0]);
 combat_health_runtime::bindings[0]={};combat_health_runtime::ledger.racers[0]={};
 combat_health_runtime::motions[0]={};combat_health_runtime::impactContacts[0]={};
 cameraReported=false;fromCamera=lastCamera;blend=distant?0:1.5f;fade=distant?1.f:0;
 disabled=disabledVehicle=0;switched=true;
 currentHandle=handle;
 // The native mixer has one local-player SFX state. The retained wreck must
 // release that state before the replacement's real SoundRacer can claim it.
 RetirePlayerAudio(old);
 // DriverClass::None alone still leaves native behavior tasks alive. Keep
 // the race-owned object for retry, but stop its simulation immediately.
 Vehicle(old)->ForceStopOn(static_cast<NFSPluginSDK::MW05::IVehicle::ForceStopType>(3));
 Vehicle(old)->Deactivate();
 Log("RPD_WRECK_DISABLED handle=%u native_deactivate=1 retained_loading_owner=%u",disabled?disabled:hot_pursuit_runtime::Handle(old),unsigned(old==homeVehicle));
 BindPlayerAudio(next);
 Log("RPD_PLAYER_NATIVE_COMMITTED handle=%u loading=%u old_is_player=%u native_sound_retained=1",handle,unsigned(Vehicle(next)->IsLoading()),unsigned(Sim(old)->IsPlayer()));
 // All bindings are committed before retiring the pooled NPC.
 Sim(ally)->Kill();
 rpd_police_autodrive::Frame();
 Log("RPD_OFFICER_HANDOFF new_handle=%u native_player_attachment=1 camera=%s timer_retained=1",handle,distant?"fade":"smooth");return true;
}
void Frame(){
 if(rpd_mission::Current()&&currentHandle)RetireDistantWrecks();
 if(nativeDefeat||rpd_mission::ResultPending()){fade=0;return;}
 if(!rpd_mission::Current()||!combat_health_runtime::Racing())return;
 float now=0;if(!Read(0x9885D8,now))return;
 const float dt=lastTime>=0?std::clamp(now-lastTime,0.f,.1f):0;lastTime=now;
 if(blend>0)blend=std::max(0.f,blend-dt);
 if(switched){fade=std::max(0.f,fade-dt*2);return;}
 if(!disabled||epoch!=combat_health_runtime::generation)return;
 waiting+=dt;
 if(waiting<3){if(waiting>=2.5f){Ally();if(distant)fade=std::clamp((waiting-2.5f)*2,0.f,1.f);}return;}
 const auto next=reservedHandle&&gauntlet_runtime::Vehicle(reservedHandle)==reservedAlly&&!Vehicle(reservedAlly)->IsDestroyed()?reservedAlly:0;
 if(next&&Transfer(next))return;
 if(waiting<(pendingPlayer?12.f:5.f))return; // Complete native resource loading before possession.
 nativeDefeat=true;

 Log("RPD_OFFICER_HANDOFF_UNAVAILABLE native_defeat_menu=1 no_surviving_ally=%u player_initialization_failed=%u native_destroy=0",unsigned(!next),unsigned(next!=0));
}
bool Camera(float* matrix){
 if(!matrix||!rpd_mission::Current())return false;
 if(blend>0&&cameraReady){
  if(worldLogEnabled&&!cameraReported){cameraReported=true;Log("RPD_HANDOFF_CAMERA smooth=1 duration=1.5 from=%.3f,%.3f,%.3f to=%.3f,%.3f,%.3f",fromCamera[12],fromCamera[13],fromCamera[14],matrix[12],matrix[13],matrix[14]);}
  float t=1-std::clamp(blend/1.5f,0.f,1.f);t=t*t*(3-2*t);
  // Blend the native camera, then restore an orthonormal right-handed basis.
  for(unsigned i=0;i<16;++i)matrix[i]=fromCamera[i]+(matrix[i]-fromCamera[i])*t;
  auto unit=[](float* v){float n=std::sqrt(v[0]*v[0]+v[1]*v[1]+v[2]*v[2]);if(n>.001f)for(unsigned i=0;i<3;++i)v[i]/=n;};
  unit(matrix);float dot=matrix[0]*matrix[4]+matrix[1]*matrix[5]+matrix[2]*matrix[6];
  for(unsigned i=0;i<3;++i)matrix[4+i]-=matrix[i]*dot;unit(matrix+4);
  matrix[8]=matrix[1]*matrix[6]-matrix[2]*matrix[5];matrix[9]=matrix[2]*matrix[4]-matrix[0]*matrix[6];matrix[10]=matrix[0]*matrix[5]-matrix[1]*matrix[4];
 }
 std::copy(matrix,matrix+16,lastCamera.begin());cameraReady=true;return blend>0;
}
}
bool MWELRpdPrepareResult(){return rpd_officer_handoff::PrepareResult();}
bool MWELRpdOfficerDisabled(unsigned handle){return rpd_officer_handoff::Begin(handle);}

bool MWELRpdNoAllyDefeat(){return rpd_mission::Current()&&rpd_officer_handoff::nativeDefeat;}
