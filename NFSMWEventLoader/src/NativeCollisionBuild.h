#pragma once
// Decode the receiving Rockport installation, never a foreign editor cache.
// The layout/filters match tools/extract_native_collision.py. All offsets are
// bounded by their own container; dynamic/destructible instances stay native.
#include "../../World2MW/src/NativeLoader/CollisionWorld.h"
#include <filesystem>
#include <fstream>
#include <map>
#include <numeric>
#include <stdexcept>
namespace native_collision_build {
using namespace w2mw;
struct Bytes {
 const unsigned char* data;size_t size;
 void Check(size_t offset,size_t count)const{if(offset>size||count>size-offset)throw std::runtime_error("Native CARP extent");}
 template<class T>T Read(size_t offset)const{Check(offset,sizeof(T));T value;memcpy(&value,data+offset,sizeof(value));return value;}
 Bytes Slice(size_t offset,size_t count)const{Check(offset,count);return {data+offset,count};}
};
inline std::vector<unsigned char> ReadFile(const std::filesystem::path& path,size_t limit){
 std::ifstream f(path,std::ios::binary|std::ios::ate);if(!f)throw std::runtime_error("Native CARP source unavailable");auto length=f.tellg();
 if(length<0||uint64_t(length)>limit)throw std::runtime_error("Native CARP source size");std::vector<unsigned char> out(static_cast<size_t>(length));f.seekg(0);if(!f.read(reinterpret_cast<char*>(out.data()),out.size()))throw std::runtime_error("Native CARP source truncated");return out;
}
template<class F>void Chunks(Bytes bytes,F visit){
 for(size_t o=0;o<bytes.size;){auto tag=bytes.Read<uint32_t>(o),size=bytes.Read<uint32_t>(o+4);auto payload=bytes.Slice(o+8,size);visit(tag,payload);o+=8+size;}
}
inline bool Finite(Vec3 p){return std::isfinite(p.x)&&std::isfinite(p.y)&&std::isfinite(p.z)&&std::max({fabs(p.x),fabs(p.y),fabs(p.z)})<100000;}
struct Face {Vec3 p[3];uint32_t surface;bool wall;};
inline std::vector<Face> Article(Bytes b){
 auto strips=b.Read<uint16_t>(0),stripBytes=b.Read<uint16_t>(2),edges=b.Read<uint16_t>(4),edgeBytes=b.Read<uint16_t>(6);auto surfaces=b.Read<uint8_t>(9);
 if(b.Read<uint8_t>(8)||edgeBytes!=edges*32u||b.Read<uint16_t>(10)!=surfaces*4u||!surfaces)throw std::runtime_error("Native CARP article layout");
 b.Check(16,size_t(stripBytes)+edgeBytes+surfaces*4u);b.Check(16,strips*16u);std::vector<Face> out;
 auto surface=[&](unsigned i){if(i>=surfaces)throw std::runtime_error("Native CARP surface");return b.Read<uint32_t>(16+stripBytes+edgeBytes+i*4u);};
 for(unsigned i=0;i<strips;++i){size_t s=16+i*16;auto center=b.Read<Vec3>(s);auto radius=b.Read<uint16_t>(s+12),offset=b.Read<uint16_t>(s+14);
  if(offset<strips*16u||offset>=stripBytes)throw std::runtime_error("Native CARP strip offset");size_t v=16+offset;auto count=b.Read<uint8_t>(v+6);
  if(count<3||offset+count*8u>stripBytes||!Finite(center))throw std::runtime_error("Native CARP strip span");std::vector<Vec3> points;
  for(unsigned j=0;j<count;++j){Vec3 p{center.x+b.Read<int16_t>(v+j*8)/128.f,center.y+b.Read<int16_t>(v+j*8+2)/128.f,center.z+b.Read<int16_t>(v+j*8+4)/128.f};auto d=Sub(p,center);if(!Finite(p)||sqrt(Dot(d,d))>radius/16.f+1.001f)throw std::runtime_error("Native CARP strip sphere");points.push_back(p);}
  for(unsigned j=2;j<count;++j){auto id=surface(b.Read<uint8_t>(v+j*8+6));if(b.Read<uint8_t>(v+j*8+7)&128)continue;unsigned a=j%2?j-1:j-2,c=j%2?j-2:j-1;out.push_back({{points[a],points[c],points[j]},id,false});}
 }
 for(unsigned i=0;i<edges;++i){size_t o=16+stripBytes+i*32;auto a=b.Read<Vec3>(o),c=b.Read<Vec3>(o+16);if(!Finite(a)||!Finite(c))throw std::runtime_error("Native CARP edge coordinate");auto id=surface(b.Read<uint8_t>(o+12));Vec3 q[4]={a,{c.x,a.y,c.z},c,{a.x,c.y,a.z}};out.push_back({{q[0],q[1],q[2]},id,true});out.push_back({{q[0],q[2],q[3]},id,true});}
 return out;
}
struct Item {CollisionTriangle triangle;uint16_t group;};
inline void Pack(Bytes b,std::vector<Item>& output){
 size_t marker=0;while(marker+4<=std::min<size_t>(128,b.size)&&b.Read<uint32_t>(marker)!=0x43415250)++marker;
 if(marker+4>std::min<size_t>(128,b.size))throw std::runtime_error("Native CARP directory absent");
 struct Entry {Bytes bytes;unsigned count;};std::map<unsigned,Entry> instances;std::map<unsigned,std::vector<Face>> articles;
 for(size_t o=marker+16;o<marker+4096;o+=16){auto tag=b.Read<uint32_t>(o);if(tag==0xAAAAAAAA)break;
  auto kind=tag>>16;if(kind!=0x6361&&kind!=0x6369)continue;auto packed=b.Read<uint32_t>(o+4),count=b.Read<uint32_t>(o+8),relative=b.Read<uint32_t>(o+12);
  if((packed&255)!=3)throw std::runtime_error("Native CARP directory type");b.Check(o,relative);auto bytes=b.Slice(o+relative,packed>>8);auto id=tag&65535;
  if(kind==0x6361){if(articles.count(id))throw std::runtime_error("Native CARP duplicate article");articles.emplace(id,Article(bytes));}
  else if(!instances.emplace(id,Entry{bytes,count}).second)throw std::runtime_error("Native CARP duplicate instances");
 }
 // Full 255-entry directories have no spare sentinel slot.
 for(const auto& pair:instances){auto e=pair.second;if(e.count>100000||e.bytes.size!=uint64_t(e.count)*64)throw std::runtime_error("Native CARP instances layout");
  for(unsigned i=0;i<e.count;++i){size_t o=i*64;if(e.bytes.Read<uint8_t>(o+18)&6)continue;auto x=e.bytes.Read<Vec3>(o),z=e.bytes.Read<Vec3>(o+32),position=e.bytes.Read<Vec3>(o+48),y=Cross(z,x);Vec3 basis[]={x,y,z};
   if(!Finite(position))throw std::runtime_error("Native CARP instance coordinate");for(unsigned a=0;a<3;++a)for(unsigned c=0;c<3;++c)if(!std::isfinite(Dot(basis[a],basis[c]))||fabs(Dot(basis[a],basis[c])-(a==c?1.f:0.f))>=.015f)throw std::runtime_error("Native CARP non-rigid transform");
   auto group=e.bytes.Read<uint16_t>(o+24),id=e.bytes.Read<uint16_t>(o+26);auto found=articles.find(id);if(group>=4096||found==articles.end())throw std::runtime_error("Native CARP instance binding");
   for(const auto& face:found->second){CollisionTriangle t{};t.surface=face.surface;t.wall=face.wall;
    for(unsigned v=0;v<3;++v){auto p=Sub(face.p[v],position);t.p[v]={p.x*x.x+p.y*y.x+p.z*z.x,p.x*x.y+p.y*y.y+p.z*z.y,p.x*x.z+p.y*y.z+p.z*z.z};if(!Finite(t.p[v]))throw std::runtime_error("Native CARP world coordinate");}
    auto n=Cross(Sub(t.p[1],t.p[0]),Sub(t.p[2],t.p[0]));auto length=sqrt(Dot(n,n));if(length<1e-7f||(!face.wall&&fabs(n.y)/length<.05f))continue;if(!face.wall&&n.y<0)std::swap(t.p[0],t.p[1]);
    if(output.size()>=4000000)throw std::runtime_error("Native CARP triangle limit");output.push_back({t,group});
   }
  }
 }
}
inline void Build(const std::filesystem::path& game,CollisionWorld& world,std::vector<uint16_t>& groups){
 const auto track=game/L"TRACKS/L2RA.BUN",stream=game/L"TRACKS/STREAML2RA.BUN";auto data=ReadFile(track,64*1024*1024);Bytes table{nullptr,0};
 Chunks({data.data(),data.size()},[&](unsigned tag,Bytes b){if(tag==0x34110){if(table.data)throw std::runtime_error("Native CARP section table duplicated");table=b;}});
 if(!table.data||table.size%92)throw std::runtime_error("Native CARP section table");std::ifstream f(stream,std::ios::binary|std::ios::ate);if(!f)throw std::runtime_error("Native CARP stream missing");auto fileSize=f.tellg();std::vector<Item> items;std::vector<unsigned char> section;
 for(size_t o=0;o<table.size;o+=92){if(table.Read<uint8_t>(o)=='X'||table.Read<uint8_t>(o)=='Y')continue;
  auto offset=table.Read<uint32_t>(o+20),size=table.Read<uint32_t>(o+24),compressed=table.Read<uint32_t>(o+28);
  if(table.Read<uint32_t>(o+12)||table.Read<uint32_t>(o+16)!=1||size!=compressed||size>64*1024*1024||uint64_t(offset)+size>uint64_t(fileSize))throw std::runtime_error("Native CARP section extent or compression");
  section.resize(size);f.seekg(offset);if(!f.read(reinterpret_cast<char*>(section.data()),size))throw std::runtime_error("Native CARP section truncated");
  Chunks({section.data(),section.size()},[&](unsigned tag,Bytes b){if(tag==0x3B801)Pack(b,items);});
 }
 if(items.empty())throw std::runtime_error("Native CARP contains no static collision");
 // Own a complete BVH before publishing any geometry to the query provider.
 std::vector<BvhNode> nodes;nodes.reserve(items.size()/3);std::vector<CollisionTriangle> triangles;triangles.reserve(items.size());std::vector<uint16_t> ordered;ordered.reserve(items.size());
 auto center=[](const Item& t,unsigned axis){return ((&t.triangle.p[0].x)[axis]+(&t.triangle.p[1].x)[axis]+(&t.triangle.p[2].x)[axis])/3;};
 auto build=[&](auto&& self,size_t first,size_t end,unsigned depth)->unsigned{
  if(depth>=96)throw std::runtime_error("Native CARP BVH depth");const float inf=std::numeric_limits<float>::infinity();Vec3 low{inf,inf,inf},high{-inf,-inf,-inf},clow=low,chigh=high;
  for(size_t i=first;i<end;++i)for(unsigned axis=0;axis<3;++axis){for(auto p:items[i].triangle.p){(&low.x)[axis]=std::min((&low.x)[axis],(&p.x)[axis]);(&high.x)[axis]=std::max((&high.x)[axis],(&p.x)[axis]);}auto c=center(items[i],axis);(&clow.x)[axis]=std::min((&clow.x)[axis],c);(&chigh.x)[axis]=std::max((&chigh.x)[axis],c);}
  unsigned index=unsigned(nodes.size());nodes.push_back({low,high,0,0});
  if(end-first<=8){nodes[index].left=unsigned(triangles.size());nodes[index].right=0x80000000u|unsigned(end-first);for(size_t i=first;i<end;++i){triangles.push_back(items[i].triangle);ordered.push_back(items[i].group);}}
  else{unsigned axis=0;for(unsigned a=1;a<3;++a)if((&chigh.x)[a]-(&clow.x)[a]>(&chigh.x)[axis]-(&clow.x)[axis])axis=a;auto middle=first+(end-first)/2;std::nth_element(items.begin()+first,items.begin()+middle,items.begin()+end,[&](const auto& a,const auto& b){return center(a,axis)<center(b,axis);});auto left=self(self,first,middle,depth+1),right=self(self,middle,end,depth+1);nodes[index].left=left;nodes[index].right=right;}
  return index;
 };build(build,0,items.size(),0);
 std::map<unsigned,unsigned> ids;for(const auto& t:triangles)ids.emplace(t.surface,0);if(ids.size()>=1000)throw std::runtime_error("Native CARP surface count");std::vector<std::array<char,80>> names;
 for(auto& pair:ids){pair.second=unsigned(names.size());std::array<char,80> name{};snprintf(name.data(),name.size(),"%08X",pair.first);names.push_back(name);}for(auto& t:triangles)t.surface=ids.at(t.surface);
 world.nodes=std::move(nodes);world.triangles=std::move(triangles);world.surfaces=std::move(names);groups=std::move(ordered);if(!world.PrepareQueries())throw std::runtime_error("Native CARP BVH invalid");
}
}
