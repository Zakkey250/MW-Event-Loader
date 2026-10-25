#pragma once
#include "EventCore.h"
namespace mwel {
inline unsigned NativeKey(const std::string& text){unsigned key=0xFFFFFFFF;for(unsigned char c:text)key=key*33+c;return key;}
inline unsigned TitleKey(std::string token){std::replace(token.begin(),token.end(),'.','_');for(auto& c:token)c=char(std::toupper(static_cast<unsigned char>(c)));return NativeKey("TRACKNAME_"+token);}
// Installed event.json time is the update authority across different UUIDs.
// Revisions are only comparable within a UUID. Ties use the UUID, never directory enumeration.
template<class Modified,class Rejected> void SelectPackages(std::vector<EventPackage>& input,Modified modified,Rejected rejected){
 struct Row {size_t index;fs::file_time_type time;};std::vector<Row> rows;
 for(size_t i=0;i<input.size();++i)rows.push_back({i,modified(input[i])});
 std::sort(rows.begin(),rows.end(),[&](const Row& a,const Row& b){return a.time!=b.time?a.time>b.time:input[a.index].id<input[b.index].id;});
 std::map<unsigned,size_t> collections,titles;std::map<std::string,size_t> tokens;std::set<size_t> selected;
 for(auto row:rows){auto& p=input[row.index];const auto key=Correction(p.authoring)?NativeKey(p.authoring.at("correctionTarget").at("name").get<std::string>()):NativeKey(p.nativeNamespace),title=TitleKey(p.nativeEventId);
  size_t conflict=input.size();const char* reason=nullptr;
  if(auto i=collections.find(key);i!=collections.end()){conflict=i->second;reason="collection";}
  else if(auto i=tokens.find(p.nativeEventId);i!=tokens.end()){conflict=i->second;reason="event_token";}
  else if(auto i=titles.find(title);i!=titles.end()){conflict=i->second;reason="title_key";}
  if(reason){rejected(p,input[conflict],reason);continue;}
  collections.emplace(key,row.index);tokens.emplace(p.nativeEventId,row.index);titles.emplace(title,row.index);selected.insert(row.index);
 }
 std::vector<EventPackage> output;for(size_t i=0;i<input.size();++i)if(selected.count(i))output.push_back(std::move(input[i]));input=std::move(output);
}
}
