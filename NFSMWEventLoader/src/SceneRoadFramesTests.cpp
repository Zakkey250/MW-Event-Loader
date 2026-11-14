#include <Windows.h>
#include <d3dx9.h>
#include <vector>
#include <fstream>
#include <string>
#include <cmath>
#include <algorithm>
#include <stdexcept>
#include <filesystem>
#include <cstdio>
#include <cstring>
#include "../tools/CarbonIntro/SceneAssets.h"
#include "CarbonIntroPolicy.h"
namespace fs=std::filesystem;
static unsigned checks=0;
static void Check(bool ok,const char* why){++checks;if(!ok)throw std::runtime_error(why);}
template<class T>void Write(std::ofstream& out,const T& value){out.write(reinterpret_cast<const char*>(&value),sizeof(value));}
static void Fixture(const fs::path& folder,unsigned magic,unsigned eyeRef=1,unsigned lookRef=2,bool truncate=false){
 fs::create_directories(folder);
 {std::ofstream f(folder/"camera.cic",std::ios::binary);unsigned h[]={0x31434943,2,120};f.write(reinterpret_cast<const char*>(h),sizeof(h));Write(f,5.f);CameraSample a{{1,0,1},{0,1,1},0,24,100,0},b=a;b.eye.x=9;Write(f,a);Write(f,b);}
 {std::ofstream f(folder/"cars.cic",std::ios::binary);unsigned h[]={0x52434943,2,15};f.write(reinterpret_cast<const char*>(h),sizeof(h));D3DXMATRIX m;D3DXMatrixIdentity(&m);for(unsigned i=0;i<2;++i){Write(f,1u);Write(f,m);}}
 for(const auto& row:std::vector<std::pair<const char*,unsigned>>{{"presentation.bin",0x31504943},{"rolling-start.bin",0x31535052},{"cues.cic",0x43424943}}){std::ofstream f(folder/row.first,std::ios::binary);Write(f,row.second);Write(f,row.first==std::string("rolling-start.bin")?1u:0u);}
 {std::ofstream f(folder/"camera-road-stations.bin",std::ios::binary);Write(f,magic);Write(f,2u);Write(f,0.f);Write(f,eyeRef);if(magic==0x33534352){Write(f,0.f);Write(f,1u);}if(!truncate){Write(f,10.f);Write(f,eyeRef);if(magic==0x33534352){Write(f,0.f);Write(f,lookRef);}}}
}
static void TravelTests(){
 SceneAssets a;a.rollingStart=true;a.participants=2;a.rate=120;a.duration=2;
 for(unsigned slot=0;slot<2;++slot)for(unsigned n=0;n<=30;++n){D3DXMATRIX m;D3DXMatrixRotationZ(&m,.2f);m._41=float(slot)*6;m._42=float(n)*20-600+slot*12;m._43=float(n)*.1f;a.cars[slot].push_back(m);}
 for(unsigned n=0;n<=240;++n){float t=float(n)/120;auto p=a.Car(0,t);a.cameras.push_back({{p._41+4,p._42-8,p._43+5},{p._41,p._42,p._43},.1f,24,100,float(n%50)});a.cameraRoadStations.push_back(p._42);a.cameraLookRoadStations.push_back(p._42+3);a.cameraRoadReferences.push_back(n<120?1:2);a.cameraLookRoadReferences.push_back(1);}
 for(unsigned n=0;n<a.cameras.size();++n){auto p=a.Car(0,float(n)/120);a.cameraRoadOrigins.push_back({p._41,p._42,p._43});a.cameraLookRoadOrigins.push_back({p._41+2,p._42+3,p._43+1});}
 auto before=a;a.CompressRollingTravel(2);
 Check(a.duration==before.duration&&a.rate==before.rate&&a.cameras.size()==before.cameras.size(),"travel compression must preserve source timing and frame counts");
 Check(a.cars[0].back()._42==before.cars[0].back()._42,"player GO stays at the event locator");
 for(unsigned n=0;n<=30;++n){auto p=a.Car(0,float(n)/15);auto q=before.Car(0,float(n)/15);Check(std::abs((p._42-a.cars[0].back()._42)*3-(q._42-before.cars[0].back()._42))<.001f,"player travel is one third at the original source time");Check(std::abs((a.cars[1][n]._42-a.cars[0][n]._42)-4)<.001f&&std::abs(a.cars[1][n]._41-a.cars[0][n]._41-2)<.001f,"spatial formation shrinks while actual model scale stays intact");}
 for(unsigned n=0;n<a.cameras.size();++n){Check(std::abs((a.cameras[n].eye.y-a.cameraRoadStations[n])-(before.cameras[n].eye.y-before.cameraRoadStations[n]))<.001f,"eye arm follows its own native avatar station");Check(std::abs((a.cameras[n].look.y-a.cameraLookRoadStations[n])-(before.cameras[n].look.y-before.cameraLookRoadStations[n]))<.001f,"look arm follows its independent native avatar station");Check(a.cameras[n].timeScale==100&&a.cameras[n].fade==before.cameras[n].fade&&a.cameraRoadReferences[n]==before.cameraRoadReferences[n],"cuts fades and time scale must be unchanged");Check(std::abs((a.cameraLookRoadStations[n]-a.cameraRoadStations[n])*3-3)<.001f,"independent camera road stations use the same spatial mapping");}
 for(unsigned n=0;n<a.cameras.size();++n){auto oldEye=before.cameras[n].eye-before.cameraRoadOrigins[n],newEye=a.cameras[n].eye-a.cameraRoadOrigins[n];auto oldLook=before.cameras[n].look-before.cameraLookRoadOrigins[n],newLook=a.cameras[n].look-a.cameraLookRoadOrigins[n];auto eyeDelta=oldEye-newEye,lookDelta=oldLook-newLook;Check(D3DXVec3Length(&eyeDelta)<.001f&&D3DXVec3Length(&lookDelta)<.001f,"full 3D camera arms must survive independent reference origin compression");}
 auto legacy=before;legacy.cameraRoadOrigins.clear();auto unchanged=legacy.cars[0][0];legacy.CompressRollingTravel(2);Check(!memcmp(&unchanged,&legacy.cars[0][0],sizeof(unchanged)),"legacy packs must never receive a partial unmatched camera transform");
 auto aerial=before;for(unsigned n=0;n<aerial.cameras.size();++n){aerial.cameraRoadOrigins[n].z=144;aerial.cameras[n].eye={116,aerial.cameraRoadOrigins[n].y,144};}aerial.CompressRollingTravel(2);
 for(unsigned n=0;n<aerial.cameras.size();++n){Check(std::abs(aerial.cameras[n].eye.x-aerial.cameraRoadOrigins[n].x)<=4.001f&&aerial.cameras[n].eye.z<=30,"city aerial remains over the road, outside rooftop booms");Check(aerial.cameras[n].timeScale==before.cameras[n].timeScale,"aerial fitting must not retime the source");}
 before.rollingStart=false;unchanged=before.cars[0][0];before.CompressRollingTravel(2);Check(!memcmp(&unchanged,&before.cars[0][0],sizeof(unchanged)),"ordinary race intros must retain their source paths");
}
int main(int argc,char** argv){fs::path folder;try{
 for(unsigned n=2;n<=8;++n){auto suffix=std::to_string(n);Check(mwel_carbon::Participants("HPR_RACE_ALIGN_"+suffix)==n,"HP grid counts match selected entrants");Check(!mwel_carbon::HprRacerAllowed("HPR_RACE_ALIGN_"+suffix,"sprint","drift"),"drift must reject a moving grid approach");Check(mwel_carbon::HprRacerAllowed("HPR_RACE_ALIGN_ONLINE_"+suffix,"sprint","drift"),"drift may use a stopped-player launch");}
 Check(!mwel_carbon::HprRacerAllowed("HPR_RACE_ROLLING_2","sprint","drift"),"drift rejects running start");
 Check(mwel_carbon::HprRacerAllowed("HPR_GAUNTLET_CANNONBALL","tollboothrace","gauntlet")&&!mwel_carbon::HprRacerAllowed("HPR_GAUNTLET_CANNONBALL","sprint","armsrace"),"Gauntlet uses a one-player source scene only in its correct mode");
 {SceneAssets a;D3DXMATRIX m;D3DXMatrixIdentity(&m);a.cars[0].assign(8,m);Check(a.StartsStopped(),"standing launch measured from actual initial poses");a.cars[0][7]._41=.1f;Check(!a.StartsStopped(),"rolling movement cannot be declared a launch by its name");a.cars[0][7]._41=NAN;Check(!a.StartsStopped(),"invalid launch motion rejected");}
 TravelTests();
 if(argc==3&&std::string(argv[1])=="--scene"){SceneAssets a;a.Load(argv[2]);std::printf("SCENE_ASSETS_PASS participants=%u duration=%.3f camera_frames=%zu independent_road_frames=%zu\n",a.participants,a.duration,a.cameras.size(),a.cameraLookRoadStations.size());return 0;}
 Check(argc==2,"supply temporary HPR_ fixture folder");folder=fs::absolute(argv[1]);Check(folder.filename()=="HPR_ROAD_FRAME_TEST","bounded test folder required");Check(!fs::exists(folder),"preserve existing data");
 Fixture(folder,0x32534352);{SceneAssets a;a.Load(folder.string());Check(a.rollingStart,"legacy rolling flag");Check(a.cameraRoadStations==a.cameraLookRoadStations&&a.cameraRoadReferences==a.cameraLookRoadReferences,"RCS2 view unchanged");Check(std::abs(a.CameraRoadStation(.5f/120)-5)<.001f&&std::abs(a.Camera(.5f/120).eye.x-5)<.001f,"legacy interpolation");}
 Fixture(folder,0x33534352);{SceneAssets a;a.Load(folder.string());Check(a.Camera(.5f/120).eye.x==1,"look-avatar cut must not blend camera");Check(a.CameraRoadStation(.5f/120)==0&&a.CameraRoadStation(.5f/120,true)==0,"both frames must hold at a source cut");Check(a.CameraRoadStation(1.f/120)==10&&a.CameraRoadStation(1.f/120,true)==0,"independent eye/look frames");}
 for(unsigned magic:{0u,0x33534352u}){Fixture(folder,magic,65,65);bool refused=false;try{SceneAssets a;a.Load(folder.string());}catch(const std::exception&){refused=true;}Check(refused,"invalid format/avatar accepted");}
 Fixture(folder,0x33534352,1,2,true);{bool refused=false;try{SceneAssets a;a.Load(folder.string());}catch(const std::exception&){refused=true;}Check(refused,"truncated independent frame accepted");}
 fs::remove_all(folder);std::printf("SCENE_ROAD_FRAMES_PASS %u checks; RCS2 compatibility, independent RCS3 frames, source cuts and malformed data\n",checks);return 0;
 }catch(const std::exception& error){std::fprintf(stderr,"FAIL %s\n",error.what());return 1;}}
