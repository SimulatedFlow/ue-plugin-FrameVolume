// Copyright 2026 Silvan Teufel. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "Subsystems/WorldSubsystem.h"
#include "FrameVolumeTypes.h"
#include "FrameVolumeSubsystem.generated.h"

class AFrameVolume;
class AHUD;
class APlayerCameraManager;
class UCanvas;
class UCurveFloat;
class UFrameVolumeCameraModifier;

/** Fired when the volume that owns the camera changes. Either argument may be null. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FFrameVolumeActiveChanged, AFrameVolume*, NewVolume, AFrameVolume*, OldVolume);

/** Fired when the followed actor enters or leaves a volume, whether or not that volume wins the camera. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FFrameVolumeOccupancyChanged, AFrameVolume*, Volume);

/**
 * The whole plugin, running once per frame in one place.
 *
 * Its job in one sentence: work out which volume the followed actor is in, turn that volume's row into a
 * camera position, and move from wherever the camera was to there over a measured length of time.
 *
 * **There is no blend stack.** This matters enough to be the first thing said about it. Systems that
 * queue blends drift: leave a volume mid-blend, walk back in, and the queue is two entries deep and the
 * camera arrives late, or twice. Here there is exactly one current state, one target state and one alpha
 * between them. When the target changes mid-blend the current state is replaced by the point the blend
 * had actually reached, and the alpha restarts from zero. The camera therefore leaves from where it
 * visibly is - never from where a finished blend would have put it - and nothing accumulates.
 *
 * **One tick, one trace, one spline query.** Whatever a level does, an update is: a bounds test per
 * volume plus an exact test for the few that pass, one sort, one shot evaluated, at most one sphere
 * sweep, and at most one closest-point-on-spline. That number does not grow with the number of volumes
 * the actor is standing in, and it does not grow with the number of volumes in the map.
 *
 * Game and PIE only. An editor viewport camera is the level designer's, not the plugin's - taking it
 * over while somebody is building the level is the sort of help nobody asked for.
 */
UCLASS(meta = (DisplayName = "Frame Volume Subsystem"))
class FRAMEVOLUME_API UFrameVolumeSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	//~ USubsystem interface
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	//~ UWorldSubsystem interface
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

	//~ FTickableGameObject interface
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

	/** The subsystem for whatever world the context object belongs to, or null. */
	static UFrameVolumeSubsystem* Get(const UObject* WorldContextObject);

	//~ Volume registry ----------------------------------------------------------------------------------

	/** Called by AFrameVolume. Harmless to call twice. */
	void RegisterVolume(AFrameVolume* Volume);

	/** Called by AFrameVolume on EndPlay. */
	void UnregisterVolume(AFrameVolume* Volume);

	/** Every volume in this world, in registration order. */
	UFUNCTION(BlueprintPure, Category = "FrameVolume")
	void GetAllVolumes(TArray<AFrameVolume*>& OutVolumes) const;

	/** Every volume the followed actor is standing in, highest priority first. */
	UFUNCTION(BlueprintPure, Category = "FrameVolume")
	void GetInsideVolumes(TArray<AFrameVolume*>& OutVolumes) const;

	/** True while the followed actor is inside this particular volume. */
	UFUNCTION(BlueprintPure, Category = "FrameVolume")
	bool IsInsideVolume(const AFrameVolume* Volume) const;

	/** The volume that currently owns the camera, or null when the game camera has it back. */
	UFUNCTION(BlueprintPure, Category = "FrameVolume")
	AFrameVolume* GetActiveVolume() const;

	//~ Follow target ------------------------------------------------------------------------------------

	/**
	 * Frame this actor instead of the local player's pawn.
	 *
	 * Null puts automatic following back, if the project settings allow it. Changing the followed actor
	 * does not cut: the shot is recomputed around the new actor and blended to, like any other change.
	 */
	UFUNCTION(BlueprintCallable, Category = "FrameVolume")
	void SetFollowTarget(AActor* NewTarget);

	/** The actor the shots are built around, or null if there is none yet. */
	UFUNCTION(BlueprintPure, Category = "FrameVolume")
	AActor* GetFollowTarget() const;

	//~ Overrides ----------------------------------------------------------------------------------------

	/**
	 * Take the camera with a shot that is in no volume and no table - a boss reveal, a death cam, a
	 * cutscene beat.
	 *
	 * An override beats every volume regardless of priority, and blends in and out exactly like one. It
	 * is a single slot rather than a stack, again on purpose: two systems both pushing an override is a
	 * bug in the game, and it should show up as the second one winning rather than as a queue that has to
	 * be unwound in the right order.
	 */
	UFUNCTION(BlueprintCallable, Category = "FrameVolume")
	void PushOverride(const FFrameVolumeRow& Row);

	/** Hand the camera back to the volumes, or to the game if the actor is in none. */
	UFUNCTION(BlueprintCallable, Category = "FrameVolume")
	void ClearOverride();

	/** True while an override is in force. */
	UFUNCTION(BlueprintPure, Category = "FrameVolume")
	bool IsOverrideActive() const { return bOverrideActive; }

	//~ Blend --------------------------------------------------------------------------------------------

	/** How far the blend in flight has got, 0..1. 1 means the camera has arrived. */
	UFUNCTION(BlueprintPure, Category = "FrameVolume")
	float GetBlendAlpha() const { return BlendAlpha; }

	/** The shot the camera is on the way to, in world space. */
	UFUNCTION(BlueprintPure, Category = "FrameVolume")
	FFrameCameraState GetTargetState() const { return TargetState; }

	/** What the camera is doing right now, blend included. */
	UFUNCTION(BlueprintPure, Category = "FrameVolume")
	FFrameCameraState GetCurrentState() const;

	/**
	 * Force every blend to this many seconds, ignoring the rows. Negative turns the forcing off.
	 *
	 * This is what Frame.Blend sets and what the demo's blend-time button calls. It is a debugging and
	 * demonstration tool, not a design one - the design lives in the rows.
	 */
	UFUNCTION(BlueprintCallable, Category = "FrameVolume")
	void SetBlendTimeOverride(float Seconds);

	/** The forced blend time, or a negative number when the rows are in charge. */
	UFUNCTION(BlueprintPure, Category = "FrameVolume")
	float GetBlendTimeOverride() const { return BlendTimeOverride; }

	/**
	 * Force every shot to this field of view. Zero or negative gives the rows control again.
	 *
	 * A whole-game setting rather than a per-shot one: accessibility options that widen the view, and the
	 * tight-versus-wide comparison a level designer wants to make across a run without editing a table.
	 * It goes through the blend like everything else, so switching it is a move rather than a pop.
	 */
	UFUNCTION(BlueprintCallable, Category = "FrameVolume")
	void SetFieldOfViewOverride(float Degrees);

	/** The forced field of view, or a non-positive number when the rows are in charge. */
	UFUNCTION(BlueprintPure, Category = "FrameVolume")
	float GetFieldOfViewOverride() const { return FieldOfViewOverride; }

	//~ Switches -----------------------------------------------------------------------------------------

	/** Turn camera control on or off without touching the level. Blends out cleanly when switched off. */
	UFUNCTION(BlueprintCallable, Category = "FrameVolume")
	void SetEnabled(bool bInEnabled);

	/** True while the plugin is allowed to write to the camera. */
	UFUNCTION(BlueprintPure, Category = "FrameVolume")
	bool IsEnabled() const { return bEnabled; }

	/** Turn collision avoidance off for every shot, whatever the rows say. */
	UFUNCTION(BlueprintCallable, Category = "FrameVolume")
	void SetCollisionAvoidanceEnabled(bool bInEnabled);

	/** True while collision avoidance is globally allowed. Rows can still switch it off individually. */
	UFUNCTION(BlueprintPure, Category = "FrameVolume")
	bool IsCollisionAvoidanceEnabled() const { return bCollisionAvoidanceEnabled; }

	/** Draw the volumes, the target point and the sweep in the world. */
	UFUNCTION(BlueprintCallable, Category = "FrameVolume")
	void SetDebugDraw(bool bInEnabled);

	/** True while debug drawing is on. */
	UFUNCTION(BlueprintPure, Category = "FrameVolume")
	bool IsDebugDrawEnabled() const { return bDebugDraw; }

	//~ Numbers ------------------------------------------------------------------------------------------

	/** Everything the counters box draws, measured this tick. */
	const FFrameVolumeStats& GetStats() const { return Stats; }

	/** Blueprint copy of the counters. */
	UFUNCTION(BlueprintPure, Category = "FrameVolume", meta = (DisplayName = "Get Frame Volume Stats"))
	FFrameVolumeStats GetStatsCopy() const { return Stats; }

	/** Print the counters to the log. Frame.Stats. */
	void LogStats() const;

	/** Print every volume in the world, with priority, row and whether it is occupied. Frame.List. */
	void LogVolumeList() const;

	//~ Events -------------------------------------------------------------------------------------------

	/** The volume owning the camera changed. */
	UPROPERTY(BlueprintAssignable, Category = "FrameVolume|Events")
	FFrameVolumeActiveChanged OnActiveVolumeChanged;

	/** The followed actor stepped into a volume. */
	UPROPERTY(BlueprintAssignable, Category = "FrameVolume|Events")
	FFrameVolumeOccupancyChanged OnVolumeEntered;

	/** The followed actor stepped out of a volume. */
	UPROPERTY(BlueprintAssignable, Category = "FrameVolume|Events")
	FFrameVolumeOccupancyChanged OnVolumeExited;

	//~ Camera hand-off ----------------------------------------------------------------------------------

	/**
	 * Called by the camera modifier, once per rendered frame.
	 *
	 * GameState is what the game's own camera worked out for this frame; OutState is what the camera
	 * should do instead. Returns false when the plugin has nothing to say, and the point of view is then
	 * left untouched rather than overwritten with a copy of itself.
	 *
	 * The blend *out* is evaluated here rather than on the tick for one reason: its target is the game's
	 * camera, and the game's camera is only known at this moment. Evaluating it a tick later would blend
	 * towards where the game camera was last frame, which on a moving character is a visible lag in
	 * exactly the moment the plugin is supposed to be getting out of the way.
	 */
	bool ApplyToCamera(const FFrameCameraState& GameState, FFrameCameraState& OutState);

	/** Draw the counters box on any HUD. Used by AFrameVolumeHUD and by the optional global hook. */
	void DrawStatsBox(UCanvas* Canvas, const FVector2D& Origin, float Width) const;

	/** How many lines tall the counters box is right now. Grows with the number of overlapping volumes. */
	int32 GetStatsLineCount() const;

private:
	//~ Update steps -------------------------------------------------------------------------------------

	/** Copy the project settings into the fields the tick reads. */
	void ApplySettings();

	/** Resolve the followed actor, re-acquiring the local pawn if that is allowed. */
	void UpdateFollowTarget();

	/** Rebuild the inside set and fire enter/exit. */
	void UpdateOccupancy();

	/** Pick the shot that wins and re-target the blend if it changed. */
	void UpdateActiveShot(float DeltaTime);

	/** Turn a row into a world-space shot around the pivot. */
	FFrameCameraState EvaluateShot(const FFrameVolumeRow& Row, const AFrameVolume* Volume, FVector& OutPivot) const;

	/** Move the camera in until nothing is between it and the pivot. Returns the distance taken off. */
	float ApplyCollisionAvoidance(const FFrameVolumeRow& Row, const FVector& Pivot, FFrameCameraState& InOutState) const;

	/** Alpha put through the row's curve. */
	float CurveAlpha(float RawAlpha, EFrameBlendCurve Curve, const TSoftObjectPtr<UCurveFloat>& CustomCurve) const;

	/** Restart the blend from wherever it visibly got to. */
	void RetargetBlend(const FFrameCameraState& FromState, float Duration, EFrameBlendCurve Curve, const TSoftObjectPtr<UCurveFloat>& CustomCurve);

	/** Find, or make, the camera modifier on the local player's camera manager. */
	void EnsureCameraModifier();

	/** Take the modifier off the camera manager again. */
	void RemoveCameraModifier();

	/** The pivot point the shots are built around, or false when there is no followed actor. */
	bool GetPivot(const FFrameVolumeRow& Row, FVector& OutPivot) const;

	/** Register or unregister the optional draw-on-any-HUD hook. */
	void RebindHudDelegate();

	/** The optional draw-on-any-HUD hook. */
	void OnAnyHUDPostRender(AHUD* HUD, UCanvas* Canvas);

	/** Draw the volumes, the target and the sweep. Compiled out of Shipping by the engine's own guard. */
	void DrawDebug() const;

	//~ Registry -----------------------------------------------------------------------------------------

	/** Every volume in the world. Weak, so a streamed-out level cannot keep one alive. */
	TArray<TWeakObjectPtr<AFrameVolume>> Volumes;

	/** The volumes the followed actor is in, highest priority first. */
	TArray<TWeakObjectPtr<AFrameVolume>> InsideVolumes;

	/** The volume that owns the camera. Null while an override holds it, and while nothing does. */
	TWeakObjectPtr<AFrameVolume> ActiveVolume;

	/** True while the shot driving the camera came from PushOverride rather than from a volume. */
	bool bActiveIsOverride = false;

	/** True while something - a volume or an override - is framing. False means the blend is on its way out. */
	bool bHasActiveShot = false;

	/** The actor the shots are built around. */
	TWeakObjectPtr<AActor> FollowTarget;

	/** True while the followed actor was set from Blueprint rather than found automatically. */
	bool bFollowTargetIsExplicit = false;

	//~ Blend state --------------------------------------------------------------------------------------

	/** Where the blend started. Replaced by the reached state whenever the target changes. */
	FFrameCameraState CurrentState;

	/** Where the blend is going. Recomputed every tick, because the actor it is built around moves. */
	FFrameCameraState TargetState;

	/** What the game's own camera did on the last rendered frame. */
	FFrameCameraState LastGameState;

	/** True once LastGameState holds something the modifier actually gave us. */
	bool bHasGameState = false;

	/** True while the plugin has a state to write. False means the camera is entirely the game's. */
	bool bControlling = false;

	/** True while the blend's destination is the game camera rather than a shot. */
	bool bReleasing = false;

	/** 0..1. */
	float BlendAlpha = 1.0f;

	/** Seconds the blend in flight lasts. */
	float BlendDuration = 0.0f;

	/** Seconds elapsed in the blend in flight. */
	float BlendElapsed = 0.0f;

	/** Curve of the blend in flight. */
	EFrameBlendCurve ActiveBlendCurve = EFrameBlendCurve::EaseInOut;

	/** Custom curve of the blend in flight, if it has one. */
	TSoftObjectPtr<UCurveFloat> ActiveCustomCurve;

	/** The row driving the camera this tick. */
	FFrameVolumeRow ActiveRow;

	/** The pivot the active shot was built around, kept for the sweep and the debug draw. */
	FVector ActivePivot = FVector::ZeroVector;

	//~ Override -----------------------------------------------------------------------------------------

	bool bOverrideActive = false;
	FFrameVolumeRow OverrideRow;

	//~ Switches -----------------------------------------------------------------------------------------

	bool bEnabled = true;
	bool bCollisionAvoidanceEnabled = true;
	bool bDebugDraw = false;
	bool bAutoFollowLocalPlayer = true;
	bool bReacquireFollowTarget = true;
	bool bAutoDrawStatsOnAnyHUD = false;
	bool bTraceComplex = false;
	float ReleaseBlendTime = 0.5f;
	EFrameBlendCurve ReleaseBlendCurve = EFrameBlendCurve::EaseInOut;
	TEnumAsByte<ECollisionChannel> CollisionChannel = ECC_Camera;
	int32 MaxVolumesPerUpdate = 256;
	float UpdatesPerSecond = 0.0f;

	/** Negative means the rows decide. */
	float BlendTimeOverride = -1.0f;

	/** Non-positive means the rows decide. */
	float FieldOfViewOverride = -1.0f;

	/** Seconds owed to the update budget when UpdatesPerSecond is not zero. */
	float UpdateAccumulator = 0.0f;

	//~ Plumbing -----------------------------------------------------------------------------------------

	/** The modifier we put on the local player's camera manager. */
	TWeakObjectPtr<UFrameVolumeCameraModifier> Modifier;

	/** The camera manager the modifier sits on, so it can be taken off again. */
	TWeakObjectPtr<APlayerCameraManager> ModifierOwner;

	/** Handle for the optional draw-on-any-HUD hook. */
	FDelegateHandle HudPostRenderHandle;

	/** Frame the counters box was last drawn on, so the two draw paths cannot stack. */
	mutable uint64 LastStatsDrawFrame = 0;

	/** Everything the box reads. */
	FFrameVolumeStats Stats;

	/** Scratch for the occupancy diff, kept between ticks so the update allocates nothing. */
	TArray<TWeakObjectPtr<AFrameVolume>> PreviousInside;
};
