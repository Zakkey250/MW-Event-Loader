#pragma once
// A native CP can already be passed while its canonical next road is still
// ahead of an off-road body. Retain the preceding authored road as a physical
// recovery approach; the native pending CP and lap are never rewound.
static bool NeoRecoveryPrefix(const std::vector<YellowRecoveryPoint>& previous,const std::vector<YellowRecoveryPoint>& current,
 mwel_grid::Launch& launch,std::vector<YellowRecoveryPoint>& prefix){
 if(previous.size()<2||current.size()<2)return false;
 const auto& last=previous.back();const auto& first=current.front();
 if(mwneo::Distance({last.p[0],last.p[1],last.p[2]},{first.p[0],first.p[1],first.p[2]})>4)return false;
 float begin=std::max(previous.front().station,last.station-160.f),length=last.station-begin;
 if(length<12)return false;std::vector<YellowRecoveryPoint> points;
 for(float s=begin;s<last.station;s+=4){Vec3 p;if(!YellowLineSample(previous,s,p))return false;points.push_back({{p.x,p.y,p.z},s-begin});}
 points.push_back({{first.p[0],first.p[1],first.p[2]},length});
 auto direction=mwneo::UnitXZ({current[1].p[0]-first.p[0],0,current[1].p[2]-first.p[2]});if(mwneo::LengthXZ(direction)<.9f)return false;
 mwel_grid::Launch candidate;candidate.origin={points.front().p[0],points.front().p[1],points.front().p[2]};
 candidate.gate={first.p[0],first.p[1],first.p[2]};candidate.direction=direction;candidate.length=length;
 launch=candidate;prefix=std::move(points);return true;
}
