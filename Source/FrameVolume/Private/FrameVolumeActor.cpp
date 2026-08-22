// Copyright 2026 Silvan Teufel. All Rights Reserved.

#include "FrameVolumeActor.h"

#include "Components/BoxComponent.h"
#include "Components/BrushComponent.h"
#include "Components/SplineComponent.h"
#include "Engine/World.h"
#include "FrameVolumeLog.h"
#include "FrameVolumeSettings.h"
#include "FrameVolumeSubsystem.h"
#include "PhysicsEngine/BodySetup.h"

AFrameVolume::AFrameVolume()
{
	// Not one volume ticks. The subsystem does the whole update once per frame, for all of them.
	PrimaryActorTick.bCanEverTick = false;
	PrimaryActorTick.bStartWithTickEnabled = false;

	if (UBrushComponent* BrushComp = GetBrushComponent())
	{
		// Queries only, and no overlap events: containment is asked for by the subsystem, so paying the
		// physics scene for begin/end overlap callbacks nobody listens to would be pure cost. The brush
		// still has to answer a distance query, which is what QueryOnly leaves in place.
		BrushComp->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		BrushComp->SetGenerateOverlapEvents(false);
		BrushComp->SetCanEverAffectNavigation(false);
		BrushComp->bAlwaysCreatePhysicsState = true;
	}

	// A camera volume should look like a camera volume in a viewport full of trigger volumes.
	bColored = true;
	BrushColor = FColor(72, 168, 255, 255);

	FallbackBounds = CreateDefaultSubobject<UBoxComponent>(TEXT("FallbackBounds"));
	FallbackBounds->SetupAttachment(GetBrushComponent());
	FallbackBounds->SetBoxExtent(FVector(500.0f, 500.0f, 300.0f));
	FallbackBounds->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	FallbackBounds->SetGenerateOverlapEvents(false);
	FallbackBounds->SetHiddenInGame(true);
	FallbackBounds->ShapeColor = FColor(72, 168, 255);
	// Learned the hard way on another plugin's demo: a shape component that affects navigation punches a
	// hole in the navmesh underneath itself, and the agents that were supposed to walk through the volume
	// walk around it instead.
	FallbackBounds->SetCanEverAffectNavigation(false);

	Anchor = CreateDefaultSubobject<USceneComponent>(TEXT("Anchor"));
	Anchor->SetupAttachment(GetBrushComponent());
	Anchor->SetRelativeLocation(FVector(0.0f, -600.0f, 350.0f));

	Rail = CreateDefaultSubobject<USplineComponent>(TEXT("Rail"));
	Rail->SetupAttachment(GetBrushComponent());
	Rail->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Rail->SetCanEverAffectNavigation(false);
	Rail->SetClosedLoop(false);
	// Starts empty. A spline component ships with two default points; clearing them is what makes
	// "has a usable rail" mean "somebody built a rail here" rather than "this class has a spline on it".
	Rail->ClearSplinePoints(false);
}

void AFrameVolume::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	// Registering here rather than only in BeginPlay covers the volume that is spawned into a world which
	// has already begun play - it would otherwise sit in the level doing nothing until the next travel.
	RegisterWithSubsystem();
}

void AFrameVolume::BeginPlay()
{
	Super::BeginPlay();

	RegisterWithSubsystem();

	if (!bUseInlineShot && Shot.DataTable == nullptr)
	{
		UE_LOG(LogFrameVolume, Verbose,
			TEXT("%s has no shot row; it will frame with the project fallback shot."), *GetActorNameOrLabel());
	}
}

void AFrameVolume::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UFrameVolumeSubsystem* Subsystem = UFrameVolumeSubsystem::Get(this))
	{
		Subsystem->UnregisterVolume(this);
	}
	bRegistered = false;

	Super::EndPlay(EndPlayReason);
}

void AFrameVolume::RegisterWithSubsystem()
{
	if (bRegistered)
	{
		return;
	}

	if (UFrameVolumeSubsystem* Subsystem = UFrameVolumeSubsystem::Get(this))
	{
		Subsystem->RegisterVolume(this);
		bRegistered = true;
	}
}

bool AFrameVolume::GetShot(FFrameVolumeRow& OutRow) const
{
	if (bUseInlineShot)
	{
		OutRow = InlineShot;
		return true;
	}

	if (Shot.DataTable)
	{
		static const FString Context(TEXT("FrameVolume shot"));
		if (const FFrameVolumeRow* Found = Shot.DataTable->FindRow<FFrameVolumeRow>(Shot.RowName, Context, /*bWarnIfMissing*/ false))
		{
			OutRow = *Found;
			return true;
		}
	}

	OutRow = UFrameVolumeSettings::Get().FallbackShot;
	return false;
}

int32 AFrameVolume::GetEffectivePriority() const
{
	if (bOverridePriority)
	{
		return PriorityOverride;
	}

	FFrameVolumeRow Row;
	GetShot(Row);
	return Row.Priority;
}

FName AFrameVolume::GetShotRowName() const
{
	if (bUseInlineShot)
	{
		return NAME_None;
	}
	return Shot.DataTable ? Shot.RowName : NAME_None;
}

bool AFrameVolume::HasBrushGeometry() const
{
	const UBrushComponent* BrushComp = GetBrushComponent();
	// A brush is built by the editor's brush builder, not by the constructor, so a spawned volume reaches
	// this with a BrushComponent that has no body setup at all. Asking the body setup rather than the
	// UModel is what makes this line up exactly with what EncompassesPoint can actually answer.
	return BrushComp != nullptr
		&& BrushComp->BrushBodySetup != nullptr
		&& BrushComp->BrushBodySetup->AggGeom.GetElementCount() > 0;
}

bool AFrameVolume::HasUsableRail() const
{
	return Rail != nullptr && Rail->GetNumberOfSplinePoints() >= 2;
}

FBox AFrameVolume::GetQueryBounds() const
{
	if (HasBrushGeometry())
	{
		if (const UBrushComponent* BrushComp = GetBrushComponent())
		{
			return BrushComp->Bounds.GetBox();
		}
	}

	if (FallbackBounds)
	{
		const FVector Extent = FallbackBounds->GetScaledBoxExtent();
		const FTransform& Xf = FallbackBounds->GetComponentTransform();
		// The box is oriented, so its world bounds are the eight transformed corners, not the extent
		// added to the origin. Getting this wrong makes the cheap pass reject points that are inside.
		FBox Box(ForceInit);
		for (int32 Corner = 0; Corner < 8; ++Corner)
		{
			const FVector Local(
				(Corner & 1) ? Extent.X : -Extent.X,
				(Corner & 2) ? Extent.Y : -Extent.Y,
				(Corner & 4) ? Extent.Z : -Extent.Z);
			Box += Xf.TransformPosition(Local);
		}
		return Box;
	}

	return FBox(GetActorLocation(), GetActorLocation());
}

bool AFrameVolume::ContainsPoint(const FVector& WorldPoint) const
{
	if (HasBrushGeometry())
	{
		return EncompassesPoint(WorldPoint);
	}

	if (FallbackBounds)
	{
		// Pure maths, no physics query: transform into the box's space and compare against the extent.
		const FVector Local = FallbackBounds->GetComponentTransform().InverseTransformPosition(WorldPoint);
		const FVector Extent = FallbackBounds->GetScaledBoxExtent();
		return FMath::Abs(Local.X) <= Extent.X
			&& FMath::Abs(Local.Y) <= Extent.Y
			&& FMath::Abs(Local.Z) <= Extent.Z;
	}

	return false;
}

bool AFrameVolume::IsOccupied() const
{
	const UFrameVolumeSubsystem* Subsystem = UFrameVolumeSubsystem::Get(this);
	return Subsystem && Subsystem->IsInsideVolume(this);
}

bool AFrameVolume::IsActiveVolume() const
{
	const UFrameVolumeSubsystem* Subsystem = UFrameVolumeSubsystem::Get(this);
	return Subsystem && Subsystem->GetActiveVolume() == this;
}
