#include "EventCore.h"
#include "NativeSaveFilter.h"
#include <iostream>
#include <cstring>
static void Check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
int wmain(int argc,wchar_t** argv){
 try{
  Check(argc==2,"Provide the verified game executable");auto bytes=mwel::Read(argv[1]);auto hash=mwel::Digest(bytes);
  Check(hash=="80774C2E5D619B4F120B48D4462896FD504C263399D203A238769CFFDE1D253C"||hash=="B248271BF8EAC8C9B283B8C95E3ADD672B713BF529B05F1780E58268493B9D06"||hash=="0C5675A08CD71FD6D31CA87E992A915054BD8B80D268BFF0561D7ECC2067E342","Executable identity differs");
  constexpr unsigned relocation=0x1FC00000;
  auto image=static_cast<BYTE*>(VirtualAlloc(reinterpret_cast<void*>(0x20000000),0x680000,MEM_RESERVE|MEM_COMMIT,PAGE_EXECUTE_READWRITE));
  Check(image==reinterpret_cast<void*>(0x20000000),"Native image mapping failed");
  auto dos=reinterpret_cast<const IMAGE_DOS_HEADER*>(bytes.data());auto nt=reinterpret_cast<const IMAGE_NT_HEADERS32*>(bytes.data()+dos->e_lfanew);auto sections=IMAGE_FIRST_SECTION(nt);
  for(unsigned i=0;i<nt->FileHeader.NumberOfSections;++i)memcpy(image+sections[i].VirtualAddress,bytes.data()+sections[i].PointerToRawData,sections[i].SizeOfRawData);
  const BYTE guard[]={0xA1,0x04,0xE0,0x91,0,0x85,0xC0};Check(!memcmp(image+0x16D810,guard,sizeof(guard)),"Native serializer guard differs");
  for(unsigned address=0x56D810;address+4<=0x56D8BE;++address){auto operand=reinterpret_cast<unsigned*>(address+relocation);if(*operand==0x91E004||*operand==0x9260B2)*operand+=relocation;}
  *reinterpret_cast<BYTE*>(0x9260B2+relocation)=0;
  std::vector<event_save::Record> baseline;for(unsigned i=0;i<388;++i)baseline.push_back({i+1,2,0x42000000+i,1000+i});
  unsigned database[16]{};database[0]=unsigned(baseline.size());database[15]=unsigned(baseline.data());*reinterpret_cast<unsigned*>(0x91E004+relocation)=unsigned(database);
  auto native=reinterpret_cast<void*(__stdcall*)(void*,void*)>(0x56D810+relocation);
  std::vector<BYTE> expected(event_save::ChunkBytes,0);Check(native(expected.data(),expected.data()+expected.size())==expected.data()+expected.size(),"Native chunk boundary differs");
  unsigned scenarios=0;
  for(unsigned insertion:{0u,1u,150u,299u,300u,388u}){
   auto records=baseline;records.insert(records.begin()+insertion,{0xEE001u,3,0x42ABCDEF,123});auto before=records;
   database[0]=unsigned(records.size());database[15]=unsigned(records.data());
   std::vector<BYTE> actual(event_save::ChunkBytes,0);Check(native(actual.data(),actual.data()+actual.size())==actual.data()+actual.size(),"Additional native chunk boundary differs");
   unsigned removed=0;auto filtered=event_save::OriginalRecords(records,1,[](unsigned id){return id==0xEE001u;},&removed);unsigned count=unsigned(filtered.size());memcpy(actual.data(),&count,4);memcpy(actual.data()+4,filtered.data(),filtered.size()*sizeof(event_save::Record));
   Check(removed==1&&actual==expected,"Original progress displaced at 300-record boundary");Check(!memcmp(records.data(),before.data(),records.size()*sizeof(event_save::Record)),"Native progress changed");++scenarios;
  }
  auto two=baseline;two.insert(two.begin(),{0xEE001u,3,1,2});two.insert(two.begin()+299,{0xEE002u,3,3,4});auto filtered=event_save::OriginalRecords(two,2,[](unsigned id){return id==0xEE001u||id==0xEE002u;});
  Check(filtered.size()==300&&!memcmp(filtered.data(),baseline.data(),300*sizeof(event_save::Record)),"Multiple additional records displaced original progress");
  auto small=std::vector<event_save::Record>(baseline.begin(),baseline.begin()+12);small.insert(small.begin()+3,{0xEE001u,3,1,2});Check(event_save::OriginalRecords(small,1,[](unsigned id){return id==0xEE001u;}).size()==12,"Small database capacity differs");
  bool refused=false;try{event_save::OriginalRecords(small,14,[](unsigned){return false;});}catch(...){refused=true;}Check(refused,"Invalid capacity accepted");
  VirtualFree(image,0,MEM_RELEASE);std::cout<<"PASS native_binary_serializer=1 executable="<<hash<<" insertion_positions="<<scenarios<<" multiple_additional=1 original_records_exact=1 native_memory_unchanged=1 invalid_capacity_refused=1\n";return 0;
 }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
