#include "CollectionLookupCache.h"
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <memory>
#include <random>
#include <stdexcept>
#include <vector>
using namespace std::chrono_literals;
void Check(bool value,const char* text){if(!value)throw std::runtime_error(text);}
struct Entry {struct {unsigned key;} collection;};
using Entries=std::vector<std::unique_ptr<Entry>>;
Entry* Linear(Entries& e,unsigned key){for(auto& row:e)if(row&&row->collection.key==key)return row.get();return nullptr;}
void Add(Entries& e,unsigned key){e.push_back(std::make_unique<Entry>(Entry{{key}}));}
int main(){try{
 mwel_lookup::IndexCache<> cache;Entries e;std::mt19937 rng(42);
 for(unsigned i=0;i<500;++i)Add(e,i*64+1); // Deliberate direct-map collisions.
 for(unsigned round=0;round<4000;++round){
  unsigned key=rng()%32000;Check(cache.Find(e,key)==Linear(e,key),"lookup differs");
  if(round%17==0&&e.size()>2)std::swap(e[rng()%e.size()],e[rng()%e.size()]);
  if(round%67==0)e.erase(e.begin()+rng()%e.size());
 }
 e.clear();Check(!cache.Find(e,1),"stale index after clear");Add(e,1);Check(cache.Find(e,1)==e[0].get(),"same-key reconstruction stale");
 Check(!cache.Find(e,77),"missing key found");Add(e,77);Check(cache.Find(e,77)==e.back().get(),"negative lookup prevented append");
 e.clear();for(unsigned i=0;i<512;++i)Add(e,0x10000001+i*33);
 volatile unsigned checksum=0;
 auto bench=[&](bool indexed){auto start=std::chrono::steady_clock::now();for(unsigned i=0;i<1000000;++i){auto key=e[448+i%32]->collection.key;auto row=indexed?cache.Find(e,key):Linear(e,key);checksum+=row->collection.key;}return std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();};
 const auto linear=bench(false),indexed=bench(true);
 std::printf("PASS differential_mutations=4000 linear_ms=%.3f indexed_ms=%.3f checksum=%u\n",linear,indexed,unsigned(checksum));return 0;
 }catch(const std::exception& e){std::fprintf(stderr,"FAIL %s\n",e.what());return 1;}}
