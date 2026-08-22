// Copyright 2026 Silvan Teufel. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "Engine/DeveloperSettings.h"
#include "FrameVolumeTypes.h"
#include "FrameVolumeSettings.generated.h"

/**
 * Project-wide defaults for FrameVolume, under Project Settings -> Plugins -> FrameVolume.
 *
 * Everything here is a default or a ceiling, never a per-shot value: what a particular volume does is in
 * its DataTable row, because that is the thing a designer changes twenty times an afternoon. What is
 * here is what a project decides once - which channel the sweep uses, whether the camera is taken over
 * at all, how big the counters box is.
 */
UCLASS(config = Game, defaultconfig, meta = (DisplayName = "FrameVolume"))
class FRAMEVOLUME_API UFrameVolumeSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UFrameVolumeSettings();

	//~ UDeveloperSettings interface
	virtual FName GetCategoryName() const override;
	virtual FName GetSectionName() const override;

	/** The settings object, never null. */
	static const UFrameVolumeSettings& Get();

	//~ Master switch ------------------------------------------------------------------------------------

	/**
	 * Let volumes take the camera at all.
	 *
	 * Off leaves the subsystem running - volumes still register, entry and exit still fire, the counters
	 * box still fills in - but nothing is written to the camera. That is what a project wants while it is
	 * bisecting a camera problem, and it is a far smaller change than ripping the volumes out of a map.
	 */
	UPROPERTY(config, EditAnywhere, Category = "General")
	bool bEnabled = true;

	/**
	 * Follow the local player's pawn without being told to.
	 *
	 * On means a map with volumes in it works the moment it is played, with no Blueprint at all. Off is
	 * for games where the camera follows something that is not the pawn - a ball, a vehicle the player
	 * is not in, a squad's centre of mass - and SetFollowTarget is called once at startup.
	 */
	UPROPERTY(config, EditAnywhere, Category = "General")
	bool bAutoFollowLocalPlayer = true;

	/**
	 * Re-check the followed pawn every tick while there is none.
	 *
	 * Costs one weak-pointer test per tick and covers respawns, late possession and seamless travel. Off
	 * only resolves the pawn once.
	 */
	UPROPERTY(config, EditAnywhere, Category = "General", meta = (EditCondition = "bAutoFollowLocalPlayer"))
	bool bReacquireFollowTarget = true;

	//~ Defaults -----------------------------------------------------------------------------------------

	/**
	 * The shot used when a volume has no row, an unresolvable row, or an empty handle.
	 *
	 * A volume with a broken reference frames something plausible instead of dropping the camera into the
	 * floor, and the counters box says which row it failed to find. A silent fallback that looks right is
	 * worth more than a correct crash on the day a DataTable is renamed.
	 */
	UPROPERTY(config, EditAnywhere, Category = "Defaults")
	FFrameVolumeRow FallbackShot;

	/**
	 * Seconds to blend back to the game camera on leaving the last volume.
	 *
	 * Separate from any row's blend time on purpose - the row describes getting into a shot, and getting
	 * out of one is a different move that usually wants to be quicker.
	 */
	UPROPERTY(config, EditAnywhere, Category = "Defaults",
		meta = (ClampMin = "0.0", UIMax = "5.0", ForceUnits = "s"))
	float ReleaseBlendTime = 0.5f;

	/** Curve used for the blend back to the game camera. */
	UPROPERTY(config, EditAnywhere, Category = "Defaults")
	EFrameBlendCurve ReleaseBlendCurve = EFrameBlendCurve::EaseInOut;

	//~ Collision ----------------------------------------------------------------------------------------

	/** Channel the collision-avoidance sweep is traced on. */
	UPROPERTY(config, EditAnywhere, Category = "Collision")
	TEnumAsByte<ECollisionChannel> CollisionChannel = ECC_Camera;

	/** Trace against complex collision. Off - the simple shapes - is what a camera sweep wants. */
	UPROPERTY(config, EditAnywhere, Category = "Collision")
	bool bTraceComplex = false;

	//~ Budget -------------------------------------------------------------------------------------------

	/**
	 * How many volumes may be point-tested per tick.
	 *
	 * The test is a bounding-box compare first and an exact brush query only for the few that pass, so
	 * the real cost is far below this number. The ceiling exists so that a map that accidentally holds a
	 * thousand volumes degrades into ignoring some rather than into a frame spike.
	 */
	UPROPERTY(config, EditAnywhere, Category = "Budget", meta = (ClampMin = "1", UIMax = "512"))
	int32 MaxVolumesPerUpdate = 256;

	/**
	 * Updates per second. 0 means every frame.
	 *
	 * A camera that is blending wants every frame. This is here for the projects that would rather spend
	 * the sweep on something else and can live with a camera that resolves at 30 Hz.
	 */
	UPROPERTY(config, EditAnywhere, Category = "Budget", meta = (ClampMin = "0.0", UIMax = "120.0"))
	float UpdatesPerSecond = 0.0f;

	//~ Presentation -------------------------------------------------------------------------------------

	/** Start with the counters box on. */
	UPROPERTY(config, EditAnywhere, Category = "Presentation")
	bool bShowStatsByDefault = true;

	/**
	 * Draw the counters box through AHUD::OnHUDPostRender as well, so a project keeps its own HUD class.
	 *
	 * AFrameVolumeHUD exists for projects that have no HUD of their own. This is for the far more common
	 * case where a project already has one and is not about to reparent it to see a camera's numbers.
	 */
	UPROPERTY(config, EditAnywhere, Category = "Presentation")
	bool bAutoDrawStatsOnAnyHUD = false;

	/** Start with the debug drawing on. Equivalent to Frame.Debug 1 at startup. */
	UPROPERTY(config, EditAnywhere, Category = "Presentation")
	bool bDebugDrawByDefault = false;
};
