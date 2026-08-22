// Copyright 2026 Silvan Teufel. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "FrameVolumeTypes.h"
#include "FrameVolumeStatics.generated.h"

class AActor;
class AFrameVolume;

/**
 * The whole plugin from Blueprint, without a reference to the subsystem in sight.
 *
 * Every call is world-context-aware and safe in a world that has no FrameVolume subsystem at all:
 * queries answer nothing, setters do nothing, and nobody crashes. That matters more than it sounds - it
 * means a Blueprint written against FrameVolume still runs in a test map where no volume was ever placed.
 */
UCLASS(meta = (DisplayName = "FrameVolume"))
class FRAMEVOLUME_API UFrameVolumeStatics : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	//~ Queries ------------------------------------------------------------------------------------------

	/** The volume that currently owns the camera, or null when the game camera has it. */
	UFUNCTION(BlueprintPure, Category = "FrameVolume", meta = (WorldContext = "WorldContextObject"))
	static AFrameVolume* GetActiveVolume(const UObject* WorldContextObject);

	/** How far the blend in flight has got, 0..1. 1 means the camera has arrived. */
	UFUNCTION(BlueprintPure, Category = "FrameVolume", meta = (WorldContext = "WorldContextObject"))
	static float GetBlendAlpha(const UObject* WorldContextObject);

	/** True while the plugin is writing to the camera at all. */
	UFUNCTION(BlueprintPure, Category = "FrameVolume", meta = (WorldContext = "WorldContextObject"))
	static bool IsControllingCamera(const UObject* WorldContextObject);

	/** Every volume the followed actor is standing in, highest priority first. */
	UFUNCTION(BlueprintPure, Category = "FrameVolume", meta = (WorldContext = "WorldContextObject"))
	static void GetInsideVolumes(const UObject* WorldContextObject, TArray<AFrameVolume*>& OutVolumes);

	/** Every volume in this world, whether occupied or not. */
	UFUNCTION(BlueprintPure, Category = "FrameVolume", meta = (WorldContext = "WorldContextObject"))
	static void GetAllVolumes(const UObject* WorldContextObject, TArray<AFrameVolume*>& OutVolumes);

	/** What the camera is doing right now, blend included. */
	UFUNCTION(BlueprintPure, Category = "FrameVolume", meta = (WorldContext = "WorldContextObject"))
	static FFrameCameraState GetCurrentCameraState(const UObject* WorldContextObject);

	/** Everything the counters box draws. */
	UFUNCTION(BlueprintPure, Category = "FrameVolume", meta = (WorldContext = "WorldContextObject"))
	static FFrameVolumeStats GetStats(const UObject* WorldContextObject);

	/** The shot a volume resolves to. False when the volume had to fall back; OutRow is filled either way. */
	UFUNCTION(BlueprintCallable, Category = "FrameVolume")
	static bool GetVolumeShot(const AFrameVolume* Volume, FFrameVolumeRow& OutRow);

	//~ Framing ------------------------------------------------------------------------------------------

	/** Frame this actor instead of the local player's pawn. Null puts automatic following back. */
	UFUNCTION(BlueprintCallable, Category = "FrameVolume", meta = (WorldContext = "WorldContextObject"))
	static void SetFollowTarget(const UObject* WorldContextObject, AActor* NewTarget);

	/** The actor the shots are built around. */
	UFUNCTION(BlueprintPure, Category = "FrameVolume", meta = (WorldContext = "WorldContextObject"))
	static AActor* GetFollowTarget(const UObject* WorldContextObject);

	/**
	 * Take the camera with a shot that is in no volume, beating every volume until it is cleared.
	 *
	 * One slot, not a stack. A second push replaces the first rather than queueing behind it.
	 */
	UFUNCTION(BlueprintCallable, Category = "FrameVolume", meta = (WorldContext = "WorldContextObject"))
	static void PushOverride(const UObject* WorldContextObject, const FFrameVolumeRow& Shot);

	/** Hand the camera back to the volumes, or to the game if the actor is in none. */
	UFUNCTION(BlueprintCallable, Category = "FrameVolume", meta = (WorldContext = "WorldContextObject"))
	static void ClearOverride(const UObject* WorldContextObject);

	/** True while an override is in force. */
	UFUNCTION(BlueprintPure, Category = "FrameVolume", meta = (WorldContext = "WorldContextObject"))
	static bool IsOverrideActive(const UObject* WorldContextObject);

	/**
	 * A shot built in one node, for the Blueprints that push an override rather than author a table row.
	 *
	 * Everything not named here takes the struct's default, which is a sane third-person shot - so a
	 * two-pin call is enough to get something on screen.
	 */
	UFUNCTION(BlueprintPure, Category = "FrameVolume", meta = (AdvancedDisplay = "4"))
	static FFrameVolumeRow MakeOrbitShot(float Distance = 600.0f, float Pitch = -15.0f, float Yaw = 0.0f,
		float FieldOfView = 75.0f, float BlendTime = 0.75f, EFrameBlendCurve BlendCurve = EFrameBlendCurve::EaseInOut,
		int32 Priority = 0);

	//~ Switches -----------------------------------------------------------------------------------------

	/** Turn camera control on or off. Off blends back to the game camera and then stops writing. */
	UFUNCTION(BlueprintCallable, Category = "FrameVolume", meta = (WorldContext = "WorldContextObject"))
	static void SetFrameVolumeEnabled(const UObject* WorldContextObject, bool bEnabled);

	/** True while the plugin is allowed to write to the camera. */
	UFUNCTION(BlueprintPure, Category = "FrameVolume", meta = (WorldContext = "WorldContextObject"))
	static bool IsFrameVolumeEnabled(const UObject* WorldContextObject);

	/** Force every blend to this many seconds. Negative gives the rows control again. */
	UFUNCTION(BlueprintCallable, Category = "FrameVolume", meta = (WorldContext = "WorldContextObject"))
	static void SetBlendTimeOverride(const UObject* WorldContextObject, float Seconds);

	/** The forced blend time, or a negative number when the rows are in charge. */
	UFUNCTION(BlueprintPure, Category = "FrameVolume", meta = (WorldContext = "WorldContextObject"))
	static float GetBlendTimeOverride(const UObject* WorldContextObject);

	/** Force every shot to this field of view. Zero or negative gives the rows control again. */
	UFUNCTION(BlueprintCallable, Category = "FrameVolume", meta = (WorldContext = "WorldContextObject"))
	static void SetFieldOfViewOverride(const UObject* WorldContextObject, float Degrees);

	/** The forced field of view, or a non-positive number when the rows are in charge. */
	UFUNCTION(BlueprintPure, Category = "FrameVolume", meta = (WorldContext = "WorldContextObject"))
	static float GetFieldOfViewOverride(const UObject* WorldContextObject);

	/** Turn collision avoidance off for every shot, whatever the rows say. */
	UFUNCTION(BlueprintCallable, Category = "FrameVolume", meta = (WorldContext = "WorldContextObject"))
	static void SetCollisionAvoidanceEnabled(const UObject* WorldContextObject, bool bEnabled);

	/** True while collision avoidance is globally allowed. */
	UFUNCTION(BlueprintPure, Category = "FrameVolume", meta = (WorldContext = "WorldContextObject"))
	static bool IsCollisionAvoidanceEnabled(const UObject* WorldContextObject);

	/** Draw the volumes, the pivot, the target shot and the collision sweep in the world. */
	UFUNCTION(BlueprintCallable, Category = "FrameVolume", meta = (WorldContext = "WorldContextObject"))
	static void SetDebugDraw(const UObject* WorldContextObject, bool bEnabled);

	/** True while debug drawing is on. */
	UFUNCTION(BlueprintPure, Category = "FrameVolume", meta = (WorldContext = "WorldContextObject"))
	static bool IsDebugDrawEnabled(const UObject* WorldContextObject);
};
