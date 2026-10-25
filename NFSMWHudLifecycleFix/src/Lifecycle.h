#pragma once
#include <cstdint>

namespace hudfix {
// No timing guesses: a generation is completed only after the live bindings exist.
struct Lifecycle {
    uintptr_t hud=0, widget=0, root=0;
    uint32_t reset=0;
    bool active=false, complete=false;
    bool Observe(uintptr_t h, uintptr_t w, uintptr_t r, uint32_t generation,
                 bool visible, bool ready) {
        if(h!=hud || w!=widget || r!=root || generation!=reset || visible!=active) {
            hud=h; widget=w; root=r; reset=generation; active=visible; complete=false;
        }
        return active && ready && !complete;
    }
    void Applied() { complete=true; }
};
inline bool Eligible(uint64_t features, uint64_t mask) {return (features & mask)!=0;}
// FEObject_SetVisible (00514CC0): respect visibility lock 0x40, clear hidden
// bit 0, and mark rendering dirty. Apply to the parent ONLY: recursive showing
// would expose animation-controlled child elements.
inline uint32_t VisibleRootFlags(uint32_t flags) {
    return (flags&0x40) || !(flags&1) ? flags : ((flags&~1u)|0x02400000u);
}
}
