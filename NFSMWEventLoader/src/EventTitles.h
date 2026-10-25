#pragma once
// Supply only the owned event-name language keys. The installation's language
// table and its ordinary translations remain owned by the native game.
namespace event_titles {
struct Title {std::wstring unicode;std::string event;std::map<std::string,std::string> encoded;std::wstring japanese;};
std::map<unsigned,Title> titles;
const char*(__fastcall* originalLookup)(void*,unsigned)=nullptr;
std::wstring Unicode(const std::string& input){
 auto count=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,input.data(),int(input.size()),nullptr,0);
 if(!count)throw std::runtime_error("Invalid event title UTF-8");
 std::wstring wide(count,L'\0');MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,input.data(),int(input.size()),wide.data(),count);
 return wide;
}
// NFS language files use a native byte-to-Unicode table, not a Windows code
// page. Resolve the active table's path (including Japanese Bridge overrides)
// and invert that table without rewriting language resources.
std::string NativeLanguagePath(){
 int current=-1;if(!Copy(reinterpret_cast<void*>(0x8F41C0),&current,4))throw std::runtime_error("current language unreadable");
 for(unsigned i=0;i<10;++i){unsigned entry[5]{};if(!Copy(reinterpret_cast<void*>(0x8F40F8+i*20),entry,sizeof(entry)))throw std::runtime_error("language entry unreadable");
  if(int(entry[0])!=current||!entry[2])continue;
  std::string path;for(unsigned j=0;j<260;++j){char c=0;if(!Copy(reinterpret_cast<void*>(entry[2]+j),&c,1))throw std::runtime_error("language path unreadable");if(!c){auto native=mwel::fs::path(path).lexically_normal();if(native.is_absolute()||path.find("..")!=std::string::npos)throw std::runtime_error("language path outside installation");return path;}path+=c;}
  throw std::runtime_error("language path unterminated");
 }
 throw std::runtime_error("active language unavailable");
}
std::map<wchar_t,std::string>& NativeCodec(const std::string& path){
 static std::map<std::string,std::map<wchar_t,std::string>> codecs;
 if(auto found=codecs.find(path);found!=codecs.end())return found->second;
 auto bytes=mwel::Read(game/mwel::fs::path(path),4*1024*1024);
 auto word=[&](size_t offset){unsigned value=0;if(offset>bytes.size()||bytes.size()-offset<4)throw std::runtime_error("language codec bounds");memcpy(&value,bytes.data()+offset,4);return value;};
 if(word(0)!=0x39000||word(4)>bytes.size()-8)throw std::runtime_error("unsupported native language chunk");
 auto offset=size_t(word(8))+8;auto count=word(offset);
 if(count<256||count>65536||offset+4>bytes.size()||size_t(count)*2>bytes.size()-offset-4)throw std::runtime_error("native language codec table bounds");
 std::vector<unsigned short> table(count);memcpy(table.data(),bytes.data()+offset+4,count*2);
 std::map<wchar_t,std::string> inverse;for(unsigned c=1;c<128;++c)inverse.emplace(wchar_t(c),std::string(1,char(c)));
 for(unsigned first=128;first<256;++first){auto value=table[first];if(value>=128)inverse.emplace(wchar_t(value),std::string(1,char(first)));
  else if(value)for(unsigned tail=128;tail<256;++tail){unsigned index=(unsigned(value)<<7)+tail-128;if(index<table.size()&&table[index]>=128){std::string token;token+=char(first);token+=char(tail);inverse.emplace(wchar_t(table[index]),std::move(token));}}
 }
 Log("EVENT_TITLE_NATIVE_CODEC path=%s glyphs=%u language_files_modified=0",path.c_str(),unsigned(inverse.size()));
 return codecs.emplace(path,std::move(inverse)).first->second;
}
const char* __fastcall Lookup(void* unused,unsigned key){
 auto found=titles.find(key);if(found==titles.end())return originalLookup(unused,key);
 auto& title=found->second;
 try{
  auto path=NativeLanguagePath();auto cached=title.encoded.find(path);
  if(cached==title.encoded.end()){
   auto& codec=NativeCodec(path);std::string encoded;unsigned missing=0;
   auto language=path;std::transform(language.begin(),language.end(),language.begin(),[](unsigned char c){return char(std::tolower(c));});
   const auto& unicode=!title.japanese.empty()&&language.find("japanese")!=std::string::npos?title.japanese:title.unicode;
   for(auto character:unicode){auto glyph=codec.find(character);if(glyph==codec.end()){encoded+='?';++missing;}else encoded+=glyph->second;}
   cached=title.encoded.emplace(path,std::move(encoded)).first;
   Log("EVENT_TITLE_LOOKUP key=%08X event=%s encoding=NFS_NATIVE path=%s unsupported_glyphs=%u frontend_visual_pending=1",key,title.event.c_str(),path.c_str(),missing);
  }
  return cached->second.c_str();
 }catch(const std::exception& error){static std::set<unsigned> failed;if(failed.insert(key).second)Log("EVENT_TITLE_FAILED key=%08X reason=%s",key,error.what());return originalLookup(unused,key);}
}
bool Install(){
 if(!registration_experiment::enabled)return true;
 for(const auto& package:packages){
  const unsigned key=mwel::TitleKey(package.nativeEventId);
  auto title=package.authoring.at("title").get<std::string>();if(title.find('\0')!=std::string::npos)throw std::runtime_error("Embedded zero in event title");
  if(!titles.emplace(key,Title{Unicode(title),package.id,{}}).second)Log("EVENT_TITLE_COLLISION key=%08X id=%s native_title_fallback=1",key,package.id.c_str());
 }
 const BYTE guard[]={0x53,0x8B,0x1D,0x80,0xCF,0x91,0};auto address=reinterpret_cast<void*>(0x56BB80);
 if(memcmp(address,guard,sizeof(guard))||MH_CreateHook(address,Lookup,reinterpret_cast<void**>(&originalLookup))!=MH_OK)return false;
 if(MH_EnableHook(address)!=MH_OK){MH_RemoveHook(address);return false;}
 Log("EVENT_TITLES_ACTIVE entries=%u language_files_modified=0",unsigned(titles.size()));return true;
}
void Audit(){
 if(!originalLookup)return;
 for(const auto& item:titles){auto expected=Lookup(nullptr,item.first);auto actual=reinterpret_cast<const char*(__cdecl*)(unsigned)>(0x56BC10)(item.first);
  Log("EVENT_TITLE_NATIVE_LOOKUP key=%08X exact_owned_text=%u",item.first,unsigned(actual==expected));}
}
}
