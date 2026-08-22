// Copyright 2026 Simulated Flow. All Rights Reserved.

#include "FrameVolumeSubsystem.h"

#include "CanvasItem.h"
#include "CanvasTypes.h"
#include "Camera/PlayerCameraManager.h"
#include "CollisionQueryParams.h"
#include "Components/SceneComponent.h"
#include "Components/SplineComponent.h"
#include "Curves/CurveFloat.h"
#include "DrawDebugHelpers.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "Engine/World.h"
#include "FrameVolumeActor.h"
#include "FrameVolumeCameraModifier.h"
#include "FrameVolumeLog.h"
#include "FrameVolumeSettings.h"
#include "GameFramework/HUD.h"
#include "GameFramework/PlayerController.h"
#include "GlobalRenderResources.h"
#include "HAL/IConsoleManager.h"
#include "Misc/StringBuilder.h"
#include "SceneTypes.h"

namespace FrameVolumePrivate
{
	/** How many overlapping volumes the counters box lists before it starts counting instead. */
	static constexpr int32 MaxListedInsideVolumes = 6;

	/** Lines the counters box always draws, before the per-volume ones. */
	static constexpr int32 FixedStatsLines = 12;

	static constexpr float LineHeight = 15.0f;
	static constexpr float BoxPadding = 8.0f;

	static const FLinearColor PanelBackground(0.0f, 0.0f, 0.0f, 0.62f);
	static const FLinearColor HeadingColor(0.42f, 0.78f, 1.0f, 1.0f);
	static const FLinearColor BodyColor(0.9f, 0.9f, 0.9f, 1.0f);
	static const FLinearColor GoodColor(0.55f, 0.95f, 0.55f, 1.0f);
	static const FLinearColor WarnColor(0.98f, 0.78f, 0.35f, 1.0f);
	static const FLinearColor DimColor(0.62f, 0.62f, 0.62f, 1.0f);

	static void DrawFilledRect(UCanvas* Canvas, const FVector2D& Position, const FVector2D& Size, const FLinearColor& Color)
	{
		FCanvasTileItem Tile(Position, GWhiteTexture, Size, Color);
		Tile.BlendMode = SE_BLEND_Translucent;
		Canvas->DrawItem(Tile);
	}

	static UFrameVolumeSubsystem* GetSubsystem(UWorld* World)
	{
		return World ? World->GetSubsystem<UFrameVolumeSubsystem>() : nullptr;
	}

	static const TCHAR* ModeName(EFrameCameraMode Mode)
	{
		switch (Mode)
		{
		case EFrameCameraMode::Anchor: return TEXT("Anchor");
		case EFrameCameraMode::Rail:   return TEXT("Rail");
		case EFrameCameraMode::Orbit:  return TEXT("Orbit");
		default:                       return TEXT("Orbit");
		}
	}

	static const TCHAR* CurveName(EFrameBlendCurve Curve)
	{
		switch (Curve)
		{
		case EFrameBlendCurve::Linear:    return TEXT("Linear");
		case EFrameBlendCurve::EaseIn:    return TEXT("EaseIn");
		case EFrameBlendCurve::EaseOut:   return TEXT("EaseOut");
		case EFrameBlendCurve::EaseInOut: return TEXT("EaseInOut");
		case EFrameBlendCurve::Custom:    return TEXT("Custom");
		default:                          return TEXT("EaseInOut");
		}
	}

	/** A twelve-cell text bar, so the blend can be read from a video frame without reading the number. */
	static void AppendBar(TStringBuilder<192>& Builder, float Alpha)
	{
		const int32 Cells = 12;
		const int32 Filled = FMath::Clamp(FMath::RoundToInt(Alpha * Cells), 0, Cells);
		Builder.AppendChar(TEXT('['));
		for (int32 Index = 0; Index < Cells; ++Index)
		{
			Builder.AppendChar(Index < Filled ? TEXT('=') : TEXT('.'));
		}
		Builder.AppendChar(TEXT(']'));
	}
}

//~ Lifetime -----------------------------------------------------------------------------------------------

void UFrameVolumeSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	ApplySettings();

	CurrentState = FFrameCameraState();
	TargetState = FFrameCameraState();
	LastGameState = FFrameCameraState();

	UE_LOG(LogFrameVolume, Log,
		TEXT("FrameVolume up: %s, follow %s, collision channel %d, release blend %.2fs."),
		bEnabled ? TEXT("enabled") : TEXT("disabled"),
		bAutoFollowLocalPlayer ? TEXT("local player") : TEXT("explicit"),
		static_cast<int32>(CollisionChannel.GetValue()),
		ReleaseBlendTime);
}

void UFrameVolumeSubsystem::Deinitialize()
{
	if (HudPostRenderHandle.IsValid())
	{
		AHUD::OnHUDPostRender.Remove(HudPostRenderHandle);
		HudPostRenderHandle.Reset();
	}

	RemoveCameraModifier();

	Volumes.Reset();
	InsideVolumes.Reset();
	PreviousInside.Reset();
	ActiveVolume.Reset();
	FollowTarget.Reset();

	Super::Deinitialize();
}

bool UFrameVolumeSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	// Game and PIE, and deliberately not Editor. The editor viewport camera belongs to whoever is building
	// the level; a plugin that grabs it while a room is being blocked out is a plugin that gets removed.
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

TStatId UFrameVolumeSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UFrameVolumeSubsystem, STATGROUP_Tickables);
}

UFrameVolumeSubsystem* UFrameVolumeSubsystem::Get(const UObject* WorldContextObject)
{
	if (!WorldContextObject)
	{
		return nullptr;
	}

	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr;
	return World ? World->GetSubsystem<UFrameVolumeSubsystem>() : nullptr;
}

void UFrameVolumeSubsystem::ApplySettings()
{
	const UFrameVolumeSettings& Settings = UFrameVolumeSettings::Get();

	bEnabled = Settings.bEnabled;
	bAutoFollowLocalPlayer = Settings.bAutoFollowLocalPlayer;
	bReacquireFollowTarget = Settings.bReacquireFollowTarget;
	bAutoDrawStatsOnAnyHUD = Settings.bAutoDrawStatsOnAnyHUD;
	bDebugDraw = Settings.bDebugDrawByDefault;
	bTraceComplex = Settings.bTraceComplex;

	ReleaseBlendTime = FMath::Max(0.0f, Settings.ReleaseBlendTime);
	ReleaseBlendCurve = Settings.ReleaseBlendCurve;
	CollisionChannel = Settings.CollisionChannel;
	MaxVolumesPerUpdate = FMath::Max(1, Settings.MaxVolumesPerUpdate);
	UpdatesPerSecond = FMath::Max(0.0f, Settings.UpdatesPerSecond);

	RebindHudDelegate();
}

//~ Registry -----------------------------------------------------------------------------------------------

void UFrameVolumeSubsystem::RegisterVolume(AFrameVolume* Volume)
{
	if (!Volume)
	{
		return;
	}

	Volumes.AddUnique(Volume);
	Stats.RegisteredVolumes = Volumes.Num();
}

void UFrameVolumeSubsystem::UnregisterVolume(AFrameVolume* Volume)
{
	if (!Volume)
	{
		return;
	}

	Volumes.Remove(Volume);
	InsideVolumes.Remove(Volume);

	if (ActiveVolume.Get() == Volume)
	{
		// Do not cut. The volume is gone, so the next tick resolves whatever is left and blends to it -
		// which is the same behaviour as walking out of it, and the same behaviour a streamed-out level
		// should have.
		ActiveVolume.Reset();
	}

	Stats.RegisteredVolumes = Volumes.Num();
}

void UFrameVolumeSubsystem::GetAllVolumes(TArray<AFrameVolume*>& OutVolumes) const
{
	OutVolumes.Reset(Volumes.Num());
	for (const TWeakObjectPtr<AFrameVolume>& Weak : Volumes)
	{
		if (AFrameVolume* Volume = Weak.Get())
		{
			OutVolumes.Add(Volume);
		}
	}
}

void UFrameVolumeSubsystem::GetInsideVolumes(TArray<AFrameVolume*>& OutVolumes) const
{
	OutVolumes.Reset(InsideVolumes.Num());
	for (const TWeakObjectPtr<AFrameVolume>& Weak : InsideVolumes)
	{
		if (AFrameVolume* Volume = Weak.Get())
		{
			OutVolumes.Add(Volume);
		}
	}
}

bool UFrameVolumeSubsystem::IsInsideVolume(const AFrameVolume* Volume) const
{
	if (!Volume)
	{
		return false;
	}

	for (const TWeakObjectPtr<AFrameVolume>& Weak : InsideVolumes)
	{
		if (Weak.Get() == Volume)
		{
			return true;
		}
	}
	return false;
}

AFrameVolume* UFrameVolumeSubsystem::GetActiveVolume() const
{
	return ActiveVolume.Get();
}

//~ Follow target ------------------------------------------------------------------------------------------

void UFrameVolumeSubsystem::SetFollowTarget(AActor* NewTarget)
{
	FollowTarget = NewTarget;
	bFollowTargetIsExplicit = (NewTarget != nullptr);
}

AActor* UFrameVolumeSubsystem::GetFollowTarget() const
{
	return FollowTarget.Get();
}

void UFrameVolumeSubsystem::UpdateFollowTarget()
{
	if (FollowTarget.IsValid())
	{
		return;
	}

	if (bFollowTargetIsExplicit)
	{
		// The actor a project asked for has been destroyed. Fall back to automatic rather than freezing on
		// a stale shot: a death cam that never comes back is worse than one that snaps to the respawn.
		bFollowTargetIsExplicit = false;
	}

	if (!bAutoFollowLocalPlayer)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	if (const APlayerController* PC = World->GetFirstPlayerController())
	{
		if (APawn* Pawn = PC->GetPawn())
		{
			FollowTarget = Pawn;
			return;
		}

		// No pawn yet - a spectator, or the frame before possession. The controller itself has a location
		// and is enough to keep the volumes resolving until a pawn shows up.
		if (bReacquireFollowTarget)
		{
			FollowTarget.Reset();
		}
	}
}

//~ Occupancy ----------------------------------------------------------------------------------------------

void UFrameVolumeSubsystem::UpdateOccupancy()
{
	PreviousInside = InsideVolumes;
	InsideVolumes.Reset();

	const AActor* Target = FollowTarget.Get();
	if (Target)
	{
		const FVector Point = Target->GetActorLocation();

		int32 Tested = 0;
		for (int32 Index = Volumes.Num() - 1; Index >= 0; --Index)
		{
			AFrameVolume* Volume = Volumes[Index].Get();
			if (!Volume)
			{
				Volumes.RemoveAtSwap(Index, EAllowShrinking::No);
				continue;
			}

			if (!Volume->bVolumeEnabled)
			{
				continue;
			}

			if (++Tested > MaxVolumesPerUpdate)
			{
				// A ceiling, not a truncation to hide: it is on the counters box as a volume count that no
				// longer matches what is being tested, and in the log once.
				UE_LOG(LogFrameVolume, Warning,
					TEXT("More than %d frame volumes in this world; the rest are not being tested. Raise MaxVolumesPerUpdate."),
					MaxVolumesPerUpdate);
				break;
			}

			// Cheap first: a world-space box compare rejects almost everything for the price of six floats.
			// Only what survives that pays for the exact brush query.
			if (!Volume->GetQueryBounds().IsInsideOrOn(Point))
			{
				continue;
			}

			if (Volume->ContainsPoint(Point))
			{
				InsideVolumes.Add(Volume);
			}
		}

		Stats.RegisteredVolumes = Volumes.Num();

		InsideVolumes.Sort([](const TWeakObjectPtr<AFrameVolume>& A, const TWeakObjectPtr<AFrameVolume>& B)
		{
			const AFrameVolume* VA = A.Get();
			const AFrameVolume* VB = B.Get();
			if (!VA)
			{
				return false;
			}
			if (!VB)
			{
				return true;
			}

			const int32 PriorityA = VA->GetEffectivePriority();
			const int32 PriorityB = VB->GetEffectivePriority();
			if (PriorityA != PriorityB)
			{
				return PriorityA > PriorityB;
			}

			// Equal priority: the smaller volume wins. The small volume around the doorway is the specific
			// case inside the big volume around the hall, and a designer who nests two volumes without
			// setting priorities means the inner one.
			return VA->GetQueryBounds().GetVolume() < VB->GetQueryBounds().GetVolume();
		});
	}

	// Enter and exit, from the difference between the two sets. Both lists are tiny - an actor standing in
	// five volumes at once is already an unusual level - so a pair of linear scans beats a hash set.
	for (const TWeakObjectPtr<AFrameVolume>& Weak : InsideVolumes)
	{
		AFrameVolume* Volume = Weak.Get();
		if (Volume && !PreviousInside.Contains(Weak))
		{
			OnVolumeEntered.Broadcast(Volume);
		}
	}

	for (const TWeakObjectPtr<AFrameVolume>& Weak : PreviousInside)
	{
		AFrameVolume* Volume = Weak.Get();
		if (Volume && !InsideVolumes.Contains(Weak))
		{
			OnVolumeExited.Broadcast(Volume);
		}
	}
}

//~ Shots --------------------------------------------------------------------------------------------------

bool UFrameVolumeSubsystem::GetPivot(const FFrameVolumeRow& Row, FVector& OutPivot) const
{
	const AActor* Target = FollowTarget.Get();
	if (!Target)
	{
		return false;
	}

	// A world-space offset, not an actor-space one. Actor space would swing the whole shot round every
	// time the character turned on the spot, which is the opposite of what a framing offset is for.
	OutPivot = Target->GetActorLocation() + Row.PivotOffset;
	return true;
}

FFrameCameraState UFrameVolumeSubsystem::EvaluateShot(const FFrameVolumeRow& Row, const AFrameVolume* Volume, FVector& OutPivot) const
{
	FFrameCameraState State;
	State.FieldOfView = FMath::Clamp(FieldOfViewOverride > 0.0f ? FieldOfViewOverride : Row.FieldOfView, 5.0f, 170.0f);

	FVector Pivot = FVector::ZeroVector;
	if (!GetPivot(Row, Pivot))
	{
		Pivot = Volume ? Volume->GetActorLocation() : FVector::ZeroVector;
	}
	OutPivot = Pivot;

	const bool bRailUsable = Row.Mode == EFrameCameraMode::Rail && Volume && Volume->HasUsableRail();
	const bool bAnchorUsable = Row.Mode == EFrameCameraMode::Anchor && Volume && Volume->Anchor != nullptr;

	if (bRailUsable)
	{
		USplineComponent* Rail = Volume->Rail;

		// A projection, not a follow: the closest point on the line the designer drew. The camera slides
		// along the rail as the actor walks past and can never leave it, which is the whole reason to use
		// a rail instead of an orbit - the shot stays on the track however the actor moves.
		State.Location = Rail->FindLocationClosestToWorldLocation(Pivot, ESplineCoordinateSpace::World);
		State.Rotation = Row.bLookAtPivot
			? (Pivot - State.Location).Rotation()
			: Rail->FindRotationClosestToWorldLocation(Pivot, ESplineCoordinateSpace::World);
	}
	else if (bAnchorUsable)
	{
		State.Location = Volume->Anchor->GetComponentLocation();
		State.Rotation = Row.bLookAtPivot
			? (Pivot - State.Location).Rotation()
			: Volume->Anchor->GetComponentRotation();
	}
	else
	{
		// Orbit, and also where a Rail row with no rail and an Anchor row with no anchor end up. Falling
		// back to a shot that works beats framing the inside of the floor because a spline was never given
		// any points.
		float YawDegrees = Row.Yaw;
		if (Row.bYawRelativeToVolume && Volume)
		{
			YawDegrees += Volume->GetActorRotation().Yaw;
		}

		const FRotator ViewRotation(FMath::Clamp(Row.Pitch, -89.0f, 89.0f), YawDegrees, 0.0f);
		State.Location = Pivot - ViewRotation.Vector() * FMath::Max(0.0f, Row.Distance);
		State.Rotation = ViewRotation;
	}

	State.Rotation += Row.RotationOffset;
	State.Rotation.Normalize();
	return State;
}

float UFrameVolumeSubsystem::ApplyCollisionAvoidance(const FFrameVolumeRow& Row, const FVector& Pivot, FFrameCameraState& InOutState) const
{
	if (!bCollisionAvoidanceEnabled || !Row.bAvoidCollision)
	{
		return 0.0f;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return 0.0f;
	}

	const FVector Delta = InOutState.Location - Pivot;
	const float Desired = Delta.Size();
	if (Desired <= KINDA_SMALL_NUMBER)
	{
		return 0.0f;
	}

	FCollisionQueryParams Params(SCENE_QUERY_STAT(FrameVolumeCameraSweep), bTraceComplex);
	Params.AddIgnoredActor(FollowTarget.Get());
	// A sphere that starts inside something is not a reason to slam the camera onto the pivot. Discarding
	// initial overlaps means a character standing in a bush or half inside a doorframe keeps its shot.
	Params.bFindInitialOverlaps = false;

	FHitResult Hit;
	const bool bHit = World->SweepSingleByChannel(
		Hit,
		Pivot,
		InOutState.Location,
		FQuat::Identity,
		CollisionChannel,
		FCollisionShape::MakeSphere(FMath::Max(0.0f, Row.CollisionProbeRadius)),
		Params);

	if (!bHit)
	{
		return 0.0f;
	}

	const FVector Direction = Delta / Desired;
	const float HitDistance = FVector::Dist(Pivot, Hit.Location);

	float NewDistance = HitDistance - FMath::Max(0.0f, Row.CollisionPullIn);
	NewDistance = FMath::Clamp(NewDistance, FMath::Min(Row.MinCollisionDistance, Desired), Desired);

	InOutState.Location = Pivot + Direction * NewDistance;
	return Desired - NewDistance;
}

//~ Blend --------------------------------------------------------------------------------------------------

float UFrameVolumeSubsystem::CurveAlpha(float RawAlpha, EFrameBlendCurve Curve, const TSoftObjectPtr<UCurveFloat>& CustomCurve) const
{
	const float Alpha = FMath::Clamp(RawAlpha, 0.0f, 1.0f);

	switch (Curve)
	{
	case EFrameBlendCurve::Linear:
		return Alpha;

	case EFrameBlendCurve::EaseIn:
		return Alpha * Alpha;

	case EFrameBlendCurve::EaseOut:
		return 1.0f - FMath::Square(1.0f - Alpha);

	case EFrameBlendCurve::EaseInOut:
		return FMath::SmoothStep(0.0f, 1.0f, Alpha);

	case EFrameBlendCurve::Custom:
		if (const UCurveFloat* Resolved = CustomCurve.Get())
		{
			return Resolved->GetFloatValue(Alpha);
		}
		// The curve is soft and may simply not be loaded yet. Ease-in-out for a frame or two is invisible;
		// a hard zero would be a cut.
		return FMath::SmoothStep(0.0f, 1.0f, Alpha);

	default:
		return Alpha;
	}
}

void UFrameVolumeSubsystem::RetargetBlend(const FFrameCameraState& FromState, float Duration,
	EFrameBlendCurve Curve, const TSoftObjectPtr<UCurveFloat>& CustomCurve)
{
	// This is the whole no-stack rule in four lines: the blend restarts from where the camera visibly is,
	// not from where the previous blend was going to end up. Nothing is queued and nothing accumulates, so
	// walking in and out of a volume ten times in two seconds costs the same as doing it once.
	CurrentState = FromState;
	BlendAlpha = 0.0f;
	BlendElapsed = 0.0f;
	BlendDuration = FMath::Max(0.0f, Duration);
	ActiveBlendCurve = Curve;
	ActiveCustomCurve = CustomCurve;

	if (!ActiveCustomCurve.IsNull() && Curve == EFrameBlendCurve::Custom)
	{
		// One synchronous load, at the moment the blend starts, rather than a resolve attempt every frame.
		ActiveCustomCurve.LoadSynchronous();
	}
}

FFrameCameraState UFrameVolumeSubsystem::GetCurrentState() const
{
	if (!bControlling)
	{
		return bHasGameState ? LastGameState : TargetState;
	}

	const FFrameCameraState& Destination = bReleasing ? LastGameState : TargetState;
	return FFrameCameraState::Blend(CurrentState, Destination, CurveAlpha(BlendAlpha, ActiveBlendCurve, ActiveCustomCurve));
}

void UFrameVolumeSubsystem::SetBlendTimeOverride(float Seconds)
{
	BlendTimeOverride = Seconds;
}

void UFrameVolumeSubsystem::SetFieldOfViewOverride(float Degrees)
{
	if (FMath::IsNearlyEqual(FieldOfViewOverride, Degrees))
	{
		return;
	}

	FieldOfViewOverride = Degrees;

	// Restart the blend from where the camera is so the change is a move rather than a pop. Without this
	// the field of view would jump: the alpha is already at 1, so the new target would be reached instantly.
	if (bControlling && !bReleasing)
	{
		const float Duration = BlendTimeOverride >= 0.0f ? BlendTimeOverride : ActiveRow.BlendTime;
		RetargetBlend(GetCurrentState(), Duration, ActiveRow.BlendCurve, ActiveRow.CustomCurve);
	}
}

//~ Overrides ----------------------------------------------------------------------------------------------

void UFrameVolumeSubsystem::PushOverride(const FFrameVolumeRow& Row)
{
	OverrideRow = Row;
	bOverrideActive = true;
}

void UFrameVolumeSubsystem::ClearOverride()
{
	bOverrideActive = false;
}

//~ Switches -----------------------------------------------------------------------------------------------

void UFrameVolumeSubsystem::SetEnabled(bool bInEnabled)
{
	bEnabled = bInEnabled;
}

void UFrameVolumeSubsystem::SetCollisionAvoidanceEnabled(bool bInEnabled)
{
	bCollisionAvoidanceEnabled = bInEnabled;
}

void UFrameVolumeSubsystem::SetDebugDraw(bool bInEnabled)
{
	bDebugDraw = bInEnabled;
}

//~ Camera plumbing ----------------------------------------------------------------------------------------

void UFrameVolumeSubsystem::EnsureCameraModifier()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	APlayerController* PC = World->GetFirstPlayerController();
	APlayerCameraManager* Manager = PC ? PC->PlayerCameraManager : nullptr;
	if (!Manager)
	{
		return;
	}

	if (Modifier.IsValid() && ModifierOwner.Get() == Manager)
	{
		return;
	}

	RemoveCameraModifier();

	if (UCameraModifier* Created = Manager->AddNewCameraModifier(UFrameVolumeCameraModifier::StaticClass()))
	{
		Modifier = Cast<UFrameVolumeCameraModifier>(Created);
		ModifierOwner = Manager;
		UE_LOG(LogFrameVolume, Verbose, TEXT("FrameVolume attached its camera modifier to %s."), *Manager->GetName());
	}
}

void UFrameVolumeSubsystem::RemoveCameraModifier()
{
	if (APlayerCameraManager* Manager = ModifierOwner.Get())
	{
		if (UFrameVolumeCameraModifier* Existing = Modifier.Get())
		{
			Manager->RemoveCameraModifier(Existing);
		}
	}

	Modifier.Reset();
	ModifierOwner.Reset();
}

bool UFrameVolumeSubsystem::ApplyToCamera(const FFrameCameraState& GameState, FFrameCameraState& OutState)
{
	// Always remember what the game's own camera did, even while the plugin is idle. It is the destination
	// of the blend out, and it has to be current the instant the last volume is left.
	LastGameState = GameState;
	bHasGameState = true;

	if (!bEnabled || !bControlling)
	{
		return false;
	}

	const FFrameCameraState& Destination = bReleasing ? GameState : TargetState;
	const float Curved = CurveAlpha(BlendAlpha, ActiveBlendCurve, ActiveCustomCurve);

	OutState = FFrameCameraState::Blend(CurrentState, Destination, Curved);

	Stats.CurvedAlpha = Curved;
	return true;
}

//~ Tick ---------------------------------------------------------------------------------------------------

void UFrameVolumeSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	float StepTime = DeltaTime;
	if (UpdatesPerSecond > 0.0f)
	{
		UpdateAccumulator += DeltaTime;
		const float Interval = 1.0f / UpdatesPerSecond;
		if (UpdateAccumulator < Interval)
		{
			return;
		}
		StepTime = UpdateAccumulator;
		UpdateAccumulator = 0.0f;
	}

	const double StartSeconds = FPlatformTime::Seconds();

	EnsureCameraModifier();
	UpdateFollowTarget();
	UpdateOccupancy();
	UpdateActiveShot(StepTime);

	Stats.UpdateMilliseconds = static_cast<float>((FPlatformTime::Seconds() - StartSeconds) * 1000.0);

	DrawDebug();
}

void UFrameVolumeSubsystem::UpdateActiveShot(float DeltaTime)
{
	//~ 1. Which shot wins -----------------------------------------------------------------------------

	AFrameVolume* DesiredVolume = nullptr;
	FFrameVolumeRow DesiredRow;
	bool bDesiredIsOverride = false;
	bool bHaveShot = false;

	if (bEnabled)
	{
		if (bOverrideActive)
		{
			DesiredRow = OverrideRow;
			bDesiredIsOverride = true;
			bHaveShot = true;
		}
		else
		{
			for (const TWeakObjectPtr<AFrameVolume>& Weak : InsideVolumes)
			{
				if (AFrameVolume* Volume = Weak.Get())
				{
					DesiredVolume = Volume;
					Volume->GetShot(DesiredRow);
					if (Volume->bOverridePriority)
					{
						DesiredRow.Priority = Volume->PriorityOverride;
					}
					bHaveShot = true;
					break;
				}
			}
		}
	}

	//~ 2. Did the source change? ----------------------------------------------------------------------

	const bool bSourceChanged =
		bHaveShot != bHasActiveShot
		|| bDesiredIsOverride != bActiveIsOverride
		|| DesiredVolume != ActiveVolume.Get();

	if (bSourceChanged)
	{
		AFrameVolume* const PreviousVolume = ActiveVolume.Get();

		// Where the camera visibly is, right now. Not where the last blend was heading.
		FFrameCameraState From;
		if (bControlling)
		{
			From = GetCurrentState();
		}
		else if (bHasGameState)
		{
			From = LastGameState;
		}
		else
		{
			// First frame of a world, before the camera has rendered once. Starting the blend from the
			// destination makes the first shot appear without a lurch out of the origin.
			From = EvaluateShot(DesiredRow, DesiredVolume, ActivePivot);
		}

		if (bHaveShot)
		{
			const float Duration = BlendTimeOverride >= 0.0f ? BlendTimeOverride : DesiredRow.BlendTime;
			RetargetBlend(From, Duration, DesiredRow.BlendCurve, DesiredRow.CustomCurve);
			bReleasing = false;
			bControlling = true;
		}
		else
		{
			const float Duration = BlendTimeOverride >= 0.0f ? BlendTimeOverride : ReleaseBlendTime;
			RetargetBlend(From, Duration, ReleaseBlendCurve, nullptr);
			bReleasing = true;
			// bControlling stays on until the release blend finishes; that is what makes leaving a volume
			// a move rather than a cut.
		}

		ActiveVolume = DesiredVolume;
		bActiveIsOverride = bDesiredIsOverride;
		bHasActiveShot = bHaveShot;

		OnActiveVolumeChanged.Broadcast(DesiredVolume, PreviousVolume);
	}

	//~ 3. Where the shot points, this frame -----------------------------------------------------------

	float CollisionPull = 0.0f;

	if (bHaveShot)
	{
		ActiveRow = DesiredRow;

		FVector Pivot = FVector::ZeroVector;
		TargetState = EvaluateShot(DesiredRow, DesiredVolume, Pivot);
		ActivePivot = Pivot;

		Stats.TargetDistance = FVector::Dist(Pivot, TargetState.Location);
		CollisionPull = ApplyCollisionAvoidance(DesiredRow, Pivot, TargetState);
		Stats.ActualDistance = FVector::Dist(Pivot, TargetState.Location);
	}
	else
	{
		Stats.TargetDistance = 0.0f;
		Stats.ActualDistance = 0.0f;
	}

	//~ 4. Advance the one blend there is ---------------------------------------------------------------

	if (bControlling)
	{
		BlendElapsed += DeltaTime;
		BlendAlpha = BlendDuration <= KINDA_SMALL_NUMBER
			? 1.0f
			: FMath::Clamp(BlendElapsed / BlendDuration, 0.0f, 1.0f);

		if (bReleasing && BlendAlpha >= 1.0f)
		{
			// Arrived back at the game camera. Stop writing entirely, so a project can prove the plugin is
			// idle rather than quietly copying the point of view onto itself every frame.
			bControlling = false;
			bReleasing = false;
		}
	}
	else
	{
		BlendAlpha = 1.0f;
	}

	//~ 5. Numbers --------------------------------------------------------------------------------------

	const AFrameVolume* StatVolume = ActiveVolume.Get();

	Stats.ActiveVolumeName = bActiveIsOverride
		? FString(TEXT("(Blueprint override)"))
		: (StatVolume ? StatVolume->GetActorNameOrLabel() : FString());
	Stats.ActiveRowName = StatVolume ? StatVolume->GetShotRowName() : NAME_None;
	Stats.ActivePriority = bHaveShot ? ActiveRow.Priority : 0;
	Stats.ActiveMode = bHaveShot ? ActiveRow.Mode : EFrameCameraMode::Orbit;
	Stats.BlendAlpha = BlendAlpha;
	Stats.BlendTime = BlendDuration;
	Stats.BlendCurve = ActiveBlendCurve;
	Stats.TargetFieldOfView = bHaveShot ? TargetState.FieldOfView : 0.0f;
	Stats.bCollisionEngaged = CollisionPull > 0.01f;
	Stats.CollisionPullDistance = CollisionPull;
	Stats.bOverrideActive = bOverrideActive;
	Stats.bControllingCamera = bControlling;
	Stats.RegisteredVolumes = Volumes.Num();

	Stats.bRailActive = false;
	Stats.RailDistance = 0.0f;
	if (bHaveShot && ActiveRow.Mode == EFrameCameraMode::Rail && StatVolume && StatVolume->HasUsableRail())
	{
		const USplineComponent* Rail = StatVolume->Rail;
		const float Key = Rail->FindInputKeyClosestToWorldLocation(ActivePivot);
		Stats.bRailActive = true;
		Stats.RailDistance = Rail->GetDistanceAlongSplineAtSplineInputKey(Key);
	}

	const AActor* Target = FollowTarget.Get();
	Stats.FollowTargetName = Target ? Target->GetActorNameOrLabel() : FString(TEXT("(none)"));

	Stats.InsideVolumeNames.Reset(InsideVolumes.Num());
	for (const TWeakObjectPtr<AFrameVolume>& Weak : InsideVolumes)
	{
		if (const AFrameVolume* Volume = Weak.Get())
		{
			Stats.InsideVolumeNames.Add(FString::Printf(TEXT("%s  p%d"), *Volume->GetActorNameOrLabel(), Volume->GetEffectivePriority()));
		}
	}

	if (!bControlling)
	{
		Stats.CurvedAlpha = 1.0f;
	}
}

//~ Debug drawing ------------------------------------------------------------------------------------------

void UFrameVolumeSubsystem::DrawDebug() const
{
#if ENABLE_DRAW_DEBUG
	if (!bDebugDraw)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const AFrameVolume* Active = ActiveVolume.Get();

	for (const TWeakObjectPtr<AFrameVolume>& Weak : Volumes)
	{
		const AFrameVolume* Volume = Weak.Get();
		if (!Volume || !Volume->bVolumeEnabled)
		{
			continue;
		}

		const bool bIsActive = (Volume == Active);
		const bool bIsInside = IsInsideVolume(Volume);
		const FColor Color = bIsActive ? FColor::Green : (bIsInside ? FColor::Yellow : FColor(60, 120, 190));

		const FBox Box = Volume->GetQueryBounds();
		DrawDebugBox(World, Box.GetCenter(), Box.GetExtent(), Color, false, -1.0f, 0, bIsActive ? 3.0f : 1.0f);

		if (Volume->HasUsableRail())
		{
			const USplineComponent* Rail = Volume->Rail;
			const int32 Segments = FMath::Clamp(Rail->GetNumberOfSplinePoints() * 8, 8, 128);
			const float Length = Rail->GetSplineLength();
			FVector Previous = Rail->GetLocationAtDistanceAlongSpline(0.0f, ESplineCoordinateSpace::World);
			for (int32 Step = 1; Step <= Segments; ++Step)
			{
				const FVector Point = Rail->GetLocationAtDistanceAlongSpline(Length * Step / Segments, ESplineCoordinateSpace::World);
				DrawDebugLine(World, Previous, Point, Color, false, -1.0f, 0, 2.0f);
				Previous = Point;
			}
		}
	}

	if (bControlling)
	{
		// The pivot, the shot the plugin is aiming for, and the line the sweep travelled - the three things
		// that explain any camera behaviour this plugin can produce.
		DrawDebugSphere(World, ActivePivot, 12.0f, 12, FColor::White, false, -1.0f, 0, 1.5f);
		DrawDebugSphere(World, TargetState.Location, 18.0f, 12, FColor::Green, false, -1.0f, 0, 2.0f);
		DrawDebugLine(World, ActivePivot, TargetState.Location,
			Stats.bCollisionEngaged ? FColor::Red : FColor::Green, false, -1.0f, 0, 1.5f);

		const FFrameCameraState Now = GetCurrentState();
		DrawDebugSphere(World, Now.Location, 14.0f, 12, FColor::Cyan, false, -1.0f, 0, 2.0f);
	}
#endif
}

//~ Counters box -------------------------------------------------------------------------------------------

int32 UFrameVolumeSubsystem::GetStatsLineCount() const
{
	const int32 Listed = FMath::Min(Stats.InsideVolumeNames.Num(), FrameVolumePrivate::MaxListedInsideVolumes);
	return FrameVolumePrivate::FixedStatsLines + Listed;
}

void UFrameVolumeSubsystem::DrawStatsBox(UCanvas* Canvas, const FVector2D& Origin, float Width) const
{
	using namespace FrameVolumePrivate;

	if (!Canvas)
	{
		return;
	}

	UFont* Font = GEngine ? GEngine->GetSmallFont() : nullptr;
	if (!Font)
	{
		return;
	}

	LastStatsDrawFrame = GFrameCounter;

	const float BoxHeight = GetStatsLineCount() * LineHeight + BoxPadding * 2.0f;
	DrawFilledRect(Canvas,
		FVector2D(Origin.X - BoxPadding, Origin.Y - BoxPadding),
		FVector2D(Width, BoxHeight),
		PanelBackground);

	float LineY = static_cast<float>(Origin.Y);
	auto DrawLine = [&](FStringView Line, const FLinearColor& Color)
	{
		FCanvasTextStringViewItem Item(FVector2D(Origin.X, LineY), Line, Font, Color);
		Canvas->DrawItem(Item);
		LineY += LineHeight;
	};

	TStringBuilder<192> Line;

	Line.Reset();
	Line.Append(TEXT("FrameVolume"));
	DrawLine(Line.ToView(), HeadingColor);

	// The active shot, and whether the plugin is writing to the camera at all. Together these two lines
	// answer "is this thing doing anything" without anybody having to guess from the picture.
	Line.Reset();
	if (Stats.bControllingCamera)
	{
		Line.Appendf(TEXT("Active         %s"),
			Stats.ActiveVolumeName.IsEmpty() ? TEXT("(releasing to game camera)") : *Stats.ActiveVolumeName);
	}
	else
	{
		Line.Append(TEXT("Active         (game camera)"));
	}
	DrawLine(Line.ToView(), Stats.bControllingCamera ? GoodColor : DimColor);

	Line.Reset();
	Line.Appendf(TEXT("Row            %s   priority %d   %s"),
		Stats.ActiveRowName.IsNone() ? TEXT("(inline/none)") : *Stats.ActiveRowName.ToString(),
		Stats.ActivePriority,
		ModeName(Stats.ActiveMode));
	DrawLine(Line.ToView(), BodyColor);

	Line.Reset();
	Line.Appendf(TEXT("Inside         %d of %d volume(s)"), Stats.InsideVolumeNames.Num(), Stats.RegisteredVolumes);
	DrawLine(Line.ToView(), Stats.InsideVolumeNames.Num() > 1 ? WarnColor : BodyColor);

	// Every volume the actor is standing in, not just the winner. Overlap resolution that cannot be seen
	// is overlap resolution nobody believes.
	const int32 Listed = FMath::Min(Stats.InsideVolumeNames.Num(), MaxListedInsideVolumes);
	for (int32 Index = 0; Index < Listed; ++Index)
	{
		Line.Reset();
		Line.Appendf(TEXT("  %s %s"), Index == 0 ? TEXT("*") : TEXT("-"), *Stats.InsideVolumeNames[Index]);
		DrawLine(Line.ToView(), Index == 0 ? GoodColor : DimColor);
	}

	Line.Reset();
	Line.Appendf(TEXT("Blend          %.2f  "), Stats.CurvedAlpha);
	AppendBar(Line, Stats.CurvedAlpha);
	DrawLine(Line.ToView(), Stats.CurvedAlpha < 1.0f ? WarnColor : BodyColor);

	Line.Reset();
	Line.Appendf(TEXT("Blend time     %.2f s   %s%s"),
		Stats.BlendTime,
		CurveName(Stats.BlendCurve),
		GetBlendTimeOverride() >= 0.0f ? TEXT("   (forced)") : TEXT(""));
	DrawLine(Line.ToView(), BodyColor);

	Line.Reset();
	Line.Appendf(TEXT("Distance       %.0f cm target"), Stats.TargetDistance);
	DrawLine(Line.ToView(), BodyColor);

	Line.Reset();
	if (Stats.bCollisionEngaged)
	{
		Line.Appendf(TEXT("Collision      IN  %.0f cm  (pulled %.0f cm)"), Stats.ActualDistance, Stats.CollisionPullDistance);
	}
	else if (bCollisionAvoidanceEnabled)
	{
		Line.Appendf(TEXT("Collision      clear at %.0f cm"), Stats.ActualDistance);
	}
	else
	{
		Line.Append(TEXT("Collision      OFF"));
	}
	DrawLine(Line.ToView(), Stats.bCollisionEngaged ? WarnColor : (bCollisionAvoidanceEnabled ? GoodColor : DimColor));

	Line.Reset();
	Line.Appendf(TEXT("FOV            %.1f deg"), Stats.TargetFieldOfView);
	DrawLine(Line.ToView(), BodyColor);

	Line.Reset();
	if (Stats.bRailActive)
	{
		Line.Appendf(TEXT("Rail           ON  %.0f cm along"), Stats.RailDistance);
	}
	else
	{
		Line.Append(TEXT("Rail           off"));
	}
	DrawLine(Line.ToView(), Stats.bRailActive ? GoodColor : DimColor);

	Line.Reset();
	Line.Appendf(TEXT("Follow         %s%s"),
		*Stats.FollowTargetName,
		Stats.bOverrideActive ? TEXT("   [override]") : TEXT(""));
	DrawLine(Line.ToView(), Stats.bOverrideActive ? WarnColor : BodyColor);

	Line.Reset();
	Line.Appendf(TEXT("Update         %.3f ms"), Stats.UpdateMilliseconds);
	DrawLine(Line.ToView(), BodyColor);
}

void UFrameVolumeSubsystem::RebindHudDelegate()
{
	if (bAutoDrawStatsOnAnyHUD && !HudPostRenderHandle.IsValid())
	{
		HudPostRenderHandle = AHUD::OnHUDPostRender.AddUObject(this, &UFrameVolumeSubsystem::OnAnyHUDPostRender);
	}
	else if (!bAutoDrawStatsOnAnyHUD && HudPostRenderHandle.IsValid())
	{
		AHUD::OnHUDPostRender.Remove(HudPostRenderHandle);
		HudPostRenderHandle.Reset();
	}
}

void UFrameVolumeSubsystem::OnAnyHUDPostRender(AHUD* HUD, UCanvas* Canvas)
{
	if (!bAutoDrawStatsOnAnyHUD || !HUD || !Canvas)
	{
		return;
	}

	if (HUD->GetWorld() != GetWorld())
	{
		return;
	}

	// AFrameVolumeHUD already drew this frame - do not stack a second box on top of it.
	if (LastStatsDrawFrame == GFrameCounter)
	{
		return;
	}

	DrawStatsBox(Canvas, FVector2D(28.0f, 90.0f), 380.0f);
}

//~ Log ----------------------------------------------------------------------------------------------------

void UFrameVolumeSubsystem::LogStats() const
{
	UE_LOG(LogFrameVolume, Display, TEXT("FrameVolume:"));
	UE_LOG(LogFrameVolume, Display, TEXT("  Enabled          %s"), bEnabled ? TEXT("yes") : TEXT("no"));
	UE_LOG(LogFrameVolume, Display, TEXT("  Controlling      %s"), Stats.bControllingCamera ? TEXT("yes") : TEXT("no"));
	UE_LOG(LogFrameVolume, Display, TEXT("  Active           %s (row %s, priority %d, %s)"),
		Stats.ActiveVolumeName.IsEmpty() ? TEXT("(none)") : *Stats.ActiveVolumeName,
		*Stats.ActiveRowName.ToString(), Stats.ActivePriority, FrameVolumePrivate::ModeName(Stats.ActiveMode));
	UE_LOG(LogFrameVolume, Display, TEXT("  Inside           %d of %d volume(s)"),
		Stats.InsideVolumeNames.Num(), Stats.RegisteredVolumes);
	for (const FString& Name : Stats.InsideVolumeNames)
	{
		UE_LOG(LogFrameVolume, Display, TEXT("    %s"), *Name);
	}
	UE_LOG(LogFrameVolume, Display, TEXT("  Blend            %.3f raw, %.3f curved, %.2f s, %s"),
		Stats.BlendAlpha, Stats.CurvedAlpha, Stats.BlendTime, FrameVolumePrivate::CurveName(Stats.BlendCurve));
	UE_LOG(LogFrameVolume, Display, TEXT("  Distance         %.0f cm target, %.0f cm actual"),
		Stats.TargetDistance, Stats.ActualDistance);
	UE_LOG(LogFrameVolume, Display, TEXT("  Collision        %s%s"),
		bCollisionAvoidanceEnabled ? TEXT("on") : TEXT("off"),
		Stats.bCollisionEngaged ? *FString::Printf(TEXT(", engaged (-%.0f cm)"), Stats.CollisionPullDistance) : TEXT(""));
	UE_LOG(LogFrameVolume, Display, TEXT("  FOV              %.1f deg"), Stats.TargetFieldOfView);
	UE_LOG(LogFrameVolume, Display, TEXT("  Rail             %s (%.0f cm along)"),
		Stats.bRailActive ? TEXT("on") : TEXT("off"), Stats.RailDistance);
	UE_LOG(LogFrameVolume, Display, TEXT("  Follow           %s"), *Stats.FollowTargetName);
	UE_LOG(LogFrameVolume, Display, TEXT("  Override         %s"), Stats.bOverrideActive ? TEXT("yes") : TEXT("no"));
	UE_LOG(LogFrameVolume, Display, TEXT("  Update           %.3f ms"), Stats.UpdateMilliseconds);
}

void UFrameVolumeSubsystem::LogVolumeList() const
{
	UE_LOG(LogFrameVolume, Display, TEXT("FrameVolume: %d volume(s) in this world."), Volumes.Num());

	for (const TWeakObjectPtr<AFrameVolume>& Weak : Volumes)
	{
		const AFrameVolume* Volume = Weak.Get();
		if (!Volume)
		{
			continue;
		}

		FFrameVolumeRow Row;
		const bool bResolved = Volume->GetShot(Row);

		UE_LOG(LogFrameVolume, Display,
			TEXT("  %-28s p%-4d %-7s dist %5.0f  fov %4.1f  blend %4.2fs  %s%s%s%s"),
			*Volume->GetActorNameOrLabel(),
			Volume->GetEffectivePriority(),
			FrameVolumePrivate::ModeName(Row.Mode),
			Row.Distance,
			Row.FieldOfView,
			Row.BlendTime,
			bResolved ? TEXT("") : TEXT("[fallback shot] "),
			Volume->bVolumeEnabled ? TEXT("") : TEXT("[disabled] "),
			IsInsideVolume(Volume) ? TEXT("[inside] ") : TEXT(""),
			Volume == ActiveVolume.Get() ? TEXT("[ACTIVE]") : TEXT(""));
	}
}

//~ Console commands ---------------------------------------------------------------------------------------

namespace FrameVolumePrivate
{
	static FAutoConsoleCommandWithWorldAndArgs CmdStats(
		TEXT("Frame.Stats"),
		TEXT("Frame.Stats - print the measured camera counters to the log."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic([](const TArray<FString>& Args, UWorld* World)
		{
			const UFrameVolumeSubsystem* Subsystem = GetSubsystem(World);
			if (!Subsystem)
			{
				UE_LOG(LogFrameVolume, Warning, TEXT("Frame.Stats: no FrameVolume subsystem in this world."));
				return;
			}
			Subsystem->LogStats();
		}));

	static FAutoConsoleCommandWithWorldAndArgs CmdList(
		TEXT("Frame.List"),
		TEXT("Frame.List - print every frame volume in the world, with priority, row and occupancy."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic([](const TArray<FString>& Args, UWorld* World)
		{
			const UFrameVolumeSubsystem* Subsystem = GetSubsystem(World);
			if (!Subsystem)
			{
				UE_LOG(LogFrameVolume, Warning, TEXT("Frame.List: no FrameVolume subsystem in this world."));
				return;
			}
			Subsystem->LogVolumeList();
		}));

	static FAutoConsoleCommandWithWorldAndArgs CmdBlend(
		TEXT("Frame.Blend"),
		TEXT("Frame.Blend [Seconds] - force every blend to this length. Negative gives the rows control again."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic([](const TArray<FString>& Args, UWorld* World)
		{
			UFrameVolumeSubsystem* Subsystem = GetSubsystem(World);
			if (!Subsystem)
			{
				UE_LOG(LogFrameVolume, Warning, TEXT("Frame.Blend: no FrameVolume subsystem in this world."));
				return;
			}

			if (Args.Num() == 0)
			{
				const float Current = Subsystem->GetBlendTimeOverride();
				UE_LOG(LogFrameVolume, Display, TEXT("Frame.Blend: %s"),
					Current >= 0.0f ? *FString::Printf(TEXT("forced to %.2f s"), Current) : TEXT("off - the rows decide"));
				return;
			}

			const float Seconds = FCString::Atof(*Args[0]);
			Subsystem->SetBlendTimeOverride(Seconds);
			UE_LOG(LogFrameVolume, Display, TEXT("Frame.Blend: %s"),
				Seconds >= 0.0f ? *FString::Printf(TEXT("every blend forced to %.2f s"), Seconds) : TEXT("off - the rows decide"));
		}));

	static FAutoConsoleCommandWithWorldAndArgs CmdDebug(
		TEXT("Frame.Debug"),
		TEXT("Frame.Debug [0|1] - draw the volumes, the pivot, the target shot and the collision sweep."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic([](const TArray<FString>& Args, UWorld* World)
		{
			UFrameVolumeSubsystem* Subsystem = GetSubsystem(World);
			if (!Subsystem)
			{
				UE_LOG(LogFrameVolume, Warning, TEXT("Frame.Debug: no FrameVolume subsystem in this world."));
				return;
			}

			const bool bEnable = Args.Num() > 0 ? (FCString::Atoi(*Args[0]) != 0) : !Subsystem->IsDebugDrawEnabled();
			Subsystem->SetDebugDraw(bEnable);
			UE_LOG(LogFrameVolume, Display, TEXT("Frame.Debug: %s"), bEnable ? TEXT("on") : TEXT("off"));
		}));

	static FAutoConsoleCommandWithWorldAndArgs CmdCollision(
		TEXT("Frame.Collision"),
		TEXT("Frame.Collision [0|1] - turn collision avoidance on or off for every shot."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic([](const TArray<FString>& Args, UWorld* World)
		{
			UFrameVolumeSubsystem* Subsystem = GetSubsystem(World);
			if (!Subsystem)
			{
				UE_LOG(LogFrameVolume, Warning, TEXT("Frame.Collision: no FrameVolume subsystem in this world."));
				return;
			}

			const bool bEnable = Args.Num() > 0 ? (FCString::Atoi(*Args[0]) != 0) : !Subsystem->IsCollisionAvoidanceEnabled();
			Subsystem->SetCollisionAvoidanceEnabled(bEnable);
			UE_LOG(LogFrameVolume, Display, TEXT("Frame.Collision: %s"), bEnable ? TEXT("on") : TEXT("off"));
		}));
}
