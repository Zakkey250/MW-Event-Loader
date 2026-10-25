// Independent event ASI course-reader state.
struct YellowNavPoint {float p[3],d[3];};
static std::vector<std::vector<YellowNavPoint>> yellowNavLegs;
static unsigned yellowNavHash=0;
static bool LoadYellowNavigation(unsigned hash){
 if(yellowNavHash==hash)return !yellowNavLegs.empty();
 yellowNavHash=hash;yellowNavLegs.clear();
 char config[MAX_PATH],file[MAX_PATH];if(!CourseFile(hash,"opponents.ini",config,sizeof(config)))return false;
 if(!GetPrivateProfileIntA("AI","YellowRoute",0,config))return false;
 if(!CourseFile(hash,"ai-route.bin",file,sizeof(file)))return false;
 FILE* f=nullptr;fopen_s(&f,file,"rb");if(!f)return false;
 unsigned header[4]{};bool ok=fread(header,4,4,f)==4&&header[0]==0x49413257&&header[1]==1&&header[2]==hash&&header[3]>0&&header[3]<=257;
 std::vector<std::vector<YellowNavPoint>> legs;
 if(ok)for(unsigned i=0;i<header[3];++i){
  unsigned n=0;if(fread(&n,4,1,f)!=1||!n||n>256){ok=false;break;}
  std::vector<YellowNavPoint> points(n);if(fread(points.data(),sizeof(YellowNavPoint),n,f)!=n){ok=false;break;}
  for(auto& point:points)for(unsigned j=0;j<3;++j)if(!std::isfinite(point.p[j])||fabs(point.p[j])>100000||!std::isfinite(point.d[j])||fabs(point.d[j])>1.01f)ok=false;
  if(!ok)break;legs.push_back(std::move(points));
 }
 if(ok&&fgetc(f)!=EOF)ok=false;fclose(f);
 if(!ok){Log("YELLOW_NAVIGATION_REFUSED invalid_sidecar event=%08X",hash);return false;}
 yellowNavLegs=std::move(legs);Log("YELLOW_NAVIGATION_LOADED event=%08X legs=%u original_CP_order=1 GPS=0",hash,unsigned(yellowNavLegs.size()));return true;
}

struct YellowRecoveryPoint {float p[3];float station=0;};
static unsigned yellowRecoveryHash=0;
static std::vector<std::vector<YellowRecoveryPoint>> yellowRecoveryLegs;
static bool LoadYellowBranches(unsigned hash);
static bool LoadYellowRecoveryRoute(unsigned hash){
 if(yellowRecoveryHash==hash)return !yellowRecoveryLegs.empty();
 yellowRecoveryHash=hash;yellowRecoveryLegs.clear();
 char file[MAX_PATH];if(!CourseFile(hash,"ai-recovery.bin",file,sizeof(file)))return false;
 FILE* f=nullptr;fopen_s(&f,file,"rb");if(!f)return false;
 unsigned header[4]{};bool ok=fread(header,4,4,f)==4&&header[0]==0x43523257&&header[1]==1&&header[2]==hash&&header[3]==yellowNavLegs.size();
 std::vector<std::vector<YellowRecoveryPoint>> legs;
 if(ok)for(unsigned i=0;i<header[3];++i){unsigned n=0;if(fread(&n,4,1,f)!=1||n<2||n>8192){ok=false;break;}
  std::vector<YellowRecoveryPoint> points(n);for(unsigned j=0;j<n;++j){if(fread(points[j].p,4,3,f)!=3){ok=false;break;}
   for(auto x:points[j].p)if(!std::isfinite(x)||fabs(x)>100000)ok=false;
   if(j){float sum=0;for(unsigned k=0;k<3;++k){float d=points[j].p[k]-points[j-1].p[k];sum+=d*d;}points[j].station=points[j-1].station+sqrtf(sum);}
  }if(!ok)break;legs.push_back(std::move(points));
 }
 if(ok&&fgetc(f)!=EOF)ok=false;fclose(f);if(!ok)return false;
 yellowRecoveryLegs=std::move(legs);if(!LoadYellowBranches(hash)){yellowRecoveryLegs.clear();return false;}Log("YELLOW_AI_RECOVERY_ROUTE event=%08X legs=%u selected_roads_only=1",hash,unsigned(yellowRecoveryLegs.size()));return true;
}

#include <map>
struct YellowBranchVariant {std::vector<YellowNavPoint> guides;std::vector<YellowRecoveryPoint> road;};
static std::vector<std::vector<YellowBranchVariant>> yellowBranchLegs;
#include "../../World2MW/src/NativeLoader/YellowBranchGraph.h"
static std::vector<std::vector<YellowLocalAlternative>> yellowLocalBranchLegs;
static bool LoadYellowBranches(unsigned hash){
 yellowBranchLegs.clear();yellowLocalBranchLegs.clear();char file[MAX_PATH];if(!CourseFile(hash,"ai-branches.bin",file,sizeof(file)))return false;
 FILE* f=nullptr;fopen_s(&f,file,"rb");if(!f)return GetFileAttributesA(file)==INVALID_FILE_ATTRIBUTES&&GetLastError()==ERROR_FILE_NOT_FOUND;
 unsigned h[4]{};bool ok=fread(h,4,4,f)==4&&h[0]==0x56423257&&(h[1]==1||h[1]==2||h[1]==3)&&h[2]==hash&&h[3]==yellowNavLegs.size();
 if(ok&&(h[1]==2||h[1]==3)){unsigned total=0;std::vector<std::vector<YellowLocalAlternative>> legs;
  for(unsigned leg=0;leg<h[3]&&ok;++leg){unsigned count=0;if(fread(&count,4,1,f)!=1||count>4096){ok=false;break;}
   std::vector<YellowLocalAlternative> routes;auto& main=yellowRecoveryLegs[leg];unsigned prior=0;
   for(unsigned i=0;i<count&&ok;++i){unsigned n[4]{};const unsigned fields=h[1]==3?4:3;
    if(fread(n,4,fields,f)!=fields||n[0]>=n[1]||n[1]>=main.size()||n[2]<2||n[2]>8192||(h[1]==3&&n[3]>1)||(i&&n[0]<prior)){ok=false;break;}
    total+=n[2];if(total>262144){ok=false;break;}prior=n[0];YellowLocalAlternative route;route.start=n[0];route.end=n[1];route.priority=h[1]==3?n[3]:0;route.points.resize(n[2]);
    for(auto& p:route.points){if(fread(p.data(),4,3,f)!=3){ok=false;break;}for(float x:p)if(!std::isfinite(x)||fabs(x)>100000)ok=false;}
    if(ok){float gap=0;for(unsigned k=0;k<3;++k){float a=route.points.front()[k]-main[route.start].p[k],b=route.points.back()[k]-main[route.end].p[k];gap+=a*a+b*b;}if(gap>.0001f)ok=false;}
    if(ok)routes.push_back(std::move(route));
   }if(ok)legs.push_back(std::move(routes));
  }
  if(ok&&fgetc(f)!=EOF)ok=false;fclose(f);if(!ok){Log("LOCAL_BRANCH_REFUSED_425 event=%08X invalid_payload=1",hash);return false;}
  // Legacy v3 brown alternatives are exclusions, never random low-priority choices.
  unsigned rejected=0,excluded=0;for(unsigned leg=0;leg<legs.size();++leg){std::vector<std::array<float,3>> main;for(const auto& p:yellowRecoveryLegs[leg])main.push_back({p.p[0],p.p[1],p.p[2]});auto& options=legs[leg];auto end=std::remove_if(options.begin(),options.end(),[&](const auto& route){if(route.priority!=0){++excluded;return true;}if(!YellowForwardAlternative(main,route)){++rejected;return true;}return false;});options.erase(end,options.end());}
  Log("WEB_FORWARD_BRANCH_FILTER_428 event=%08X rejected_reverse_spurs=%u CP_order_preserved=1",hash,rejected);
  yellowLocalBranchLegs=std::move(legs);Log("LOCAL_BRANCH_GRAPH_425 event=%08X per_departure_choice=1 committed_until_merge=1 discouraged_excluded=%u",hash,excluded);return true;
 }
 unsigned totalPoints=0,totalGuides=0;std::vector<std::vector<YellowBranchVariant>> legs;
 if(ok)for(unsigned leg=0;leg<h[3];++leg){unsigned count=0;if(fread(&count,4,1,f)!=1||count>31){ok=false;break;}
  std::vector<YellowBranchVariant> variants;
  for(unsigned v=0;v<count&&ok;++v){unsigned n[2]{};
   if(fread(n,4,2,f)!=2||n[0]<1||n[0]>256||n[1]<2||n[1]>8192){ok=false;break;}
   totalGuides+=n[0];totalPoints+=n[1];if(totalGuides>65536||totalPoints>262144){ok=false;break;}
   YellowBranchVariant variant;variant.guides.resize(n[0]);variant.road.resize(n[1]);
   for(auto& g:variant.guides){if(fread(g.p,4,3,f)!=3||fread(g.d,4,3,f)!=3){ok=false;break;}
    for(float x:g.p)if(!std::isfinite(x)||fabs(x)>100000)ok=false;
    float norm=0;for(float x:g.d){if(!std::isfinite(x))ok=false;norm+=x*x;}if(norm<.9f||norm>1.1f)ok=false;}
   for(unsigned i=0;i<n[1]&&ok;++i){auto& p=variant.road[i];if(fread(p.p,4,3,f)!=3){ok=false;break;}
    for(float x:p.p)if(!std::isfinite(x)||fabs(x)>100000)ok=false;
    if(i){float sum=0;for(unsigned k=0;k<3;++k){float d=p.p[k]-variant.road[i-1].p[k];sum+=d*d;}p.station=variant.road[i-1].station+sqrtf(sum);}}
   if(ok){auto& main=yellowRecoveryLegs[leg];float gap=0;
    for(unsigned k=0;k<3;++k){float a=variant.road.front().p[k]-main.front().p[k],b=variant.road.back().p[k]-main.back().p[k],c=variant.guides.back().p[k]-main.back().p[k];gap+=a*a+b*b+c*c;}
    if(gap>.0001f||variant.road.back().station<.01f)ok=false;}
   if(ok)variants.push_back(std::move(variant));
  }if(!ok)break;legs.push_back(std::move(variants));
 }
 if(ok&&fgetc(f)!=EOF)ok=false;fclose(f);if(!ok){Log("ORANGE_BRANCH_REFUSED event=%08X invalid_payload=1",hash);return false;}
 yellowBranchLegs=std::move(legs);Log("ORANGE_BRANCH_CHOICES_V1 event=%08X per_racer_per_leg=1 physical_CP_unchanged=1",hash);return true;
}
static bool YellowCustomThroughCP(const std::vector<YellowRecoveryPoint>& current,const std::vector<YellowRecoveryPoint>& next,float station,std::vector<YellowRecoveryPoint>& combined){
 if(current.size()<2||next.size()<2||!std::isfinite(station)||station<0||station>current.back().station||current.back().station-station>300)return false;
 // Only extend an already contiguous authored edge chain. Never fill a gap.
 float gap=0;for(unsigned k=0;k<3;++k){float d=current.back().p[k]-next.front().p[k];gap+=d*d;}if(gap>0.000001f)return false;
 combined=current;const float offset=current.back().station;
 for(unsigned i=1;i<next.size();++i){auto point=next[i];point.station+=offset;combined.push_back(point);}return true;
}
