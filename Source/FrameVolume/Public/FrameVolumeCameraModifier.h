// Copyright 2026 Simulated Flow. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Camera/CameraModifier.h"
#include "FrameVolumeCameraModifier.generated.h"

/**
 * The one place FrameVolume touches the engine's camera.
 *
 * A camera modifier rather than a view target, and that choice is the reason the plugin drops into an
 * existing project without an argument. Setting a view target replaces whatever the game was doing with
 * its camera; a modifier is handed the point of view the game just worked out and hands one back. A
 * third-person spring arm, a first-person camera, a cinematic that is already running - all of them keep
 * running underneath, and when the last volume is left the plugin blends back into whatever they made
 * and then stops writing entirely.
 *
 * There is nothing to place and nothing to configure. The world subsystem adds one of these to the local
 * player's camera manager the first time it needs it, and removes it when the world goes away.
 *
 * The modifier holds no state of its own. It reads the blend the subsystem computed on its tick, and
 * feeds the game's own point of view back the other way so that the blend *out* of the plugin has
 * something current to aim at.
 */
UCLASS(NotBlueprintable, meta = (DisplayName = "Frame Volume Camera Modifier"))
class FRAMEVOLUME_API UFrameVolumeCameraModifier : public UCameraModifier
{
	GENERATED_BODY()

public:
	UFrameVolumeCameraModifier();

protected:
	virtual void ModifyCamera(float DeltaTime, FVector ViewLocation, FRotator ViewRotation, float FOV,
		FVector& NewViewLocation, FRotator& NewViewRotation, float& NewFOV) override;
};
