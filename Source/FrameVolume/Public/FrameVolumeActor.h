// Copyright 2026 Simulated Flow. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "GameFramework/Volume.h"
#include "FrameVolumeTypes.h"
#include "FrameVolumeActor.generated.h"

class UBoxComponent;
class USceneComponent;
class USplineComponent;

/**
 * A piece of the level that says how the camera sees it.
 *
 * Drag one in, brush it to the shape of the room, point it at a DataTable row, and the camera takes that
 * framing while the followed actor is inside. Nothing else has to be wired: no Blueprint, no trigger, no
 * camera actor in the map. Overlap two of them and the higher priority wins, and the change between them
 * is blended rather than cut.
 *
 * Three things are worth knowing about how it is put together.
 *
 * **It does not tick.** Not one of these has a tick function. The world subsystem does the whole update
 * once per frame for every volume there is, which is why a level with two hundred of them costs the same
 * as a level with two.
 *
 * **It does not use overlap events.** Containment is asked for, not waited for: the subsystem tests the
 * followed actor's position against each volume, cheap bounds first and the exact brush only for the few
 * that pass. That is a deliberate trade. Overlap events are free until the followed actor is something
 * that generates none - a spectator pawn, a camera rig, an actor with collision switched off, an actor
 * teleported straight into the middle of a volume - and then they are silently, unfixably wrong. Asking
 * costs a handful of microseconds and is right for every actor in the engine.
 *
 * **The brush is not the only shape it can have.** A volume placed in the editor has brush geometry and
 * uses it. A volume spawned at runtime has none - a brush is built by the editor, not by SpawnActor - so
 * this class also carries a box, and falls back to it when the brush is empty. That means a volume made
 * from script or placed procedurally works exactly like one brushed by hand.
 */
UCLASS(Blueprintable, meta = (DisplayName = "Frame Volume"), HideCategories = (Navigation, Physics, Collision))
class FRAMEVOLUME_API AFrameVolume : public AVolume
{
	GENERATED_BODY()

public:
	AFrameVolume();

	//~ Shot ---------------------------------------------------------------------------------------------

	/** The DataTable row that describes the shot. Row type FrameVolumeRow. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FrameVolume",
		meta = (RowType = "/Script/FrameVolume.FrameVolumeRow", EditCondition = "!bUseInlineShot"))
	FDataTableRowHandle Shot;

	/**
	 * Ignore the DataTable and use InlineShot instead.
	 *
	 * For the one volume in a level that is a special case, and for prototyping before there is a table
	 * worth making. Everything else should point at a row, or the plugin's whole argument is lost.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FrameVolume")
	bool bUseInlineShot = false;

	/** The shot, written on this volume rather than in a table. Only read when bUseInlineShot is set. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FrameVolume", meta = (EditCondition = "bUseInlineShot"))
	FFrameVolumeRow InlineShot;

	/** Raise or lower this one volume's priority without touching the row every other volume shares. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FrameVolume")
	bool bOverridePriority = false;

	/** Priority used when bOverridePriority is set. Higher wins. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FrameVolume", meta = (EditCondition = "bOverridePriority"))
	int32 PriorityOverride = 0;

	/** Take this volume out of the running without deleting it or moving it out of the level. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FrameVolume")
	bool bVolumeEnabled = true;

	//~ Components ---------------------------------------------------------------------------------------

	/**
	 * Where an Anchor-mode shot stands. Move it in the viewport to frame the shot by eye.
	 *
	 * Also drawn as the camera's home when debug drawing is on, whatever the mode, so it is obvious at a
	 * glance which volume is which.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "FrameVolume")
	TObjectPtr<USceneComponent> Anchor;

	/**
	 * The camera rail for a Rail-mode shot.
	 *
	 * Empty by default - a spline with no points is skipped and the shot falls back to an orbit, so the
	 * component costs nothing on the volumes that do not use it. Give it points and the camera rides the
	 * nearest point on it, which is a projection rather than a follow: the camera slides along the rail
	 * as the actor walks past it and never leaves the line the rail draws.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "FrameVolume")
	TObjectPtr<USplineComponent> Rail;

	/**
	 * Fallback shape, used when this volume has no brush geometry.
	 *
	 * A volume placed in the editor has a brush and this box is ignored. A volume spawned from script has
	 * no brush at all, and without this it would contain nothing, ever, silently.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FrameVolume")
	TObjectPtr<UBoxComponent> FallbackBounds;

	//~ Queries ------------------------------------------------------------------------------------------

	/**
	 * Resolve this volume's shot: inline, DataTable row, or the project's fallback.
	 *
	 * Returns false when it had to fall back, and OutRow is filled in either way - a caller that does not
	 * care about the difference can ignore the result and still have a usable shot.
	 */
	UFUNCTION(BlueprintCallable, Category = "FrameVolume")
	bool GetShot(FFrameVolumeRow& OutRow) const;

	/** The priority this volume runs at: the override if set, otherwise the row's. */
	UFUNCTION(BlueprintPure, Category = "FrameVolume")
	int32 GetEffectivePriority() const;

	/** Name of the row this volume resolved to, or None for an inline or unresolved shot. */
	UFUNCTION(BlueprintPure, Category = "FrameVolume")
	FName GetShotRowName() const;

	/**
	 * Is this world position inside the volume?
	 *
	 * The exact brush test when there is a brush, the box when there is not. Cheap enough to call every
	 * frame - which is exactly what the subsystem does.
	 */
	UFUNCTION(BlueprintPure, Category = "FrameVolume")
	bool ContainsPoint(const FVector& WorldPoint) const;

	/** True while the followed actor is inside this volume. */
	UFUNCTION(BlueprintPure, Category = "FrameVolume")
	bool IsOccupied() const;

	/** True while this volume is the one driving the camera. */
	UFUNCTION(BlueprintPure, Category = "FrameVolume")
	bool IsActiveVolume() const;

	/** True when the rail has enough points to ride. */
	UFUNCTION(BlueprintPure, Category = "FrameVolume")
	bool HasUsableRail() const;

	/** World-space bounds used for the cheap rejection pass. */
	FBox GetQueryBounds() const;

	/** True when the brush carries real collision geometry, so the exact test is available. */
	bool HasBrushGeometry() const;

protected:
	//~ AActor interface
	virtual void PostInitializeComponents() override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	/** Register with the world subsystem, once, whichever of the two entry points gets here first. */
	void RegisterWithSubsystem();

	/** True once this volume has told the subsystem it exists. */
	bool bRegistered = false;
};
