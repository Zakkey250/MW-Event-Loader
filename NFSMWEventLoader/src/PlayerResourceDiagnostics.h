#pragma once
// Read-only lifecycle evidence. No resource retains/releases or repairs here.
// Diagnostic OFF bypasses traversal and formatting entirely.
ULONGLONG playerResourceUntil=0,playerResourceNext=0;
void PlayerResourceSnapshot(const char* boundary){
 if(!worldLogEnabled)return;
 auto loader=reinterpret_cast<BYTE*>(0x9B2C00);
 auto partPool=Read<BYTE*>(reinterpret_cast<void*>(0x9B091C));
 Log("EVENT_PART_MODEL_POOL boundary=%s used=%u peak=%u stride=%u capacity=%u read_only=1",boundary,
  Read<unsigned>(partPool,0x18),Read<unsigned>(partPool,0x1C),Read<unsigned>(partPool,0x28),Read<unsigned>(partPool,0x2C));
 Log("EVENT_CAR_RESOURCE_BOUNDARY boundary=%s flow=%u mode=%u car_loading=%u loaded_rides=%u allocated_rides=%u render_count=%u",
  boundary,Read<unsigned>(reinterpret_cast<void*>(0x925E90)),Read<unsigned>(Read<BYTE*>(reinterpret_cast<void*>(0x91E000)),0x1960),Read<unsigned>(loader,0x14),Read<unsigned>(loader,0x1C),Read<unsigned>(loader,0x20),Read<unsigned>(reinterpret_cast<void*>(0x9B37D4)));
 auto sentinel=loader+0x58;auto node=Read<BYTE*>(sentinel);BYTE* seen[64]{};
 for(unsigned i=0;node&&node!=sentinel&&i<64;++i){
  for(unsigned j=0;j<i;++j)if(seen[j]==node)return;seen[i]=node;
  auto car=Read<BYTE*>(node,0x348),skin=Read<BYTE*>(node,0x350),solid=Read<BYTE*>(car,0x1C);
  Log("EVENT_CAR_RESOURCE_RIDE boundary=%s owner=%08X ride=%08X instances=%d state=%u player=%u car=%08X car_state=%u solid=%08X solid_instances=%d skin=%08X skin_states=%08X permanent_layers=%u temporary_layers=%u",
   boundary,unsigned(node),unsigned(node+0x1C),Read<int>(node,8),Read<unsigned>(node,0x0C),Read<unsigned>(node,0x18),unsigned(car),Read<unsigned>(car,0x18),unsigned(solid),int(Read<short>(solid,0x14)),unsigned(skin),Read<unsigned>(skin,0x0C),Read<unsigned>(skin,0x20),Read<unsigned>(skin,0x180));
  node=Read<BYTE*>(node);
 }
 auto connections=Read<BYTE**>(reinterpret_cast<void*>(0x9B37CC));unsigned count=Read<unsigned>(reinterpret_cast<void*>(0x9B37D4));
 if(!connections||count>64)return;
 for(unsigned i=0;i<count;++i){auto conn=Read<BYTE*>(connections,i*4);auto render=Read<BYTE*>(conn,0x44);auto ride=Read<BYTE*>(render,0x84);
  Log("EVENT_CAR_RESOURCE_RENDER boundary=%s connection=%08X render=%08X ride=%08X model_type=%u render_head=%08X,%08X,%08X,%08X",
   boundary,unsigned(conn),unsigned(render),unsigned(ride),Read<unsigned>(ride),Read<unsigned>(render),Read<unsigned>(render,4),Read<unsigned>(render,8),Read<unsigned>(render,12));
  if(ride&&Read<unsigned>(ride,0x308)==0){
   auto key=Read<unsigned>(ride,60);auto cached=Read<BYTE*>(render,0x244);auto resident=key&&package_resources::originalTexture?static_cast<BYTE*>(package_resources::originalTexture(key,0,0)):nullptr;
   const unsigned cachedHash=cached&&cached!=reinterpret_cast<BYTE*>(~0u)?Read<unsigned>(cached,36):0;
   Log("EVENT_PLAYER_SKIN_REFERENCE boundary=%s expected=%08X cached=%08X cached_hash=%08X resident=%08X resident_hash=%08X",boundary,key,unsigned(cached),cachedHash,unsigned(resident),Read<unsigned>(resident,36));
  }
 }
}
void PlayerResourceBoundary(const char* boundary){
 if(!worldLogEnabled)return;PlayerResourceSnapshot(boundary);playerResourceUntil=GetTickCount64()+10000;playerResourceNext=0;
}
void PlayerResourceFrame(bool racing=false){
 if(!worldLogEnabled)return;auto now=GetTickCount64();
 if(racing){if(now<playerResourceNext)return;playerResourceNext=now+5000;PlayerResourceSnapshot("active_race");return;}
 if(now>playerResourceUntil||now<playerResourceNext)return;
 playerResourceNext=now+1000;PlayerResourceSnapshot("post_exit_frame");
}
