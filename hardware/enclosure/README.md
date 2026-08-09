# Enclosure

Retro CRT-monitor-styled back housing for the Waveshare ESP32-S3-Touch-LCD-1.69
module used in this project. Two printable parts, meant to be glued:

- **`frame`** — a flat-sided box sized to the module, tapering back through an
  inset conical section to a small rounded rear cap with a vent grille. The
  module drops in from the front and friction-fits the pocket; no bezel, no
  screws — the module's own frame already reads as the screen's bezel.
- **`base`** — a round swivel-puck pedestal on a straight neck, trimmed flush
  at the exact point it meets the frame's outer surface (a self-locating,
  contour-matched glue joint — see `base_part()`).

## Building

```
pip install cadquery
python3 generate.py
```

Outputs land in `models/`: `frame.stl` and `base.stl` (ASCII STL), the only
two files tracked in git -- everything else under `models/` is gitignored.

## Where the dimensions came from

Waveshare doesn't publish a dimensioned drawing for this module, so `generate.py`
measures everything directly off the vendor STEP file, checked into this repo
at `vendor/ESP32-S3-Touch-LCD-1_69.stp` (not `~/Downloads`, so the build is
reproducible without depending on local files outside the repo).

Most dimensions come from bounding-box/face inspection (see the constants
block at the top of the script). `board_shelf_wire()` extracts the module's
exact outer contour (real, non-circular-arc curvature) from a face at
`z=2.0` in the STEP file and uses it directly, rather than approximating it
as a rounded rect. The case's outer wall sits flush against this contour
(zero offset, no flareout past the module's real edge); the pocket is inset
inward by `WALL`. The module's own frame/glass pokes proud through the front
opening above the shelf.

The STEP file's largest-volume solid is a courtyard/envelope box (constant
33.13 x 41.13mm cross-section, not tapering with the real part) rather than
literal material — the real PCB and components are smaller (~29.85 x
37.14mm), which is why insetting the pocket by a full 1mm wall still clears
them with room to spare. Any fit check against the module should skip that
solid (see below) or it reads as a false collision.

Confirmed against the STEP geometry:
- Board outline: 33.13 x 41.13mm, 10.7mm thick, shelf at z=2.0
- USB-C connector: left edge, vertically centered on the board
- Three tactile buttons: right edge, precise Y positions in `BUTTON_Y_CENTERS`

Not individually identified (a cluster in the bottom tail — likely
JST/microSD/speaker): no cutout is provided for it currently.

## Fit verification

Every rebuild should be checked, not just visually inspected:

```python
import cadquery as cq
import generate as g

frame = g.frame_part()
base = g.base_part()
assert len(frame.solids().vals()) == 1
assert len(base.solids().vals()) == 1
assert sum(s.Volume() for s in frame.intersect(base).solids().vals()) == 0

pivot_y, pivot_z = g.puck_bottom()
mod = cq.importers.importStep(g.STEP_PATH)
solids = sorted(mod.solids().vals(), key=lambda s: -s.Volume())
real = cq.Workplane(obj=cq.Compound.makeCompound(solids[1:]))  # skip solid[0]: a courtyard/
                                                                # envelope box, not real material
board = real.rotate((0, pivot_y, pivot_z), (1, pivot_y, pivot_z), -g.TILT_DEG)
assert sum(s.Volume() for s in board.intersect(frame).solids().vals()) == 0
```

And at the mesh level (catches tessellation-only seam bugs the BREP checks
above can miss — see git history for a real instance of this):

```python
import trimesh
for name in ["frame", "base"]:
    m = trimesh.load(f"models/{name}.stl")
    assert m.is_watertight and m.body_count == 1
```
