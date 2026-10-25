// Explicit no-save local-race fixture. Production and profile-backed races
// never enter this path. It changes only native per-vehicle attributes, not
// banks, tuning, boost policy, save records or controller behavior.
bool performanceFixture=false;
unsigned performanceFixtureHandles[16]{};
bool ApplyPerformanceFixture(BYTE* vehicle,unsigned handle,unsigned slot){
 if(!performanceFixture||slot>=16||performanceFixtureHandles[slot]==handle)return true;
 const BYTE maximumGuard[]={0x83,0xEC,0x20,0x33,0xC0,0x89,0x04,0x24};
 const BYTE junkmanGuard[]={0x6A,0xFF,0x68,0x90,0xAB,0x87,0x00};
 if(memcmp(reinterpret_cast<void*>(0x67F950),maximumGuard,sizeof(maximumGuard))||
    memcmp(reinterpret_cast<void*>(0x678150),junkmanGuard,sizeof(junkmanGuard)))return false;
 using namespace NFSPluginSDK::MW05;
 using Upgrades=Physics::Upgrades::Package;using Type=Physics::Upgrades::Type;
 __try {
  auto& attributes=*reinterpret_cast<Attrib::Gen::pvehicle*>(vehicle+0x24);
  if(!attributes.mCollection||!attributes.mLayoutPtr)return false;
  Upgrades::SetMaximum(attributes);
  unsigned mask=0,levels[7]{};float percent[7]{};
  for(unsigned i=0;i<7;++i){
   if(Upgrades::CanInstallJunkman(attributes,Type(i))){Upgrades::SetJunkman(attributes,Type(i));mask|=1u<<i;}
   levels[i]=unsigned(Upgrades::GetLevel(attributes,Type(i)));
   percent[i]=Upgrades::GetPercent(attributes,Type(i));
  }
  // AI private customization records must agree with the live package;
  // the no-save debug player may have no record at all.
  auto record=Read<BYTE*>(vehicle,0x38);
  if(record){auto package=record+offsetof(FECustomizationRecord,mInstalledPhysics);
   memcpy(package,levels,sizeof(levels));memcpy(package+28,&mask,sizeof(mask));}
  performanceFixtureHandles[slot]=handle;
  Log("EXPERIMENT_NATIVE_PERFORMANCE_FIXTURE handle=%u slot=%u maximum_native=1 junkman_mask=%u levels=%u,%u,%u,%u,%u,%u,%u percent=%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f record=%u save_load=skipped",handle,slot,mask,levels[0],levels[1],levels[2],levels[3],levels[4],levels[5],levels[6],percent[0],percent[1],percent[2],percent[3],percent[4],percent[5],percent[6],unsigned(record!=nullptr));
  return true;
 }__except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
