"""
Retro-monitor back housing for the Waveshare ESP32-S3-Touch-LCD-1.69 module.

Single-part, open-front shell shaped like an actual CRT monitor: a flat-sided
cuboid frame sized tightly to the module -- depth equals the module's own
thickness plus a 1mm clearance, no extra padding -- then an inset conical
taper (smaller than the frame's own boundary, never flush with it) narrowing
back to a small rounded rear cap with a vent grille. A two-step pedestal neck
and round swivel-puck base attach behind the frame, under the cone, like a
real CRT stand.

The module drops in from the front and simply friction-fits the pocket (no
bezel, no retention tabs -- the module's own frame already reads as the
screen's bezel). USB-C and the three tactile buttons get precise cutouts
measured directly off the vendor STEP file (vendor/ESP32-S3-Touch-LCD-1_69.stp),
not the datasheet, since the datasheet does not publish a dimensioned drawing.
The board's own cavity/pocket shape is also traced directly off that file's
real geometry (see board_shelf_wire()), not approximated.

Run: python3 generate.py
Outputs an STL (print) + STEP (edit), plus PNG renders, into ./out
"""

import os

import cadquery as cq

STEP_PATH = os.path.join(os.path.dirname(__file__), "vendor", "ESP32-S3-Touch-LCD-1_69.stp")

# ---------------------------------------------------------------------------
# Module dimensions, measured from the STEP file's bounding boxes / faces.
# All coordinates share the STEP file's own frame: X centered on 0, Y centered
# on the board (board Y center sits at -1.27), Z=+3.8 is the front glass
# face, Z=-6.9 is the deepest connector.
# ---------------------------------------------------------------------------
BOARD_W = 33.13
BOARD_H = 41.13
BOARD_CORNER_R = 1.2  # kept tight so the pocket corner doesn't undercut the module's corner tabs
BOARD_Z_FRONT = 3.8
BOARD_Z_BACK = -6.9

# The PCB steps up to the display module's own frame at z=2.0 -- a ~2mm-wide
# ledge running the full BOARD_W x BOARD_H outer perimeter (real curvature,
# not a simple radius). That's the natural seat for the case: the case's
# front rim sits flush on this shelf, and the module's own frame/glass (from
# here up to BOARD_Z_FRONT) pokes proud through the opening, fully exposed.
SHELF_Z = 2.0

_shelf_wire_cache = None


def board_shelf_wire():
    """The exact outer contour of the PCB-to-frame step, traced directly off
    the STEP file (true curvature) rather than approximated as a rounded
    rect -- so the case's seat matches the module exactly."""
    global _shelf_wire_cache
    if _shelf_wire_cache is not None:
        return _shelf_wire_cache
    step = cq.importers.importStep(STEP_PATH)
    solids = sorted(step.solids().vals(), key=lambda s: -s.Volume())
    for f in solids[0].Faces():
        bb = f.BoundingBox()
        if abs(bb.zmin - SHELF_Z) < 0.05 and abs(bb.xlen - BOARD_W) < 0.1:
            wires = sorted(f.Wires(), key=lambda w: -w.BoundingBox().xlen)
            _shelf_wire_cache = wires[0]
            return _shelf_wire_cache
    raise RuntimeError("could not find the board's shelf wire in the STEP file")


def shelf_outline(clearance):
    """The exact shelf contour, offset by `clearance` (negative = inward).
    Shared by the pocket (inset by WALL) and the outer wall (zero offset,
    flush with the module's real envelope), so the whole frame -- not just
    the cavity -- tracks the module's real curvature instead of an
    approximated rounded rect."""
    return board_shelf_wire().offset2D(clearance)[0]


def shelf_solid(clearance, extra_depth=1.0, extra_front=0.5):
    """A solid extruded from the exact shelf contour (offset outward by
    `clearance`)."""
    face = cq.Face.makeFromWires(shelf_outline(clearance))
    depth = (FRONT_Z - BOX_END_Z) + extra_depth
    solid = cq.Solid.extrudeLinear(face, cq.Vector(0, 0, -depth))
    return cq.Workplane(obj=solid).translate((0, 0, extra_front))


def shelf_pocket(clearance):
    return shelf_solid(clearance)

# USB-C connector: left edge, absolute STEP-frame position (solid bbox
# x[-16.23,-8.70] y[-5.81,3.78] z[-5.85,-1.69]).
USB_Y_CENTER = -1.0
USB_SLOT_W = 10.0
USB_Z_CENTER = -3.8
USB_SLOT_H = 4.6

# Three tactile buttons, right edge, absolute STEP-frame positions (solid
# bboxes x[11.9,15.6] z[-4.30,-2.10], one per y range below).
BUTTON_Y_CENTERS = [-9.76, -0.85, 8.07]
BUTTON_Z_CENTER = -3.2
BUTTON_PILL_LEN = 4.5     # along Y
BUTTON_PILL_WIDTH = 2.2    # along Z

# ---------------------------------------------------------------------------
# Front collar / box -- the flat-sided cuboid that actually holds the module.
# A real CRT's case is a box right behind the bezel, not curved -- the taper
# only starts further back, and inset from this box's own edges.
# ---------------------------------------------------------------------------
CLR_Z = 0.5             # depth clearance -- frame depth = board thickness + this
WALL = 1.2               # shell thickness throughout -- thickened from 1.0 to tighten the
                          # pocket fit (the 1.0mm wall left the module sliding loose in testing)

# The outer wall sits flush with the module's own envelope (BOARD_W x
# BOARD_H, from board_shelf_wire()) -- zero outward offset, no flareout.
# The real PCB + components are considerably smaller than that envelope
# (measured off the STEP file: ~29.85 x 37.14mm), so insetting the pocket
# inward by WALL still leaves real clearance around the actual board, it
# just doesn't hug the module's outer envelope the way the box's own outer
# wall does.
CASE_W = BOARD_W
CASE_H = BOARD_H
CASE_CX = 0.0
CASE_CY = -1.273  # board's own Y center

FRONT_Z = SHELF_Z                     # case's front rim sits flush on the shelf, not the glass
BOX_END_Z = BOARD_Z_BACK - CLR_Z      # frame depth = board thickness + CLR_Z, no more

# ---------------------------------------------------------------------------
# CRT cone -- tapers from an inset start (smaller than the box, never flush
# with its boundary) down through a waist to a small rounded rear cap. Each
# station is a uniform scale of the box's own shape, so the rear cap reads as
# a downscaled copy of the front, not a different shape.
# ---------------------------------------------------------------------------
CONE_WALL = 1.0
# The cone's own corner rounding is independent of the box's (which is
# pinned tight to BOARD_CORNER_R + WALL) -- it just needs to stay well above
# CONE_WALL at every station so the shell offset stays valid, even at the
# small rear cap.
CONE_CORNER_R_BASE = 8.0
# Real CRTs keep most of their bulk close to the front -- the back is nearly
# as wide as the bezel, and even the rear vent panel is a substantial
# fraction of it, not a thin point. The previous scales (0.58 / 0.36) tapered
# far too aggressively for a convincing CRT silhouette.
CONE_START_SCALE = 0.85
CONE_WAIST_SCALE = 0.72
CONE_REAR_SCALE = 0.55

CONE_WAIST_D = 12   # depth behind BOX_END_Z
CONE_REAR_D = 22    # depth behind BOX_END_Z

WAIST_W = CASE_W * CONE_WAIST_SCALE
WAIST_H = CASE_H * CONE_WAIST_SCALE

VENT_SLOT_W = 4.0
VENT_SLOT_H = 1.2
VENT_GAP = 1.8
VENT_ROWS = 4
VENT_COLS = 2
VENT_COL_GAP = 3.0

# ---------------------------------------------------------------------------
# Pedestal stand -- a short neck down to a round swivel-base puck. Cross-
# sections are in the X-Z plane (width x depth) and stack downward in Y.
# ---------------------------------------------------------------------------
NECK_W, NECK_D, NECK_R = 13.0, 14.0, 3.0
PUCK_THICK = 3.0

# The neck is straight (no angle) and attaches to the cone at its waist. The
# puck is a true circle centered directly under it (same X and Z). The whole
# housing tilts back around the puck's own bottom (see back_housing()), which
# swings the front frame up and away from the puck -- so the puck's size
# isn't capped by the untilted front-clearance math anymore; it only has to
# clear the frame in the actual (tilted) geometry, checked directly below.
STAND_Z_CENTER = BOX_END_Z - CONE_WAIST_D
PUCK_D = 34.0
PUCK_Z_CENTER = STAND_Z_CENTER
STAND_OVERLAP = 1.5  # how far the neck's raw (pre-trim) top reaches past the cone's true surface
NECK_DROP = 20.0  # total Y span of the neck, from its cone attach point down to the puck
TILT_DEG = 9  # slight backward lean, like a monitor on a stand

# Mating peg -- a pin on the base that plugs into a socket hole in the
# frame, past the neck's own flush contour cut. Gives the joint real
# interlocking retention (resists twisting/sliding) beyond just the glued
# contour surface, and self-aligns the two parts during assembly.
PEG_D = 4.0
# The wide neck itself gets trimmed flush at the frame's true outer surface
# by the cut in base_part() (cone_envelope() is a filled solid, not the
# hollow shelled cone -- it removes *everything* inside the cone's contour,
# not just a thin shell) -- so the entire STAND_OVERLAP allowance the neck
# uses to fuse robustly with the cone is exposed as visible peg, not hidden
# inside the neck. PEG_PROTRUSION is the actual, total visible length past
# that true surface -- the only number that controls how long the peg looks.
PEG_PROTRUSION = 2.0
PEG_EMBED = 4.0     # how far the peg is embedded into the neck's own remaining body, for fusion
PEG_CLEARANCE = 0.3  # socket radius vs. peg radius


def rrect(w, h, r):
    return cq.Sketch().rect(w, h).vertices().fillet(r)




def disk_xz(diameter, thickness):
    """A true circular disk, cross-section in the X-Z plane, extrudes
    downward along -Y."""
    return cq.Workplane("XZ").circle(diameter / 2).extrude(thickness)


def side_pill(y_center, z_center, length_y, width_z, at_x_max):
    """A pill/stadium-shaped through-wall cutout on the left or right side
    wall: a rounded rect in the Y-Z cross-section, extruded along X."""
    r = min(length_y, width_z) / 2 - 0.01
    shape = cq.Workplane("YZ").rect(length_y, width_z).extrude(WALL * 4).edges("|X").fillet(r)
    wall_x = (CASE_W / 2 if at_x_max else -CASE_W / 2) + CASE_CX
    return shape.translate((wall_x - WALL * 2, y_center, z_center))


def front_box():
    # Outer wall sits flush with the module's exact envelope contour (zero
    # offset -- not an approximated rounded rect, and no flareout past the
    # module's real edge). The pocket is inset inward by WALL, giving the
    # wall real thickness without the case ever extending past the module.
    outer = shelf_solid(0)
    pocket = shelf_pocket(-WALL)
    box = outer.cut(pocket)

    box = box.cut(side_pill(USB_Y_CENTER, USB_Z_CENTER, USB_SLOT_W, USB_SLOT_H, at_x_max=False))
    for by in BUTTON_Y_CENTERS:
        box = box.cut(side_pill(by, BUTTON_Z_CENTER, BUTTON_PILL_LEN, BUTTON_PILL_WIDTH, at_x_max=True))

    return box


SEAL_STEP = 6.0  # depth from the box's own back face to where the inset taper starts


def cone_envelope():
    """The cone's solid (unshelled) outer loft -- used both as the basis for
    the real hollow cone and as a filled cutting tool elsewhere.

    The first station ("seal") exactly matches the box's own outer
    cross-section -- the real module contour, flush with zero offset -- at
    the same Z where the box ends, not the smaller inset "start" size. A
    separate ring sized to approximately bridge the two, glued on with a
    boolean union, left a coincident-but-not-quite-overlapping seam that
    (even though watertight at the BREP level) exported as two disconnected
    shells in the STL mesh. Sharing the box's exact boundary at the loft's
    very first station avoids that seam entirely -- box and cone weld along
    an identical, not approximate, contour. The taper down to the smaller
    inset "start" size happens over the next SEAL_STEP mm, still within
    this one continuous loft."""
    seal_wire = shelf_outline(0).moved(cq.Location(cq.Vector(0, 0, BOX_END_Z - SHELF_Z)))
    seal = cq.Sketch().face(seal_wire)
    start = rrect(CASE_W * CONE_START_SCALE, CASE_H * CONE_START_SCALE, CONE_CORNER_R_BASE * CONE_START_SCALE).moved(
        cq.Location(cq.Vector(CASE_CX, CASE_CY, BOX_END_Z - SEAL_STEP))
    )
    waist = rrect(WAIST_W, WAIST_H, CONE_CORNER_R_BASE * CONE_WAIST_SCALE).moved(
        cq.Location(cq.Vector(CASE_CX, CASE_CY, BOX_END_Z - CONE_WAIST_D))
    )
    rear = rrect(CASE_W * CONE_REAR_SCALE, CASE_H * CONE_REAR_SCALE, CONE_CORNER_R_BASE * CONE_REAR_SCALE).moved(
        cq.Location(cq.Vector(CASE_CX, CASE_CY, BOX_END_Z - CONE_REAR_D))
    )
    return cq.Workplane("XY").placeSketch(seal, start, waist, rear).loft()


def crt_cone():
    cone = cone_envelope()
    cone = cone.faces(">Z").shell(-CONE_WALL)

    rear_z = BOX_END_Z - CONE_REAR_D
    for row in range(VENT_ROWS):
        for col in range(VENT_COLS):
            vx = CASE_CX + (col - (VENT_COLS - 1) / 2) * VENT_COL_GAP
            vy = CASE_CY + (VENT_ROWS - 1) / 2 * (VENT_SLOT_H + VENT_GAP) - row * (VENT_SLOT_H + VENT_GAP)
            r = min(VENT_SLOT_W, VENT_SLOT_H) / 2 - 0.01
            vent = (
                cq.Workplane("XY")
                .rect(VENT_SLOT_W, VENT_SLOT_H)
                .extrude(CONE_WALL + 1)
                .edges("|Z")
                .fillet(r)
                .translate((vx, vy, rear_z))
            )
            cone = cone.cut(vent)

    return cone


def stand_attach_y():
    # STAND_Z_CENTER sits exactly at the cone's waist station, so the cone's
    # actual cross-section there is WAIST_W x WAIST_H -- not the full case
    # size. Using the wrong (larger) boundary left the stand floating in
    # space, not touching the cone at all. STAND_OVERLAP pushes the neck up
    # well past the cone's own surface, deep into its hollow body, so the
    # union has real volume to fuse -- not just a coincident face.
    return CASE_CY - WAIST_H / 2 + STAND_OVERLAP


def puck_bottom():
    """Y and Z of the puck's bottom-center -- the point that actually rests
    on a desk, used as the tilt pivot so the base doesn't swing off to the
    side when the housing leans back."""
    bottom_y = stand_attach_y()
    puck_top_y = bottom_y - NECK_DROP
    return puck_top_y - PUCK_THICK, PUCK_Z_CENTER


def peg_socket_y():
    """Y span of the peg/socket, centered on the neck's attach point:
    (deepest point past the frame's real surface, shallowest point embedded
    in the neck)."""
    outer_y = CASE_CY - WAIST_H / 2  # approx. frame's true (un-overlapped) outer surface here
    peg_top_y = outer_y + PEG_PROTRUSION
    peg_bottom_y = stand_attach_y() - PEG_EMBED
    return peg_bottom_y, peg_top_y


def stand_peg():
    peg_bottom_y, peg_top_y = peg_socket_y()
    return disk_xz(PEG_D, peg_top_y - peg_bottom_y).translate((CASE_CX, peg_top_y, STAND_Z_CENTER))


def peg_socket_cutter():
    """The hole the peg plugs into -- cut into both the real frame (so the
    socket physically exists) and the envelope used to trim the base (so
    the peg survives that trim instead of being cut away as if it were
    buried in solid material)."""
    peg_bottom_y, peg_top_y = peg_socket_y()
    pad = 1.0
    return disk_xz(PEG_D + 2 * PEG_CLEARANCE, (peg_top_y - peg_bottom_y) + 2 * pad).translate(
        (CASE_CX, peg_top_y + pad, STAND_Z_CENTER)
    )


def pedestal_stand():
    bottom_y = stand_attach_y()
    puck_top_y = bottom_y - NECK_DROP

    # Straight, vertical neck -- no angle, no Z shift along its height.
    neck = cq.Workplane("XZ").rect(NECK_W, NECK_D).extrude(bottom_y - puck_top_y).edges("|Y").fillet(NECK_R)
    neck = neck.translate((CASE_CX, bottom_y, STAND_Z_CENTER))
    puck = disk_xz(PUCK_D, PUCK_THICK).translate((CASE_CX, puck_top_y, PUCK_Z_CENTER))
    return neck.union(puck).union(stand_peg())


def frame_solid():
    """Front box + cone -- solid and complete on its own, no stand. This is
    what used to receive the pedestal via a fused overlap; on its own it's
    already a closed, watertight part. The socket cut gives the base's peg
    something to plug into."""
    return front_box().union(crt_cone()).cut(peg_socket_cutter())


def _tilt(shape):
    pivot_y, pivot_z = puck_bottom()
    return shape.rotate((0, pivot_y, pivot_z), (1, pivot_y, pivot_z), -TILT_DEG)


def frame_part():
    return _tilt(frame_solid())


def base_part():
    """The pedestal, trimmed flush at the frame's own outer surface -- the
    neck used to overlap *into* the frame so the two would fuse into one
    solid. Cut against the SOLID cone envelope (not the hollow shelled
    frame_solid()) so the trim stops exactly at the outer surface instead of
    leaving a disconnected island of neck material stranded inside the
    cone's hollow interior."""
    envelope = front_box().union(cone_envelope()).cut(peg_socket_cutter())
    return _tilt(pedestal_stand().cut(envelope))


def back_housing():
    """Combined reference/preview -- both parts glued together."""
    return frame_part().union(base_part())


def board_reference():
    """Real module geometry, used only for the assembly-check render."""
    return cq.importers.importStep(STEP_PATH)


if __name__ == "__main__":
    import os

    out = os.path.join(os.path.dirname(__file__), "models")
    os.makedirs(out, exist_ok=True)

    frame = frame_part()
    base = base_part()

    for name, shape in [("frame", frame), ("base", base)]:
        cq.exporters.export(shape, os.path.join(out, f"{name}.stl"), opt={"ascii": True})

    print("done ->", out)
