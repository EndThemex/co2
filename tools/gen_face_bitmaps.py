# Generate include/ui_face_bitmaps.h from the IrisOLED repository bitmaps.
#
# Prerequisite: git clone --depth 1 https://github.com/orji123/Irisoled
#               into %TEMP%\Irisoled
# Run:          python tools/gen_face_bitmaps.py
#
# IrisOLED is MIT-licensed (c) 2025 Chijindu-Orji Iseh-Ntah; the generated
# header embeds the original frames verbatim (128x64, Adafruit page format).
import os, re

ARR_DIR = os.path.join(os.environ["TEMP"], "Irisoled", "extras",
                       "eye expressions", "bitmap arrays")
OUT = os.path.join(os.path.dirname(__file__), "..", "include", "ui_face_bitmaps.h")

FILES = {f.name.lower().replace(" ", "_").replace(".h", ""): f.path
         for f in os.scandir(ARR_DIR) if f.name.lower().endswith(".h")}

# Mood page frames (see ui_face.h for the mood -> frame mapping).
FRAMES = ["normal", "blink", "look_left", "look_right", "happy", "sad",
          "sleepy", "worried", "disoriented", "surprised"]

def load(name):
    with open(FILES[name], "r", encoding="utf-8", errors="ignore") as f:
        txt = f.read()
    m = re.search(r"\{([^}]*)\}", txt, re.S)
    vals = [int(v, 16) for v in re.findall(r"0x[0-9a-fA-F]{2}", m.group(1))]
    assert len(vals) == 1024, (name, len(vals))
    return vals

lines = []
lines.append("#pragma once")
lines.append("// ============================================================================")
lines.append("// Face expression frames ported from the IrisOLED library (MIT license).")
lines.append("//   https://github.com/orji123/Irisoled")
lines.append("//   Copyright (c) 2025 Chijindu-Orji Iseh-Ntah")
lines.append("//")
lines.append("// Each frame is a standalone full-screen 128x64 monochrome bitmap in")
lines.append("// Adafruit_GFX::drawBitmap() format: row-major, 16 bytes per row")
lines.append("// (byteWidth = 128/8), MSB = leftmost pixel. Render with")
lines.append("// Canvas::draw_bitmap(), which mirrors that exact semantics.")
lines.append("// Regenerate with: python tools/gen_face_bitmaps.py")
lines.append("// ============================================================================")
lines.append("")
lines.append("#include <cstdint>")
lines.append("")
lines.append("namespace Ui")
lines.append("{")
lines.append("    namespace Face")
lines.append("    {")
lines.append("        namespace Bmp")
lines.append("        {")
for name in FRAMES:
    vals = load(name)
    cname = "IRIS_" + name.upper()
    lines.append("            // %d lit pixels; ink bbox covers the face silhouette." %
                 sum(bin(b).count("1") for b in vals))
    lines.append("            inline const uint8_t %s[1024] = {" % cname)
    for row in range(0, 1024, 16):
        lines.append("                " + ", ".join("0x%02X" % v for v in vals[row:row + 16]) + ",")
    lines.append("            };")
    lines.append("")
lines.append("        } // namespace Bmp")
lines.append("    } // namespace Face")
lines.append("} // namespace Ui")
lines.append("")

with open(OUT, "w", encoding="utf-8", newline="\n") as f:
    f.write("\n".join(lines))
print("wrote", os.path.abspath(OUT), "(%d frames)" % len(FRAMES))
