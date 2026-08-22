// Copyright 2026 Silvan Teufel. All Rights Reserved.

#include "FrameVolumeStatics.h"

#include "FrameVolumeActor.h"
#include "FrameVolumeSubsystem.h"

namespace FrameVolumeStaticsPrivate
{
	static UFrameVolumeSubsystem* Resolve(const UObject* WorldContextObject)
	{
		return UFrameVolumeSubsystem::Get(WorldContextObject);
	}
}

//~ Queries ------------------------------------------------------------------------------------------------

AFrameVolume* UFrameVolumeStatics::GetActiveVolume(const UObject* WorldContextObject)
{
	const UFrameVolumeSubsystem* Subsystem = FrameVolumeStaticsPrivate::Resolve(WorldContextObject);
	return Subsystem ? Subsystem->GetActiveVolume() : nullptr;
}

float UFrameVolumeStatics::GetBlendAlpha(const UObject* WorldContextObject)
{
	const UFrameVolumeSubsystem* Subsystem = FrameVolumeStaticsPrivate::Resolve(WorldContextObject);
	// No subsystem means nothing is blending, and "arrived" is the honest answer to that.
	return Subsystem ? Subsystem->GetBlendAlpha() : 1.0f;
}

bool UFrameVolumeStatics::IsControllingCamera(const UObject* WorldContextObject)
{
	const UFrameVolumeSubsystem* Subsystem = FrameVolumeStaticsPrivate::Resolve(WorldContextObject);
	return Subsystem && Subsystem->GetStats().bControllingCamera;
}

void UFrameVolumeStatics::GetInsideVolumes(const UObject* WorldContextObject, TArray<AFrameVolume*>& OutVolumes)
{
	OutVolumes.Reset();
	if (const UFrameVolumeSubsystem* Subsystem = FrameVolumeStaticsPrivate::Resolve(WorldContextObject))
	{
		Subsystem->GetInsideVolumes(OutVolumes);
	}
}

void UFrameVolumeStatics::GetAllVolumes(const UObject* WorldContextObject, TArray<AFrameVolume*>& OutVolumes)
{
	OutVolumes.Reset();
	if (const UFrameVolumeSubsystem* Subsystem = FrameVolumeStaticsPrivate::Resolve(WorldContextObject))
	{
		Subsystem->GetAllVolumes(OutVolumes);
	}
}

FFrameCameraState UFrameVolumeStatics::GetCurrentCameraState(const UObject* WorldContextObject)
{
	const UFrameVolumeSubsystem* Subsystem = FrameVolumeStaticsPrivate::Resolve(WorldContextObject);
	return Subsystem ? Subsystem->GetCurrentState() : FFrameCameraState();
}

FFrameVolumeStats UFrameVolumeStatics::GetStats(const UObject* WorldContextObject)
{
	const UFrameVolumeSubsystem* Subsystem = FrameVolumeStaticsPrivate::Resolve(WorldContextObject);
	return Subsystem ? Subsystem->GetStats() : FFrameVolumeStats();
}

bool UFrameVolumeStatics::GetVolumeShot(const AFrameVolume* Volume, FFrameVolumeRow& OutRow)
{
	if (!Volume)
	{
		OutRow = FFrameVolumeRow();
		return false;
	}
	return Volume->GetShot(OutRow);
}

//~ Framing ------------------------------------------------------------------------------------------------

void UFrameVolumeStatics::SetFollowTarget(const UObject* WorldContextObject, AActor* NewTarget)
{
	if (UFrameVolumeSubsystem* Subsystem = FrameVolumeStaticsPrivate::Resolve(WorldContextObject))
	{
		Subsystem->SetFollowTarget(NewTarget);
	}
}

AActor* UFrameVolumeStatics::GetFollowTarget(const UObject* WorldContextObject)
{
	const UFrameVolumeSubsystem* Subsystem = FrameVolumeStaticsPrivate::Resolve(WorldContextObject);
	return Subsystem ? Subsystem->GetFollowTarget() : nullptr;
}

void UFrameVolumeStatics::PushOverride(const UObject* WorldContextObject, const FFrameVolumeRow& Shot)
{
	if (UFrameVolumeSubsystem* Subsystem = FrameVolumeStaticsPrivate::Resolve(WorldContextObject))
	{
		Subsystem->PushOverride(Shot);
	}
}

void UFrameVolumeStatics::ClearOverride(const UObject* WorldContextObject)
{
	if (UFrameVolumeSubsystem* Subsystem = FrameVolumeStaticsPrivate::Resolve(WorldContextObject))
	{
		Subsystem->ClearOverride();
	}
}

bool UFrameVolumeStatics::IsOverrideActive(const UObject* WorldContextObject)
{
	const UFrameVolumeSubsystem* Subsystem = FrameVolumeStaticsPrivate::Resolve(WorldContextObject);
	return Subsystem && Subsystem->IsOverrideActive();
}

FFrameVolumeRow UFrameVolumeStatics::MakeOrbitShot(float Distance, float Pitch, float Yaw, float FieldOfView,
	float BlendTime, EFrameBlendCurve BlendCurve, int32 Priority)
{
	FFrameVolumeRow Row;
	Row.Mode = EFrameCameraMode::Orbit;
	Row.Distance = Distance;
	Row.Pitch = Pitch;
	Row.Yaw = Yaw;
	Row.FieldOfView = FieldOfView;
	Row.BlendTime = BlendTime;
	Row.BlendCurve = BlendCurve;
	Row.Priority = Priority;
	return Row;
}

//~ Switches -----------------------------------------------------------------------------------------------

void UFrameVolumeStatics::SetFrameVolumeEnabled(const UObject* WorldContextObject, bool bEnabled)
{
	if (UFrameVolumeSubsystem* Subsystem = FrameVolumeStaticsPrivate::Resolve(WorldContextObject))
	{
		Subsystem->SetEnabled(bEnabled);
	}
}

bool UFrameVolumeStatics::IsFrameVolumeEnabled(const UObject* WorldContextObject)
{
	const UFrameVolumeSubsystem* Subsystem = FrameVolumeStaticsPrivate::Resolve(WorldContextObject);
	return Subsystem && Subsystem->IsEnabled();
}

void UFrameVolumeStatics::SetBlendTimeOverride(const UObject* WorldContextObject, float Seconds)
{
	if (UFrameVolumeSubsystem* Subsystem = FrameVolumeStaticsPrivate::Resolve(WorldContextObject))
	{
		Subsystem->SetBlendTimeOverride(Seconds);
	}
}

float UFrameVolumeStatics::GetBlendTimeOverride(const UObject* WorldContextObject)
{
	const UFrameVolumeSubsystem* Subsystem = FrameVolumeStaticsPrivate::Resolve(WorldContextObject);
	return Subsystem ? Subsystem->GetBlendTimeOverride() : -1.0f;
}

void UFrameVolumeStatics::SetFieldOfViewOverride(const UObject* WorldContextObject, float Degrees)
{
	if (UFrameVolumeSubsystem* Subsystem = FrameVolumeStaticsPrivate::Resolve(WorldContextObject))
	{
		Subsystem->SetFieldOfViewOverride(Degrees);
	}
}

float UFrameVolumeStatics::GetFieldOfViewOverride(const UObject* WorldContextObject)
{
	const UFrameVolumeSubsystem* Subsystem = FrameVolumeStaticsPrivate::Resolve(WorldContextObject);
	return Subsystem ? Subsystem->GetFieldOfViewOverride() : -1.0f;
}

void UFrameVolumeStatics::SetCollisionAvoidanceEnabled(const UObject* WorldContextObject, bool bEnabled)
{
	if (UFrameVolumeSubsystem* Subsystem = FrameVolumeStaticsPrivate::Resolve(WorldContextObject))
	{
		Subsystem->SetCollisionAvoidanceEnabled(bEnabled);
	}
}

bool UFrameVolumeStatics::IsCollisionAvoidanceEnabled(const UObject* WorldContextObject)
{
	const UFrameVolumeSubsystem* Subsystem = FrameVolumeStaticsPrivate::Resolve(WorldContextObject);
	return Subsystem && Subsystem->IsCollisionAvoidanceEnabled();
}

void UFrameVolumeStatics::SetDebugDraw(const UObject* WorldContextObject, bool bEnabled)
{
	if (UFrameVolumeSubsystem* Subsystem = FrameVolumeStaticsPrivate::Resolve(WorldContextObject))
	{
		Subsystem->SetDebugDraw(bEnabled);
	}
}

bool UFrameVolumeStatics::IsDebugDrawEnabled(const UObject* WorldContextObject)
{
	const UFrameVolumeSubsystem* Subsystem = FrameVolumeStaticsPrivate::Resolve(WorldContextObject);
	return Subsystem && Subsystem->IsDebugDrawEnabled();
}
