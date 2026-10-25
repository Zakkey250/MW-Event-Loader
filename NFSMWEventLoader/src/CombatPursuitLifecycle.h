#pragma once
namespace combat_pursuit_lifecycle {
// Both modes temporarily own the same native CopMgr limits. Retire the old
// mode before either Frame installs limits or requests cars for the new one.
// Reordering the two Frame calls alone only reverses the broken transition.
inline void BeforeFrame(){
 if(hot_pursuit_runtime::owner&&!hot_pursuit_runtime::Active())hot_pursuit_runtime::Reset();
 if(gauntlet_runtime::owner&&!gauntlet_runtime::Current())gauntlet_runtime::Reset();
}
}
