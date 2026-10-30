#pragma once
#include <array>
#include <algorithm>
#include <cmath>
namespace mwel_drift_proximity {
struct Point {float x,z;};
struct Hull {float x=0,y=0,z=0,fx=0,fz=1,width=1,length=2;};
inline float Cross(Point a,Point b,Point c){return (b.x-a.x)*(c.z-a.z)-(b.z-a.z)*(c.x-a.x);}
inline float PointSegment(Point p,Point a,Point b){float x=b.x-a.x,z=b.z-a.z,n=x*x+z*z;float t=n>1e-8f?std::clamp(((p.x-a.x)*x+(p.z-a.z)*z)/n,0.f,1.f):0;return std::hypot(p.x-a.x-t*x,p.z-a.z-t*z);}
inline float Segment(Point a,Point b,Point c,Point d){
 if(Cross(a,b,c)*Cross(a,b,d)<0&&Cross(c,d,a)*Cross(c,d,b)<0)return 0;
 return std::min({PointSegment(a,c,d),PointSegment(b,c,d),PointSegment(c,a,b),PointSegment(d,a,b)});
}
inline std::array<Point,4> Corners(Hull h){float n=std::hypot(h.fx,h.fz);h.fx/=n;h.fz/=n;std::array<Point,4> p{};unsigned i=0;for(auto signs:std::array<Point,4>{{{1,1},{-1,1},{-1,-1},{1,-1}}})p[i++]={h.x+signs.x*h.width*(-h.fz)+signs.z*h.length*h.fx,h.z+signs.x*h.width*h.fx+signs.z*h.length*h.fz};return p;}
inline bool Valid(Hull h){return std::isfinite(h.x)&&std::isfinite(h.y)&&std::isfinite(h.z)&&std::isfinite(h.fx)&&std::isfinite(h.fz)&&std::isfinite(h.width)&&std::isfinite(h.length)&&std::hypot(h.fx,h.fz)>.9f&&h.width>.2f&&h.width<5&&h.length>.5f&&h.length<12;}
inline bool Contains(Hull h,Point p){float n=std::hypot(h.fx,h.fz),x=p.x-h.x,z=p.z-h.z;return std::abs((x*h.fx+z*h.fz)/n)<=h.length&&std::abs((-x*h.fz+z*h.fx)/n)<=h.width;}
inline float Clearance(Hull a,Hull b){
 if(!Valid(a)||!Valid(b)||std::abs(a.y-b.y)>2.5f||std::hypot(a.x-b.x,a.z-b.z)>30)return 10000;
 auto x=Corners(a),y=Corners(b);if(Contains(a,y[0])||Contains(b,x[0]))return 0;
 float distance=10000;for(unsigned i=0;i<4;++i)for(unsigned j=0;j<4;++j)distance=std::min(distance,Segment(x[i],x[(i+1)%4],y[j],y[(j+1)%4]));return distance;
}
// Positive space between actual hulls/wall faces. Touching/overlap gives no
// reward; score policy separately requires a grounded, advancing real slide.
inline bool NearWall(float d){return std::isfinite(d)&&d>=.15f&&d<=1.5f;}
inline bool NearCar(float d){return std::isfinite(d)&&d>=.15f&&d<=2.f;}
}
