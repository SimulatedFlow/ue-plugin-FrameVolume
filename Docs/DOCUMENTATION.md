# FrameVolume — Documentation

**Camera framing volumes with priority and blending.**

| | |
|---|---|
| **Engine** | Unreal Engine **5.8** |
| **Type** | Code plugin, C++ with full Blueprint surface |
| **Modules** | One: `FrameVolume` (Runtime, `LoadingPhase: PreDefault`) |
| **Editor module** | None — everything in the plugin ships |
| **Dependencies** | `Core`, `CoreUObject`, `Engine`, `DeveloperSettings` (+ `RenderCore`, private). No Niagara, no Chaos, no UMG, no third-party code. |
| **Network** | Not replicated (client-side presentation) |
| **Built & verified on** | Win64 — `Build.bat` (Development Editor) and `RunUAT BuildPlugin -Rocket -StrictIncludes` (Development + Shipping), zero warnings |
| **Enabled in descriptor** | Win64, Mac, Linux |
| **Version** | 1.0.0 |

---

## Table of contents

1. [What it is](#1-what-it-is)
2. [Installation](#2-installation)
3. [Quick start — five minutes](#3-quick-start--five-minutes)
4. [Concepts](#4-concepts)
5. [The volume — `AFrameVolume`](#5-the-volume--aframevolume)
6. [Framing modes](#6-framing-modes)
7. [Collision avoidance](#7-collision-avoidance)
8. [The followed actor](#8-the-followed-actor)
9. [Overrides](#9-overrides)
10. [Class and API overview](#10-class-and-api-overview)
11. [Code examples](#11-code-examples)
12. [Project settings](#12-project-settings)
13. [The counters box](#13-the-counters-box)
14. [Console commands](#14-console-commands)
15. [Cost](#15-cost)
16. [Supported platforms and engine versions](#16-supported-platforms-and-engine-versions)
17. [Troubleshooting](#17-troubleshooting)
18. [Limitations, stated plainly](#18-limitations-stated-plainly)
19. [Support](#19-support)

---

## 1. What it is

A **Frame Volume** is a piece of the level that says how the camera sees it. While the followed actor is
inside, the camera takes the framing described by **one row of a DataTable**. Overlapping volumes are
resolved by priority. Every change is blended, never cut.

Nothing else has to be wired up: no Blueprint, no trigger actor, no camera actor placed in the map. The
product *is* the camera.

It is a **camera modifier, not a view target** — your spring arm, first-person camera or running cinematic
keeps computing its point of view underneath, and when the last volume is left FrameVolume blends back
into it and stops writing entirely.

---

## 2. Installation

### From Fab

1. Install the plugin to the engine from the Epic Games Launcher (*Library → Fab Library → Install to
   Engine*), **or** copy the extracted folder to `<YourProject>/Plugins/FrameVolume`.
2. Open the project. Enable it under *Edit → Plugins → Gameplay → **FrameVolume*** and restart the editor
   when prompted.
3. Done. There is nothing to add to a `Build.cs`, no subsystem to spawn and no actor to place for the
   plugin itself to be live.

### From source (C++ projects)

```
<YourProject>/
  Plugins/
    FrameVolume/
      FrameVolume.uplugin
      Source/FrameVolume/...
      Content/FrameVolume/...      <- demo map, DataTable, demo Blueprints
      Resources/Icon128.png
```

Right-click the `.uproject` → *Generate Visual Studio project files*, then build. The module loads at
`PreDefault`, so a volume in a map that is loaded on startup can take the camera on the very first frame.

### Using it from C++

Add the module to your own module's dependencies:

```csharp
// YourGame.Build.cs
PublicDependencyModuleNames.AddRange(new string[] { "FrameVolume" });
```

Blueprint-only projects need no step beyond enabling the plugin — the entire API is on the
**FrameVolume** function library.

### The demo map

`/FrameVolume/FrameVolume/Maps/L_FrameVolumeDemo` is a five-volume promenade (arena, corridor, doorway,
overlook, rail) with a self-driving tour pawn, a Canvas counters box and a clickable panel of switches.
Open it and press Play — every claim in this document is visible in it without touching a setting.

---

## 3. Quick start — five minutes

**1 — Make a shot table.**
*Content Browser → Miscellaneous → Data Table*, row structure **FrameVolumeRow**. Call it
`DT_CameraShots`. Add a row named `Corridor`:

| Field | Value |
|---|---|
| Mode | `Orbit` |
| Distance | `450` |
| Pitch | `-10` (negative = camera above, looking down) |
| Yaw | `0` |
| Field Of View | `65` |
| Blend Time | `0.8` |
| Priority | `10` |

**2 — Place a volume.**
Drag a **Frame Volume** from *Place Actors → Volumes* into the level and brush it to the shape of the
room, exactly like a trigger volume.

**3 — Point it at the row.**
In the Details panel, set **Shot** → Data Table `DT_CameraShots`, Row Name `Corridor`.

**4 — Play and walk in.**
The camera takes the framing over 0.8 seconds. That is the whole setup.

**5 — See the numbers (optional but recommended).**
Set the map's HUD class to **Frame Volume HUD** (*World Settings → Game Mode → HUD Class*), or — if the
project already has a HUD it does not want to reparent — turn on *Project Settings → Plugins →
FrameVolume → **Auto Draw Stats On Any HUD***. Then type `Frame.Debug 1` in the console to see the
volumes, the pivot, the target shot and the collision sweep drawn in the world.

**6 — Add a second volume that overlaps the first**, give its row `Priority 20`, and walk through both.
The counters box lists both; the higher priority owns the camera; stepping out of it blends down to the
other.

---

## 4. Concepts

### 4.1 The shot

A **shot** is where the camera is, where it looks and how wide — `FFrameCameraState` (location, rotation,
field of view). It is the only thing that is ever blended.

### 4.2 The row

`FFrameVolumeRow` is a shot as a DataTable row. Two volumes at opposite ends of a level point at the same
row; changing the row changes both, and neither map has to be opened to do it.

| Field | Meaning |
|---|---|
| `Mode` | `Orbit`, `Anchor` or `Rail`. |
| `Distance` | Centimetres from the pivot. Orbit only. Default `600`. |
| `Pitch` | The camera's **view pitch** in degrees. Negative looks down, which places the camera *above* the pivot — the default `-15` is the usual over-the-shoulder-ish tilt. Positive looks up and places the camera below. Orbit only. |
| `Yaw` | Degrees around the pivot. Orbit only; see `bYawRelativeToVolume`. |
| `bYawRelativeToVolume` | Measure `Yaw` against the volume's own rotation instead of world north. **On by default** — this is what makes one row reusable in four corridors facing four directions. Turn it off when a row means a compass direction. |
| `FieldOfView` | Degrees, horizontal. Default `75`. |
| `PivotOffset` | Offset from the followed actor's origin to the point the shot is built around. Default `(0,0,60)` — roughly chest height, because framing a capsule's feet is not what anyone means by "look at the player". |
| `bLookAtPivot` | Anchor and Rail only. Off keeps the anchor's own rotation, or the spline's tangent — a fixed security-camera angle, or a dolly that stares straight ahead. |
| `RotationOffset` | Added after everything else. Small tilts and lead room. |
| `BlendTime` | Seconds to reach this shot. `0` is a cut. Default `0.75`. |
| `BlendCurve` | `Linear`, `EaseIn`, `EaseOut`, `EaseInOut` (default) or `Custom`. |
| `CustomCurve` | Soft pointer to a `UCurveFloat`, read over 0..1. Soft on purpose: a project with two hundred rows and no custom curves loads no curve assets at all. |
| `bAvoidCollision` | Sweep from the pivot to the camera and stop at the first blocker. |
| `CollisionProbeRadius` | Sweep radius in cm. The camera keeps at least this far off any surface. Default `14`. |
| `CollisionPullIn` | Extra cm off the surface after a hit, so a near-parallel wall cannot clip. Default `6`. |
| `MinCollisionDistance` | Never pull closer to the pivot than this, however hard the geometry pushes. Default `60`. |
| `Priority` | Higher wins where volumes overlap. |

### 4.3 Priority resolution

Every frame the subsystem builds the set of volumes the followed actor is standing in and sorts it:

1. **Higher `Priority` first** — the volume's `PriorityOverride` if it sets one, otherwise the row's.
2. **Equal priority → the smaller volume wins.** A designer who nests a small volume inside a large one
   without setting priorities means the inner one: the small volume around the doorway is the specific
   case inside the big volume around the hall.

The winner owns the camera. The counters box lists **all** of them, not just the winner, so overlap
resolution is something you can see rather than something you have to trust.

### 4.4 The blend — one, not a stack

There is one current shot, one target shot and one alpha. When the target changes mid-blend:

```
CurrentState  = <the shot the blend had visibly reached>
BlendAlpha    = 0
BlendDuration = <the new row's BlendTime>
```

Nothing is queued, so nothing accumulates. The camera always leaves from where it *is*, never from where a
finished blend would have put it. Walk in and out of a doorway ten times in two seconds and nothing
overshoots and nothing arrives late.

The target shot itself is **recomputed every frame**, because the actor it is built around moves. That is
what makes a blend into an orbit shot arrive smoothly at a moving character rather than at the spot the
character was standing when the blend started.

Rotation is interpolated as a **quaternion slerp**, not a rotator lerp — a rotator lerp takes the long way
round whenever a yaw crosses 180°, which on a camera reads as a full spin in the middle of a calm move.

### 4.5 Leaving every volume

The blend out has its own length and curve (*Release Blend Time* / *Release Blend Curve* in the project
settings), because getting out of a shot is a different move from getting into one and usually wants to be
quicker. Its destination is the game's own camera, sampled on the frame it is used — so it aims at where
the spring arm is *now*, not where it was last tick. When the blend finishes, the plugin stops writing to
the camera entirely.

### 4.6 How the camera is taken

Through a `UCameraModifier` (`UFrameVolumeCameraModifier`), added automatically to the local player's
camera manager the first time it is needed and removed when the world goes away. **Not** a view target.

That is why the plugin drops into an existing project without an argument about who owns the camera: a
spring arm, a first-person camera or a running cinematic keeps computing its point of view underneath,
FrameVolume is handed that point of view and hands one back, and when it is done it hands back exactly
what it was given. Camera shakes still read on top — the modifier runs in priority band 0, shakes run
after it.

---

## 5. The volume — `AFrameVolume`

`AFrameVolume` derives from `AVolume`. Brush it to the shape of the room like any trigger volume.

| Property | Meaning |
|---|---|
| `Shot` | The DataTable row handle. Row type `FrameVolumeRow`. |
| `bUseInlineShot` / `InlineShot` | Ignore the table and write the shot on this volume. For the one special case in a level, and for prototyping before there is a table worth making. |
| `bOverridePriority` / `PriorityOverride` | Raise or lower this one volume without touching the row every other volume shares. |
| `bVolumeEnabled` | Take it out of the running without deleting it or moving it out of the level. |
| `Anchor` | Where an `Anchor`-mode shot stands. Move it in the viewport to frame the shot by eye. Also drawn as the volume's home in debug draw, whatever the mode. |
| `Rail` | The spline a `Rail`-mode shot rides. Starts with **no points** — give it at least two to make it usable. Costs nothing on the volumes that do not use it. |
| `FallbackBounds` | Shape used when the volume has no brush geometry (see 5.2). |

### 5.1 Containment is asked for, not waited for

The volume registers **no overlap events**. Every frame the subsystem tests the followed actor's position:
a world-space bounds compare first (six floats), and the exact brush query only for the few that pass.

This is deliberate. Overlap events are free right up until the followed actor is something that generates
none — a spectator pawn, a camera rig, an actor with collision switched off, an actor teleported straight
into the middle of a volume — and then they are silently, unfixably wrong. Asking costs a handful of
microseconds and is correct for every actor in the engine.

### 5.2 Brush or box

A volume placed in the editor has brush geometry and uses it. A volume **spawned at runtime** may have
none — a brush is built by the editor's brush builder, not by `SpawnActor` — so the class also carries a
`FallbackBounds` box and uses that when the brush is empty. Volumes created from script or placed
procedurally therefore behave like hand-brushed ones.

### 5.3 A missing row is not a crash

A volume with an empty handle, a deleted table or a renamed row falls back to *Project Settings → Plugins
→ FrameVolume → **Fallback Shot***: a plain, unremarkable third-person shot. `Frame.List` marks it
`[fallback shot]`. A silent fallback that looks right beats a correct crash on the day a table is renamed
— but it is on the list, so it cannot hide.

---

## 6. Framing modes

### Orbit

```
ViewRotation = (Pitch, Yaw [+ volume yaw if bYawRelativeToVolume], 0)
Camera       = Pivot − ViewRotation.Vector() × Distance,  looking along ViewRotation
```

The everyday case. `bYawRelativeToVolume` adds the volume's own yaw, so one row is reusable across
differently oriented rooms.

> **Sign of `Pitch`:** `Pitch` is the camera's look direction, so **negative pitch puts the camera above
> the pivot looking down** and positive puts it below looking up. The shipped default `-15` is a gentle
> downward tilt.

### Anchor

The camera stands at the volume's `Anchor` component and, by default (`bLookAtPivot`), looks at the pivot.
Frame it by dragging the component in the viewport. With `bLookAtPivot` off it keeps the anchor's own
rotation — a fixed security-camera angle.

### Rail

The camera sits at the point on the volume's `Rail` spline **nearest the pivot**
(`FindLocationClosestToWorldLocation`). This is a **projection, not a follow**: the camera slides along
the rail as the actor walks past and can never leave the line the designer drew. One spline query per
frame.

A `Rail` row on a volume with fewer than two spline points, or an `Anchor` row on a volume with no anchor,
falls back to `Orbit`. Falling back to a shot that works beats framing the inside of the floor.

---

## 7. Collision avoidance

One sphere sweep per frame, from the pivot out to the camera, on the channel set in the project settings
(`ECC_Camera` by default). On a hit the camera moves in to the hit point minus `CollisionPullIn`, never
closer than `MinCollisionDistance`.

Initial overlaps are discarded on purpose: a character standing in a bush or half inside a doorframe keeps
its shot instead of having the camera slammed onto its own head.

Off is a real option, not a degraded one — an anchor camera bolted to a ceiling corner has nothing to
collide with and should not pay for a trace to find that out sixty times a second.

The counters box reports `Collision IN … (pulled … cm)` on the frames the sweep engages, so "is the wall
doing this?" is a question with an answer on screen.

---

## 8. The followed actor

By default the local player's pawn, re-acquired if it is destroyed (respawn, late possession, seamless
travel) while `bReacquireFollowTarget` is on.

`SetFollowTarget` points the shots at something else — a ball, a vehicle the player is not in, a squad's
centre of mass. Changing it **does not cut**: the shot is recomputed around the new actor and blended to,
like any other change. Passing `null` puts automatic following back.

---

## 9. Overrides

`PushOverride(Shot)` takes the camera with a shot that is in no volume and no table — a boss reveal, a
death cam, a cutscene beat. It beats every volume regardless of priority and blends in and out like one.
`ClearOverride()` hands the camera back to the volumes, or to the game if the actor is in none.

It is **one slot, not a stack**. A second push replaces the first. Two systems both pushing an override is
a bug in the game, and it should show up as the second one winning rather than as a queue that has to be
unwound in the right order.

`MakeOrbitShot` builds a row in one Blueprint node for exactly this case.

---

## 10. Class and API overview

### C++ classes

| Type | Kind | Role |
|---|---|---|
| `AFrameVolume` | `AVolume` | The volume. Row handle, priority, optional anchor and rail. Does not tick. |
| `UFrameVolumeSubsystem` | `UTickableWorldSubsystem` | The whole runtime: registry, occupancy, priority resolution, blend, collision, stats. Game and PIE only. |
| `UFrameVolumeCameraModifier` | `UCameraModifier` | The one place the plugin touches the engine camera. Added and removed automatically; not Blueprintable. |
| `UFrameVolumeStatics` | `UBlueprintFunctionLibrary` | The whole plugin from Blueprint, world-context-aware. |
| `UFrameVolumeSettings` | `UDeveloperSettings` | Project-wide defaults under *Plugins → FrameVolume*. |
| `AFrameVolumeHUD` | `AHUD` | The Canvas counters box. Optional. |

### Structs and enums

| Type | Role |
|---|---|
| `FFrameVolumeRow` (`FTableRowBase`) | One shot: framing, blend, collision, priority. |
| `FFrameCameraState` | Location + rotation + FOV. The only thing blended. Has a static `Blend(From, To, Alpha)`. |
| `FFrameVolumeStats` | Everything the counters box and `Frame.Stats` read, measured where the work happens. |
| `EFrameCameraMode` | `Orbit`, `Anchor`, `Rail`. |
| `EFrameBlendCurve` | `Linear`, `EaseIn`, `EaseOut`, `EaseInOut`, `Custom`. |

### Blueprint library — `UFrameVolumeStatics`

Every call is world-context-aware and safe in a world that has **no** FrameVolume subsystem: queries
answer nothing, setters do nothing, nobody crashes. A Blueprint written against FrameVolume still runs in
a test map where no volume was ever placed.

**Queries**

| Node | Returns |
|---|---|
| `GetActiveVolume` | The volume owning the camera, or null. |
| `GetBlendAlpha` | 0..1. 1 means the camera has arrived. |
| `IsControllingCamera` | True while the plugin writes to the camera at all. |
| `GetInsideVolumes` | Every volume the followed actor is in, highest priority first. |
| `GetAllVolumes` | Every volume in the world. |
| `GetCurrentCameraState` | What the camera is doing right now, blend included. |
| `GetStats` | The full `FFrameVolumeStats`. |
| `GetVolumeShot` | The row a volume resolves to. False when it had to fall back; the row is filled either way. |

**Framing** — `SetFollowTarget`, `GetFollowTarget`, `PushOverride`, `ClearOverride`, `IsOverrideActive`,
`MakeOrbitShot`.

**Switches** — `SetFrameVolumeEnabled`, `SetBlendTimeOverride`, `SetFieldOfViewOverride`,
`SetCollisionAvoidanceEnabled`, `SetDebugDraw`, and a getter for each.

### Subsystem-only API (C++ and Blueprint via `Get World Subsystem`)

`GetTargetState`, `GetCurrentState`, `SetEnabled`, `SetBlendTimeOverride`, `SetFieldOfViewOverride`,
`IsInsideVolume`, `GetStatsCopy`, `LogStats`, `LogVolumeList`, `DrawStatsBox`, `ApplyToCamera`.

**Events** (multicast, `BlueprintAssignable`):

| Delegate | Signature |
|---|---|
| `OnActiveVolumeChanged` | `(AFrameVolume* New, AFrameVolume* Old)` — either may be null. |
| `OnVolumeEntered` | `(AFrameVolume* Volume)` — fires whether or not that volume wins the camera. |
| `OnVolumeExited` | `(AFrameVolume* Volume)` |

### On the volume itself

`GetShot`, `GetEffectivePriority`, `GetShotRowName`, `ContainsPoint`, `IsOccupied`, `IsActiveVolume`,
`HasUsableRail` — all Blueprint-callable.

---

## 11. Code examples

### 11.1 Reacting to the active volume changing

```cpp
#include "FrameVolumeSubsystem.h"
#include "FrameVolumeActor.h"

void AMyPlayerController::BeginPlay()
{
    Super::BeginPlay();

    if (UFrameVolumeSubsystem* Frame = UFrameVolumeSubsystem::Get(this))
    {
        Frame->OnActiveVolumeChanged.AddDynamic(this, &AMyPlayerController::HandleActiveVolumeChanged);
        Frame->OnVolumeEntered.AddDynamic(this, &AMyPlayerController::HandleVolumeEntered);
    }
}

void AMyPlayerController::HandleActiveVolumeChanged(AFrameVolume* NewVolume, AFrameVolume* OldVolume)
{
    // Either argument may be null: null NewVolume means the game camera has it back.
    const FName Row = NewVolume ? NewVolume->GetShotRowName() : NAME_None;
    UE_LOG(LogTemp, Log, TEXT("Camera now framed by %s (row %s)"),
        NewVolume ? *NewVolume->GetName() : TEXT("the game"), *Row.ToString());
}
```

### 11.2 A boss-reveal override, then hand the camera back

```cpp
#include "FrameVolumeStatics.h"

void AMyBoss::PlayReveal()
{
    FFrameVolumeRow Shot;
    Shot.Mode        = EFrameCameraMode::Orbit;
    Shot.Distance    = 900.0f;
    Shot.Pitch       = -25.0f;   // negative: camera above, looking down
    Shot.Yaw         = 140.0f;
    Shot.FieldOfView = 55.0f;
    Shot.BlendTime   = 1.2f;
    Shot.BlendCurve  = EFrameBlendCurve::EaseInOut;

    // Frame the boss rather than the player for the duration of the beat.
    UFrameVolumeStatics::SetFollowTarget(this, this);
    UFrameVolumeStatics::PushOverride(this, Shot);

    FTimerHandle Handle;
    GetWorldTimerManager().SetTimer(Handle, this, &AMyBoss::EndReveal, 3.0f, false);
}

void AMyBoss::EndReveal()
{
    UFrameVolumeStatics::ClearOverride(this);   // back to the volumes
    UFrameVolumeStatics::SetFollowTarget(this, nullptr); // back to the local pawn
}
```

Both calls blend. Neither cuts.

### 11.3 Spawning a volume from script

A spawned volume has no editor brush, so give `FallbackBounds` an extent — the class falls back to it
automatically.

```cpp
#include "FrameVolumeActor.h"
#include "Components/BoxComponent.h"

AFrameVolume* AMyLevelScript::SpawnFramingVolume(const FVector& Where, const FVector& Extent)
{
    FActorSpawnParameters Params;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    AFrameVolume* Volume = GetWorld()->SpawnActor<AFrameVolume>(Where, FRotator::ZeroRotator, Params);
    if (!Volume)
    {
        return nullptr;
    }

    // One-off shot, no table involved.
    Volume->bUseInlineShot          = true;
    Volume->InlineShot.Mode         = EFrameCameraMode::Orbit;
    Volume->InlineShot.Distance     = 500.0f;
    Volume->InlineShot.FieldOfView  = 70.0f;
    Volume->InlineShot.BlendTime    = 0.6f;
    Volume->InlineShot.Priority     = 25;

    if (Volume->FallbackBounds)
    {
        Volume->FallbackBounds->SetBoxExtent(Extent);
    }
    return Volume;
}
```

Registration with the subsystem happens on `PostInitializeComponents`/`BeginPlay` — there is nothing to
call.

### 11.4 Reading the numbers

```cpp
#include "FrameVolumeSubsystem.h"

void AMyDebugActor::LogCameraState()
{
    const UFrameVolumeSubsystem* Frame = UFrameVolumeSubsystem::Get(this);
    if (!Frame)
    {
        return;
    }

    const FFrameVolumeStats& S = Frame->GetStats();
    UE_LOG(LogTemp, Log, TEXT("%s p%d | inside %d | alpha %.2f | dist %.0f/%.0f | fov %.1f | %s | %.3f ms"),
        S.ActiveVolumeName.IsEmpty() ? TEXT("(game camera)") : *S.ActiveVolumeName,
        S.ActivePriority, S.InsideVolumeNames.Num(), S.CurvedAlpha,
        S.ActualDistance, S.TargetDistance, S.TargetFieldOfView,
        S.bCollisionEngaged ? TEXT("collision IN") : TEXT("clear"),
        S.UpdateMilliseconds);
}
```

### 11.5 Accessibility: a global field-of-view slider

```cpp
// 0 or negative gives the rows control back. The change goes through the blend, so it is a move, not a pop.
UFrameVolumeStatics::SetFieldOfViewOverride(this, WidenedFov);
```

### 11.6 Blueprint

The equivalents, node for node:

| You want | Node |
|---|---|
| React to a framing change | *Get World Subsystem → Frame Volume Subsystem* → bind **On Active Volume Changed** |
| Cutscene camera | **Make Orbit Shot** → **Push Override** … later **Clear Override** |
| Frame a vehicle | **Set Follow Target** (null to go back to the pawn) |
| Compare blend lengths live | **Set Blend Time Override** `0.2` / `2.0`, `-1` to give the rows control |
| Turn the plugin off in an options menu | **Set Frame Volume Enabled** (blends out cleanly, then stops writing) |
| Show the sweep | **Set Debug Draw** |

The demo panel `WBP_FrameVolumeDemoPanel` in `/FrameVolume/FrameVolume/UI/` is a working example of all
six wired to buttons.

---

## 12. Project settings

*Project Settings → Plugins → FrameVolume.* Everything here is a default or a ceiling, never a per-shot
value — what a particular volume does is in its row.

| Setting | Default | Meaning |
|---|---|---|
| `bEnabled` | on | Let volumes take the camera at all. Off keeps everything else running — volumes register, events fire, the counters box fills in — but nothing is written to the camera. That is what a project wants while bisecting a camera problem. |
| `bAutoFollowLocalPlayer` | on | Follow the local pawn without being told to. A map with volumes works the moment it is played, with no Blueprint at all. |
| `bReacquireFollowTarget` | on | Keep looking for the pawn while there is none. Covers respawn, late possession and travel. |
| `FallbackShot` | orbit | Used when a volume's row cannot be resolved. |
| `ReleaseBlendTime` | 0.5 s | Blend back to the game camera on leaving the last volume. |
| `ReleaseBlendCurve` | `EaseInOut` | Curve for that blend. |
| `CollisionChannel` | `ECC_Camera` | Channel the sweep traces on. |
| `bTraceComplex` | off | Simple shapes are what a camera sweep wants. |
| `MaxVolumesPerUpdate` | 256 | Ceiling on volumes point-tested per tick, so a map that accidentally holds a thousand degrades into ignoring some rather than into a frame spike. |
| `UpdatesPerSecond` | 0 (every frame) | Resolve at a fixed rate instead. A blending camera wants every frame; this is for projects that would rather spend the sweep elsewhere. |
| `bShowStatsByDefault` | on | Start with the counters box visible. |
| `bAutoDrawStatsOnAnyHUD` | off | Draw the box through `AHUD::OnHUDPostRender`, so a project keeps its own HUD class. |
| `bDebugDrawByDefault` | off | Equivalent to `Frame.Debug 1` at startup. |

---

## 13. The counters box

`AFrameVolumeHUD` draws it on `UCanvas`. Set it as the map's HUD class, or turn on
`bAutoDrawStatsOnAnyHUD` and keep your own. The two paths know about each other and cannot stack.

It shows: the active volume or `(game camera)`, the row name, priority and mode, **every** volume the
actor is standing in with its priority, blend progress as a number and a bar, blend time and curve, target
and actual distance, collision state and how many centimetres it took off, field of view, rail state and
distance along the rail, the followed actor, and milliseconds per tick.

Position and width are properties on the HUD (`StatsBoxOrigin`, `StatsBoxWidth`), and `ToggleStats()` is
what a STATS button in a widget calls.

**Canvas rather than UMG**, for two reasons pulling the same way. It has to survive a cooked Shipping
build, where `DrawDebug` is compiled out and a debug widget is usually stripped. And anything that has to
be *clicked* belongs in UMG instead: an `AHUD` hit box is tested against
`UGameViewportClient::GetMousePosition()`, which reports nothing on a machine with no mouse attached — a
capture rig, a build agent, a headless test — so the click never lands. Numbers here, controls in a
widget.

---

## 14. Console commands

```
Frame.Stats             print the measured counters to the log
Frame.List              every volume: priority, row, mode, distance, fov, blend, occupancy, active
Frame.Blend [seconds]   force every blend to this length; negative gives the rows control back
Frame.Debug [0|1]       draw the volumes, the pivot, the target shot and the collision sweep
Frame.Collision [0|1]   turn collision avoidance on or off for every shot
```

`Frame.Blend` with no argument prints the current setting rather than changing it. `Frame.Debug` and
`Frame.Collision` with no argument toggle. All five report to the log when there is no FrameVolume
subsystem in the world rather than failing silently.

---

## 15. Cost

One tick of one world subsystem, for the whole world:

- one world-space bounds compare per volume, and the exact brush query only for those that pass
- one sort of the inside set (typically one to three entries)
- one shot evaluated
- at most one sphere sweep
- at most one closest-point-on-spline query

None of that grows with the number of volumes the actor is standing in, and only the first line grows with
the number of volumes in the map. **No volume has a tick function.** The occupancy diff reuses a scratch
array kept between ticks, so a steady-state update allocates nothing.

The measured milliseconds are on the counters box, so the claim is checkable rather than asserted.

---

## 16. Supported platforms and engine versions

| | |
|---|---|
| **Engine version** | Unreal Engine **5.8** |
| **Built and verified** | **Win64** — `RunUAT BuildPlugin -Rocket -StrictIncludes`, Development *and* Shipping configurations, zero warnings |
| **Enabled in the descriptor** | `Win64`, `Mac`, `Linux` (`PlatformAllowList`) |
| **Not built for this release** | Mac and Linux. The code uses no platform-specific API — only `Core`, `CoreUObject`, `Engine`, `DeveloperSettings` and `RenderCore` — so it is expected to build, but it has not been verified and is not claimed. |
| **Target build platforms** | Win64 |
| **Development platforms** | Win64 |
| **Configurations** | Development, DebugGame, Shipping. The counters box and the console commands survive Shipping; `Frame.Debug`'s world drawing is compiled out there by the engine's own `ENABLE_DRAW_DEBUG` guard. |
| **Server builds** | The subsystem is client-side; a dedicated server world has no local player camera manager and nothing is written. |

---

## 17. Troubleshooting

| Symptom | Cause and fix |
|---|---|
| Nothing happens when walking into a volume | *Project Settings → Plugins → FrameVolume → Enabled* is off, or the volume's `bVolumeEnabled` is off. Type `Frame.List` — it prints every volume and whether it is occupied. |
| The camera frames the floor | An `Orbit` row with a positive `Pitch` puts the camera *below* the pivot. Negative pitch looks down from above. |
| A `Rail` row behaves like an orbit | The volume's `Rail` spline has fewer than two points. `HasUsableRail` says so; so does the counters box (`Rail: no`). |
| The counters box is not drawn | The map's HUD class is not `AFrameVolumeHUD` — either set it, or turn on *Auto Draw Stats On Any HUD*. |
| The camera stutters while blending | `UpdatesPerSecond` is above zero. The blend advances on the subsystem tick, so resolving at 30 Hz interpolates at 30 Hz. Set it to `0`. |
| A row's changes do not show up | The volume has `bUseInlineShot` on and is not reading the table at all. |
| The camera clips a wall | `bAvoidCollision` is off on that row, or the wall does not block the `ECC_Camera` channel. `Frame.Debug 1` draws the sweep. |
| Volumes spawned from script contain nothing | Give `FallbackBounds` an extent — a spawned volume has no editor brush (see 11.3). |
| Split screen frames the wrong player | Known limitation: the modifier attaches to the first local player's camera manager. |

---

## 18. Limitations, stated plainly

- **Single local player.** The modifier is attached to the first local player's camera manager. Split
  screen frames player one.
- **No replication.** Camera framing is a client-side presentation concern; nothing here is sent over the
  network.
- **Editor worlds are not supported.** `DoesSupportWorldType` is Game and PIE. The editor viewport camera
  belongs to whoever is building the level, and taking it over while somebody is building is the sort of
  help nobody asked for.
- **`UpdatesPerSecond` above zero makes blends step.** The blend advances on the subsystem tick.
- **No Sequencer integration and no camera shake of its own.** The engine has both; FrameVolume composes
  with them rather than replacing them.
- **Win64 built and verified.** Mac and Linux are enabled in the descriptor but were not built for this
  release.

---

## 19. Support

- **Documentation:** this file and `README.md`
- **Docs / issues:** <https://github.com/SimulatedFlow/ue-plugin-FrameVolume>
- **E-mail:** teufelsilvan@gmail.com

---

© 2026 Silvan Teufel. All Rights Reserved.
