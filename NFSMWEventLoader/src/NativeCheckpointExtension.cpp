#include "NativeCheckpointExtension.h"
#include <cstdio>
#include <cstring>
#include "../../integration/MWCheckpointRuntime.h"
void MWELNeoLog(const char* text);
static void CheckpointLog(const char* format,...){char text[512]{};va_list args;va_start(args,format);vsnprintf_s(text,sizeof(text),_TRUNCATE,format,args);va_end(args);MWELNeoLog(text);}
bool MWELInstallCheckpointExtension(){return mw_checkpoint::InstallCheckpointExtension(GetModuleHandleW(L"World2MWNativeLoader.asi"),"W2MW_CheckpointRuntimeV1",CheckpointLog);}
#pragma comment(linker,"/EXPORT:MWEL_CheckpointRuntimeV1=_MWEL_CheckpointRuntimeV1")
extern "C" __declspec(dllexport) unsigned __cdecl MWEL_CheckpointRuntimeV1(mw_checkpoint::Runtime* out,unsigned size){return mw_checkpoint::QueryRuntime(out,size);}
