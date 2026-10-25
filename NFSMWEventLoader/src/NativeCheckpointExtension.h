#pragma once
#include "../../integration/MWCheckpointRuntimeApi.h"
bool MWELInstallCheckpointExtension();
extern "C" __declspec(dllexport) unsigned __cdecl MWEL_CheckpointRuntimeV1(mw_checkpoint::Runtime*,unsigned);
