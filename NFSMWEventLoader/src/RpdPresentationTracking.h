#pragma once
// Speech must describe the same real pursuit that its foreground selection
// uses. Prefer a unit with fresh native sight; never invent visibility or
// change gameplay pursuit membership to make a voice line work.
namespace rpd_presentation_tracking {
struct Selection {unsigned race=0,epoch=0,group=0,target=0,witness=0;ULONGLONG refresh=0;};
Selection selected;
unsigned RadioPursuit(unsigned officer){
 using gauntlet_runtime::Read;
 if(GetCurrentThreadId()!=combat_health_runtime::gameThread||!rpd_mission::Current()||
    !combat_health_runtime::Racing()||rpd_mission::ResultPending())return 0;
 unsigned race=0,count=0,actualOfficer=0;
 if(!Read(0x91E000,race)||!race||!Read(race+0x1958,count)||count<2||count>9||
    !Read(race+0x18,actualOfficer)||actualOfficer!=officer)return 0;
 const auto epoch=combat_health_runtime::generation;
 const auto now=GetTickCount64();
 if(selected.race==race&&selected.epoch==epoch&&now<selected.refresh&&
    selected.target&&hot_pursuit_runtime::RunningTarget(selected.target)&&
    hot_pursuit_runtime::FindPursuit(selected.target)==selected.group)return selected.group;
 const auto attached=hot_pursuit_runtime::OfficerPursuit(officer);
 unsigned speech=0,foreground=0;
 if(Read(0x993CC8,speech)&&speech)Read(speech+0x130,foreground);
 combat_impact::Motion local{};const auto car=gauntlet_runtime::Vehicle(officer);
 const bool localPose=car&&combat_health_runtime::NativeMotion(car-0x80,local);
 unsigned bestGroup=0,bestTarget=0,bestWitness=0,bestPriority=~0u;
 float bestDistance=1e30f;
 for(unsigned slot=1;slot<count;++slot){
  unsigned target=0;BYTE busted=0,bailed=0;
  if(!Read(race+0x18+slot*0x194,target)||!target||!hot_pursuit_runtime::RunningTarget(target))continue;
  const auto group=hot_pursuit_runtime::FindPursuit(target);
  if(!group||!Read(group+0xE8,busted)||busted||!Read(group+0xE9,bailed)||bailed)continue;
  const auto witness=hot_pursuit_runtime::SightWitness(group);
  const unsigned priority=witness?(group==attached?0u:group==foreground?1u:2u):(group==attached?3u:4u);
  float distance=1e30f;combat_impact::Motion motion{};const auto suspect=gauntlet_runtime::Vehicle(target);
  if(localPose&&suspect&&combat_health_runtime::NativeMotion(suspect-0x80,motion)){
   const auto delta=local.position-motion.position;distance=combat_impact::Dot(delta,delta);
  }
  if(priority>bestPriority||(priority==bestPriority&&distance>=bestDistance))continue;
  bestGroup=group;bestTarget=target;bestWitness=witness;bestPriority=priority;bestDistance=distance;
 }
 if(worldLogEnabled&&(selected.race!=race||selected.epoch!=epoch||selected.target!=bestTarget))
  Log("RPD_RADIO_SUBJECT_SELECTED racer=%u group=%08X witness=%u attached=%08X native_sight_only=1",bestTarget,bestGroup,bestWitness,attached);
 selected={race,epoch,bestGroup,bestTarget,bestWitness,now+150};
 return bestGroup;
}
}
