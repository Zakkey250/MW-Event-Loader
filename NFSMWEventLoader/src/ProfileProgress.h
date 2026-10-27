// Additional results are stored only in the event loader's profile sidecar.
// An unnamed SkipFE player is never confused with a real loaded profile.
namespace profile_progress {
bool restorePending=false;
std::string restoredIdentity;
// Prefix layout from the SDK cFrontEndDatabase/UserProfile definitions.
// Keep profile access independent of option/car/audio structures.
struct FrontendPrefix {BYTE players,boot,saved,loaded,dirty,overwrite;unsigned stable;char joyports[2];void* profiles[2];};
struct UserPrefix {char name[32];bool named;};
static_assert(offsetof(FrontendPrefix,loaded)==3&&offsetof(FrontendPrefix,profiles)==0x10&&offsetof(UserPrefix,named)==32);
std::string CurrentIdentity(){
 FrontendPrefix* database=nullptr;UserPrefix* profile=nullptr;bool loaded=false,named=false;char name[32]{};
 if(!Copy(reinterpret_cast<void*>(0x91CF90),&database,sizeof(database))||!database||
    !Copy(reinterpret_cast<BYTE*>(database)+offsetof(FrontendPrefix,loaded),&loaded,sizeof(loaded))||!loaded||
    !Copy(reinterpret_cast<BYTE*>(database)+offsetof(FrontendPrefix,profiles),&profile,sizeof(profile))||!profile||
    !Copy(reinterpret_cast<BYTE*>(profile)+offsetof(UserPrefix,named),&named,sizeof(named))||!named||
    !Copy(profile,name,sizeof(name)))return {};
 auto end=std::find(std::begin(name),std::end(name),'\0');if(end==std::begin(name)||end==std::end(name))return {};
 return std::string(name,end);
}
bool NativeRecords(void* database,std::vector<event_save::Record>& records,unsigned& address){
 unsigned count=0;address=0;
 if(!database||!Copy(database,&count,4)||count>4096||!Copy(static_cast<BYTE*>(database)+0x3C,&address,4)||!address)return false;
 records.resize(count);return Copy(reinterpret_cast<void*>(address),records.data(),records.size()*sizeof(event_save::Record));
}
void SaveNativeProgress(void* database){
 auto identity=CurrentIdentity();if(identity.empty())return;
 std::vector<event_save::Record> records;unsigned address=0;if(!NativeRecords(database,records,address))throw std::runtime_error("Sidecar native records unavailable");
 std::vector<std::pair<mwel::EventPackage,event_save::Record>> progress;
 for(const auto& package:packages)if(!mwel::Correction(package.authoring)){
  auto id=registration_experiment::Hash(package.nativeEventId.c_str());
  auto found=std::find_if(records.begin(),records.end(),[&](const auto& r){return r.id==id;});
  // A cleared native database is meaningful: do not resurrect old completion.
  progress.emplace_back(package,found==records.end()?event_save::Record{id,0,0,0}:*found);
 }
 mwel::ProfileSidecar sidecar(mwel_paths::Profiles(game),identity);bool saved=sidecar.SaveNativeProgress(progress);
 Log("EVENT_PROGRESS_NATIVE_CAPTURE profile_hash=%s records=%u committed=%u native_save_written=0",mwel::Digest(identity).c_str(),unsigned(progress.size()),unsigned(saved));
}
void RestoreNativeProgress(){
 auto identity=CurrentIdentity();if(identity.empty()){restoredIdentity.clear();return;}
 if(!restorePending&&identity==restoredIdentity)return;
 void* database=nullptr;if(!Copy(reinterpret_cast<void*>(0x91E004),&database,4)||!database)return;
 std::vector<event_save::Record> records;unsigned address=0;if(!NativeRecords(database,records,address))return;
 try{
  auto candidate=records;mwel::ProfileSidecar sidecar(mwel_paths::Profiles(game),identity);unsigned restored=0;
  for(const auto& package:packages)if(!mwel::Correction(package.authoring)){
   auto id=registration_experiment::Hash(package.nativeEventId.c_str());event_save::Record value{};
   if(!sidecar.NativeProgress(package,id,value))continue;
   auto found=std::find_if(candidate.begin(),candidate.end(),[&](const auto& r){return r.id==id;});
   if(found==candidate.end())found=std::find_if(candidate.begin(),candidate.end(),[](const auto& r){return !r.id;});
   if(found==candidate.end())throw std::runtime_error("No additional progress slot available");
   *found=value;++restored;
  }
  // Build and validate the whole replacement first. Only owned/empty slots
  // change; original native progress and correction-mode saves remain native.
  for(unsigned i=0;i<records.size();++i)if(memcmp(&records[i],&candidate[i],sizeof(event_save::Record))){
   if(records[i].id&&!std::any_of(packages.begin(),packages.end(),[&](const auto& p){return !mwel::Correction(p.authoring)&&registration_experiment::Hash(p.nativeEventId.c_str())==records[i].id;}))throw std::runtime_error("Original progress replacement refused");
  }
  for(unsigned i=0;i<records.size();++i)if(memcmp(&records[i],&candidate[i],sizeof(event_save::Record)))memcpy(reinterpret_cast<void*>(address+i*sizeof(event_save::Record)),&candidate[i],sizeof(event_save::Record));
  restoredIdentity=identity;restorePending=false;
  Log("EVENT_PROGRESS_NATIVE_RESTORE profile_hash=%s records=%u original_records_preserved=1 native_save_written=0",mwel::Digest(identity).c_str(),restored);
 }catch(const std::exception& error){restorePending=false;restoredIdentity=identity;Log("EVENT_PROGRESS_RESTORE_REFUSED reason=%s",error.what());}
}
void Record(const mwel::EventPackage& package,void* nativeRecord,bool genuine,bool forced,bool survival=false){
 using NFSPluginSDK::MW05::GRacerInfo;
 static_assert(sizeof(GRacerInfo)==0x194&&offsetof(GRacerInfo,mRaceTimer)==0x14C);
 if((!genuine&&!survival)||forced)return;
 auto identity=CurrentIdentity();if(identity.empty()){Log("EVENT_PROGRESS_SKIPPED reason=no_loaded_named_profile");return;}
 GRacerInfo info{};if(!Copy(nativeRecord,&info,sizeof(info))||info.mDNF||!info.mFinishedRacing||info.mRaceTimer.mRunning)return;
 // Native Finish awards first-place rewards when mRanking == 1 (00611B11).
 // The native ranking is already one-based.
 if(info.mRanking<1||info.mRanking>16)return;
 mwel::FinishEvidence finish{genuine,forced,unsigned(info.mRanking),info.mRaceTimer.mTotalTime,std::max(0.f,info.mPointTotal),survival};
 try{mwel::ProfileSidecar sidecar(mwel_paths::Profiles(game),identity);
  bool saved=sidecar.Record(package,finish);Log("EVENT_PROGRESS_SIDECAR saved=%u profile_hash=%s event=%s revision=%u place=%u native_seconds=%.6f sidecar_native_save_written=0 save_isolation_verified=0",unsigned(saved),mwel::Digest(identity).c_str(),package.id.c_str(),package.revision,finish.place,finish.elapsedSeconds);
 }catch(const std::exception& error){Log("EVENT_PROGRESS_FAILED reason=%s",error.what());}
}
}
