#pragma once
#include <array>
#include <string>
#include <algorithm>
#include <cctype>
namespace mwel_drift {
inline std::wstring SplitLabel(std::string path){
 std::transform(path.begin(),path.end(),path.begin(),[](unsigned char c){return char(std::tolower(c));});
 if(path.find("japanese")!=std::string::npos)return L"\u30b9\u30d7\u30ea\u30c3\u30c8\u30dd\u30a4\u30f3\u30c8";
 return L"SPLIT POINTS";
}
inline std::array<std::wstring,2> BonusLabels(std::string path){
 std::transform(path.begin(),path.end(),path.begin(),[](unsigned char c){return char(std::tolower(c));});
 if(path.find("japanese")!=std::string::npos)return {L"\u58c1\u63a5\u8fd1",L"\u4ed6\u8eca\u63a5\u8fd1"};
 if(path.find("german")!=std::string::npos)return {L"WAND",L"VERKEHR"};
 if(path.find("french")!=std::string::npos)return {L"MUR",L"TRAFIC"};
 if(path.find("italian")!=std::string::npos)return {L"MURO",L"TRAFFICO"};
 if(path.find("spanish")!=std::string::npos)return {L"MURO",L"TR\u00c1FICO"};
 if(path.find("polish")!=std::string::npos)return {L"\u015aCIANA",L"RUCH"};
 if(path.find("russian")!=std::string::npos)return {L"\u0421\u0422\u0415\u041d\u0410",L"\u0422\u0420\u0410\u0424\u0418\u041a"};
 if(path.find("dutch")!=std::string::npos)return {L"MUUR",L"VERKEER"};
 if(path.find("swedish")!=std::string::npos)return {L"V\u00c4GG",L"TRAFIK"};
 if(path.find("korean")!=std::string::npos)return {L"\ubcbd \uadfc\uc811",L"\ucc28\ub7c9 \uadfc\uc811"};
 if(path.find("simplified")!=std::string::npos)return {L"\u8d34\u5899",L"\u8d34\u8f66"};
 if(path.find("chinese")!=std::string::npos)return {L"\u8cbc\u7246",L"\u8cbc\u8eca"};
 return {L"WALL",L"TRAFFIC"};
}
inline std::array<std::wstring,4> Labels(std::string path){
 std::transform(path.begin(),path.end(),path.begin(),[](unsigned char c){return char(std::tolower(c));});
 struct Locale {const char* key;std::array<const wchar_t*,4> words;};
 const Locale locales[]={
  {"japanese",{L"\u5408\u8a08",L"\u30c9\u30ea\u30d5\u30c8",L"\u5f97\u70b9\u78ba\u5b9a",L"\u5931\u6557"}},
  {"german",{L"GESAMT",L"DRIFT",L"GESICHERT",L"VERLOREN"}},
  {"french",{L"TOTAL",L"D\u00c9RAPAGE",L"VALID\u00c9",L"PERDU"}},
  {"italian",{L"TOTALE",L"DERAPATA",L"CONFERMATO",L"PERSO"}},
  {"spanish",{L"TOTAL",L"DERRAPE",L"GUARDADO",L"PERDIDO"}},
  {"polish",{L"SUMA",L"DRIFT",L"ZAPISANO",L"STRATA"}},
  {"russian",{L"\u0421\u0427\u0401\u0422",L"\u0414\u0420\u0418\u0424\u0422",L"\u0417\u0410\u0427\u0422\u0415\u041d\u041e",L"\u041f\u041e\u0422\u0415\u0420\u042f\u041d\u041e"}},
  {"dutch",{L"TOTAAL",L"DRIFT",L"BIJGETELD",L"VERLOREN"}},
  {"swedish",{L"TOTALT",L"SLADD",L"SPARAT",L"F\u00d6RLORAT"}},
  {"korean",{L"\ucd1d\uc810",L"\ub4dc\ub9ac\ud504\ud2b8",L"\uc810\uc218 \ud655\uc815",L"\uc2e4\ud328"}},
  {"simplified",{L"\u603b\u5206",L"\u6f02\u79fb",L"\u5f97\u5206\u786e\u8ba4",L"\u5931\u8bef"}},
  {"chinese",{L"\u7e3d\u5206",L"\u6f02\u79fb",L"\u5f97\u5206\u78ba\u5b9a",L"\u5931\u8aa4"}}
 };
 for(const auto& locale:locales)if(path.find(locale.key)!=std::string::npos)return {locale.words[0],locale.words[1],locale.words[2],locale.words[3]};
 return {L"TOTAL",L"DRIFT",L"BANKED",L"LOST"};
}
}
