#pragma once
#include <algorithm>
#include <cmath>
#include <string>
namespace rpd_intro_music_policy {
inline float Gain(float elapsed,float countdown,float go){
 if(!std::isfinite(elapsed)||!std::isfinite(countdown)||!std::isfinite(go)||countdown<0||go<=countdown)return 0;
 return std::clamp((go-elapsed)/(go-countdown),0.f,1.f);
}
inline bool FileName(const std::string& value){
 if(value.size()<5||value.size()>48||value.substr(value.size()-4)!=".asf")return false;
 return std::all_of(value.begin(),value.end()-4,[](unsigned char c){return (c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='_'||c=='-';});
}
}
