#include "../../integration/InstalledCarType.h"
#include <array>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <vector>

static unsigned char fixtureCode[0x400000]{};
void Check(bool ok,const char* text){if(!ok)throw std::runtime_error(text);}
int main(int argc,char** argv){try{
 Check(argc==2,"Provide verified speed.exe");
 std::ifstream input(argv[1],std::ios::binary);
 std::vector<unsigned char> image{std::istreambuf_iterator<char>(input),{}};
 auto dos=reinterpret_cast<const IMAGE_DOS_HEADER*>(image.data());
 auto pe=reinterpret_cast<const IMAGE_NT_HEADERS32*>(image.data()+dos->e_lfanew);
 Check(pe->Signature==IMAGE_NT_SIGNATURE&&pe->FileHeader.Machine==IMAGE_FILE_MACHINE_I386,"Not native x86 PE");
 Check(reinterpret_cast<uintptr_t>(fixtureCode)<=0x730000&&reinterpret_cast<uintptr_t>(fixtureCode)+sizeof(fixtureCode)>=0x740000,"Fixture pad layout changed");
 DWORD oldProtect=0;Check(VirtualProtect(reinterpret_cast<void*>(0x730000),0x10000,PAGE_EXECUTE_READWRITE,&oldProtect)!=0,"Fixture code protection failed");
 auto source=[&](unsigned address){unsigned rva=address-0x400000;auto s=IMAGE_FIRST_SECTION(pe);
  for(unsigned i=0;i<pe->FileHeader.NumberOfSections;++i)if(rva>=s[i].VirtualAddress&&rva<s[i].VirtualAddress+s[i].SizeOfRawData)return image.data()+s[i].PointerToRawData+rva-s[i].VirtualAddress;
  throw std::runtime_error("Native accessor not file-backed");};
 const unsigned char lookup[]={0x8B,0x54,0x24,0x04,0x56,0x8B,0x35};
 const unsigned char index[]={0x8B,0x44,0x24,0x04,0x8B,0x0D};
 Check(!std::memcmp(source(0x739940),lookup,sizeof(lookup))&&!std::memcmp(source(0x739920),index,sizeof(index)),"Native accessor ABI changed");
 std::memcpy(reinterpret_cast<void*>(0x739940),source(0x739940),0x2E);
 std::memcpy(reinterpret_cast<void*>(0x739920),source(0x739920),0x13);
 std::array<std::array<unsigned char,0xD0>,130> entries{};
 const unsigned char* table=entries[0].data();
 // Model Unlimiter's relocated table operand and extended count, keeping the
 // actual native lookup/index machine code and the old globals untouched.
 *reinterpret_cast<unsigned*>(0x739947)=unsigned(&table);
 *reinterpret_cast<unsigned*>(0x739926)=unsigned(&table);
 Check(*reinterpret_cast<unsigned char*>(0x73995B)==0x83,"Native count instruction changed");
 // The original CMP imm8 84 cannot encode 130. This fixture uses 120, beyond
 // the old 84-entry boundary, without changing native instruction lengths.
 *reinterpret_cast<unsigned char*>(0x73995D)=120;
 auto row=[&](unsigned type,const char* name,unsigned usage){auto& e=entries[type];
  strcpy_s(reinterpret_cast<char*>(e.data()),16,name);unsigned hash=~0u;for(auto p=name;*p;++p)hash=hash*33+static_cast<unsigned char>(*p);
  std::memcpy(e.data()+0x50,&hash,4);std::memcpy(e.data()+0x90,&type,4);e[0x94]=static_cast<unsigned char>(usage);};
 row(1,"BM WM",0);row(110,"ADDON",0);row(111,"POLICE",1);
 Check(mwcartype::FindPlayable("ADDON")==110,"Expanded-table vehicle missing");
 Check(mwcartype::FindPlayable("POLICE")==-1,"Police admitted as a playable racer");
 Check(mwcartype::FindPlayable("MISSING")==-1,"Absent model accepted through native fallback");
 unsigned wrong=1;std::memcpy(entries[110].data()+0x90,&wrong,4);
 Check(mwcartype::FindPlayable("ADDON")==-1,"Mismatched native index accepted");
 Check(mwcartype::FindPlayable("")==-1&&mwcartype::FindPlayable(nullptr)==-1,"Empty model accepted");
 Check(mwcartype::FindPlayable("123456789012345678901234567890123")==-1,"Unbounded model accepted");
 DWORD ignored=0;VirtualProtect(reinterpret_cast<void*>(0x730000),0x10000,oldProtect,&ignored);
 std::puts("PASS real native car accessors: relocated table, >84 models, police/absent/index/length refusal; original globals untouched");return 0;
}catch(const std::exception& error){std::fprintf(stderr,"FAIL %s\n",error.what());return 1;}}
