#include "UpdateNotice.h"
#include "UpdateRelease.h"
#include "ModUpdateDialog.h"
#include <winhttp.h>
#include <atomic>
#include <memory>
#pragma comment(lib,"winhttp.lib")
#pragma comment(lib,"advapi32.lib")
#pragma comment(lib,"user32.lib")
void MWELUpdateLog(const char*);
namespace event_loader_update { namespace {
constexpr char repository[]="Zakkey250/MW-Event-Loader";
constexpr char assetPrefix[]="MW-Event-Loader-";
constexpr wchar_t apiPath[]=L"/repos/Zakkey250/MW-Event-Loader/releases?per_page=100";
constexpr wchar_t releasesUrl[]=L"https://github.com/Zakkey250/MW-Event-Loader/releases";
std::atomic<bool> checked{false};
mod_update::KernelHandle checkedEvent;
struct InternetHandle {
    HINTERNET value;
    explicit InternetHandle(HINTERNET v):value(v){}
    ~InternetHandle(){if(value)WinHttpCloseHandle(value);}
    InternetHandle(const InternetHandle&)=delete;
    InternetHandle& operator=(const InternetHandle&)=delete;
};
bool FetchReleases(std::string& output) {
    const auto deadline=GetTickCount64()+10000;
    InternetHandle session(WinHttpOpen(L"MW-Event-Loader-UpdateNotice/1",WINHTTP_ACCESS_TYPE_NO_PROXY,
                                     WINHTTP_NO_PROXY_NAME,WINHTTP_NO_PROXY_BYPASS,0));
    if(!session.value||!WinHttpSetTimeouts(session.value,1500,2000,2000,2000))return false;
    InternetHandle connection(WinHttpConnect(session.value,L"api.github.com",INTERNET_DEFAULT_HTTPS_PORT,0));
    if(!connection.value)return false;
    InternetHandle request(WinHttpOpenRequest(connection.value,L"GET",apiPath,nullptr,WINHTTP_NO_REFERER,
                                              WINHTTP_DEFAULT_ACCEPT_TYPES,WINHTTP_FLAG_SECURE));
    if(!request.value)return false;
    DWORD redirects=WINHTTP_OPTION_REDIRECT_POLICY_NEVER,auth=WINHTTP_AUTOLOGON_SECURITY_LEVEL_HIGH;
    DWORD disabled=WINHTTP_DISABLE_COOKIES;
    if(!WinHttpSetOption(request.value,WINHTTP_OPTION_REDIRECT_POLICY,&redirects,sizeof(redirects))||
       !WinHttpSetOption(request.value,WINHTTP_OPTION_AUTOLOGON_POLICY,&auth,sizeof(auth))||
       !WinHttpSetOption(request.value,WINHTTP_OPTION_DISABLE_FEATURE,&disabled,sizeof(disabled)))return false;
    constexpr wchar_t headers[]=L"Accept: application/vnd.github+json\r\nX-GitHub-Api-Version: 2022-11-28\r\n";
    if(!WinHttpSendRequest(request.value,headers,static_cast<DWORD>(-1),WINHTTP_NO_REQUEST_DATA,0,0,0)||
       GetTickCount64()>=deadline||!WinHttpReceiveResponse(request.value,nullptr))return false;
    DWORD status=0,size=sizeof(status);
    if(!WinHttpQueryHeaders(request.value,WINHTTP_QUERY_STATUS_CODE|WINHTTP_QUERY_FLAG_NUMBER,WINHTTP_HEADER_NAME_BY_INDEX,
                            &status,&size,WINHTTP_NO_HEADER_INDEX)||status!=200)return false;
    output.clear();char buffer[8192];
    while(GetTickCount64()<deadline) {
        DWORD read=0;
        if(!WinHttpReadData(request.value,buffer,sizeof(buffer),&read))return false;
        if(!read)return !output.empty();
        if(output.size()+read>2*1024*1024)return false;
        output.append(buffer,read);
    }
    return false;
}

bool AllowedForFlow(unsigned flow) noexcept {return flow<6;}
bool StartupNoticeAllowed() noexcept {
    unsigned flow=6;SIZE_T read=0;
    const auto address=reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr))+0x525E90;
    return ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(address),&flow,sizeof(flow),&read)&&read==sizeof(flow)&&AllowedForFlow(flow);
}
bool Enabled(const std::filesystem::path& ini) {
    return GetPrivateProfileIntW(L"Updates",L"Enabled",1,ini.c_str())!=0;
}
bool UseJapanese(const std::filesystem::path& ini) {
    wchar_t language[256]{};
    GetPrivateProfileStringW(L"Updates",L"Language",L"auto",language,256,ini.c_str());
    if(_wcsicmp(language,L"auto"))return mod_update::Japanese(language);
    const auto scripts=ini.parent_path().parent_path();
    GetPrivateProfileStringW(L"LANGUAGE",L"Language",L"",language,256,(scripts/L"NFSMostWanted.WidescreenFix.ini").c_str());
    if(language[0])return mod_update::Japanese(language);
    HKEY key=nullptr;
    if(RegOpenKeyExW(HKEY_LOCAL_MACHINE,L"SOFTWARE\\EA Games\\Need for Speed Most Wanted",0,KEY_QUERY_VALUE|KEY_WOW64_32KEY,&key)==ERROR_SUCCESS){
        DWORD size=sizeof(language),type=0;
        if(RegQueryValueExW(key,L"Language",nullptr,&type,reinterpret_cast<BYTE*>(language),&size)!=ERROR_SUCCESS||type!=REG_SZ)language[0]=0;
        language[255]=0;RegCloseKey(key);
    }
    return mod_update::Japanese(language);
}
struct Context {HMODULE module;std::filesystem::path ini;};
DWORD WINAPI NoticeWorker(void* pointer) {
    auto context=std::unique_ptr<Context>(static_cast<Context*>(pointer));const auto module=context->module;
    try{
        const auto eventName=mod_update::QueueName(GetCurrentProcessId())+L".Checked.MW-Event-Loader";
        checkedEvent.value=CreateEventW(nullptr,TRUE,TRUE,eventName.c_str());
        if(checkedEvent.value&&GetLastError()!=ERROR_ALREADY_EXISTS&&StartupNoticeAllowed()){
            std::string payload;
            if(!FetchReleases(payload))MWELUpdateLog("UPDATE_NOTICE unavailable offline_or_HTTP_or_timeout startup_unaffected=1");
            else {
                const auto update=mod_update::FindUpdate(payload,Version,repository,assetPrefix);
                if(update&&StartupNoticeAllowed()){
                    const bool ja=UseJapanese(context->ini);const std::wstring current(Version,Version+sizeof(Version)-1),latest(update->tag.begin(),update->tag.end());
                    std::wstring text=ja?L"新しいバージョンが公開されています。\r\n\r\n現在: ":L"A newer version is available.\r\n\r\nInstalled: ";
                    text+=current+(ja?L"\r\n公開版: ":L"\r\nAvailable: ")+latest;
                    text+=ja?L"\r\n\r\n更新はゲーム終了後に行ってください。\r\n自動ダウンロード・インストールは行いません。":L"\r\n\r\nUpdate after closing the game.\r\nNo automatic download or installation.";
                    text+=L"\r\n\r\ngithub.com/Zakkey250/MW-Event-Loader/releases";
                    mod_update::ShowSerializedNotice(module,ja?L"MW Event Loader — 更新通知":L"MW Event Loader — Update available",text.c_str(),ja?L"閉じて続行":L"Close and continue",StartupNoticeAllowed,90000,60000,releasesUrl,ja?L"GitHubを開く":L"Open GitHub");
                    MWELUpdateLog("UPDATE_NOTICE eligible_release notification_only downloads=0");
                }else MWELUpdateLog("UPDATE_NOTICE no_newer_loader_release_or_gameplay_active");
            }
        }
    }catch(...){MWELUpdateLog("UPDATE_NOTICE skipped_after_error startup_unaffected=1");}
    context.reset();FreeLibraryAndExitThread(module,0);return 0;
}
} // namespace
void StartNotices(const std::filesystem::path& ini) noexcept {
    try{
        if(!Enabled(ini)||checked.exchange(true))return;
        HMODULE module=nullptr;
        if(!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,reinterpret_cast<LPCWSTR>(&StartNotices),&module))return;
        try{
            auto context=std::make_unique<Context>(Context{module,ini});
            HANDLE worker=CreateThread(nullptr,0,NoticeWorker,context.get(),0,nullptr);
            if(worker){context.release();CloseHandle(worker);}else FreeLibrary(module);
        }catch(...){FreeLibrary(module);}
    }catch(...){/* Notification failure must never affect event loading. */}
}
} // namespace event_loader_update
