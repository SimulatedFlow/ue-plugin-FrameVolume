# FrameVolume

**Camera framing volumes with priority and blending, for Unreal Engine 5.8.**

Drop a volume into a level, point it at a row of a DataTable, and the camera takes that framing the
moment the player walks in. Distance, pitch, yaw, field of view, blend time, blend curve and priority are
**one row**, not eight numbers typed into every volume's details panel. Overlap two volumes and the
higher priority wins. Step out and the camera moves back — blended, never cut.

No Blueprint, no trigger, no camera actor in the map. Place the volume, pick a row, play.

---

## The part that matters: there is no blend stack

Systems that queue camera blends drift. Leave a volume mid-blend, walk straight back in, and the queue is
two entries deep: the camera arrives late, or twice, or somewhere nobody asked for.

FrameVolume has exactly **one current shot, one target shot and one alpha between them**. When the target
changes mid-blend, the current shot is replaced by the point the blend had actually reached, and the
alpha restarts at zero. The camera therefore always leaves from where it visibly is. Walk in and out of a
doorway ten times in two seconds and nothing accumulates, nothing overshoots, nothing arrives late.

## What it does

- **Three ways to frame.** Orbit the followed actor at a distance and two angles; sit on a fixed anchor
  point; or ride a spline rail at the point nearest the player — a projection, so the camera glides along
  the track and can never leave it.
- **Priority resolution.** Overlapping volumes are sorted by priority, highest wins. Equal priorities are
  broken by the smaller volume, because the small volume around the doorway is the specific case inside
  the big volume around the hall.
- **Collision avoidance.** One optional sphere sweep per frame from the player out to the camera; the
  camera stops in front of whatever is in the way. Switchable per row and globally.
- **A camera modifier, not a view target.** Your spring arm, first-person camera or running cinematic
  keeps working underneath. When the last volume is left the plugin blends back into it and then stops
  writing to the camera entirely.
- **One tick for the whole world.** No volume ticks. One bounds test per volume, an exact test only for
  the few that pass, one sort, one sweep, one spline query. Two hundred volumes cost what two do.
- **Numbers you can read.** A Canvas statistics box — active volume and its priority, *every* volume the
  actor is standing in, blend progress, target distance and field of view, whether collision is engaging,
  and milliseconds per tick. It survives a Shipping build.

## Quick start

1. Enable the plugin and restart the editor.
2. Create a DataTable with the row type **FrameVolumeRow**. Add a row, e.g. `Corridor`:
   Distance `450`, Pitch `-10`, FOV `65`, BlendTime `0.8`.
3. Place a **Frame Volume** in the level, brush it to the shape of the room, and set **Shot** to your
   table and the `Corridor` row.
4. Press Play and walk in. That is the whole setup.

To see the numbers, set the map's HUD class to **Frame Volume HUD** — or, if the project already has a
HUD, turn on *Project Settings → Plugins → FrameVolume → Auto Draw Stats On Any HUD*.

## Console commands

| Command | What it does |
|---|---|
| `Frame.Stats` | Print the measured counters to the log. |
| `Frame.List` | Print every volume, with priority, row, occupancy and which one is active. |
| `Frame.Blend <s>` | Force every blend to this length. Negative gives the rows control back. |
| `Frame.Debug 0\|1` | Draw the volumes, the pivot, the target shot and the collision sweep. |
| `Frame.Collision 0\|1` | Turn collision avoidance on or off for every shot. |

## Blueprint

Everything is on the **FrameVolume** function library, world-context-aware and safe in a map with no
volumes at all: `GetActiveVolume`, `GetBlendAlpha`, `GetInsideVolumes`, `SetFollowTarget`, `PushOverride`,
`ClearOverride`, `SetBlendTimeOverride`, `SetFieldOfViewOverride`, `SetCollisionAvoidanceEnabled`,
`SetDebugDraw`, `GetStats`.

The subsystem also broadcasts `OnActiveVolumeChanged`, `OnVolumeEntered` and `OnVolumeExited`.

## What it is not

No Niagara, no Chaos, no editor module, no network replication, no cutscene sequencer and no camera
shake — the engine already has one, and it composes with this rather than being replaced by it.

## Requirements

Unreal Engine 5.8. Built and verified on **Win64**; Mac and Linux are enabled in the plugin descriptor
but were not built for this release.

## Documentation

Full reference in [`Docs/DOCUMENTATION.md`](Docs/DOCUMENTATION.md).

---

© 2026 Silvan Teufel. All Rights Reserved.
