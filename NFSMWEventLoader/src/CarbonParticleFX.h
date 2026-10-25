#pragma once
#include <cstdint>
// Private C++ integration, not an exported MOD ABI. All calls belong to the
// game thread. NativeGetter must bypass the existing collection hooks.
using MWELCarbonNativeCollection=void*(__cdecl*)(std::uint32_t,std::uint32_t);
void* MWELCarbonEffectCollection(std::uint32_t classKey,std::uint32_t key);
bool MWELCarbonParticlePrepare(const char* definitionFile,MWELCarbonNativeCollection nativeGetter);
bool MWELCarbonParticleBegin();
bool MWELCarbonParticleUpdate(const float positions[2][3]);
void MWELCarbonParticleRelease();
const char* MWELCarbonParticleStatus();
struct MWELCarbonParticleStats{unsigned created,destroyed,live,createRefused,releaseRefused,nativeGroups,nativeEmitters;float requested[2][3],native[2][3],positionError;};
void MWELCarbonParticleDiagnosticMode(bool enabled);
MWELCarbonParticleStats MWELCarbonParticleDiagnostics();
