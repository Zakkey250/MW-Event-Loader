#pragma once
namespace mwel_rpd_retry {
constexpr bool SameOwner(bool rpd, unsigned race, unsigned key, unsigned epoch,
                         unsigned expectedRace, unsigned expectedKey, unsigned expectedEpoch) {
 return rpd && race && race == expectedRace && key == expectedKey && epoch == expectedEpoch;
}
constexpr bool SceneReleased(unsigned retiring, unsigned current, bool flowReadable, unsigned flow) {
 return !retiring && !current && flowReadable && !flow;
}
constexpr bool SimulationResumed(bool readable, unsigned world, unsigned mode) {
 // EN1.3 006F6D9A..006F6DCC advances simulation time only in mode 3.
 // An unpaused but still inactive/loading strategy must not start a retry.
 return readable && world && mode == 3;
}
}
