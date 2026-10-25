#pragma once

namespace event_preview {
struct NativeBounds { float lower[2], upper[2]; };

// 00510D30 computes zoom height as lower.y - upper.y. Package metadata uses
// top-to-bottom image coordinates instead. Match native 00510C10's 1/8 UV
// framing margin, while translating that vertical axis into the game's order.
// New package bounds are the course's UV rectangle within the FULL world
// texture. The native menu owns camera centering/zoom; art is never re-cropped.
constexpr NativeBounds ToNative(float x0,float y0,float x1,float y1) noexcept {
 constexpr float padding=0.125f;
 return {{x0-padding,y1+padding},{x1+padding,y0-padding}};
}
}
