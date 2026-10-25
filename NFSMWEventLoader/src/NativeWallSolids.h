#pragma once
#include <fstream>
#include <set>
// Register byte-preserved solids from the current installation. Streaming may
// omit these solids in W2C; never substitute editor meshes or bundled DDS.
namespace native_wall_solids {
struct Chunk {unsigned tag;size_t start,end;};
std::vector<void*> resident;
unsigned Word(const std::vector<BYTE>& bytes,size_t offset){if(offset+4>bytes.size())throw std::runtime_error("Truncated native wall resource");unsigned value;memcpy(&value,bytes.data()+offset,4);return value;}
std::vector<Chunk> Chunks(const std::vector<BYTE>& bytes,size_t start,size_t end){
 std::vector<Chunk> result;if(end>bytes.size())throw std::runtime_error("Native wall resource range");
 for(auto at=start;at<end;){if(end-at<8)throw std::runtime_error("Truncated native wall chunk");auto length=Word(bytes,at+4);if(length>end-at-8)throw std::runtime_error("Native wall chunk exceeds source");result.push_back({Word(bytes,at),at,at+8+length});at+=8+length;}return result;
}
void Append(std::vector<BYTE>& out,unsigned tag,const BYTE* bytes,size_t size){if(size>32*1024*1024)throw std::runtime_error("Native wall resource too large");auto at=out.size();out.resize(at+8+size);memcpy(out.data()+at,&tag,4);auto n=unsigned(size);memcpy(out.data()+at+4,&n,4);if(size)memcpy(out.data()+at+8,bytes,size);}
bool Register(const std::vector<BYTE>& source,size_t sourceOffset,std::set<unsigned>& needed){
 auto children=Chunks(source,8,source.size());std::vector<Chunk> selected;std::set<unsigned> keys;Chunk header{};
 for(const auto& chunk:children){
  if(chunk.tag==0x80134001)header=chunk;
  if(chunk.tag!=0x80134010)continue;
  for(const auto& field:Chunks(source,chunk.start+8,chunk.end))if(field.tag==0x134011){
   auto at=((sourceOffset+field.start+8+15)&~size_t(15))-sourceOffset;if(at+20>field.end||source[at+12]!=22)throw std::runtime_error("Unsupported native wall solid version");
   auto key=Word(source,at+16);if(needed.count(key)){selected.push_back(chunk);keys.insert(key);}break;
  }
 }
 if(selected.empty())return true;if(!header.tag)throw std::runtime_error("Missing native wall solid list header");
 std::vector<BYTE> fields;
 for(const auto& field:Chunks(source,header.start+8,header.end)){
  std::vector<BYTE> payload(source.begin()+field.start+8,source.begin()+field.end);
  if(field.tag==0x134002){if(payload.size()<16)throw std::runtime_error("Native wall list header");auto n=unsigned(selected.size());memcpy(payload.data()+12,&n,4);}
  if(field.tag==0x134003){if(payload.size()%8)throw std::runtime_error("Native wall solid index");std::vector<BYTE> retained;for(size_t at=0;at<payload.size();at+=8)if(keys.count(Word(payload,at)))retained.insert(retained.end(),payload.begin()+at,payload.begin()+at+8);payload=std::move(retained);}
  if(field.tag==0x134004)payload.clear();
  Append(fields,field.tag,payload.data(),payload.size());
 }
 std::vector<BYTE> body;Append(body,0x80134001,fields.data(),fields.size());
 for(const auto& chunk:selected){auto desired=(sourceOffset+chunk.start)%128;auto pad=(desired+128-(8+body.size())%128)%128;if(pad){if(pad<8)pad+=128;std::vector<BYTE> zero(pad-8);Append(body,0,zero.data(),zero.size());}body.insert(body.end(),source.begin()+chunk.start,source.begin()+chunk.end);}
 std::vector<BYTE> bundle;Append(bundle,0x80134000,body.data(),body.size());
 auto memory=VirtualAlloc(nullptr,bundle.size(),MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);if(!memory)throw std::runtime_error("Native wall solid allocation");memcpy(memory,bundle.data(),bundle.size());
 // The native loader mutates and retains the resource. Preserve its backing
 // buffer for the process lifetime, including failed partial registration.
 resident.push_back(memory);if(!reinterpret_cast<int(__cdecl*)(void*)>(0x5075A0)(memory))throw std::runtime_error("Native wall solid registration refused");
 for(auto key:keys)needed.erase(key);return true;
}
bool Load(std::set<unsigned> needed){
 const BYTE guard[]={0x8B,0x44,0x24,0x04,0x81,0x38,0,0x40,0x13,0x80};if(memcmp(reinterpret_cast<void*>(0x5075A0),guard,sizeof(guard)))throw std::runtime_error("Native solid loader ABI mismatch");
 wchar_t executable[MAX_PATH];if(!GetModuleFileNameW(nullptr,executable,MAX_PATH))return false;auto root=mwel::fs::path(executable).parent_path();
 for(const wchar_t* relative:{L"TRACKS/STREAML2RA.BUN",L"GLOBAL/GLOBALB.BUN",L"GLOBAL/GLOBALA.BUN",L"GLOBAL/InGameA.bun",L"GLOBAL/InGameB.bun"}){
  if(needed.empty())break;std::ifstream input(root/relative,std::ios::binary);if(!input)continue;input.seekg(0,std::ios::end);auto length=uint64_t(input.tellg());input.seekg(0);
  for(uint64_t at=0;at<length&&!needed.empty();){unsigned header[2]{};input.read(reinterpret_cast<char*>(header),8);if(!input||length-at<8||header[1]>length-at-8)throw std::runtime_error("Invalid installed wall resource chunks");
   auto end=at+8+header[1];if(header[0]==0x80134000){if(header[1]>32*1024*1024)throw std::runtime_error("Installed native solid list too large");std::vector<BYTE> bytes(8+header[1]);memcpy(bytes.data(),header,8);input.read(reinterpret_cast<char*>(bytes.data()+8),header[1]);if(!input)throw std::runtime_error("Cannot read installed wall solid");Register(bytes,size_t(at),needed);}
   input.seekg(end);at=end;
  }
 }
 return needed.empty();
}
}
