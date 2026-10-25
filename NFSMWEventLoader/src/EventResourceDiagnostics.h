#pragma once
// Included inside runtime_lifecycle; bounded read-only samples on the game
// observer thread. Never resize an initialized pool or construct audio here.
ULONGLONG resourceSampleAt=0;
void Resources(BYTE* race,unsigned key){
 if(!worldLogEnabled)return;
 const auto now=GetTickCount64();if(now<resourceSampleAt)return;resourceSampleAt=now+5000;
 const unsigned racers=Read<unsigned>(race,0x1958);unsigned handles[16]{},registered=0;bool unique=racers>0&&racers<=16;
 for(unsigned slot=0;slot<racers&&slot<16;++slot){handles[slot]=Read<unsigned>(race,0x18+slot*0x194);registered+=handles[slot]!=0;
  for(unsigned previous=0;previous<slot;++previous)if(handles[slot]&&handles[slot]==handles[previous])unique=false;
 }
 Log("EVENT_FIELD_HANDLE_HEALTH key=%08X racers=%u registered=%u all_registered=%u unique_handles=%u",key,racers,registered,unsigned(registered==racers),unsigned(unique));
 auto ride=Read<BYTE*>(reinterpret_cast<void*>(0x9B0A0C));
 const bool rideValid=ride&&Read<unsigned>(ride,0x0C)==0x8B49F4&&Read<unsigned>(ride,0x28)==0x6D4;
 auto pool=Read<BYTE*>(reinterpret_cast<void*>(0x911F6C));
 const bool audioValid=pool&&Read<unsigned>(pool)==0x898D24&&Read<unsigned>(pool,0x0C)==3&&Read<unsigned>(pool,0x1C)==1;
 unsigned audioSlots=audioValid?Read<unsigned>(pool,0x14):0,active=0,bound=0;
 bool complete=audioValid&&audioSlots<=64;auto node=audioValid?Read<BYTE*>(pool,0x10):nullptr;
 BYTE* seen[64]{};unsigned owners[64]{};
 for(unsigned i=0;complete&&i<audioSlots;++i){
  if(!node||Read<unsigned>(node)!=0x896FA8){complete=false;break;}
  for(unsigned j=0;j<i;++j)if(seen[j]==node)complete=false;if(!complete)break;seen[i]=node;
  if(Read<BYTE>(node,0x38)){++active;auto owner=Read<unsigned>(node,0x34);if(owner){
   for(unsigned j=0;j<bound;++j)if(owners[j]==owner)complete=false;owners[bound++]=owner;
  }}node=Read<BYTE*>(node,4);
 }if(node)complete=false;
 nfsmw_limitbreaker::ResourceLimitsV1 limits{};unsigned capability=0;
 auto lb=GetModuleHandleW(L"NFSMWLimitBreaker.asi");auto query=lb?reinterpret_cast<nfsmw_limitbreaker::QueryResourceLimitsV1>(GetProcAddress(lb,"NFSMWLB_QueryResourceLimitsV1")):nullptr;
 if(query)capability=query(&limits,sizeof(limits));
 PROCESS_MEMORY_COUNTERS_EX process{};process.cb=sizeof(process);
 const bool memoryValid=GetProcessMemoryInfo(GetCurrentProcess(),reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&process),sizeof(process))!=0;
 MEMORYSTATUSEX memory{};memory.dwLength=sizeof(memory);GlobalMemoryStatusEx(&memory);
 Log("EVENT_RESOURCE_HEALTH key=%08X racers=%u vehicle_live=%u vehicle_capacity=%u ride_read=%u ride_used=%u ride_peak=%u ride_capacity=%u car_arena_bytes=%u audio_read=%u audio_slots=%u audio_active=%u audio_bound=%u audio_complete=%u audio_mixer_bytes=%u audio_budget=%u lb_query=%u lb_logical_limit=%u process_memory_read=%u private_bytes=%llu working_set_bytes=%llu physical_free_bytes=%llu read_only=1 audible_output_verified=0",key,Read<unsigned>(race,0x1958),Read<unsigned>(reinterpret_cast<void*>(0x92CD24)),Read<unsigned>(reinterpret_cast<void*>(0x92CD20)),unsigned(rideValid),rideValid?Read<unsigned>(ride,0x18):0,rideValid?Read<unsigned>(ride,0x1C):0,rideValid?Read<unsigned>(ride,0x2C):0,Read<unsigned>(reinterpret_cast<void*>(0x9B2C00),0x64),unsigned(audioValid),audioSlots,active,bound,unsigned(complete),Read<unsigned>(reinterpret_cast<void*>(0x912848)),Read<unsigned>(reinterpret_cast<void*>(0x911ED4)),capability,capability?limits.logicalVehicleLimit:0,unsigned(memoryValid),static_cast<unsigned long long>(process.PrivateUsage),static_cast<unsigned long long>(process.WorkingSetSize),memory.ullAvailPhys);
}
