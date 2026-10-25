#pragma once
#include "NativeSaveFilter.h"
// Keep additional event identities out of the fixed 300-record standard-save
// chunk. The native database and original event progress stay intact in memory.
namespace native_save_isolation {
using Record=event_save::Record;
constexpr unsigned ChunkBytes=event_save::ChunkBytes;
void*(__stdcall* originalSerialize)(void*,void*)=nullptr;
bool enabled=false;
bool auditing=false;
void(__thiscall* originalImport)(void*,const Record*,unsigned)=nullptr;
void __fastcall Import(void* database,void*,const Record* source,unsigned count){
 originalImport(database,source,count);
 profile_progress::restorePending=true;
 // The profile import precedes the frontend's loaded/name publication.
 // Restore from the next display frame, after the synchronous load returns.
 profile_progress::restoredIdentity.clear();
}
bool Additional(unsigned id){
 if(!registration_experiment::enabled||!registration_experiment::enumerateAdditional)return false;
 for(const auto& package:packages)if(!mwel::Correction(package.authoring)&&registration_experiment::Hash(package.nativeEventId.c_str())==id)return true;
 return false;
}
unsigned Filter(void* begin,void* database){
 unsigned nativeCount=0,records=0;
 if(!Copy(database,&nativeCount,4)||nativeCount>4096||!Copy(static_cast<BYTE*>(database)+0x3C,&records,4)||(nativeCount&&!records))throw std::runtime_error("native save database invalid");
 std::vector<Record> source(nativeCount);unsigned removed=0;
 // Filter before the native 300-slot limit, so an additional record cannot
 // displace the last original record from the standard save.
 if(nativeCount&&!Copy(reinterpret_cast<void*>(records),source.data(),source.size()*sizeof(Record)))throw std::runtime_error("native save record unreadable");
 unsigned added=0;if(registration_experiment::enumerateAdditional)for(const auto& clone:registration_experiment::clones)added+=clone->race&&!clone->correction;
 auto retained=event_save::OriginalRecords(source,added,Additional,&removed);auto capacity=unsigned(retained.size());
 memcpy(begin,&capacity,4);memset(static_cast<BYTE*>(begin)+4,0,300*sizeof(Record));
 if(capacity)memcpy(static_cast<BYTE*>(begin)+4,retained.data(),capacity*sizeof(Record));
 return removed;
}
void* __stdcall Serialize(void* begin,void* end){
 unsigned* database=nullptr;if(!enabled||!Copy(reinterpret_cast<void*>(0x91E004),&database,4)||!database)return originalSerialize(begin,end);
 try{
  if(uintptr_t(end)<uintptr_t(begin))throw std::runtime_error("native save buffer reversed");
  std::vector<BYTE> output(ChunkBytes,0);
  if(originalSerialize(output.data(),output.data()+output.size())!=output.data()+output.size())throw std::runtime_error("native save boundary differs");
  if(!auditing){profile_progress::RestoreNativeProgress();profile_progress::SaveNativeProgress(database);}
  auto removed=Filter(output.data(),database);unsigned count=0;memcpy(&count,output.data(),4);
  auto available=uintptr_t(end)-uintptr_t(begin);
  // Match native partial-buffer writes: a complete count and complete records.
  if(available>=4){memcpy(begin,output.data(),4);auto slots=std::min<size_t>(count,(available-4)/sizeof(Record));if(slots)memcpy(static_cast<BYTE*>(begin)+4,output.data()+4,slots*sizeof(Record));}
  Log("EVENT_NATIVE_SAVE_FILTER additional_records_removed=%u native_progress_memory_modified=0 fixed_chunk_bytes=%u filter_before_native_limit=1",removed,ChunkBytes);
  return reinterpret_cast<void*>(uintptr_t(begin)+ChunkBytes);
 }catch(const std::exception& error){
  // A short return shifts every following field in the profile and places
  // its MD5 trailer 4,804 bytes early. Preserve the retail serializer's
  // complete/partial-buffer contract when an optional filter cannot run.
  // In particular the frontend can save during database initialization.
  Log("EVENT_NATIVE_SAVE_FILTER_FALLBACK reason=%s serializer=native fixed_layout_preserved=1",error.what());
  return originalSerialize(begin,end);
 }
}
bool Install(){
 if(!registration_experiment::enabled)return true;
 const BYTE guard[]={0xA1,0x04,0xE0,0x91,0,0x85,0xC0};auto address=reinterpret_cast<void*>(0x56D810);
 if(memcmp(address,guard,sizeof(guard))||MH_CreateHook(address,Serialize,reinterpret_cast<void**>(&originalSerialize))!=MH_OK)return false;
 const BYTE importGuard[]={0x51,0x53,0x55,0x56,0x8B,0xF1,0x8B,0x0E};auto importAddress=reinterpret_cast<void*>(0x5FB770);
 if(memcmp(importAddress,importGuard,sizeof(importGuard))||MH_CreateHook(importAddress,Import,reinterpret_cast<void**>(&originalImport))!=MH_OK){MH_RemoveHook(address);return false;}
 if(MH_EnableHook(importAddress)!=MH_OK){MH_RemoveHook(importAddress);MH_RemoveHook(address);return false;}
 if(MH_EnableHook(address)!=MH_OK){MH_DisableHook(importAddress);MH_RemoveHook(importAddress);MH_RemoveHook(address);return false;}
 enabled=true;Log("EVENT_NATIVE_SAVE_ISOLATION_ACTIVE additional_only=1 standard_save_acceptance_pending=1");return true;
}
void Audit(void* database){
 if(!enabled)return;
 unsigned count=0,records=0;if(!Copy(database,&count,4)||!Copy(static_cast<BYTE*>(database)+0x3C,&records,4)||count>4096||!records)throw std::runtime_error("native save audit database invalid");
 std::string before(count*sizeof(Record),'\0');if(!Copy(reinterpret_cast<void*>(records),before.data(),before.size()))throw std::runtime_error("native save audit memory unreadable");
 std::vector<BYTE> output(ChunkBytes,0xCC);auditing=true;auto end=reinterpret_cast<void*(__stdcall*)(void*,void*)>(0x56D810)(output.data(),output.data()+output.size());auditing=false;
 unsigned written=0;memcpy(&written,output.data(),4);bool absent=written<=300;for(unsigned i=0;i<written&&i<300;++i){Record record{};memcpy(&record,output.data()+4+i*sizeof(Record),sizeof(record));absent&=!Additional(record.id);}
 std::vector<Record> expected;unsigned removed=0;
 for(unsigned i=0;i<count;++i){Record record{};memcpy(&record,before.data()+i*sizeof(Record),sizeof(record));if(record.id&&Additional(record.id)){++removed;continue;}expected.push_back(record);}
 bool originalExact=written<=expected.size()&&memcmp(output.data()+4,expected.data(),written*sizeof(Record))==0;
 std::string after(before.size(),'\0');bool unchanged=Copy(reinterpret_cast<void*>(records),after.data(),after.size())&&before==after;
 Log("EVENT_NATIVE_SAVE_SERIALIZER_AUDIT native_binary_called=1 boundary_exact=%u additional_absent=%u native_memory_unchanged=%u original_records_exact=%u additional_records_present=%u native_save_file_written=0",unsigned(end==output.data()+output.size()),unsigned(absent),unsigned(unchanged),unsigned(originalExact),removed);
 if(!absent||!unchanged||!originalExact||end!=output.data()+output.size())throw std::runtime_error("native save serializer audit failed");
}
}
