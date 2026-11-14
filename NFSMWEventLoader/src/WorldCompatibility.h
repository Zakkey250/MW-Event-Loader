#pragma once
#include "WorldProviderModule.h"
// Included after EventCore. Cache hashes retain strict extraction integrity;
// race portability intentionally ignores cars, saves, language and textures.
namespace world_compatibility {
using mwel::Json;using mwel::fs::path;
inline constexpr auto RockportSource="B4DA68460C9F642C76E2D603D3D50C6036F0C393BBDB11C7CB5ED690F7FD70D8";
inline constexpr auto PublicMergedSource="6EEDDFFF2FBB9A83E738B1DC91A9D6C314BEA88C60765CC0F2ACFD6BBD2E4009";
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
 const bool custom=mwel::fs::is_regular_file(game/L"NativeWorld"/L"index.txt")&&(mwel_world_provider::Module()!=nullptr);
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
 Json receiving,rockport,publicW2C;
 public:
 explicit Context(const path& game):receiving(Current(game)){
  // The public converter retains the original Rockport event groups,
  // placements and road ordinals in this verified extended master.
  // Accept previously authored Rockport events on that merged layout too.
  const auto module=GetModuleHandleW(L"W2CMWPublic.asi");
  if(!module||mwel_world_provider::SourceModule()!=module)return;
  const auto master=FileDigest(game/L"TRACKS"/L"L2RA.BUN");
  if(master==PublicMergedSource){
   const std::string material=std::string("MWEL_WORLD_V1\nw2c\ntracks/l2ra.bun:")+master+"\n";
   publicW2C={{"schema",1},{"kind","w2c"},{"id",mwel::Digest(material)}};
  }
  if(master==PublicMergedSource||master==RockportSource)
   rockport={{"schema",1},{"kind","rockport"},{"id",mwel::Digest("MWEL_WORLD_V1\nrockport\n")}};
 }
 bool Allows(const mwel::EventPackage& package)const{
  const auto found=package.authoring.find("worldCompatibility");
  return found!=package.authoring.end()&&(Same(*found,receiving)||Same(*found,rockport)||Same(*found,publicW2C));
 }
};
}
