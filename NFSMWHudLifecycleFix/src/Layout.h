#pragma once
#include <cstdint>
#include <cmath>
namespace hudfix {
// Remember the translation we applied, not an absolute hard-coded HUD position.
// Recycled FE objects and native animation/reset changes invalidate the baseline.
struct Layout {
    uintptr_t object=0, data=0;
    float base=0, last=0;
    float Baseline(uintptr_t o,uintptr_t d,float current) {
        if(o!=object || d!=data || std::fabs(current-last)>0.001f)base=current;
        object=o;data=d;return base;
    }
    void Applied(float value){last=value;}
};
}
