#include "EventCore.h"
#include "RadarTransition.h"
#include <iostream>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <cctype>
static void Check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
struct Backend {
 bool query=true,load=true;unsigned owners=1;std::string prefix="MINI_MAP_OWNED",order;
 bool Query(std::string& p,std::string& f){order+='Q';p="MINI_MAP";f="TRACKS/L2RA/MINI_MAP.BIN";return query;}
 void ClearTiles(){order+='C';}
 void Release(){order+='R';Check(owners==1,"Wrong native owner count");--owners;}
 void ResetIndex(){order+='I';}
 void SetPrefix(const std::string& p){order+='P';prefix=p;}
 bool Load(const std::string&){order+='L';Check(owners==0,"Overlapping native TPK owners");if(load)++owners;return load;}
};
static int __cdecl Format(char* output,int size,const char* format,...){va_list args;va_start(args,format);int n=vsnprintf(output,size,format,args);va_end(args);return n;}
static void __cdecl Upper(char* text){for(;*text;++text)*text=char(std::toupper(static_cast<unsigned char>(*text)));}
static unsigned __cdecl Exists(const char*){return 1;}
int wmain(int argc,wchar_t** argv){try{
 Check(argc==2,"Provide a supported executable");
 Check(event_radar::UseCourse(0,1,123)&&!event_radar::UseCourse(2,1,123)&&!event_radar::UseCourse(0,0,123)&&!event_radar::UseCourse(0,1,0),"Retained race record overrides native roaming map");
 for(bool roaming:{false,true})for(bool owned:{false,true}){Backend b;auto r=event_radar::Restore(b,roaming,owned);if(roaming&&owned)Check(r==event_radar::RestoreResult::Queued&&b.order=="QCRIPL"&&b.owners==1&&b.prefix=="MINI_MAP","Restore ordering/ownership differs");else Check(r==event_radar::RestoreResult::Skipped&&b.order.empty()&&b.prefix=="MINI_MAP_OWNED","Unowned or racing map changed");}
 Backend missing;missing.query=false;Check(event_radar::Restore(missing,true,true)==event_radar::RestoreResult::Unavailable&&missing.order=="Q"&&missing.owners==1&&missing.prefix=="MINI_MAP_OWNED","Unavailable world map destroyed current owner");
 Backend failed;failed.load=false;Check(event_radar::Restore(failed,true,true)==event_radar::RestoreResult::Unavailable&&failed.owners==0,"Failed stream reported success");
 auto bytes=mwel::Read(argv[1]);auto hash=mwel::Digest(bytes);Check(hash=="80774C2E5D619B4F120B48D4462896FD504C263399D203A238769CFFDE1D253C"||hash=="B248271BF8EAC8C9B283B8C95E3ADD672B713BF529B05F1780E58268493B9D06"||hash=="0C5675A08CD71FD6D31CA87E992A915054BD8B80D268BFF0561D7ECC2067E342","Executable differs");
 constexpr unsigned relocation=0x1FC00000;auto image=static_cast<BYTE*>(VirtualAlloc(reinterpret_cast<void*>(0x20000000),0x680000,MEM_RESERVE|MEM_COMMIT,PAGE_EXECUTE_READWRITE));Check(image==reinterpret_cast<void*>(0x20000000),"Native mapping failed");
 auto dos=reinterpret_cast<const IMAGE_DOS_HEADER*>(bytes.data());auto nt=reinterpret_cast<const IMAGE_NT_HEADERS32*>(bytes.data()+dos->e_lfanew);auto sections=IMAGE_FIRST_SECTION(nt);for(unsigned i=0;i<nt->FileHeader.NumberOfSections;++i)memcpy(image+sections[i].VirtualAddress,bytes.data()+sections[i].PointerToRawData,sections[i].SizeOfRawData);
 const BYTE guards[][8]={{0x51,0x53,0x55,0x56,0x8B,0xE9},{0x56,0x8B,0x74,0x24,0x08,0x85,0xF6},{0x8B,0x44,0x24,0x1C,0x8B,0x4C,0x24,0x18}};unsigned addresses[]={0x5672F0,0x663F00,0x661760},lengths[]={6,7,8};for(unsigned i=0;i<3;++i)Check(!memcmp(reinterpret_cast<void*>(addresses[i]+relocation),guards[i],lengths[i]),"Native radar lifetime guard differs");
 // Execute the real native chunk appender against a padded, private buffer.
 // Two banks reproduce the overwrite at the observed adjacent FE-owner
 // offset; resetting between banks keeps that sentinel and live-slot count.
 *reinterpret_cast<unsigned*>(0x5674DB+relocation)=0x91CEFC+relocation;
 unsigned manager[256]{},chunk[]={0x3A100,4,0};constexpr unsigned sentinel=0xA17ECAFE;
 *reinterpret_cast<unsigned*>(0x91CEFC+relocation)=unsigned(manager);
 auto append=reinterpret_cast<int(__cdecl*)(void*)>(0x5674D0+relocation);
 manager[0x1E0/4]=sentinel;for(unsigned i=0;i<128;++i)Check(append(chunk)==1,"Native radar chunk rejected");
 Check(manager[0]==128&&manager[0x1E0/4]!=sentinel,"Duplicate bank overwrite not reproduced");
 event_radar::ResetCompressedIndex(manager);manager[0x1E0/4]=sentinel;
 for(unsigned bank=0;bank<3;++bank){event_radar::ResetCompressedIndex(manager);for(unsigned i=0;i<64;++i)append(chunk);Check(manager[0]==64&&manager[0x1E0/4]==sentinel,"Radar bank switch overwrote adjacent FE owner");}
 for(unsigned a=0x57BEC0;a+4<0x57BFFF;++a){auto operand=reinterpret_cast<unsigned*>(a+relocation);if(*operand==0x91E000||*operand==0x91CF90||(*operand>=0x8A0DC0&&*operand<=0x8A0E00))*operand+=relocation;}
 auto stub=[&](unsigned a,void* target){auto at=reinterpret_cast<BYTE*>(a+relocation);at[0]=0xE9;*reinterpret_cast<unsigned*>(at+1)=unsigned(target)-unsigned(at)-5;};stub(0x4647E0,Format);stub(0x460F50,Upper);stub(0x65F7A0,Exists);
 BYTE race[0x19B4]{},profile[0x30]{},career[0xB4]{};*reinterpret_cast<unsigned*>(0x91E000+relocation)=unsigned(race);*reinterpret_cast<unsigned*>(0x91CF90+relocation)=unsigned(profile);*reinterpret_cast<unsigned*>(profile+0x10)=unsigned(career);
 auto native=reinterpret_cast<bool(__cdecl*)(int,char*,int,char*,int)>(0x57BEC0+relocation);unsigned cases=0;
 for(auto rank:{1,9,13}){career[0xB0]=BYTE(rank);char prefix[32]{},path[64]{};Check(native(0,prefix,32,path,64),"Native roaming map refused");std::string expected=rank>12?"MINI_MAP_UNLOCK_1":rank>8?"MINI_MAP_UNLOCK_2":"MINI_MAP";Check(prefix==expected&&std::string(path)=="TRACKS\\L2RA\\"+expected+".BIN","Native unlock map selection differs");++cases;}
 char prefix[32]{},path[64]{};Check(!native(2,prefix,32,path,64),"Native mode 2 unexpectedly loads radar");VirtualFree(image,0,MEM_RELEASE);
 std::cout<<"PASS radar_transition=1 native_unlock_selection="<<cases<<" executable="<<hash<<" stale_record_rejected=1 single_stream_owner=1 runtime_acceptance_pending=1\n";return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
