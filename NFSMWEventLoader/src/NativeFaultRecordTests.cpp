#include "NativeFaultRecord.h"
#include <cassert>
#include <cstdio>
#include <filesystem>

static bool copy(const void* from,void* to,size_t bytes){__try{memcpy(to,from,bytes);return true;}__except(EXCEPTION_EXECUTE_HANDLER){return false;}}
static bool observerEnabled=false;
static LONG CALLBACK observer(EXCEPTION_POINTERS* fault){
 if(observerEnabled)mwel_fault_record::Capture(fault,L"synthetic-native-fault",copy);
 return EXCEPTION_CONTINUE_SEARCH;
}
static unsigned raise(){
 __try {ULONG_PTR info[2]{1,0x12345678};RaiseException(EXCEPTION_ACCESS_VIOLATION,0,2,info);}
 __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
 return 0;
}
int wmain(int argc,wchar_t** argv){
 assert(argc==2);
 const std::filesystem::path path(argv[1]);assert(!std::filesystem::exists(path));
 auto handler=AddVectoredExceptionHandler(1,observer);assert(handler);
 observerEnabled=true;mwel_fault_record::Configure(false,path.c_str());
 assert(raise()==EXCEPTION_ACCESS_VIOLATION);assert(!std::filesystem::exists(path));
 mwel_fault_record::Configure(true,path.c_str());
 assert(raise()==EXCEPTION_ACCESS_VIOLATION);
 assert(std::filesystem::file_size(path)==sizeof(mwel_fault_record::Record));
 mwel_fault_record::Record record;FILE* input=nullptr;assert(_wfopen_s(&input,path.c_str(),L"rb")==0);
 assert(fread(&record,1,sizeof(record),input)==sizeof(record));fclose(input);
 assert(!memcmp(record.magic,"MWELFLT1",8)&&record.bytes==sizeof(record));
 assert(record.code==EXCEPTION_ACCESS_VIOLATION&&record.access==1&&record.target==0x12345678);
 assert(record.process==GetCurrentProcessId()&&record.thread==GetCurrentThreadId());
 assert(record.context.Eip&&record.context.Esp&&record.address&&record.stackReadable);
 assert(!wcscmp(record.module,L"synthetic-native-fault"));
 assert(raise()==EXCEPTION_ACCESS_VIOLATION);
 assert(std::filesystem::file_size(path)==sizeof(record)); // bounded replacement
 mwel_fault_record::Configure(false,path.c_str());observerEnabled=false;
 assert(RemoveVectoredExceptionHandler(handler));
 printf("PASS OFF no file; ON bounded durable context; original SEH receives every exception; bytes=%zu\n",sizeof(record));
}
