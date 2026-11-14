#pragma once
#include "SceneRoadPath.h"
namespace mwel_scene_road {
struct CameraFrame {Point center,right,forward,up;};
inline Point Cross3(Point a,Point b){return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
inline Point Unit3(Point p){float n=std::sqrt(p.x*p.x+p.y*p.y+p.z*p.z);return n>.0001f?p*(1/n):Point{};}
inline bool ContinuousRoadGrade(Point a,Point b){
 return Finite(a)&&Finite(b)&&std::abs(b.z-a.z)<=.05f+.4f*std::hypot(b.x-a.x,b.y-a.y);
}
inline bool SubjectInView(Point eye,Point look,Point subject,float lens){
 if(!Finite(eye)||!Finite(look)||!Finite(subject)||!std::isfinite(lens)||lens<1)return false;
 const auto forward=Unit3(look-eye),d=subject-eye;
 const auto dot=[](Point a,Point b){return a.x*b.x+a.y*b.y+a.z*b.z;};
 auto right=Unit3(Cross3(forward,{0,0,1}));if(std::abs(right.x)+std::abs(right.y)+std::abs(right.z)<.9f)right=Unit3(Cross3(forward,{0,1,0}));const auto up=Unit3(Cross3(right,forward));
 const float depth=dot(d,forward),horizontal=15.96f/lens;
 return depth>.2f&&depth<250&&std::abs(dot(d,right))<=depth*horizontal+1.2f&&std::abs(dot(d,up))<=depth*horizontal*.6f+1.2f;
}
// Converted scenery can contain render-only bridge beams and roofs. Physics
// rays cannot certify those meshes. Bound the camera to the road's central
// corridor as well as testing the physical boom; do not change vehicle paths.
inline Point CameraCorridorEye(const CameraFrame& frame,Point eye){
 const auto d=eye-frame.center;
 const auto dot=[](Point a,Point b){return a.x*b.x+a.y*b.y+a.z*b.z;};
 return frame.center+frame.right*std::clamp(dot(d,frame.right),-3.5f,3.5f)
  +frame.forward*std::clamp(dot(d,frame.forward),-8.f,8.f)
  +frame.up*std::clamp(dot(d,frame.up),.75f,6.f);
}
// Clip the eye volume along a clear boom from the subject. The source clock,
// vehicle path and shot orientation are independent of collision correction.
template<class Ray>bool ClipCameraBoom(Point focus,Point& eye,Ray&& ray){
 const auto d=eye-focus;const float length=std::sqrt(d.x*d.x+d.y*d.y+d.z*d.z);
 if(!Finite(focus)||!Finite(eye)||length<1)return false;
 float fraction=1;
 for(Point offset:{Point{},Point{.45f,.45f,.3f},Point{-.45f,.45f,.3f},Point{.45f,-.45f,.3f},Point{-.45f,-.45f,.3f},Point{.45f,.45f,-.3f},Point{-.45f,.45f,-.3f},Point{.45f,-.45f,-.3f},Point{-.45f,-.45f,-.3f}}){float hit=1;
  if(ray(focus+offset,eye+offset,hit)&&std::isfinite(hit)&&hit>=0&&hit<=1)fraction=std::min(fraction,std::max(0.f,hit-1.f/length));
 }
 if(fraction>=1)return false;eye=focus+d*fraction;return true;
}
// A camera has a long lever arm above the road. A single collision triangle's
// normal is not a camera track: seams made a source-continuous shot jump metres
// between frames. Fit heading/grade to the connected road graph over a fixed
// spatial footprint, independently of frame rate and source time. Collision
// supplies height only. Vehicle poses still use their exact road frame.
template<class Ground>bool FitCameraFrame(const Path& path,float station,Ground&& ground,CameraFrame& out){
 Frame anchor;if(!path.Sample(station,anchor))return false;
 const float radius=std::max(0.f,std::min({12.f,station+path.anchor,path.samples.back().station-path.anchor-station}));
 Point along{};float longitudinal=0,spread=0;
 for(int i=-4;i<=4;++i){Frame f;const float offset=i*radius/4.f,weight=float(5-std::abs(i));if(!path.Sample(station+offset,f))return false;along=along+f.forward*weight;longitudinal+=weight*offset*f.center.z;spread+=weight*offset*offset;}
 along.z=0;along=Unit(along);if(Length(along)<.9f)return false;
 const Point horizontalRight{along.y,-along.x,0};
 const float slope=spread>.001f?longitudinal/spread:0.f;
 if(!std::isfinite(slope))return false;
 out.up=Unit3(Point{0,0,1}-along*slope);
 out.right=horizontalRight;out.forward=Unit3(Cross3(out.up,out.right));
 out.center=anchor.center+Point{anchor.forward.y,-anchor.forward.x,0}*path.lateral;
 float height=0;if(ground(out.center,height)&&std::isfinite(height))out.center.z=height;
 return Finite(out.center)&&Finite(out.right)&&Finite(out.forward)&&Finite(out.up);
}
}
