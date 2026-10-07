# -*- coding: utf-8 -*-
# One-shot layout verifier: parses ui_sim output, extracts ink runs per row,
# and checks them against the expected element boxes derived from ui_layout.h.
import re, subprocess, sys

W, H = 128, 64
# 5x7 font (5 column-bytes per char, bit0 = top row) — parsed directly from
# tools/canvas_console.cpp FONT_5x7 so the model matches the simulator 1:1.
import os
_SRC = open(os.path.join(os.path.dirname(os.path.abspath(__file__)),
                         "canvas_console.cpp"), encoding="utf-8").read()
_G = [tuple(int(x, 16) for x in e.split(","))
      for e in re.findall(r"\{(0x[0-9A-Fa-f]{2}(?:, ?0x[0-9A-Fa-f]{2}){4})\}",
                          re.search(r"FONT_5x7\[\]\[5\] = \{(.*?)\n\};", _SRC,
                                    re.S).group(1))]
_CHARS = (" !\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`"
          "abcdefghijklmnopqrstuvwxyz{|}~")
F = dict(zip(_CHARS, _G))
assert len(F) == 95 and len(_G) == 95, "font parse incomplete: %d/%d" % (len(_G), 95)
DEG = (0x06, 0x0F, 0x09, 0x0F, 0x06)

def glyph(ch):
    if ord(ch) == 0xF8:
        return DEG
    return F[ch]

def char_w(ch, size):
    return 6 * size

def render(text, x, y, size):
    """Return set of (px,py) lit by ConsoleCanvas.draw_text semantics."""
    px_set = set()
    cx = x
    for ch in text:
        g = glyph(ch)
        for col in range(5):
            bits = g[col]
            for row in range(7):
                if bits & (1 << row):
                    for dy in range(size):
                        for dx in range(size):
                            p, q = cx + col*size + dx, y + row*size + dy
                            if 0 <= p < W and 0 <= q < H:
                                px_set.add((p, q))
        cx += 6 * size
    return px_set

def right_x(text, right_edge, size):
    return right_edge - len(text) * 6 * size

# ---- Pages: element lists (text, x, y, size) mirroring ui_pages.cpp ----
def header(title, idx):
    ind = "%d/3" % (idx + 1)
    return [(title, 0, 0, 1), (ind, right_x(ind, 128, 1), 0, 1)]  # line checked separately

def page_climate(d):
    els = header("Climate", 0)
    t = "%.1f" % d['t']; h = "%.0f" % d['h']
    els.append(("Temp", 0, 11+4, 1))
    els.append((t, right_x(t, 114, 2), 11, 2))
    els.append(("\xF8" "C", 116, 11+8, 1))
    els.append(("Humidity", 0, 29+4, 1))
    els.append((h, right_x(h, 114, 2), 29, 2))
    els.append(("%", 116, 29+8, 1))
    els.append((d['status'], 0, 53, 1))
    return els

def page_air(d):
    els = header("Air Quality", 1)
    e = "%u" % d['eco2']; v = "%u" % d['tvoc']
    els.append(("eCO2", 0, 11+4, 1))
    els.append((e, right_x(e, 108, 2), 11, 2))
    els.append(("ppm", 110, 11+8, 1))
    els.append(("TVOC", 0, 29+4, 1))
    els.append((v, right_x(v, 108, 2), 29, 2))
    els.append(("ppb", 110, 29+8, 1))
    els.append(("AQI", 0, 47+4, 1))
    els.append((d['digit'], right_x(d['digit'], 64, 2), 47, 2))
    els.append((d['aqiword'], right_x(d['aqiword'], 128, 1), 47+4, 1))
    return els

def parse_sim_output(out):
    """Extract the three 64-row pages from ui_sim --all output."""
    rows = [l[1:-1] for l in out.splitlines() if l.startswith('|') and l.endswith('|')]
    assert len(rows) == 3 * H, "expected %d rows, got %d" % (3 * H, len(rows))
    return rows[:H], rows[H:2*H], rows[2*H:]

def check(page_name, expected_els, rendered_rows, hline_y, strict=True):
    ok = True
    # 1) every expected element's lit pixels must exist in the render
    want = set()
    for (t, x, y, s) in expected_els:
        want |= render(t, x, y, s)
    got = {(x, y) for y, r in enumerate(rendered_rows)
                 for x, ch in enumerate(r) if ch == '#'}
    # header separator line is verified separately; drop that row from diff
    got = {(x, y) for (x, y) in got if y != hline_y}
    line = rendered_rows[hline_y]
    if line.count('#') != W:
        ok = False
        print("  header line incomplete")
    missing = want - got
    if missing:
        ok = False
        print("  MISSING ink (element drawn differently than expected):",
              sorted(missing)[:10])
    extra = got - want
    if extra and strict:
        ok = False
        print("  UNEXPECTED ink:", sorted(extra)[:10])
    # 3) pairwise element-box disjointness (the anti-overlap guarantee)
    boxes = []
    for (t, x, y, s) in expected_els:
        boxes.append((t, x, y, x + len(t)*6*s - 1, y + 8*s - 1))
    for i in range(len(boxes)):
        for j in range(i+1, len(boxes)):
            t1, x0, y0, x1, y1 = boxes[i]
            t2, X0, Y0, X1, Y1 = boxes[j]
            if x0 <= X1 and X0 <= x1 and y0 <= Y1 and Y0 <= y1:
                ok = False
                print("  BOX OVERLAP: %r %r vs %r %r" % (t1, boxes[i], t2, boxes[j]))
    print("  %s: %s" % (page_name, "OK" if ok else "FAIL"))
    return ok

# Header line overlaps nothing by geometry (title/indicator rows 0..7, line at 8),
# excluded from pixel diff via got.discard only for the exact row; extra check:
# no expected element touches row 8.
cases = [
    ("typical", {'t': 23.4, 'h': 55, 'eco2': 1742, 'tvoc': 1187, 'status': "Comfort",
                 'digit': "4", 'aqiword': "Poor"}),
    ("extreme", {'t': -45.5, 'h': 100, 'eco2': 65535, 'tvoc': 65535, 'status': "Comfort",
                 'digit': "5", 'aqiword': "Unhealthy"}),
    ("zero",    {'t': 0.0, 'h': 0, 'eco2': 0, 'tvoc': 0, 'status': "Dry",
                 'digit': "-", 'aqiword': "Unknown"}),
]

all_ok = True
for name, d in cases:
    print("case:", name)
    c = page_climate(d); a = page_air(d)
    ok = True
    for els in (c, a):
        for (t, x, y, s) in els:
            if y <= 8 <= y + 8*s - 1 and y != 0:
                ok = False; print("  element %r crosses header line" % t)
    # simulate expected vs actual using our own renderer as the reference model
    r1 = ["." * W for _ in range(H)]; r2 = ["." * W for _ in range(H)]
    for (t, x, y, s) in c:
        for (px, py) in render(t, x, y, s):
            r1[py] = r1[py][:px] + '#' + r1[py][px+1:]
    for (t, x, y, s) in a:
        for (px, py) in render(t, x, y, s):
            r2[py] = r2[py][:px] + '#' + r2[py][px+1:]
    for (px, py) in [(x, 8) for x in range(W)]:  # header line at y=8
        r1[py] = r1[py][:px] + '#' + r1[py][px+1:]
        r2[py] = r2[py][:px] + '#' + r2[py][px+1:]
    ok &= check("Climate", c, r1, 8)
    ok &= check("Air", a, r2, 8)
    all_ok &= ok

# ---- Cross-check against REAL ui_sim render for the default dataset ----
out = subprocess.run([os.path.join(r"d:\workspace\zheteng\ESP_Projects\co2\tools",
                                   "ui_sim.exe"), "--all"],
                     capture_output=True, text=True).stdout
sim1, sim2, sim3 = parse_sim_output(out)
dd = cases[0][1]
c = page_climate(dd); a = page_air(dd)
ok1 = check("Climate(sim-real)", c, sim1, 8)
ok2 = check("Air(sim-real)", a, sim2, 8)
# Mood page: full-screen bitmap face, no text elements -> smoke-check only.
ok3 = any('#' in r for r in sim3)
if not ok3:
    print("  Mood page rendered no ink")
all_ok &= ok1 and ok2 and ok3

print("RESULT:", "ALL OK" if all_ok else "FAILURES PRESENT")
sys.exit(0 if all_ok else 1)
