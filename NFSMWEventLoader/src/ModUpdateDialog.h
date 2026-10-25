#pragma once
#include <Windows.h>
#include <cstdint>
#include <string>
#include <shellapi.h>
#pragma comment(lib,"shell32.lib")

// Cross-ASI notification protocol v1. This file can be copied into another MOD;
// no shared DLL, symbol hook, module discovery, or load-order dependency exists.
namespace mod_update {
inline std::wstring QueueName(DWORD pid) {
    return L"Local\\Zakkey250.NFSMW.UpdateNotice.v1."+std::to_wstring(pid);
}
struct KernelHandle {
    HANDLE value=nullptr;
    ~KernelHandle(){if(value)CloseHandle(value);}
    KernelHandle()=default;
    KernelHandle(const KernelHandle&)=delete;
    KernelHandle& operator=(const KernelHandle&)=delete;
};
struct DialogContext {
    const wchar_t *title,*body,*button;
    bool (*allowed)() noexcept;
    ULONGLONG deadline;
    bool created=false;
    const wchar_t* url=nullptr;
    const wchar_t* openLabel=nullptr;
};
inline INT_PTR CALLBACK UpdateDialog(HWND window,UINT message,WPARAM wp,LPARAM lp) {
    auto* context=reinterpret_cast<DialogContext*>(GetWindowLongPtrW(window,DWLP_USER));
    if(message==WM_INITDIALOG) {
        context=reinterpret_cast<DialogContext*>(lp);SetWindowLongPtrW(window,DWLP_USER,lp);
        context->created=true;SetWindowTextW(window,context->title);
        SetDlgItemTextW(window,101,context->body);SetDlgItemTextW(window,IDOK,context->button);
        ShowWindow(GetDlgItem(window,102),SW_HIDE);
        if(context->url) {SetDlgItemTextW(window,102,context->openLabel);ShowWindow(GetDlgItem(window,102),SW_SHOW);}
        RECT rect{};GetWindowRect(window,&rect);
        MONITORINFO monitor{sizeof(monitor)};
        if(GetMonitorInfoW(MonitorFromWindow(GetForegroundWindow(),MONITOR_DEFAULTTOPRIMARY),&monitor))
            SetWindowPos(window,HWND_TOP,(monitor.rcWork.left+monitor.rcWork.right-(rect.right-rect.left))/2,
                (monitor.rcWork.top+monitor.rcWork.bottom-(rect.bottom-rect.top))/2,0,0,SWP_NOSIZE);
        SetTimer(window,1,100,nullptr);
        return TRUE;
    }
    if(message==WM_COMMAND&&LOWORD(wp)==102&&context&&context->url) {
        ShellExecuteW(window,L"open",context->url,nullptr,nullptr,SW_SHOWNORMAL);return TRUE;
    }
    if(message==WM_COMMAND&&(LOWORD(wp)==IDOK||LOWORD(wp)==IDCANCEL)) {EndDialog(window,LOWORD(wp));return TRUE;}
    if(message==WM_CLOSE){EndDialog(window,IDCANCEL);return TRUE;}
    if(message==WM_TIMER&&context&&((context->allowed&&!context->allowed())||GetTickCount64()>=context->deadline)) {
        EndDialog(window,IDCANCEL);return TRUE;
    }
    return FALSE;
}
inline bool ShowSerializedNotice(HMODULE module,const wchar_t* title,const wchar_t* body,
                                  const wchar_t* button,bool (*allowed)() noexcept,
                                  DWORD queueMs=90000,DWORD visibleMs=60000,
                                  const wchar_t* url=nullptr,const wchar_t* openLabel=L"Open GitHub") {
    KernelHandle mutex;mutex.value=CreateMutexW(nullptr,FALSE,QueueName(GetCurrentProcessId()).c_str());
    if(!mutex.value)return false;
    const auto deadline=GetTickCount64()+queueMs;
    bool acquired=false;
    do {
        if(allowed&&!allowed())return false;
        const DWORD result=WaitForSingleObject(mutex.value,100);
        if(result==WAIT_OBJECT_0||result==WAIT_ABANDONED){acquired=true;break;}
        if(result!=WAIT_TIMEOUT)return false;
    }while(GetTickCount64()<deadline);
    if(!acquired)return false;
    // Mutex is never held during HTTP/file work; only around this bounded UI.
    struct Unlock {HANDLE mutex;~Unlock(){ReleaseMutex(mutex);}} unlock{mutex.value};
    if(allowed&&!allowed())return false;
    // Dialog units + Segoe UI font allow the Windows dialog manager to scale
    // controls together. No fixed game resolution, HUD hooks or foreground force.
    alignas(DWORD) WORD words[512]{};WORD* cursor=words;
    auto* dialog=reinterpret_cast<DLGTEMPLATE*>(cursor);
    dialog->style=WS_POPUP|WS_CAPTION|WS_SYSMENU|DS_MODALFRAME|DS_SETFONT;
    dialog->cdit=3;dialog->cx=350;dialog->cy=154;
    cursor+=sizeof(DLGTEMPLATE)/sizeof(WORD);
    auto word=[&](WORD v){*cursor++=v;};
    auto text=[&](const wchar_t* value){while(*value)word(static_cast<WORD>(*value++));word(0);};
    word(0);word(0);word(0);word(10);text(L"Segoe UI");
    auto item=[&](DWORD style,short x,short y,short width,short height,WORD id,WORD klass,const wchar_t* label){
        if(reinterpret_cast<std::uintptr_t>(cursor)&3)word(0);
        auto* control=reinterpret_cast<DLGITEMTEMPLATE*>(cursor);
        control->style=WS_CHILD|WS_VISIBLE|style;control->x=x;control->y=y;control->cx=width;control->cy=height;control->id=id;
        cursor+=sizeof(DLGITEMTEMPLATE)/sizeof(WORD);word(0xffff);word(klass);text(label);word(0);
    };
    item(SS_LEFT|SS_NOPREFIX,12,10,326,111,101,0x0082,L"");
    item(WS_TABSTOP|BS_DEFPUSHBUTTON,211,128,127,17,IDOK,0x0080,L"OK");
    item(WS_TABSTOP|BS_PUSHBUTTON,12,128,185,17,102,0x0080,L"Open GitHub");
    DialogContext context{title,body,button,allowed,GetTickCount64()+visibleMs,false,url,openLabel};
    // No game-window owner: do not disable/reenter the game's startup thread.
    DialogBoxIndirectParamW(module,dialog,nullptr,UpdateDialog,reinterpret_cast<LPARAM>(&context));
    return context.created;
}
}
