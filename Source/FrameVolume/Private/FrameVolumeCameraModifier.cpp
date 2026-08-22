// Copyright 2026 Simulated Flow. All Rights Reserved.

#include "FrameVolumeCameraModifier.h"

#include "Engine/World.h"
#include "FrameVolumeSubsystem.h"
#include "FrameVolumeTypes.h"

UFrameVolumeCameraModifier::UFrameVolumeCameraModifier()
{
	// 0 is the highest priority band. A camera volume decides where the camera is; a shake decides how it
	// wobbles about that point. Running first is what lets a shake still read on top of a framed shot.
	Priority = 0;

	// The plugin owns its own blend, with its own curve, measured on its own counters box. Letting the
	// modifier fade itself in as well would put a second, invisible blend in series with the visible one
	// and make the measured blend time a lie.
	AlphaInTime = 0.0f;
	AlphaOutTime = 0.0f;
	Alpha = 1.0f;

	bExclusive = false;
}

void UFrameVolumeCameraModifier::ModifyCamera(float DeltaTime, FVector ViewLocation, FRotator ViewRotation, float FOV,
	FVector& NewViewLocation, FRotator& NewViewRotation, float& NewFOV)
{
	// The engine passes the same FMinimalViewInfo fields in as both the inputs and the outputs, so the
	// inputs have to be copied out before anything is written back.
	FFrameCameraState GameState;
	GameState.Location = ViewLocation;
	GameState.Rotation = ViewRotation;
	GameState.FieldOfView = FOV;

	UFrameVolumeSubsystem* Subsystem = UFrameVolumeSubsystem::Get(this);
	if (!Subsystem)
	{
		return;
	}

	FFrameCameraState Result;
	if (!Subsystem->ApplyToCamera(GameState, Result))
	{
		// Nothing framed and nothing blending out: leave the point of view exactly as it came in. Not
		// "write the same values back" - untouched, so a project can prove the plugin is idle.
		return;
	}

	NewViewLocation = Result.Location;
	NewViewRotation = Result.Rotation;
	NewFOV = Result.FieldOfView;
}
