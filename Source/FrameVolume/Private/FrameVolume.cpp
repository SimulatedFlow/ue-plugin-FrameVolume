// Copyright 2026 Simulated Flow. All Rights Reserved.

#include "FrameVolume.h"
#include "FrameVolumeLog.h"

DEFINE_LOG_CATEGORY(LogFrameVolume);

#define LOCTEXT_NAMESPACE "FFrameVolumeModule"

void FFrameVolumeModule::StartupModule()
{
	UE_LOG(LogFrameVolume, Log, TEXT("FrameVolume started."));
}

void FFrameVolumeModule::ShutdownModule()
{
	UE_LOG(LogFrameVolume, Log, TEXT("FrameVolume shut down."));
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FFrameVolumeModule, FrameVolume)
