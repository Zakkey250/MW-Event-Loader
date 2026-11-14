#pragma once
#include <Windows.h>
#include <cstdint>
#include <cwchar>

// First-chance evidence, NOT an exception handler or a recovery mechanism.
// A single bounded record is rewritten only on an observed native fault.
// It bypasses the normal lossy/rate-limited asynchronous diagnostic queue.
namespace mwel_fault_record {
struct Record {
 char magic[8]={'M','W','E','L','F','L','T','1'};
 std::uint32_t bytes=sizeof(Record),process=0,thread=0,tick=0,code=0,address=0;
 std::uint32_t access=0,target=0,stackReadable=0;
 CONTEXT context{};
 std::uint32_t stack[128]{};
 wchar_t module[MAX_PATH]{};
};
using Reader=bool(*)(const void*,void*,size_t);
inline wchar_t destination[MAX_PATH]{};
inline bool enabled=false;
inline void Configure(bool logging,const wchar_t* path){
 enabled=false;destination[0]=0;
 if(!logging||!path||wcslen(path)>=MAX_PATH)return;
 wcscpy_s(destination,path);enabled=true;
}
inline bool Capture(EXCEPTION_POINTERS* fault,const wchar_t* module,Reader read){
 if(!enabled)return false;
 if(!fault||!fault->ExceptionRecord||!fault->ContextRecord||!read)return false;
 Record record;
 record.process=GetCurrentProcessId();record.thread=GetCurrentThreadId();record.tick=GetTickCount();
 record.code=fault->ExceptionRecord->ExceptionCode;
 record.address=std::uint32_t(reinterpret_cast<uintptr_t>(fault->ExceptionRecord->ExceptionAddress));
 if(fault->ExceptionRecord->NumberParameters>=2){
  record.access=std::uint32_t(fault->ExceptionRecord->ExceptionInformation[0]);
  record.target=std::uint32_t(fault->ExceptionRecord->ExceptionInformation[1]);
 }
 record.context=*fault->ContextRecord;
 record.stackReadable=read(reinterpret_cast<const void*>(record.context.Esp),record.stack,sizeof(record.stack));
 if(module)wcsncpy_s(record.module,module,_TRUNCATE);
 // Keep the last first-chance record for THIS process; a separate filename
 // for each start preserves earlier failures. No files/handles when OFF.
 HANDLE file=CreateFileW(destination,GENERIC_WRITE,FILE_SHARE_READ,nullptr,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL|FILE_FLAG_WRITE_THROUGH,nullptr);
 if(file==INVALID_HANDLE_VALUE)return false;
 DWORD written=0;const bool saved=WriteFile(file,&record,sizeof(record),&written,nullptr)&&written==sizeof(record);
 CloseHandle(file);return saved;
}
}
