// Private high-resolution observation inside the existing actuator update.
// No new native hooks, control changes, native writes or logger workers.
struct NeoLandingFrame {
 int leg=-1;
 float time=0,position[3]{},velocity[3]{},forward[3]{},steering=0,throttle=0,brake=0;
 NeoResetObservation reset;
 MWELNeoMotionState motion;
 float priorInputs[3]{};
};
struct NeoLandingTrace {
 std::uint64_t raceGeneration=0;
 void* race=nullptr;void* vehicle=nullptr;unsigned body=0,handle=0,lap=~0u,next=0,count=0;
 float previousTime=0,airAt=-100;bool dumped=false;
 std::array<NeoLandingFrame,256> frames{};
};
static NeoLandingTrace neoLandingTraces[16];
static bool NeoReadLandingFrame(void* vehicle,unsigned body,const MWELNeoFrameState& v,NeoLandingFrame& out){
 __try {
  if(!vehicle||!body||*reinterpret_cast<unsigned*>(body)!=0x8AC880||*reinterpret_cast<unsigned*>(static_cast<BYTE*>(vehicle)-0x78)!=v.handle)return false;
  NeoNativeDynamics dynamics;dynamics.body=body;
  auto position=**reinterpret_cast<BYTE***>(body+0x30)+0x10;dynamics.positionPointer=unsigned(position);
  memcpy(out.position,position,12);memcpy(out.velocity,position+0x10,12);
  if(std::hypot(std::hypot(out.position[0]-v.position[0],out.position[2]-v.position[2]),out.position[1]-v.position[1])>.02f)return false;
  memcpy(out.forward,v.forward,12);
  memcpy(out.priorInputs,v.priorInputs,12);
  return NeoReadResetObservation(static_cast<BYTE*>(vehicle),dynamics,out.reset);
 }__except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
static void NeoObserveLandingFrame(void* vehicle,const MWELNeoFrameState& v,float steering,float throttle,float brake){
 if(!settingsLoaded||!neoSettings.testMode||!neoSettings.telemetry)return;
 const auto race=*reinterpret_cast<BYTE**>(0x91E000);if(!race)return;
 unsigned slot=16;for(unsigned i=0;i<16;++i)if(neoFlightStates[i].race==race&&neoFlightStates[i].vehicle==vehicle&&neoFlightStates[i].handle==v.handle){slot=i;break;}
 if(slot==16)return;const auto& flight=neoFlightStates[slot];const auto& provider=neoProviders[slot];
 const float time=SimulationSeconds();if(!std::isfinite(time)||time<=0)return;
 auto& trace=neoLandingTraces[slot];
 if(trace.raceGeneration!=planRaceGeneration||trace.race!=race||trace.vehicle!=vehicle||trace.body!=flight.body||trace.handle!=v.handle||trace.lap!=provider.lap||time<trace.previousTime){
  trace.raceGeneration=planRaceGeneration;trace.race=race;trace.vehicle=vehicle;trace.body=flight.body;trace.handle=v.handle;trace.lap=provider.lap;
  trace.next=trace.count=0;trace.previousTime=0;trace.airAt=-100;trace.dumped=false;
 }
 if(time<=trace.previousTime)return;
 NeoLandingFrame sample;sample.leg=provider.leg;sample.time=time;sample.steering=steering;sample.throttle=throttle;sample.brake=brake;
 if(!NeoReadLandingFrame(vehicle,flight.body,v,sample))return;
 MWELNeoReadMotion(v.handle,sample.motion);
 trace.previousTime=time;if(sample.reset.wheels==0)trace.airAt=time;
 trace.frames[trace.next]=sample;trace.next=(trace.next+1)%trace.frames.size();trace.count=std::min(unsigned(trace.frames.size()),trace.count+1);
 const auto& r=sample.reset;float angular2=0;for(float x:r.angular)angular2+=x*x;
 if(trace.dumped||time-trace.airAt>1.f||(angular2<=25.f&&r.up[1]>=.5f))return;
 trace.dumped=true;
 Log("MW_NEOAI_LANDING_TRACE_BEGIN handle=%u CP=%d lap=%u body_pointer=%u native_seconds=%.6f samples=%u source=existing_owned_actuator_frame no_new_native_hook=1",v.handle,provider.leg,provider.lap,flight.body,time,trace.count);
 for(unsigned i=0;i<trace.count;++i){const auto& f=trace.frames[(trace.next+trace.frames.size()-trace.count+i)%trace.frames.size()];if(time-f.time>2.f)continue;const auto& q=f.reset;
  Log("MW_NEOAI_LANDING_FRAME handle=%u CP=%d lap=%u body_pointer=%u native_seconds=%.6f position=%.6f,%.6f,%.6f velocity=%.6f,%.6f,%.6f forward=%.6f,%.6f,%.6f up=%.6f,%.6f,%.6f ground_normal=%.6f,%.6f,%.6f,%.6f angular_velocity=%.6f,%.6f,%.6f steering=%.6f throttle=%.6f brake=%.6f wheels=%d compression=%.6f,%.6f,%.6f,%.6f surfaces=%u,%u,%u,%u flipped=%.6f nos_read=%u nos_requested=%u nos_engaged=%u nos_tank=%.6f power_scale=%.6f prior_inputs=%.6f,%.6f,%.6f",v.handle,f.leg,provider.lap,flight.body,f.time,f.position[0],f.position[1],f.position[2],f.velocity[0],f.velocity[1],f.velocity[2],f.forward[0],f.forward[1],f.forward[2],q.up[0],q.up[1],q.up[2],q.normal[0],q.normal[1],q.normal[2],q.normal[3],q.angular[0],q.angular[1],q.angular[2],f.steering,f.throttle,f.brake,q.wheels,q.compression[0],q.compression[1],q.compression[2],q.compression[3],q.wheelSurface[0],q.wheelSurface[1],q.wheelSurface[2],q.wheelSurface[3],q.flipped,unsigned(f.motion.nosRead),unsigned(f.motion.nosRequested),unsigned(f.motion.nosEngaged),f.motion.nosTank,f.motion.powerScale,f.priorInputs[0],f.priorInputs[1],f.priorInputs[2]);
 }
 Log("MW_NEOAI_LANDING_TRACE_END handle=%u body_pointer=%u native_seconds=%.6f",v.handle,flight.body,time);
}
