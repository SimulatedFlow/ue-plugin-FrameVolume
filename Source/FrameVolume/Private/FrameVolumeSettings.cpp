// Copyright 2026 Simulated Flow. All Rights Reserved.

#include "FrameVolumeSettings.h"

UFrameVolumeSettings::UFrameVolumeSettings()
{
	CategoryName = TEXT("Plugins");
	SectionName = TEXT("FrameVolume");

	// A wide, slightly high shoulder shot. Deliberately unremarkable: this is what a volume falls back to
	// when its row is missing, and a fallback that draws attention to itself is a fallback that gets
	// mistaken for the real shot.
	FallbackShot.Mode = EFrameCameraMode::Orbit;
	FallbackShot.Distance = 700.0f;
	FallbackShot.Pitch = -20.0f;
	FallbackShot.Yaw = 0.0f;
	FallbackShot.FieldOfView = 80.0f;
	FallbackShot.BlendTime = 0.6f;
	FallbackShot.BlendCurve = EFrameBlendCurve::EaseInOut;
	FallbackShot.Priority = 0;
}

FName UFrameVolumeSettings::GetCategoryName() const
{
	return TEXT("Plugins");
}

FName UFrameVolumeSettings::GetSectionName() const
{
	return TEXT("FrameVolume");
}

const UFrameVolumeSettings& UFrameVolumeSettings::Get()
{
	const UFrameVolumeSettings* Settings = GetDefault<UFrameVolumeSettings>();
	check(Settings);
	return *Settings;
}
