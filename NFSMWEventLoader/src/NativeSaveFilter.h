#pragma once
#include <algorithm>
#include <cstdint>
#include <stdexcept>
#include <vector>
namespace event_save {
struct Record {std::uint32_t id,flags,value,auxiliary;};
static_assert(sizeof(Record)==16);
constexpr unsigned Limit=300,ChunkBytes=4+Limit*sizeof(Record);
template<class IsAdditional>
std::vector<Record> OriginalRecords(const std::vector<Record>& records,unsigned added,IsAdditional additional,unsigned* removed=nullptr){
 if(records.size()<added||records.size()>4096)throw std::runtime_error("native save capacity invalid");
 std::vector<Record> retained;unsigned count=0;
 for(const auto& record:records){if(record.id&&additional(record.id)){++count;continue;}retained.push_back(record);}
 if(count>added)throw std::runtime_error("duplicate additional progress identity");
 retained.resize(std::min<size_t>(records.size()-added,Limit));
 if(removed)*removed=count;return retained;
}
}
