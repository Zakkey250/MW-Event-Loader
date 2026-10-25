#include "../src/UpdateNotice.cpp"
#include <iostream>
#include <thread>
#include <fstream>
void MWELUpdateLog(const char*) {}
int main(int argc,char** argv){
 using namespace mod_update;using nlohmann::json;unsigned passed=0,failed=0;
 auto expect=[&](bool okay,const char* message){std::cout<<(okay?"PASS ":"FAIL ")<<message<<'\n';okay?++passed:++failed;};
 constexpr auto repo="Zakkey250/MW-Event-Loader";constexpr auto prefix="MW-Event-Loader-";
 auto row=[&](std::string tag,bool pre=false){return json{{"tag_name",tag},{"draft",false},{"prerelease",pre},{"published_at","2026-10-25T00:00:00Z"},{"html_url",std::string("https://github.com/")+repo+"/releases/tag/"+tag},{"assets",json::array({json{{"name",std::string(prefix)+tag+".zip"},{"state","uploaded"},{"size",12000u}}})}};};
 auto find=[&](json rows){return FindUpdate(rows.dump(),event_loader_update::Version,repo,prefix);};
 expect(!find(json::array({row("v1.0.1")})),"installed version does not notify");
 expect(!find(json::array({row("v0.9.0")})),"older version does not notify");
 auto next=find(json::array({row("v1.0.2")}));expect(next&&next->tag=="v1.0.2","new stable loader ZIP is detected");
 auto converter=row("cutscene-converter-v99.0.0");converter["assets"][0]["name"]="MWCutsceneConverter-99.0.0-Windows-x64.zip";
 expect(!find(json::array({converter})),"converter release is never a loader update");
 auto foreign=row("v99.0.0");foreign["assets"][0]["name"]="MWCutsceneConverter-99.0.0-Windows-x64.zip";
 expect(!find(json::array({foreign})),"converter asset with version tag is also ignored");
 next=find(json::array({converter,row("v1.0.3"),row("v1.0.2")}));expect(next&&next->tag=="v1.0.3","mixed unordered releases select newest loader");
 expect(!find(json::array({row("v2.0.0-beta.1",true)})),"stable install does not advertise prereleases");
 for(const auto field:{"draft","prerelease","published_at","html_url","tag_name","assets"}){auto bad=row("v1.0.2");bad.erase(field);expect(!find(json::array({bad})),"incomplete release ignored");}
 auto bad=row("v1.0.2");bad["draft"]=true;expect(!find(json::array({bad})),"draft ignored");
 bad=row("v1.0.2");bad["html_url"]="https://example.invalid/";expect(!find(json::array({bad})),"untrusted page ignored");
 for(const auto name:{"MW-Event-Loader-v1.0.2-source.zip","MW-Event-Loader-v1.0.2.zip.exe"}){bad=row("v1.0.2");bad["assets"][0]["name"]=name;expect(!find(json::array({bad})),"non-runtime asset ignored");}
 bad=row("v1.0.2");bad["assets"][0]["state"]="new";expect(!find(json::array({bad})),"unfinished upload ignored");
 bad=row("v1.0.2");bad["assets"][0]["size"]=0;expect(!find(json::array({bad})),"empty asset ignored");
 for(const auto version:{"v01.0.0","1.0","1.0.0-beta.01","1.0.0+dev","v1.0.1.exe"})expect(!ParseVersion(version),"malformed version rejected");
 expect(!FindUpdate("[{\"draft\":true,\"draft\":false}]","1.0.0",repo,prefix),"duplicate metadata keys rejected");
 expect(!FindUpdate("<html>error</html>","1.0.0",repo,prefix),"HTTP error document rejected");
 expect(!FindUpdate(std::string(2*1024*1024+1,' '),"1.0.0",repo,prefix),"oversized response rejected");
 expect(!FindUpdate(std::string(40,'[')+std::string(40,']'),"1.0.0",repo,prefix),"excessive JSON depth rejected");
 for(unsigned flow=0;flow<8;++flow)expect(event_loader_update::AllowedForFlow(flow)==(flow<6),"gameplay/free-roam gate");
 const auto temporary=std::filesystem::temp_directory_path()/(L"MWEL-update-test-"+std::to_wstring(GetCurrentProcessId()));
 expect(!std::filesystem::exists(temporary),"unique temporary INI fixture");std::filesystem::create_directory(temporary);auto ini=temporary/L"EventLoader.ini";
 expect(event_loader_update::Enabled(ini),"missing update setting enables notifications");
 WritePrivateProfileStringW(L"Updates",L"Enabled",L"0",ini.c_str());expect(!event_loader_update::Enabled(ini),"INI disables checker");
 WritePrivateProfileStringW(L"Updates",L"Language",L"ja",ini.c_str());expect(event_loader_update::UseJapanese(ini),"explicit Japanese notice");
 WritePrivateProfileStringW(L"Updates",L"Language",L"fr",ini.c_str());expect(!event_loader_update::UseJapanese(ini),"other languages use English notice");
 std::filesystem::remove(ini);std::filesystem::remove(temporary);
 expect(QueueName(123)==L"Local\\Zakkey250.NFSMW.UpdateNotice.v1.123","shared self-authored MOD queue identity");
 std::atomic<unsigned> active{0},maximum{0};
 auto notify=[&]{KernelHandle mutex;mutex.value=CreateMutexW(nullptr,FALSE,QueueName(GetCurrentProcessId()).c_str());if(WaitForSingleObject(mutex.value,2000)!=WAIT_OBJECT_0)return;auto count=++active;unsigned prior=maximum.load();while(prior<count&&!maximum.compare_exchange_weak(prior,count)){}Sleep(30);--active;ReleaseMutex(mutex.value);};
 std::thread a(notify),b(notify);a.join();b.join();expect(maximum==1,"cross-module notification serialization");
 expect(!ShowSerializedNotice(GetModuleHandleW(nullptr),L"Suppressed",L"",L"OK",+[]()noexcept{return false;},100,100),"no dialog during gameplay");
 if(argc>1&&std::string_view(argv[1])=="--live"){
  std::string payload;expect(event_loader_update::FetchReleases(payload),"real GitHub read-only HTTPS succeeds");
  if(!payload.empty()){auto eligible=FindUpdate(payload,"0.0.0",repo,prefix);std::cout<<"LIVE loader="<<(eligible?eligible->tag:"none")<<'\n';expect(!FindUpdate(payload,event_loader_update::Version,repo,prefix),"same published release is not a newer update");}
 }
 if(argc>1&&std::string_view(argv[1])=="--dialog"){
  expect(ShowSerializedNotice(GetModuleHandleW(nullptr),L"MW Event Loader — 更新通知テスト",L"新しいバージョンが公開されています。\r\n現在: 1.0.0\r\n公開版: v1.0.2\r\nテスト表示。ダウンロードは行いません。",L"閉じて続行",nullptr,500,800),"Japanese dialog auto-closes");
  expect(ShowSerializedNotice(GetModuleHandleW(nullptr),L"MW Event Loader — Update test",L"A newer version is available.\r\nInstalled: 1.0.0\r\nAvailable: v1.0.2\r\nTest only. No downloads.",L"Close and continue",nullptr,500,800),"English dialog auto-closes");
 }
 std::cout<<"RESULT passed="<<passed<<" failed="<<failed<<'\n';return failed?1:0;
}
