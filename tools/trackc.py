#!/usr/bin/env python3
"""
trackc - Stunt Car Racer track compiler / decompiler.

Converts between the 804-byte Amiga track binaries in Tracks/*.bin and a
readable .trk source format, so tracks can be written by hand.

    ./tools/trackc.py decompile Tracks/LittleRamp.bin -o LittleRamp.trk
    ./tools/trackc.py compile   LittleRamp.trk -o Tracks/Custom.bin
    ./tools/trackc.py blocks                    # catalogue of height profiles
    ./tools/trackc.py selftest                  # round-trip every track in Tracks/

Two things the author never has to write, because the compiler derives them
exactly the way the original Amiga track tool did:

  * Grid positions and piece angles.  Each piece is appended to the previous
    one's exit, so the layout follows from the sequence plus a start cell and
    heading.  Verified against all 445 pieces of the 8 stock tracks.

  * Overall Y shifts.  A piece's shift is (running height - profile's first
    coord), and the running height then becomes (shift + profile's last
    coord), which joins each piece to the last with no step.  This is the rule
    at srd16a in "Reference only/StuntCarRacer.s", and it reproduces every
    shift in the stock tracks byte for byte.

The binary layout (see ReadAmigaTrackData in Track.cpp):

    0       piece count
    1       player's start piece
    2..101      Piece_X_Z_Position     x in low nibble, z in high nibble
    102..201    Piece_Angle_And_Template
    202..301    Left_Y_Coordinate_ID
    302..401    Right_Y_Coordinate_ID  bit 7 = road line colour
    402..601    Left_Overall_Y_Shift   int16 big-endian
    602..801    Right_Overall_Y_Shift  int16 big-endian
    802     standard boost
    803     super boost
"""

import argparse
import os
import re
import struct
import sys

SOURCE_FORMAT_HELP = """
the .trk source format
----------------------
Header, once, in any order:

    track   TestOval        name shown in the menus
    start   0               piece the start line sits on
    boost   34 47           standard boost, super boost
    origin  2,2 +Z          starting cell on the 16x16 grid, and heading
    height  1280 1280       world height of the left/right rail at the start
    speed   2 0x58 runup    opponent's target speed on a piece (max 16 of them)

Then one line per piece, in the order you drive them.  Grid positions, piece
angles and height shifts are all worked out for you:

    straight  0             one cell forward, flat
    right     0             45 degrees right (left, right9, left9 too)
    diag      0             one cell diagonally (diag13 is the longer one)
    straight  4/100         different profile per rail, so the road banks

The number is a height profile; 0 is flat and holds the current height.
Heights chain from piece to piece, so a climb stays high until something
descends.  List them with "blocks" - note a profile has to be long enough for
the piece: straight/right/left need 9 coords, right9/left9 10, diag 12,
diag13 14.

Flags after the profile: "light" road lines, "back" for the shifted variant of
a straight or diagonal.

Turns go in 45 degree steps, so a 90 degree corner is two curves in a row,
straights sit on the 4 axis headings and diagonals on the 4 diagonal ones.

compile refuses to write a track that does not return to its starting cell and
heading, that puts two pieces in one cell, or whose lap does not close in
height - every climb needs a matching descent.
"""

TRACK_DATA_SIZE = 804
MAX_PIECES = 100
GRID = 16

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

# Compass headings, 45 degrees apart, clockwise from +Z, as (dx, dz) cell steps.
DIRS = [(0, 1), (1, 1), (1, 0), (1, -1), (0, -1), (-1, -1), (-1, 0), (-1, 1)]
DIR_NAMES = ["+Z", "+X+Z", "+X", "+X-Z", "-Z", "-X-Z", "-X", "-X+Z"]


class Template:
    """One of the 7 usable entries of Piece_Templates[] in Track.cpp."""

    def __init__(self, num, kind, segments, turn, entry_offset):
        self.num = num                    # index into Piece_Templates[]
        self.kind = kind                  # name used in .trk source
        self.segments = segments
        self.turn = turn                  # exit heading - entry heading, in 45s
        self.entry_offset = entry_offset  # entry heading - rough angle, in 45s

    @property
    def coords(self):
        return self.segments + 1


TEMPLATES = [
    Template(0,  "straight", 8,   0,  0),
    Template(1,  "right",    8,  +1,  0),
    Template(3,  "left",     8,  -1,  0),
    Template(6,  "right9",   9,  +1,  0),
    Template(7,  "left9",    9,  -1,  0),
    Template(10, "diag",     11,  0, +1),
    Template(4,  "diag13",   13,  0, +1),
]
BY_NUM = {t.num: t for t in TEMPLATES}
BY_KIND = {t.kind: t for t in TEMPLATES}


def geometry(template_num, reverse, rough_angle):
    """Entry and exit heading of a placed piece, as indices into DIRS.

    rough_angle is the raw 0x00/0x40/0x80/0xc0 field.  A reversed piece runs
    its template backwards, so its entry is the template's exit turned around,
    and vice versa.
    """
    t = BY_NUM[template_num]
    base = ((rough_angle & 0xc0) >> 6) * 2
    t_entry = (base + t.entry_offset) % 8
    t_exit = (t_entry + t.turn) % 8
    if reverse:
        return (t_exit + 4) % 8, (t_entry + 4) % 8
    return t_entry, t_exit


def encode(kind, entry, back=False):
    """Pick the template, reverse flag and rough angle for a piece of `kind`
    entered on heading `entry`.  Returns None if that piece cannot be entered
    from that heading.

    A straight or a diagonal has two encodings with the same entry and exit -
    the template run forwards, or run backwards rotated 180 degrees.  They are
    not the same piece: the reversed one sits shifted along its own axis, which
    changes where it meets its neighbours.  `back` selects that variant.

    Straights and axis-aligned curves only connect to the 4 axis headings;
    diagonals and the exits of curves only to the 4 diagonal headings.  A
    curve leaving an axis heading uses its template forwards; one leaving a
    diagonal heading is the opposite-hand template run backwards.
    """
    t = BY_KIND[kind]
    # A curve entered from a diagonal heading is the opposite-hand template run
    # backwards, so consider that template too and select on the turn actually
    # produced rather than on which template it came from.
    mirror = {"right": "left", "left": "right", "right9": "left9", "left9": "right9"}
    candidates = [t.num]
    if kind in mirror:
        candidates.append(BY_KIND[mirror[kind]].num)
    for num in candidates:
        for reverse in ((True, False) if back else (False, True)):
            for angle in (0x00, 0x40, 0x80, 0xc0):
                e, x = geometry(num, reverse, angle)
                if e == entry and (x - e) % 8 == t.turn % 8:
                    return num, reverse, angle, x
    return None


# ---------------------------------------------------------------------------
# Height profiles, lifted out of Track.cpp so there is a single source of truth
# ---------------------------------------------------------------------------

def load_height_blocks(track_cpp=None):
    """Decode Amiga_Piece_Y[] from Track.cpp into 128 lists of Y coordinates."""
    path = track_cpp or os.path.join(REPO, "Track.cpp")
    src = open(path).read()

    arrays = {}
    for m in re.finditer(r"static unsigned char ([BW]\d+)\[\]\s*=\s*\{(.*?)\}", src, re.S):
        arrays[m.group(1)] = [int(x, 16) for x in re.findall(r"0x([0-9a-fA-F]{2})", m.group(2))]

    table = re.search(r"Amiga_Piece_Y\[NUM_AMIGA_PIECE_Y\]\s*=\s*\{(.*?)\n\t\t\};", src, re.S)
    if not table:
        raise SystemExit("could not find Amiga_Piece_Y[] in %s" % path)
    entries = re.findall(r"\{(NULL|[BW]\d+),(TRUE|FALSE),(?:sizeof\(\1\)|0)\}", table.group(1))
    if len(entries) != 128:
        raise SystemExit("expected 128 height blocks, parsed %d" % len(entries))

    blocks = []
    for name, words in entries:
        if name == "NULL":
            blocks.append([])
            continue
        raw = arrays[name]
        if words == "TRUE":
            # 15-bit big-endian words
            blocks.append([((raw[i] & 0x7f) << 8) | raw[i + 1] for i in range(0, len(raw), 2)])
        else:
            # packed byte: bits 5-7 -> Y bits 5-7, bits 0-3 -> Y bits 8-11
            blocks.append([((v << 1) & 0xe0) | ((v & 0x0f) << 8) for v in raw])
    return blocks


def describe_block(ys):
    """A short human label for a height profile."""
    if not ys:
        return "unused"
    rise = ys[-1] - ys[0]
    lo, hi = min(ys), max(ys)
    peak = max(hi - max(ys[0], ys[-1]), 0)
    dip = max(min(ys[0], ys[-1]) - lo, 0)
    steps = [abs(ys[i + 1] - ys[i]) for i in range(len(ys) - 1)]
    big = max(steps) if steps else 0
    if hi == lo:
        shape = "flat"
    elif big >= 640:
        shape = "step/jump"
    elif peak > abs(rise) // 2 and peak > 64:
        shape = "hump"
    elif dip > abs(rise) // 2 and dip > 64:
        shape = "dip"
    elif rise > 0:
        shape = "climb"
    elif rise < 0:
        shape = "descend"
    else:
        shape = "undulate"
    return "%-10s rise %+6d  range %5d" % (shape, rise, hi - lo)


# ---------------------------------------------------------------------------
# Binary <-> structure
# ---------------------------------------------------------------------------

class Piece:
    def __init__(self):
        self.template = 0
        self.reverse = False
        self.angle = 0x00
        self.spare_bit = 0     # angle byte bit 0x20, ignored by the engine
        self.left_y = 0
        self.right_y = 0
        self.left_y_bit7 = 0
        self.colour_bit = 0    # right Y ID bit 7 = road line colour
        self.cell = (0, 0)
        self.left_shift = 0
        self.right_shift = 0


MAX_SPEED_OVERRIDES = 16


class Track:
    def __init__(self):
        self.name = ""
        self.start_piece = 0
        self.standard_boost = 0
        self.super_boost = 0
        self.pieces = []
        # (piece, speed) pairs telling the opponent where to brake.  Appended
        # after the 804 fixed bytes; the stock tracks keep theirs compiled into
        # Opponent_Speeds.h instead, so this stays empty when decompiling one.
        self.speeds = []


def read_binary(path):
    data = open(path, "rb").read()
    if len(data) < TRACK_DATA_SIZE or (len(data) - TRACK_DATA_SIZE) % 2:
        raise SystemExit("%s: expected %d bytes plus an even number of speed-override "
                         "bytes, got %d" % (path, TRACK_DATA_SIZE, len(data)))

    t = Track()
    t.name = os.path.splitext(os.path.basename(path))[0]
    count = data[0]
    t.start_piece = data[1]
    xz = data[2:102]
    at = data[102:202]
    ly = data[202:302]
    ry = data[302:402]
    ls = struct.unpack(">100h", data[402:602])
    rs = struct.unpack(">100h", data[602:802])
    t.standard_boost = data[802]
    t.super_boost = data[803]

    for i in range(count):
        p = Piece()
        p.cell = (xz[i] & 0x0f, (xz[i] & 0xf0) >> 4)
        p.template = at[i] & 0x0f
        p.reverse = bool(at[i] & 0x10)
        p.spare_bit = at[i] & 0x20
        p.angle = at[i] & 0xc0
        p.left_y = ly[i] & 0x7f
        p.left_y_bit7 = ly[i] & 0x80
        p.right_y = ry[i] & 0x7f
        p.colour_bit = ry[i] & 0x80
        p.left_shift = ls[i]
        p.right_shift = rs[i]
        t.pieces.append(p)

    for off in range(TRACK_DATA_SIZE, len(data), 2):
        t.speeds.append((data[off], data[off + 1]))
    return t


def write_binary(track):
    n = len(track.pieces)
    xz = bytearray(MAX_PIECES)
    at = bytearray(MAX_PIECES)
    ly = bytearray(MAX_PIECES)
    ry = bytearray(MAX_PIECES)
    ls = [0] * MAX_PIECES
    rs = [0] * MAX_PIECES

    for i, p in enumerate(track.pieces):
        x, z = p.cell
        xz[i] = (x & 0x0f) | ((z & 0x0f) << 4)
        at[i] = (p.template & 0x0f) | (0x10 if p.reverse else 0) | p.spare_bit | p.angle
        ly[i] = p.left_y | p.left_y_bit7
        ry[i] = p.right_y | p.colour_bit
        ls[i] = p.left_shift
        rs[i] = p.right_shift

    out = bytearray()
    out.append(n)
    out.append(track.start_piece)
    out += xz + at + ly + ry
    out += struct.pack(">100h", *ls)
    out += struct.pack(">100h", *rs)
    out.append(track.standard_boost)
    out.append(track.super_boost)
    assert len(out) == TRACK_DATA_SIZE
    for piece, speed in track.speeds:
        out.append(piece)
        out.append(speed)
    return bytes(out)


# ---------------------------------------------------------------------------
# Derivation: placement and heights
# ---------------------------------------------------------------------------

def place(track, start_cell, start_heading):
    """Fill in each piece's cell from the sequence.  Returns the closing
    (cell, heading) so the caller can check the track forms a loop."""
    cell = start_cell
    heading = start_heading
    for p in track.pieces:
        entry, exit_ = geometry(p.template, p.reverse, p.angle)
        if entry != heading:
            raise SystemExit("internal error: piece entry %s != heading %s"
                             % (DIR_NAMES[entry], DIR_NAMES[heading]))
        p.cell = cell
        dx, dz = DIRS[exit_]
        cell = ((cell[0] + dx) % GRID, (cell[1] + dz) % GRID)
        heading = exit_
    return cell, heading


def derive_shifts(track, blocks, start_left=None, start_right=None):
    """Apply the original tool's continuity rule (srd16a in StuntCarRacer.s).

    start_left/start_right are the world heights of the two rails at the very
    start of piece 0; everything after is chained from them.
    """
    left, right = start_left, start_right
    for p in track.pieces:
        lb = blocks[p.left_y]
        rb = blocks[p.right_y]
        last = BY_NUM[p.template].segments
        if len(lb) <= last or len(rb) <= last:
            raise SystemExit(
                "piece uses a %d-coordinate profile but %s needs %d "
                "(left id %d has %d, right id %d has %d)"
                % (min(len(lb), len(rb)), BY_NUM[p.template].kind, last + 1,
                   p.left_y, len(lb), p.right_y, len(rb)))
        if left is None:
            left, right = lb[0], rb[0]
        p.left_shift = left - lb[0]
        p.right_shift = right - rb[0]
        left = p.left_shift + lb[last]
        right = p.right_shift + rb[last]
    return left, right


# ---------------------------------------------------------------------------
# Source format
# ---------------------------------------------------------------------------

def decompile(track, blocks):
    lines = []
    lines.append("# %s - decompiled by tools/trackc.py" % track.name)
    lines.append("")
    lines.append("track   %s" % track.name)
    lines.append("start   %d" % track.start_piece)
    lines.append("boost   %d %d" % (track.standard_boost, track.super_boost))
    first = track.pieces[0]
    entry, _ = geometry(first.template, first.reverse, first.angle)
    lines.append("origin  %d,%d %s" % (first.cell[0], first.cell[1], DIR_NAMES[entry]))
    lines.append("height  %d %d   # world height of the left/right rail at the start"
                 % (first.left_shift + blocks[first.left_y][0],
                    first.right_shift + blocks[first.right_y][0]))
    for piece, value in track.speeds:
        lines.append("speed   %-3d 0x%02x%s" % (piece, value & 0x7f,
                                                " runup" if value & 0x80 else ""))
    lines.append("")
    lines.append("# piece     profile      flags")
    for i, p in enumerate(track.pieces):
        t = BY_NUM[p.template]
        kind = t.kind
        # A curve run backwards turns the other way; name it by what it does.
        if t.turn and p.reverse:
            kind = {"right": "left", "left": "right",
                    "right9": "left9", "left9": "right9"}[t.kind]
        profile = ("%d" % p.left_y if p.left_y == p.right_y
                   else "%d/%d" % (p.left_y, p.right_y))
        flags = []
        entry = geometry(p.template, p.reverse, p.angle)[0]
        for back in (False, True):
            enc = encode(kind, entry, back)
            if enc and (enc[0], enc[1], enc[2]) == (p.template, p.reverse, p.angle):
                if back:
                    flags.append("back")
                break
        else:
            # No named encoding reproduces this piece; fall back to raw fields.
            flags.append("raw=%d,%d,0x%02x" % (p.template, int(p.reverse), p.angle))
        if p.colour_bit:
            flags.append("light")
        if p.left_y_bit7:
            flags.append("lbit7")
        lines.append("%-10s  %-11s  %s" % (kind, profile, " ".join(flags)))
    return "\n".join(lines) + "\n"


def compile_source(text, blocks, path="<source>"):
    track = Track()
    origin = None
    heading = None
    start_left = start_right = None

    for lineno, raw in enumerate(text.splitlines(), 1):
        line = raw.split("#", 1)[0].strip()
        if not line:
            continue
        parts = line.split()
        head = parts[0]

        def fail(msg):
            raise SystemExit("%s:%d: %s" % (path, lineno, msg))

        if head == "track":
            track.name = " ".join(parts[1:])
        elif head == "start":
            track.start_piece = int(parts[1])
        elif head == "boost":
            track.standard_boost, track.super_boost = int(parts[1]), int(parts[2])
        elif head == "speed":
            # speed <piece> <value> [runup]
            # value is the opponent's target speed on that piece; "runup" sets
            # bit 7, which both ignores the league's maximum and marks the
            # three pieces before it as the approach - what brakes it into a
            # jump.  Accepts 0x.. or decimal.
            piece = int(parts[1], 0)
            value = int(parts[2], 0)
            if "runup" in parts[3:]:
                value |= 0x80
            if not 0 <= piece <= 255 or not 0 <= value <= 255:
                fail("speed piece/value must fit in a byte")
            track.speeds.append((piece, value))
        elif head == "height":
            start_left, start_right = int(parts[1]), int(parts[2])
        elif head == "origin":
            cell = parts[1].split(",")
            origin = (int(cell[0]), int(cell[1]))
            name = parts[2] if len(parts) > 2 else "+Z"
            if name not in DIR_NAMES:
                fail("unknown heading %r (want one of %s)" % (name, ", ".join(DIR_NAMES)))
            heading = DIR_NAMES.index(name)
        elif head in BY_KIND or head in ("right", "left", "right9", "left9"):
            if origin is None:
                fail("piece before 'origin'")
            if len(track.pieces) >= MAX_PIECES:
                fail("more than %d pieces" % MAX_PIECES)
            p = Piece()
            profile = parts[1] if len(parts) > 1 else "0"
            if "/" in profile:
                l, r = profile.split("/")
                p.left_y, p.right_y = int(l), int(r)
            else:
                p.left_y = p.right_y = int(profile)
            back = "back" in parts[2:]
            for f in parts[2:]:
                if f == "back":
                    pass
                elif f == "light":
                    p.colour_bit = 0x80
                elif f == "lbit7":
                    p.left_y_bit7 = 0x80
                elif f.startswith("raw="):
                    t, rev, ang = f[4:].split(",")
                    p.template, p.reverse, p.angle = int(t), bool(int(rev)), int(ang, 16)
                else:
                    fail("unknown flag %r" % f)
            if not any(f.startswith("raw=") for f in parts[2:]):
                enc = encode(head, heading, back)
                if enc is None:
                    fail("a %s cannot be entered heading %s"
                         % (head, DIR_NAMES[heading]))
                p.template, p.reverse, p.angle, _ = enc
            entry, exit_ = geometry(p.template, p.reverse, p.angle)
            if entry != heading:
                fail("piece enters heading %s but the track arrives heading %s"
                     % (DIR_NAMES[entry], DIR_NAMES[heading]))
            heading = exit_
            # Angle bit 0x20 records that one profile serves both rails; it is
            # informational (the engine masks it off) but stock tracks always
            # set it exactly when the two IDs match, so keep doing that.
            if p.left_y == p.right_y:
                p.spare_bit = 0x20
            track.pieces.append(p)
        else:
            fail("unknown directive %r" % head)

    if not track.pieces:
        raise SystemExit("%s: no pieces" % path)
    if origin is None:
        raise SystemExit("%s: no 'origin'" % path)

    first = track.pieces[0]
    start_heading = geometry(first.template, first.reverse, first.angle)[0]
    end_cell, end_heading = place(track, origin, start_heading)
    derive_shifts(track, blocks, start_left, start_right)
    return track, origin, start_heading, end_cell, end_heading


def check(track, origin, start_heading, end_cell, end_heading, blocks):
    """Report anything that would make the track unraceable."""
    problems = []
    if end_cell != origin:
        problems.append("track does not close: ends at cell %d,%d, started at %d,%d"
                        % (end_cell[0], end_cell[1], origin[0], origin[1]))
    if end_heading != start_heading:
        problems.append("track does not close: ends heading %s, started heading %s"
                        % (DIR_NAMES[end_heading], DIR_NAMES[start_heading]))
    seen = {}
    for i, p in enumerate(track.pieces):
        if p.cell in seen:
            problems.append("pieces %d and %d both occupy cell %d,%d "
                            "(Track_Map holds only one)" % (seen[p.cell], i, p.cell[0], p.cell[1]))
        seen[p.cell] = i
    if len(track.speeds) > MAX_SPEED_OVERRIDES:
        problems.append("%d speed overrides, but the engine reads at most %d"
                        % (len(track.speeds), MAX_SPEED_OVERRIDES))
    for piece, _ in track.speeds:
        if piece >= len(track.pieces):
            problems.append("speed override names piece %d, past the end of the track "
                            "(%d pieces)" % (piece, len(track.pieces)))
    if track.start_piece >= len(track.pieces):
        problems.append("start piece %d is past the end of the track (%d pieces)"
                        % (track.start_piece, len(track.pieces)))

    # Heights are welded piece to piece, but the loop must also close in Y or
    # the start line sits at a different height from the finish.
    lb = blocks[track.pieces[0].left_y]
    rb = blocks[track.pieces[0].right_y]
    last = track.pieces[-1]
    lastseg = BY_NUM[last.template].segments
    close_l = last.left_shift + blocks[last.left_y][lastseg]
    close_r = last.right_shift + blocks[last.right_y][lastseg]
    start_l = track.pieces[0].left_shift + lb[0]
    start_r = track.pieces[0].right_shift + rb[0]
    if close_l != start_l or close_r != start_r:
        problems.append("height does not close: lap ends %+d/%+d from where it started "
                        "(left/right) - the start line will be a step"
                        % (close_l - start_l, close_r - start_r))
    return problems


# ---------------------------------------------------------------------------
# Commands
# ---------------------------------------------------------------------------

def cmd_decompile(args):
    blocks = load_height_blocks()
    track = read_binary(args.input)
    text = decompile(track, blocks)
    if args.output:
        open(args.output, "w").write(text)
        print("wrote %s (%d pieces)" % (args.output, len(track.pieces)))
    else:
        sys.stdout.write(text)


def cmd_compile(args):
    blocks = load_height_blocks()
    text = open(args.input).read()
    track, origin, sh, ec, eh = compile_source(text, blocks, args.input)
    problems = check(track, origin, sh, ec, eh, blocks)
    for p in problems:
        print("warning: %s" % p, file=sys.stderr)
    if problems and not args.force:
        raise SystemExit("refusing to write a broken track; pass --force to override")
    data = write_binary(track)
    out = args.output or os.path.splitext(args.input)[0] + ".bin"
    open(out, "wb").write(data)
    print("wrote %s (%d pieces, %d bytes)" % (out, len(track.pieces), len(data)))


def cmd_blocks(args):
    blocks = load_height_blocks()
    want = {t.coords for t in TEMPLATES} if args.coords is None else {args.coords}
    kinds = {}
    for t in TEMPLATES:
        kinds.setdefault(t.coords, []).append(t.kind)
    print("Height profiles, by the number of coordinates they supply.")
    print("A piece can only use a profile whose length matches its kind.\n")
    for n in sorted(want):
        used = kinds.get(n)
        print("--- %d coords: %s ---" % (n, ", ".join(used) if used else "unused by any piece"))
        for i, ys in enumerate(blocks):
            if len(ys) == n:
                print("  %3d  %s" % (i, describe_block(ys)))
        print()


def cmd_selftest(args):
    blocks = load_height_blocks()
    tracks = sorted(f for f in os.listdir(os.path.join(REPO, "Tracks")) if f.endswith(".bin"))
    failures = 0
    for name in tracks:
        path = os.path.join(REPO, "Tracks", name)
        original = open(path, "rb").read()
        track = read_binary(path)
        text = decompile(track, blocks)
        rebuilt, origin, sh, ec, eh = compile_source(text, blocks, name)
        data = write_binary(rebuilt)
        notes = []
        if data != original:
            failures += 1
            diff = [i for i in range(TRACK_DATA_SIZE) if data[i] != original[i]]
            notes.append("MISMATCH at %d bytes, first %s" % (len(diff), diff[:8]))
        problems = check(rebuilt, origin, sh, ec, eh, blocks)
        status = "ok" if not notes else notes[0]
        print("%-22s %3d pieces  round-trip %s%s"
              % (name, len(track.pieces), status,
                 "  [%d checks]" % len(problems) if problems else ""))
        for p in problems:
            print("      note: %s" % p)
    print()
    if failures:
        print("%d of %d tracks failed to round-trip" % (failures, len(tracks)))
        return 1
    print("all %d tracks in Tracks/ round-trip byte for byte" % len(tracks))
    return 0


def main():
    ap = argparse.ArgumentParser(description=__doc__.strip(),
                                 formatter_class=argparse.RawDescriptionHelpFormatter,
                                 epilog=SOURCE_FORMAT_HELP)
    sub = ap.add_subparsers(dest="cmd")

    d = sub.add_parser("decompile", help="track .bin -> readable .trk source")
    d.add_argument("input")
    d.add_argument("-o", "--output")
    d.set_defaults(func=cmd_decompile)

    c = sub.add_parser("compile", help=".trk source -> track .bin")
    c.add_argument("input")
    c.add_argument("-o", "--output")
    c.add_argument("--force", action="store_true",
                   help="write the track even if the checks fail")
    c.set_defaults(func=cmd_compile)

    b = sub.add_parser("blocks", help="list the available height profiles")
    b.add_argument("--coords", type=int, help="only profiles of this length")
    b.set_defaults(func=cmd_blocks)

    s = sub.add_parser("selftest", help="round-trip every track in Tracks/")
    s.set_defaults(func=cmd_selftest)

    args = ap.parse_args()
    if not args.cmd:
        # Running the tool bare should teach you how to use it, not scold you.
        ap.print_help()
        sys.exit(0)
    sys.exit(args.func(args) or 0)


if __name__ == "__main__":
    main()
