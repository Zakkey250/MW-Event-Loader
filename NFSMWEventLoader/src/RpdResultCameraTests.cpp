#include "RpdResultCameraPolicy.h"
#include "RpdArrestReceipt.h"
#include "RpdArrestNisPolicy.h"
#include "RpdIntroMusicPolicy.h"
#include "../../integration/RpdHeatTierPolicy.h"
#include <fstream>
#include <iostream>
#include <limits>
#include <cstdlib>
static unsigned checks=0;
static void Check(bool value,const char* name){++checks;if(!value){std::cerr<<"FAIL "<<name<<'\n';std::exit(1);}}
int main(int argc,char** argv){
 Check(argc==2,"asset argument");std::ifstream input(argv[1],std::ios::binary);std::string bytes((std::istreambuf_iterator<char>(input)),{});
 std::array<rpd_result_camera::Track,2> tracks;
 Check(rpd_result_camera::Load(bytes,tracks),"authored HP camera asset loads");
 Check(tracks[0].end>9&&tracks[1].end>3&&tracks[1].end<4,"arrest and wreck lengths retained");
 auto shot=tracks[0].At(tracks[0].end);auto framed=rpd_result_camera::FrameSuspect(shot,1,2.3f);
 Check(framed.look[0]==0&&framed.look[1]==0&&framed.look[2]==.25f,"source actor trajectory cannot move focus away from live suspect");
 float framedDistance=0;for(unsigned i=0;i<3;++i)framedDistance+=(framed.eye[i]-framed.look[i])*(framed.eye[i]-framed.look[i]);
 Check(std::sqrt(framedDistance)>6.7f,"MW vehicle bounds fit the result frame");
 auto original=tracks[0].samples[0].eye[0];auto bad=bytes;bad[0]='X';
 Check(!rpd_result_camera::Load(bad,tracks)&&tracks[0].samples[0].eye[0]==original,"bad asset leaves accepted tracks intact");
 Check(!rpd_result_camera::Load(bytes.substr(0,bytes.size()-1),tracks),"truncation rejected");
 Check(!rpd_result_camera::Load(bytes+"x",tracks),"trailing bytes rejected");
 auto invalid=tracks[0];invalid.samples[0].eye[0]=std::numeric_limits<float>::quiet_NaN();Check(!invalid.Valid(),"NaN rejected");
 invalid=tracks[0];invalid.rate=60;Check(!invalid.Valid(),"unapproved rate rejected");
 rpd_result_camera::Track cut;cut.rate=30;cut.end=1.f/30;cut.samples.resize(2);cut.samples[0].eye[0]=1;cut.samples[1].eye[0]=11;
 Check(cut.At(1.f/60).eye[0]==1,"camera cut does not sweep through scene");cut.samples[1].eye[0]=2;
 Check(std::abs(cut.At(1.f/60).eye[0]-1.5f)<.001f,"continuous source motion interpolates");
 Check(cut.At(-1).eye[0]==1&&cut.At(10).eye[0]==2,"source endpoints clamp");
 rpd_result_camera::Timeline time;time.Begin();time.Tick(1,true,1);Check(time.seconds==0,"pause freezes presentation");
 time.Tick(std::numeric_limits<float>::quiet_NaN(),false,1);Check(time.seconds==0,"invalid time ignored");
 time.Tick(5,false,1);Check(std::abs(time.seconds-.1f)<.001f,"stall cannot skip cinematic");
 for(unsigned i=0;i<20;++i)time.Tick(.1f,false,1);Check(time.held&&time.seconds==1,"terminal holds without native script dispatch");
 rpd_arrest::Receipt receipt;
 receipt.Observe(1,2,3,4,false,false);Check(!receipt.Matches(1,2,3,4),"normal chase cannot create arrest receipt");
 receipt.Observe(1,2,3,4,true,false);Check(receipt.Matches(1,2,3,4),"native GRacerInfo bust survives missing pursuit");
 receipt.Observe(1,2,3,4,false,false);Check(receipt.Matches(1,2,3,4),"activity teardown cannot lose real arrest");
 for(unsigned field=0;field<4;++field){unsigned scope[]={1,2,3,4};++scope[field];Check(!receipt.Matches(scope[0],scope[1],scope[2],scope[3]),"different race event generation or suspect rejected");}
 receipt.Observe(1,2,4,4,false,false);Check(!receipt.Matches(1,2,4,4),"retry invalidates old arrest even with reused race and handle");
 receipt.Observe(1,2,4,4,false,true);Check(receipt.Matches(1,2,4,4),"native pursuit arrest also accepted");
 receipt.Reset();Check(!receipt.Matches(1,2,4,4),"world exit clears receipt");
 for(std::size_t count: {1u,2u,4u,5u,37u,100u,1024u}){
  std::size_t previous=0;
  for(unsigned heat=1;heat<=10;++heat){const auto band=rpd_heat_tier::Candidates(count,heat);Check(band.begin>=previous&&band.begin<band.end&&band.end<=count,"every heat has a bounded nonempty ascending installed pool");previous=band.begin;}
 }
 Check(rpd_heat_tier::Candidates(100,1).begin==0&&rpd_heat_tier::Candidates(100,1).end==20,"heat one bottom quintile");
 Check(rpd_heat_tier::Candidates(100,5).begin==80&&rpd_heat_tier::Candidates(100,5).end==100,"heat five upper quintile");
 Check(rpd_heat_tier::Candidates(100,10).begin==95&&rpd_heat_tier::Candidates(100,10).end==100,"extended heat ten top five percent");
 Check(!rpd_heat_tier::Candidates(0,5).end&&!rpd_heat_tier::Candidates(100,0).end&&!rpd_heat_tier::Candidates(100,11).end,"empty catalog and invalid heat rejected");
 Check(!rpd_arrest_nis::HoldBeforeEnd(1,8)&&rpd_arrest_nis::HoldBeforeEnd(7.84f,8),"native scene length decides terminal hold, not gauge or fixed scene duration");
 Check(!rpd_arrest_nis::HoldBeforeEnd(0,0)&&!rpd_arrest_nis::HoldBeforeEnd(std::numeric_limits<float>::quiet_NaN(),8),"invalid NIS timing rejected");
 for(unsigned i=0;i<rpd_arrest_nis::scenes.size();++i)for(unsigned j=0;j<i;++j)Check(std::string(rpd_arrest_nis::scenes[i])!=rpd_arrest_nis::scenes[j],"each native arrest variant has one random-pool entry");
 using rpd_intro_music_policy::Gain;
 Check(Gain(0,12,15)==1&&Gain(12,12,15)==1,"jingle retains authored gain through countdown start");
 Check(Gain(13.5f,12,15)==.5f&&Gain(15,12,15)==0&&Gain(17,12,15)==0,"jingle fades across countdown and is silent at GO");
 Check(Gain(121.5f,120,123)==.5f&&Gain(3,0,6)==.5f,"fade follows actual scene countdown for future durations");
 Check(Gain(13,12,15)==Gain(13,12,15),"paused scene clock retains gain");
 Check(Gain(0,0,0)==0&&Gain(0,-1,3)==0&&Gain(std::numeric_limits<float>::quiet_NaN(),12,15)==0&&Gain(1,12,std::numeric_limits<float>::infinity())==0,"invalid fade clock is silent");
 for(const auto& name:{"rpd_01.asf","RPD-4.asf"})Check(rpd_intro_music_policy::FileName(name),"native catalog filename accepted");
 for(const auto& name:{".asf","../rpd.asf","a/b.asf","a\\b.asf","a.mp3","a.asf.exe"})Check(!rpd_intro_music_policy::FileName(name),"non-native or escaping catalog filename rejected");
 std::cout<<"PASS "<<checks<<" result camera and intro music checks\n";
}
