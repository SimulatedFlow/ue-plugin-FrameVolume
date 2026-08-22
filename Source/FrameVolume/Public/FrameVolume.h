// Copyright 2026 Silvan Teufel. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

/**
 * Runtime module for FrameVolume. Loads at PreDefault so the world subsystem, the volume class and the
 * Frame.* console commands all exist before the first game world is created - a volume in a map that is
 * loaded on startup must be able to take the camera on the very first frame.
 */
class FFrameVolumeModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
