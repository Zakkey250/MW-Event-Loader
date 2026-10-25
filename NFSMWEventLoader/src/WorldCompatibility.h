#pragma once
// Included after EventCore. Cache hashes retain strict extraction integrity;
// race portability intentionally ignores cars, saves, language and textures.
namespace world_compatibility {
using mwel::Json;using mwel::fs::path;
std::string FileDigest(const path& path){
 BCRYPT_ALG_HANDLE algorithm=nullptr;BCRYPT_HASH_HANDLE hash=nullptr;unsigned char bytes[32]{};
 if(BCryptOpenAlgorithmProvider(&algorithm,BCRYPT_SHA256_ALGORITHM,nullptr,0)<0)throw std::runtime_error("World hash unavailable");
 bool ok=BCryptCreateHash(algorithm,&hash,nullptr,0,nullptr,0,0)>=0;std::ifstream stream(path,std::ios::binary);char buffer[65536];
 if(!stream)ok=false;while(ok&&stream){stream.read(buffer,sizeof(buffer));const auto count=stream.gcount();if(count&&BCryptHashData(hash,reinterpret_cast<PUCHAR>(buffer),ULONG(count),0)<0)ok=false;}
 ok=ok&&stream.eof()&&BCryptFinishHash(hash,bytes,sizeof(bytes),0)>=0;
 if(hash)BCryptDestroyHash(hash);BCryptCloseAlgorithmProvider(algorithm,0);if(!ok)throw std::runtime_error("World source unreadable");
 std::string result;for(auto byte:bytes){constexpr char hex[]="0123456789ABCDEF";result+=hex[byte>>4];result+=hex[byte&15];}return result;
}
inline bool Same(const Json& a,const Json& b){return a.is_object()&&b.is_object()&&a.value("schema",0)==1&&b.value("schema",0)==1&&a.value("kind",std::string())==b.value("kind",std::string())&&a.value("id",std::string())==b.value("id",std::string())&&mwel::Sha256(a.value("id",std::string()));}
Json Current(const path& game){
 const bool custom=mwel::fs::is_regular_file(game/L"NativeWorld"/L"index.txt")&&(GetModuleHandleW(L"World2MWNativeLoader.asi")!=nullptr);
 Json files=Json::object();std::string kind=custom?"w2c":"rockport";
 if(custom)for(const auto& entry:mwel::fs::directory_iterator(game/L"NativeWorld")){
  auto name=entry.path().filename().string();std::transform(name.begin(),name.end(),name.begin(),[](unsigned char c){return char(std::tolower(c));});
  if(entry.is_symlink())throw std::runtime_error("Linked world source refused");
  if(entry.is_regular_file()&&name.size()>4&&name.substr(name.size()-4)==".wmc")files["nativeworld/"+name]=FileDigest(entry.path());
 }
 if(custom&&files.empty())throw std::runtime_error("W2C collision identity absent");
 std::string material="MWEL_WORLD_V1\n"+kind+"\n";for(const auto& item:files.items())material+=item.key()+":"+item.value().get<std::string>()+"\n";
 return Json{{"schema",1},{"kind",kind},{"files",files},{"id",mwel::Digest(material)}};
}
class Context {
 Json receiving;
 public:
 explicit Context(const path& game):receiving(Current(game)){}
 bool Allows(const mwel::EventPackage& package)const{
  const auto found=package.authoring.find("worldCompatibility");
  return found!=package.authoring.end()&&Same(*found,receiving);
 }
};
}
