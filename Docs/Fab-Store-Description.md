# FrameVolume — Fab Store Description

**Title:** FrameVolume - Camera Framing Volumes with Priority and Blending
**Price:** $14.99
**Category:** Gameplay / Camera
**Engine:** 5.8
**Licences:** Personal and Professional

---

## Short description

A volume that takes the camera the moment the player walks in. Distance, angle, field of view, blend time
and priority are one row of a DataTable — not eight numbers typed into every volume. Overlapping volumes
resolve by priority, and every change is blended, never cut.

---

## Long description

### One row, not eight numbers per volume

A `Frame Volume` is a piece of the level that says how the camera sees it. Brush it to the shape of the
room, point it at a row of a DataTable, and the camera takes that framing while the player is inside.

Distance, pitch, yaw, field of view, blend time, blend curve and priority are all in that one row. One
row placed in twenty corridors is changed once and changes all twenty — and no map has to be opened to do
it. That is the whole argument for the plugin, and everything else is built around keeping it true.

No Blueprint. No trigger actor. No camera actor placed in the map. Place the volume, pick a row, play.

### There is no blend stack

This is the part that separates it from a weekend implementation.

Systems that queue camera blends drift. Leave a volume mid-blend, walk straight back in, and the queue is
two entries deep: the camera arrives late, or twice, or somewhere nobody asked for.

FrameVolume has exactly one current shot, one target shot and one alpha between them. When the target
changes mid-blend, the current shot is replaced by **the point the blend had visibly reached**, and the
alpha restarts at zero. The camera always leaves from where it actually is. Walk in and out of a doorway
ten times in two seconds and nothing accumulates, nothing overshoots, nothing arrives late.

Rotation blends as a quaternion slerp, so a yaw crossing 180° does not spin the camera the long way round
in the middle of an otherwise calm move.

### Three ways to frame a room

- **Orbit** — a distance and two angles around the player. The everyday case. The yaw can be measured
  against the volume's own rotation, so one row frames four corridors facing four directions correctly.
- **Anchor** — a fixed point in the world, dragged into place in the viewport. The camera stands still
  and turns, or keeps a fixed angle for a security-camera shot.
- **Rail** — a spline the camera rides at the point nearest the player. A projection, not a chase: the
  camera glides along the track as the player walks past and can never leave the line you drew. One
  spline query per frame.

### Overlap, and who wins

Volumes overlap all the time in a real level. Higher priority takes the camera. Equal priorities are
broken by the smaller volume, because the small volume around the doorway is the specific case inside the
big volume around the hall. Step out of the winner and the camera moves to the one underneath — blended.

The statistics box lists **every** volume the player is standing in, not just the winner, so overlap
resolution is something you can see rather than something you have to trust.

### It does not take your camera away

FrameVolume is a camera modifier, not a view target. Your spring arm, your first-person camera, a running
cinematic — all of them keep computing their point of view underneath. The plugin is handed that point of
view and hands one back. When the last volume is left it blends back into whatever your game was doing
and then **stops writing to the camera entirely**.

That is why it drops into an existing project without an argument about who owns the camera. Camera
shakes still read on top.

### Collision avoidance

One optional sphere sweep per frame, from the player out to the camera. Hit something and the camera
moves in front of it. A character standing in a bush keeps its shot — initial overlaps are discarded on
purpose rather than slamming the camera onto the player's head. Switchable per row and globally.

### One tick for the whole world

No volume has a tick function. One world subsystem does the entire update once per frame: a bounds
compare per volume, an exact test only for the few that pass, one sort, one shot, at most one sweep, at
most one spline query. Two hundred volumes cost what two do.

Containment is asked for rather than waited for — no overlap events. That is deliberate: overlap events
are free right up until the followed actor is something that generates none (a spectator pawn, a camera
rig, an actor teleported into the middle of a volume), and then they are silently wrong. Asking costs
microseconds and is correct for every actor in the engine.

A volume spawned from script has no brush geometry — a brush is built by the editor, not by SpawnActor —
so the class carries a box and falls back to it. Procedurally placed volumes behave exactly like
hand-brushed ones.

### Numbers, not adjectives

A Canvas statistics box that survives a cooked Shipping build shows the active volume and its priority,
every volume the player is inside, blend progress as a number and a bar, blend time and curve, target
distance, whether collision is engaging and by how many centimetres, field of view, rail position, the
followed actor, and milliseconds per tick.

If your project already has a HUD class, one setting draws the same box through `AHUD::OnHUDPostRender`
instead — nothing to reparent.

### Blueprint and console

Everything is on one Blueprint library, world-context-aware and safe in a map that has no volumes at all:
`GetActiveVolume`, `GetBlendAlpha`, `GetInsideVolumes`, `SetFollowTarget`, `PushOverride`,
`ClearOverride`, `SetBlendTimeOverride`, `SetFieldOfViewOverride`, `SetCollisionAvoidanceEnabled`,
`SetDebugDraw`, `GetStats`. Plus `OnActiveVolumeChanged`, `OnVolumeEntered` and `OnVolumeExited`.

`PushOverride` takes the camera for a boss reveal or a death cam, beating every volume until it is
cleared. One slot, not a stack — a second push replaces the first, so two systems fighting over the
camera shows up as the second one winning rather than as a queue to unwind.

Console: `Frame.Stats`, `Frame.List`, `Frame.Blend <s>`, `Frame.Debug 0|1`, `Frame.Collision 0|1`.

### What it is not

No Niagara, no Chaos, no editor module, no network replication, no sequencer integration and no camera
shake — the engine has one, and it composes with this rather than being replaced by it.

---

## Technical details

**Features**
- Camera framing volumes driven by a DataTable row
- Priority resolution for overlapping volumes, smaller-volume tie-break
- Single-blend model: no stack, no queue, no accumulation
- Orbit, fixed-anchor and spline-rail framing modes
- Blend curves: linear, ease-in, ease-out, ease-in-out, custom `UCurveFloat`
- Optional collision avoidance, one sweep per frame
- Camera modifier, not a view target — composes with existing cameras
- Blueprint override slot for cutscenes and death cams
- Canvas statistics box, Shipping-safe
- Five console commands
- Project settings under *Plugins → FrameVolume*

**Code Modules**
- `FrameVolume` — Runtime, `PreDefault`

**Number of C++ Classes:** 6 (`AFrameVolume`, `UFrameVolumeSubsystem`, `UFrameVolumeCameraModifier`,
`UFrameVolumeStatics`, `UFrameVolumeSettings`, `AFrameVolumeHUD`) plus 2 structs and 2 enums

**Network Replicated:** No
**Supported Development Platforms:** Win64 (built and verified). Mac and Linux are not listed in the plugin descriptor and were not built for this release.
**Supported Target Build Platforms:** Win64
**Documentation:** `Docs/DOCUMENTATION.md`, `README.md`
**Important/Additional Notes:** Single local player — the modifier attaches to the first local player's
camera manager. Editor worlds are not supported by design; the plugin runs in Game and PIE.

---

© 2026 Silvan Teufel. All Rights Reserved.
