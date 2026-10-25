#pragma once
namespace hot_pursuit_radar {
using Object=expanded_leaderboard::Object;
using UpdateFn=void(__thiscall*)(unsigned,unsigned);
UpdateFn originalUpdate=nullptr;
struct State {Object package=nullptr;unsigned map=0;std::array<Object,32> extra{};char name[64]{};};
State state;
Object Package(unsigned map){
 const auto name=expanded_leaderboard::Read<const char*>(reinterpret_cast<void*>(map),0x10);
 auto engine=expanded_leaderboard::Read<void*>(reinterpret_cast<void*>(0x91CADC));
 return engine&&name?reinterpret_cast<Object(__thiscall*)(void*,const char*)>(0x516AF0)(engine,name):nullptr;
}
bool Live(){
 auto engine=expanded_leaderboard::Read<void*>(reinterpret_cast<void*>(0x91CADC));
 return engine&&state.package&&state.name[0]&&reinterpret_cast<Object(__thiscall*)(void*,const char*)>(0x516AF0)(engine,state.name)==state.package;
}
unsigned IconKey(unsigned index){char key[48];sprintf_s(key,"MWEL_HP_CopRadar_%u",index);return expanded_leaderboard::Hash(key);}
void Hide(){
 if(!Live())return;
 for(unsigned i=0;i<state.extra.size();++i)if(auto o=state.extra[i]){
  if(expanded_leaderboard::Find(state.package,IconKey(i+8))==o)expanded_leaderboard::Show(o,false);
  else state.extra[i]=nullptr; // package address may have been reused
 }
}
bool Visible(unsigned map){
 // Native HudElement::SetHudFeatures (005793F0) uses these two 64-bit
 // fields. The SDK's unused +28 byte is never assigned by that function.
 return (expanded_leaderboard::Read<unsigned long long>(reinterpret_cast<void*>(map),0x18)&
         expanded_leaderboard::Read<unsigned long long>(reinterpret_cast<void*>(map),0x20))!=0;
}
void FeaturesChanged(unsigned long long features){
 if(Live()&&!(expanded_leaderboard::Read<unsigned long long>(reinterpret_cast<void*>(state.map),0x18)&features))Hide();
}
void Frame(){
 if(!hot_pursuit_runtime::Active()||!combat_health_runtime::Racing()){Hide();return;}
 if(Live()&&(!Visible(state.map)||reinterpret_cast<bool(__cdecl*)()>(0x468390)()))Hide();
}
Object Icon(unsigned map,unsigned index){
 if(index<8)return expanded_leaderboard::Read<Object>(reinterpret_cast<void*>(map),0xD0+index*4);
 if(index>=40)return nullptr;
 auto package=Package(map);if(!package)return nullptr;
 if(state.map!=map||state.package!=package){
  Hide();state={};state.map=map;state.package=package;
  const auto name=expanded_leaderboard::Read<const char*>(reinterpret_cast<void*>(map),0x10);
  if(name)for(unsigned i=0;i<sizeof(state.name)-1;++i){state.name[i]=expanded_leaderboard::Read<char>(name,i);if(!state.name[i])break;}
 }
 auto& icon=state.extra[index-8];const auto key=IconKey(index);
 if(icon&&expanded_leaderboard::Find(package,key)!=icon)icon=nullptr;
 if(!icon)icon=expanded_leaderboard::Clone(package,expanded_leaderboard::Read<Object>(reinterpret_cast<void*>(map),0xD0),key);
 return icon;
}
void Draw(unsigned map,unsigned car,Object icon,unsigned color){
 if(!icon)return;
 float position[2]{},heading[2]{};
 // This native helper converts the sim's position/forward to map Z,-X.
 reinterpret_cast<void(__cdecl*)(float*,float*,unsigned)>(0x579840)(position,heading,car-0x80);
 reinterpret_cast<void(__thiscall*)(unsigned,const float*,const float*,Object,unsigned)>(0x5675D0)(map,position,heading,icon,0);
 reinterpret_cast<void(__cdecl*)(Object,unsigned)>(0x5157E0)(icon,color);
}
void __fastcall Update(unsigned map,void*,unsigned player){
 originalUpdate(map,player);

 using namespace gauntlet_runtime;
 if(!hot_pursuit_runtime::Active()||!combat_health_runtime::Racing()){Hide();return;}
 if(!Visible(map)||reinterpret_cast<bool(__cdecl*)()>(0x468390)()){Hide();return;}
 std::vector<unsigned> cars;if(!List(cars))return;
 // Native owns eight ground icon pointers. Add package-owned FE clones for
 // overflow instead of extending that array into the racer icon storage.
 for(unsigned i=0;i<8;++i)expanded_leaderboard::Show(Icon(map,i),false);
 Hide();
 auto heli=expanded_leaderboard::Read<Object>(reinterpret_cast<void*>(map),0xC8);
 auto cone=expanded_leaderboard::Read<Object>(reinterpret_cast<void*>(map),0xCC);
 expanded_leaderboard::Show(heli,false);expanded_leaderboard::Show(cone,false);
 unsigned shown=0,otherGroups=0,air=0;
 unsigned race=0,playerHandle=0;Read(0x91E000,race);Read(race+0x18,playerHandle);
 for(const auto car:cars){unsigned ai=0,driver=0,pursuit=0;
  if(!Read(car+0x94,driver)||driver!=2||!Read(car+0x54,ai)||
     !reinterpret_cast<bool(__thiscall*)(unsigned)>(0x688200)(car))continue;
  if(Helicopter(ai)){
   if(reinterpret_cast<bool(__thiscall*)(unsigned)>(0x688170)(car)||air++)continue;
   Draw(map,car,heli,0xFF90B8FF);Draw(map,car,cone,0xFF90B8FF);continue;
  }
  if(!Table(ai,0x892480)||shown>=40)continue;
  Read(ai+0x70,pursuit);const auto target=hot_pursuit_runtime::TargetHandle(pursuit);
  const bool destroyed=reinterpret_cast<bool(__thiscall*)(unsigned)>(0x688170)(car);
  const unsigned flash=expanded_leaderboard::Read<unsigned>(reinterpret_cast<void*>(map),0xC0);
  Draw(map,car,Icon(map,shown++),destroyed?0xFFCCCCCC:(flash<=2?0xFF0000FF:0xFFA00000));
  if(target&&target!=playerHandle)++otherGroups;
 }
 if(worldLogEnabled){static ULONGLONG next=0;if(GetTickCount64()>=next){next=GetTickCount64()+3000;
  Log("HOT_PURSUIT_RADAR ground=%u nonplayer=%u helicopter=%u native_transform=1 native_arrays_unchanged=1",shown,otherGroups,air);}}
}
bool Surface(){const BYTE expected[]={0x55,0x8B,0xEC,0x83,0xE4,0xF8};BYTE code[6]{};
 return originalUpdate||(Copy(reinterpret_cast<void*>(0x579E00),code,6)&&!memcmp(code,expected,6));}
}
