#pragma once
#include <Windows.h>
#include <initializer_list>

namespace mwel_world_provider {
inline HMODULE SourceModule() {
    for (auto name : {L"W2CMWPublic.asi", L"W2CMWv2.asi"}) {
        auto module = GetModuleHandleW(name);
        if (module && GetProcAddress(module, "W2MW_GeometryApiV1") &&
            GetProcAddress(module, "W2MW_CheckpointRuntimeV1") &&
            GetProcAddress(module, "W2MW_IsWorldActiveV1"))
            return module;
    }
    return nullptr;
}

inline HMODULE Module() {
    if (auto source = SourceModule())
        return source;
    if (auto legacy = GetModuleHandleW(L"World2MWNativeLoader.asi"))
        return legacy;
    return nullptr;
}
}
