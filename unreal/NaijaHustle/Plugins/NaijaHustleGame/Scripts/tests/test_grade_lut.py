"""Checks nh_grade_lut.py without the editor: python3 Scripts/tests/test_grade_lut.py"""
import os
import struct
import sys
import tempfile
import zlib

os.environ["NH_NO_MAIN"] = "1"
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
import nh_grade_lut as L

fails = 0


def ok(name, cond, detail=""):
    global fails
    print(("ok   " if cond else "FAIL ") + name + (f"  {detail}" if not cond else ""))
    fails += 0 if cond else 1


rows = L.lut_rows(L.NEUTRAL)
ok("the table is 256x16", len(rows) == 16 and all(len(r) == 256 for r in rows))
ident = all(rows[g][b * 16 + r] == (round(r * 255 / 15), round(g * 255 / 15), round(b * 255 / 15)) for r in range(16) for g in range(16) for b in range(16))
ok("neutral settings change nothing: red across a slice, green down, blue by slice", ident)

dark, bright = L.grade((0.08, 0.08, 0.08)), L.grade((0.9, 0.9, 0.9))
ok("the shipped grade lifts shadows towards teal", dark[2] > dark[0] and dark[1] > dark[0], dark)
ok("and warms the highlights", bright[0] > bright[2], bright)
ok("black is lifted a little, never crushed below zero", 0 < min(L.grade((0, 0, 0))) and max(L.grade((0, 0, 0))) < 0.08, L.grade((0, 0, 0)))
ok("every entry stays in range", all(0 <= v <= 255 for row in L.lut_rows() for px in row for v in px))
mids = [L.grade((v / 20, v / 20, v / 20)) for v in range(21)]
ok("greys stay in order (no banding or inversion)", all(sum(a) < sum(b) for a, b in zip(mids, mids[1:])))

path = os.path.join(tempfile.mkdtemp(), "lut.png")
L.write_png(path, L.lut_rows())
data = open(path, "rb").read()
w, h, depth, ctype = struct.unpack(">IIBB", data[16:26])
idat = data[data.index(b"IDAT") + 4:data.index(b"IEND") - 8]
ok("the PNG is a valid 256x16 8-bit RGB image", data[:8] == b"\x89PNG\r\n\x1a\n" and (w, h, depth, ctype) == (256, 16, 8, 2) and len(zlib.decompress(idat)) == 16 * (1 + 256 * 3))
print("ALL PASS" if not fails else f"{fails} FAILED")
sys.exit(1 if fails else 0)
