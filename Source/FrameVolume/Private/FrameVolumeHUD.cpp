// Copyright 2026 Simulated Flow. All Rights Reserved.

#include "FrameVolumeHUD.h"

#include "Engine/Canvas.h"
#include "FrameVolumeSettings.h"
#include "FrameVolumeSubsystem.h"

AFrameVolumeHUD::AFrameVolumeHUD()
{
	PrimaryActorTick.bCanEverTick = false;
}

void AFrameVolumeHUD::BeginPlay()
{
	Super::BeginPlay();

	bShowStats = UFrameVolumeSettings::Get().bShowStatsByDefault;
}

void AFrameVolumeHUD::ToggleStats()
{
	bShowStats = !bShowStats;
}

void AFrameVolumeHUD::DrawHUD()
{
	Super::DrawHUD();

	if (!Canvas || !bShowStats)
	{
		return;
	}

	// Everything drawn is read from the subsystem on the frame it is drawn. Nothing is cached here, so the
	// box cannot claim one thing while the camera does another.
	if (const UFrameVolumeSubsystem* Subsystem = UFrameVolumeSubsystem::Get(this))
	{
		Subsystem->DrawStatsBox(Canvas, StatsBoxOrigin, StatsBoxWidth);
	}
}
