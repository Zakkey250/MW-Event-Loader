#pragma once
// Body tracking error grows with travel during steering response. This is
// geometric corner clearance, never a fixed vehicle speed/power limit.
// Other providers retain YellowLineArc's original default padding.
static float NeoExecutionCornerReserve(const mwneo::Vehicle& v){
 return .025f+std::clamp((std::abs(v.speed)-25.f)*.009f,0.f,.65f);
}
