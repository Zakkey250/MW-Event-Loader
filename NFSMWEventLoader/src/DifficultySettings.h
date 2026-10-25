#pragma once
#include "DifficultyPolicy.h"
namespace mwel_difficulty {
inline Settings Load(const char* file){
 Settings s;if(!file||!*file)return s;
 auto value=[&](const char* key,float fallback,float low,float high){char text[64]{};GetPrivateProfileStringA("Difficulty",key,"",text,sizeof(text),file);return Number(text,fallback,low,high);};
 auto optIn=[&](const char* key){char text[32]{};GetPrivateProfileStringA("Difficulty",key,"",text,sizeof(text),file);return text[0]=='1'&&!text[1];};
 s.enabled=GetPrivateProfileIntA("Difficulty","Enabled",1,file)!=0;
 s.catchUp=GetPrivateProfileIntA("Difficulty","CatchUpEnabled",1,file)!=0;
 s.nitrous=GetPrivateProfileIntA("Difficulty","NitrousEnabled",1,file)!=0;
 s.engineScale=value("EnginePowerScale",1.75f,1,4);
 s.catchUpMaxScale=value("CatchUpMaxScale",2,1,2);
 s.startMetres=value("CatchUpStartMetres",50,0,5000);
 s.fullMetres=value("CatchUpFullMetres",std::max(600.f,s.startMetres+1),s.startMetres+1,20000);
 s.blacklistScaling=optIn("BlacklistScalingEnabled");
 s.blacklist15Scale=value("Blacklist15PowerScale",1.4f,1,4);
 s.blacklist1Scale=value("Blacklist1PowerScale",1.75f,1,4);
 s.farCatchUp=optIn("FarCatchUpEnabled");
 s.farMetres=value("FarCatchUpMetres",301,1,20000);
 s.farScale=value("FarCatchUpScale",2.5f,1,4);
 return s;
}
}
