#pragma once
// Traced EN1.3 PostRaceResultsScreen: overview 005964A0 chooses the time-row
// type; 005951E0 binds value FEObject at +30 and GRacerInfo at +50;
// 005953F0 formats that value. Only the owned Drift overview changes display.
// Native event type, race clocks, row ownership and result controls stay native.
namespace drift_results {
void(__thiscall* originalRow)(void*)=nullptr;
void(__thiscall* originalSetup)(void*)=nullptr;
void(__thiscall* originalPhoto)(void*)=nullptr;
unsigned reported=0;bool installed=false;bool photoReported=false;
void Reset(){reported=0;photoReported=false;}
bool Scope(){return drift_race::ResultPending();}
void __fastcall Row(void* self,void*){
 originalRow(self);if(!Scope())return;
 using namespace expanded_leaderboard;
 auto info=Read<BYTE*>(self,0x50);auto object=Read<Object>(self,0x30);
 if(!object||!info||Read<unsigned>(info,0x1C)||Read<BYTE>(info,0x190))return;
 for(unsigned i=0;i<drift_race::count;++i)if(info==reinterpret_cast<BYTE*>(drift_race::owner+0x18+i*0x194)&&Read<unsigned>(info)==drift_race::handles[i]){
  const bool predicted=(drift_race::forecastMask&(1u<<i))!=0;
  if(!predicted&&!Read<BYTE>(info,0x21))return;
  char text[48];sprintf_s(text,predicted?"~ %u PTS":"%u PTS",drift_race::scores[i].total);Text(object,text);
  if(!(reported&(1u<<i))){reported|=1u<<i;Log("DRIFT_RESULT_SCORE slot=%u score=%u rank=%u native_controls_retained=1",i,drift_race::scores[i].total,Read<unsigned>(info,0x10));}return;
 }
}
void __fastcall Setup(void* self,void*){
 originalSetup(self);if(!Scope()||expanded_leaderboard::Read<unsigned>(self,0x1E4)!=0)return;
 auto package=expanded_leaderboard::Read<const char*>(self,0x10);
 auto object=package?reinterpret_cast<expanded_leaderboard::Object(__cdecl*)(const char*,unsigned)>(0x524850)(package,0x30EE5E68):nullptr;
 if(object)expanded_leaderboard::Text(object,drift_hud::Label(1));
}
// EN1.3 InGamePhoto::Setup 005A2AD0 formats time @ speed into the
// ordinary-race value AB6AAFDD (005A2F2A), the rival value 37BEA03B,
// and the speed-trap SPEEDTRAP_SPEED. Replace these native FEStrings only
// after a real Drift finish; no clocks, event types or ordinary photos change.
void __fastcall Photo(void* self,void*){
 originalPhoto(self);if(!Scope()||!drift_race::finalScores||drift_race::faulted)return;
 auto package=expanded_leaderboard::Read<const char*>(self,0x10);if(!package)return;
 char value[48];sprintf_s(value,"%s  %u PTS",drift_hud::Label(1),drift_race::scores[0].total);
 const unsigned targets[]={0xAB6AAFDDu,0x37BEA03Bu,0x424BB244u,expanded_leaderboard::Hash("SPEEDTRAP_SPEED")};
 unsigned changed=0;for(auto key:targets){
  auto object=reinterpret_cast<expanded_leaderboard::Object(__cdecl*)(const char*,unsigned)>(0x524850)(package,key);
  if(object&&expanded_leaderboard::Read<unsigned>(object,0x18)==2){expanded_leaderboard::Text(object,value);++changed;}
 }
 if(changed&&!photoReported){photoReported=true;Log("DRIFT_PHOTO_SCORE score=%u objects=%u native_controls_retained=1",drift_race::scores[0].total,changed);}
}
bool Install(){
 if(installed)return true;
 const BYTE row[]={0x83,0xEC,0x24,0x56,0x8B,0xF1},setup[]={0x56,0x8B,0xF1,0x8B,0x86,0xCC,1,0,0},photo[]={0x55,0x8B,0xEC,0x83,0xE4,0xF8,0x81,0xEC,0x98,0,0,0};
 void* sites[]={reinterpret_cast<void*>(0x5953F0),reinterpret_cast<void*>(0x5A19C0),reinterpret_cast<void*>(0x5A2AD0)};
 if(memcmp(sites[0],row,sizeof(row))||memcmp(sites[1],setup,sizeof(setup))||memcmp(sites[2],photo,sizeof(photo)))return false;
 auto clear=[&](){for(auto site:sites){MH_DisableHook(site);MH_RemoveHook(site);}originalRow=nullptr;originalSetup=nullptr;originalPhoto=nullptr;};
 if(MH_CreateHook(sites[0],Row,reinterpret_cast<void**>(&originalRow))!=MH_OK)return false;
 if(MH_CreateHook(sites[1],Setup,reinterpret_cast<void**>(&originalSetup))!=MH_OK){clear();return false;}
 if(MH_CreateHook(sites[2],Photo,reinterpret_cast<void**>(&originalPhoto))!=MH_OK){clear();return false;}
 bool ok=true;for(auto site:sites)ok=MH_QueueEnableHook(site)==MH_OK&&ok;
 if(!ok||MH_ApplyQueued()!=MH_OK){clear();return false;}
 installed=true;return true;
}
}
