// Copyright 2026 Simulated Flow. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "FrameVolumeTypes.generated.h"

class UCurveFloat;

/**
 * How a shot works out where the camera goes.
 *
 * The three cases are genuinely different geometries, not three flags on one. Keeping them one field
 * means a row can never say "orbit at 600 cm" and "sit on the rail" at the same time and leave the
 * runtime to guess which it meant.
 */
UENUM(BlueprintType)
enum class EFrameCameraMode : uint8
{
	/** Orbit the followed actor: a distance and two angles around it. The everyday case. */
	Orbit UMETA(DisplayName = "Orbit"),

	/** A fixed point in the world - the volume's Anchor component. The camera stands still and turns. */
	Anchor UMETA(DisplayName = "Anchor Point"),

	/** A camera rail: the volume's Rail spline, at the point on it nearest the followed actor. */
	Rail UMETA(DisplayName = "Rail")
};

/** Shape of the 0..1 blend between two shots. */
UENUM(BlueprintType)
enum class EFrameBlendCurve : uint8
{
	/** Constant speed. Reads as mechanical, which is occasionally what a shot wants. */
	Linear UMETA(DisplayName = "Linear"),

	/** Starts slow, arrives at speed. */
	EaseIn UMETA(DisplayName = "Ease In"),

	/** Leaves at speed, arrives slowly. */
	EaseOut UMETA(DisplayName = "Ease Out"),

	/** Slow at both ends. The default, because it is the one that never looks like a cut. */
	EaseInOut UMETA(DisplayName = "Ease In-Out"),

	/** Whatever the row's CustomCurve says over the range 0..1. */
	Custom UMETA(DisplayName = "Custom Curve")
};

/**
 * One shot: where the camera is, where it looks, and how wide.
 *
 * This is the only thing that is ever blended. Not a stack of shots, not a queue of requests - one
 * current state, one target state, one alpha between them.
 */
USTRUCT(BlueprintType)
struct FRAMEVOLUME_API FFrameCameraState
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FrameVolume")
	FVector Location = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FrameVolume")
	FRotator Rotation = FRotator::ZeroRotator;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FrameVolume", meta = (ClampMin = "5.0", ClampMax = "170.0"))
	float FieldOfView = 90.0f;

	/**
	 * Interpolate two shots.
	 *
	 * The rotation goes through a quaternion slerp rather than a rotator lerp on purpose: a rotator lerp
	 * takes the long way round whenever a yaw crosses 180 degrees, which on a camera reads as a full spin
	 * in the middle of an otherwise calm move.
	 */
	static FFrameCameraState Blend(const FFrameCameraState& From, const FFrameCameraState& To, float Alpha)
	{
		FFrameCameraState Result;
		Result.Location = FMath::Lerp(From.Location, To.Location, Alpha);
		Result.Rotation = FQuat::Slerp(From.Rotation.Quaternion(), To.Rotation.Quaternion(), Alpha).Rotator();
		Result.FieldOfView = FMath::Lerp(From.FieldOfView, To.FieldOfView, Alpha);
		return Result;
	}
};

/**
 * A shot as a DataTable row - the whole point of the plugin.
 *
 * Every number a camera change needs is here, in one row, with a name. Two volumes on opposite ends of a
 * level that want the same framing point at the same row; changing the row changes both, and neither map
 * has to be opened to do it. The alternative - the same eight numbers typed into each volume's details
 * panel - is what this exists to replace.
 *
 * The row is a plain struct, so PushOverride can hand one to the runtime from Blueprint without a
 * DataTable being involved at all, and a volume can carry an inline copy for a one-off shot.
 */
USTRUCT(BlueprintType)
struct FRAMEVOLUME_API FFrameVolumeRow : public FTableRowBase
{
	GENERATED_BODY()

	//~ Framing ------------------------------------------------------------------------------------------

	/** Orbit the actor, stand on a fixed anchor, or ride a rail. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Framing")
	EFrameCameraMode Mode = EFrameCameraMode::Orbit;

	/** Distance from the pivot, in centimetres. Orbit only - Anchor and Rail get their position elsewhere. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Framing",
		meta = (ClampMin = "0.0", UIMin = "50.0", UIMax = "4000.0", ForceUnits = "cm"))
	float Distance = 600.0f;

	/** Angle above (positive) or below (negative) the pivot. Orbit only. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Framing",
		meta = (ClampMin = "-89.0", ClampMax = "89.0", ForceUnits = "deg"))
	float Pitch = -15.0f;

	/** Angle around the pivot. Orbit only; see bYawRelativeToVolume for what it is measured against. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Framing", meta = (ForceUnits = "deg"))
	float Yaw = 0.0f;

	/**
	 * Measure Yaw against the volume's own rotation rather than against world north.
	 *
	 * On by default, and it is the setting that makes one row reusable: a corridor row placed in four
	 * corridors facing four directions frames all four correctly, because each volume carries its own
	 * heading. Turn it off when a row means a compass direction rather than a direction relative to the
	 * thing it was placed on.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Framing")
	bool bYawRelativeToVolume = true;

	/** Horizontal field of view in degrees. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Framing",
		meta = (ClampMin = "5.0", ClampMax = "170.0", ForceUnits = "deg"))
	float FieldOfView = 75.0f;

	/**
	 * Offset from the followed actor's origin to the point the shot is built around.
	 *
	 * Defaults to roughly chest height on a standard character, because framing a capsule's feet is
	 * almost never what anyone means by "look at the player".
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Framing", meta = (ForceUnits = "cm"))
	FVector PivotOffset = FVector(0.0f, 0.0f, 60.0f);

	/**
	 * Point the camera at the pivot. Anchor and Rail only - an orbit shot always looks at its pivot.
	 *
	 * Off means the shot keeps the Anchor component's own rotation (or, on a rail, the spline's tangent),
	 * which is how a fixed security-camera angle or a dolly that stares straight ahead is written.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Framing")
	bool bLookAtPivot = true;

	/** Added to the resulting rotation after everything else. Small tilts and lead-room live here. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Framing", meta = (ForceUnits = "deg"))
	FRotator RotationOffset = FRotator::ZeroRotator;

	//~ Blend --------------------------------------------------------------------------------------------

	/** Seconds to reach this shot from whatever the camera was doing. 0 is a cut. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Blend",
		meta = (ClampMin = "0.0", UIMax = "5.0", ForceUnits = "s"))
	float BlendTime = 0.75f;

	/** Shape of the blend. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Blend")
	EFrameBlendCurve BlendCurve = EFrameBlendCurve::EaseInOut;

	/**
	 * Curve used when BlendCurve is Custom, read over the input range 0..1.
	 *
	 * Soft on purpose: a project with two hundred rows and no custom curves loads no curve assets at all.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Blend", meta = (EditCondition = "BlendCurve == EFrameBlendCurve::Custom"))
	TSoftObjectPtr<UCurveFloat> CustomCurve;

	//~ Collision ----------------------------------------------------------------------------------------

	/**
	 * Sweep from the pivot out to the camera and stop the camera at the first thing in the way.
	 *
	 * One sweep per tick, and only while this shot is the active one. Off is a real option, not a
	 * degraded one: an anchor camera bolted to a ceiling corner has nothing to collide with and should
	 * not pay for a trace to find that out sixty times a second.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Collision")
	bool bAvoidCollision = true;

	/** Radius of the sweep, in centimetres. The camera keeps at least this far off any surface. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Collision",
		meta = (ClampMin = "0.0", UIMax = "60.0", ForceUnits = "cm", EditCondition = "bAvoidCollision"))
	float CollisionProbeRadius = 14.0f;

	/** Extra centimetres to pull off the surface after a hit, so a near-parallel wall cannot clip. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Collision",
		meta = (ClampMin = "0.0", UIMax = "50.0", ForceUnits = "cm", EditCondition = "bAvoidCollision"))
	float CollisionPullIn = 6.0f;

	/** Never pull closer to the pivot than this, however hard the geometry pushes. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Collision",
		meta = (ClampMin = "0.0", UIMax = "500.0", ForceUnits = "cm", EditCondition = "bAvoidCollision"))
	float MinCollisionDistance = 60.0f;

	//~ Resolution ---------------------------------------------------------------------------------------

	/**
	 * Who wins where volumes overlap. Higher takes the camera.
	 *
	 * Equal priorities are broken by the smaller volume, which is the answer a level almost always wants:
	 * the small volume around the doorway is the specific case inside the big volume around the hall.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Resolution")
	int32 Priority = 0;
};

/**
 * Everything the counters box and Frame.Stats read, gathered once per tick by the subsystem.
 *
 * Measured where the work happens rather than recomputed for display, so the box cannot report one thing
 * while the camera does another.
 */
USTRUCT(BlueprintType)
struct FRAMEVOLUME_API FFrameVolumeStats
{
	GENERATED_BODY()

	/** Label of the volume that currently owns the camera, or empty when none does. */
	UPROPERTY(BlueprintReadOnly, Category = "FrameVolume|Stats")
	FString ActiveVolumeName;

	/** DataTable row the active volume resolved to. */
	UPROPERTY(BlueprintReadOnly, Category = "FrameVolume|Stats")
	FName ActiveRowName;

	/** Priority of the active shot. */
	UPROPERTY(BlueprintReadOnly, Category = "FrameVolume|Stats")
	int32 ActivePriority = 0;

	/** Mode of the active shot. */
	UPROPERTY(BlueprintReadOnly, Category = "FrameVolume|Stats")
	EFrameCameraMode ActiveMode = EFrameCameraMode::Orbit;

	/** Every volume the followed actor is standing in right now, highest priority first. */
	UPROPERTY(BlueprintReadOnly, Category = "FrameVolume|Stats")
	TArray<FString> InsideVolumeNames;

	/** How many volumes exist in this world. */
	UPROPERTY(BlueprintReadOnly, Category = "FrameVolume|Stats")
	int32 RegisteredVolumes = 0;

	/** Raw blend progress, 0..1. */
	UPROPERTY(BlueprintReadOnly, Category = "FrameVolume|Stats")
	float BlendAlpha = 1.0f;

	/** Blend progress after the curve. This is the one the camera actually used. */
	UPROPERTY(BlueprintReadOnly, Category = "FrameVolume|Stats")
	float CurvedAlpha = 1.0f;

	/** Length of the blend in flight, in seconds. */
	UPROPERTY(BlueprintReadOnly, Category = "FrameVolume|Stats")
	float BlendTime = 0.0f;

	/** Curve of the blend in flight. */
	UPROPERTY(BlueprintReadOnly, Category = "FrameVolume|Stats")
	EFrameBlendCurve BlendCurve = EFrameBlendCurve::EaseInOut;

	/** Distance the active shot asked for, before collision. */
	UPROPERTY(BlueprintReadOnly, Category = "FrameVolume|Stats")
	float TargetDistance = 0.0f;

	/** Distance the camera ended up at, after collision. */
	UPROPERTY(BlueprintReadOnly, Category = "FrameVolume|Stats")
	float ActualDistance = 0.0f;

	/** Field of view of the active shot. */
	UPROPERTY(BlueprintReadOnly, Category = "FrameVolume|Stats")
	float TargetFieldOfView = 0.0f;

	/** True on the ticks where the sweep hit something and moved the camera in. */
	UPROPERTY(BlueprintReadOnly, Category = "FrameVolume|Stats")
	bool bCollisionEngaged = false;

	/** How many centimetres collision took off the distance this tick. */
	UPROPERTY(BlueprintReadOnly, Category = "FrameVolume|Stats")
	float CollisionPullDistance = 0.0f;

	/** True while the active shot is riding a rail. */
	UPROPERTY(BlueprintReadOnly, Category = "FrameVolume|Stats")
	bool bRailActive = false;

	/** How far along the rail the camera sits, in centimetres from the spline's start. */
	UPROPERTY(BlueprintReadOnly, Category = "FrameVolume|Stats")
	float RailDistance = 0.0f;

	/** True while a Blueprint override is beating the volumes. */
	UPROPERTY(BlueprintReadOnly, Category = "FrameVolume|Stats")
	bool bOverrideActive = false;

	/** True while the plugin is actually writing to the camera. */
	UPROPERTY(BlueprintReadOnly, Category = "FrameVolume|Stats")
	bool bControllingCamera = false;

	/** The actor the shots are built around. */
	UPROPERTY(BlueprintReadOnly, Category = "FrameVolume|Stats")
	FString FollowTargetName;

	/** Milliseconds the whole update took, sweep included. */
	UPROPERTY(BlueprintReadOnly, Category = "FrameVolume|Stats")
	float UpdateMilliseconds = 0.0f;
};
