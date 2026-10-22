# -*- coding: utf-8 -*-
# Layout verifier for the two data pages.
#
# The ASCII console simulator is the reference renderer for ASCII elements, so
# their pixels are checked 1:1 against the real ui_sim output. CJK labels are
# checked by bounding box (ink present + no overlap) because the FontCN bitmap
# data lives in the firmware headers, not in the simulator.
#
# Worst-case widths (5-digit values, longest AQI word) are checked analytically
# against the 64px column rule; ui_sim only renders the default dataset.
import os
import re
import subprocess
import sys

W, H = 128, 64
HERE = os.path.dirname(os.path.abspath(__file__))
CJK_ADVANCE = 13

# 5x7 font (5 column-bytes per char, bit0 = top row), parsed from
# canvas_console.cpp so the model matches the simulator 1:1.
_SRC = open(os.path.join(HERE, "canvas_console.cpp"), encoding="utf-8").read()
_G = [tuple(int(x, 16) for x in e.split(","))
      for e in re.findall(r"\{(0x[0-9A-Fa-f]{2}(?:, ?0x[0-9A-Fa-f]{2}){4})\}",
                          re.search(r"FONT_5x7\[\]\[5\] = \{(.*?)\n\};", _SRC,
                                    re.S).group(1))]
_CHARS = (" !\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`"
          "abcdefghijklmnopqrstuvwxyz{|}~")
F = dict(zip(_CHARS, _G))
assert len(F) == 95 and len(_G) == 95, "font parse incomplete: %d/%d" % (len(_G), 95)
DEG = (0x06, 0x0F, 0x09, 0x0F, 0x06)


def is_cjk(ch):
    return 0x2E80 <= ord(ch)


def text_width(text, size_x):
    return sum(CJK_ADVANCE if is_cjk(ch) else 6 * size_x for ch in text)


def right_x(text, right_edge, size_x):
    return right_edge - text_width(text, size_x)


def render_ascii(text, x, y, size_x, size_y):
    """Pixels lit by ConsoleCanvas.draw_value/draw_text for a pure-ASCII string."""
    px_set = set()
    cx = x
    for ch in text:
        g = DEG if ord(ch) in (0xB0, 0xF8) else F[ch]
        for col in range(5):
            bits = g[col]
            for row in range(7):
                if bits & (1 << row):
                    for dy in range(size_y):
                        for dx in range(size_x):
                            p, q = cx + col * size_x + dx, y + row * size_y + dy
                            if 0 <= p < W and 0 <= q < H:
                                px_set.add((p, q))
        cx += 6 * size_x
    return px_set


def parse_sim_output(out):
    rows = [l[1:-1] for l in out.splitlines() if l.startswith('|') and l.endswith('|')]
    assert len(rows) == 3 * H, "expected %d rows, got %d" % (3 * H, len(rows))
    return rows[:H], rows[H:2*H], rows[2*H:]


def cjk_box(text, x, y):
    return (x, y, x + text_width(text, 1) - 1, y + 11)


def ascii_box(text, x, y, size_x, size_y):
    return (x, y, x + text_width(text, size_x) - 1, y + 8 * size_y - 1)


def page_climate(d):
    t = "%.1f" % d['t']
    h = "%.0f" % d['h']
    status = d['status']
    return [
        ("温度", 1, 4, 1, 1),
        ("°C", 28, 8, 1, 1),
        (t, 1, 20, 2, 3),
        ("湿度", 65, 4, 1, 1),
        ("%", 92, 8, 1, 1),
        (h, 64, 20, 2, 3),
        (status, 14 - text_width(status, 1) // 2, 48, 1, 1),
    ]


def page_air(d):
    return [
        ("eCO2", 0, 8, 1, 1),
        ("ppm", 26, 8, 1, 1),
        ("%u" % d['eco2'], 0, 20, 2, 3),
        ("TVOC", 64, 8, 1, 1),
        ("ppb", 90, 8, 1, 1),
        ("%u" % d['tvoc'], 64, 20, 2, 3),
        ("AQI", 0, 48, 1, 1),
        (d['digit'], 28, 44, 2, 2),
        ("质量", 72, 48, 1, 1),
        (d['aqiword'], right_x(d['aqiword'], 126, 1), 48, 1, 1),
    ]


def check_page(name, elements, rows):
    ok = True

    got = {(x, y) for y, r in enumerate(rows) for x, ch in enumerate(r) if ch == '#'}
    boxes = []
    for (text, x, y, sx, sy) in elements:
        box = cjk_box(text, x, y) if any(is_cjk(ch) for ch in text) \
            else ascii_box(text, x, y, sx, sy)
        boxes.append((text, box))

        if not any(is_cjk(ch) for ch in text):
            missing = render_ascii(text, x, y, sx, sy) - got
            if missing:
                ok = False
                print("  MISSING %r ink at %r" % (text, sorted(missing)[:6]))
        else:
            x0, y0, x1, y1 = box
            if not any((px, py) in got for px in range(x0, x1 + 1)
                       for py in range(y0, y1 + 1)):
                ok = False
                print("  MISSING %r: box has no ink" % text)

    for i in range(len(boxes)):
        for j in range(i + 1, len(boxes)):
            t1, (a0, b0, a1, b1) = boxes[i]
            t2, (c0, d0, c1, d1) = boxes[j]
            if a0 <= c1 and c0 <= a1 and b0 <= d1 and d0 <= b1:
                ok = False
                print("  BOX OVERLAP: %r vs %r" % (t1, t2))

    print("  %s: %s" % (name, "OK" if ok else "FAIL"))
    return ok


def check_worst_case_widths():
    ok = True
    values = ["-45.5", "65535", "1187", "100"]
    for v in values:
        if text_width(v, 2) > 64:
            ok = False
            print("  value %r is %dpx, wider than a 64px column"
                  % (v, text_width(v, 2)))

    # Label + unit must share a 64px label line.
    for label, unit, x in [("温度", "°C", 0), ("湿度", "%", 64),
                           ("eCO2", "ppm", 0), ("TVOC", "ppb", 64)]:
        used = text_width(label, 1) + 2 + text_width(unit, 1)
        if used > 64:
            ok = False
            print("  %r + %r uses %dpx, wider than 64px" % (label, unit, used))

    digit_end = 28 + text_width("5", 2) - 1
    worst_status_start = right_x("很差", 126, 1)
    if worst_status_start <= digit_end:
        ok = False
        print("  AQI digit (ends %d) overlaps status (starts %d)"
              % (digit_end, worst_status_start))

    quality_end = 72 + text_width("质量", 1) - 1
    if worst_status_start <= quality_end:
        ok = False
        print("  AQI quality label (ends %d) overlaps status (starts %d)"
              % (quality_end, worst_status_start))

    print("  worst-case widths: %s" % ("OK" if ok else "FAIL"))
    return ok


cases = [
    ("typical", {'t': 23.4, 'h': 55, 'eco2': 1742, 'tvoc': 1187,
                 'status': "舒适", 'digit': "4", 'aqiword': "差"}),
]

print("case:", cases[0][0])
c = page_climate(cases[0][1])
a = page_air(cases[0][1])

sim = os.path.join(HERE, "ui_sim.exe")
out = subprocess.run([sim, "--all"], capture_output=True, text=True,
                     encoding="utf-8", errors="replace").stdout
sim1, sim2, sim3 = parse_sim_output(out)

all_ok = check_page("Climate", c, sim1)
all_ok &= check_page("Air", a, sim2)
all_ok &= check_worst_case_widths()

if not any('#' in r for r in sim3):
    all_ok = False
    print("  Mood page rendered no ink")
else:
    print("  Mood: OK")

print("RESULT:", "ALL OK" if all_ok else "FAILURES PRESENT")
sys.exit(0 if all_ok else 1)
