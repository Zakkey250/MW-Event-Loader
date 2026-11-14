#pragma once
#include <algorithm>
#include <cmath>
#include <cwchar>
#include <string>
// These records do not supply native completion flags or change race ranks.
namespace event_preview_stats {
struct Finish {bool drift=false,hotPursuit=false;double angle=0;unsigned survivors=0;};
struct Record {bool drift=false,hotPursuit=false;double angle=0;unsigned points=0,survivors=0,place=0;bool policeSprint=false;unsigned arrested=0,suspects=0;};
inline bool ValidArrests(unsigned arrested,unsigned suspects){return suspects>=2&&suspects<=8&&arrested<=suspects;}
inline std::wstring Arrests(const Record& record,unsigned suspects){
 return (record.policeSprint&&record.suspects==suspects?std::to_wstring(record.arrested):L"--")+L"/"+std::to_wstring(suspects);
}
inline bool ValidAngle(double angle){return std::isfinite(angle)&&angle>=0&&angle<=80;}
inline bool ValidHot(unsigned survivors,unsigned place){return survivors>=1&&survivors<=16&&place>=1&&place<=16;}
inline unsigned BestSurvivors(unsigned previous,unsigned current){return std::max(previous,current);}
inline unsigned BestPlace(unsigned previous,unsigned current){return previous?std::min(previous,current):current;}
inline std::wstring Angle(const Record& record){
 if(!record.drift)return L"--";
 wchar_t value[40];swprintf_s(value,L"%.1f\u00b0",record.angle);return value;
}
inline std::wstring Points(const Record& record){return record.drift?std::to_wstring(record.points)+L" Pts":L"--";}
inline std::wstring Hot(const Record& record,bool japanese){
 if(!record.hotPursuit)return L"-- / --";
 return std::to_wstring(record.survivors)+(japanese?L"\u53f0 / ":L" cars / #")+std::to_wstring(record.place)+(japanese?L"\u4f4d":L"");
}
}
