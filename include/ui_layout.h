#pragma once
// ============================================================================
// UI layout constants. Hardware-independent (no Arduino dependency).
// Shared by the firmware (OLED canvas) and the PC simulator (console canvas).
//
// Coordinate convention:
//   * Every X constant is the LEFT edge (start column) of the text; drawing
//     proceeds to the right from there.
//   * Every Y constant is the TOP row of the glyph cell; glyph height depends
//     on the font size:
//       size 1 ASCII -> 6 x 8 px per char  (8 rows tall)
//       size 2 ASCII -> 12 x 16 px per char (16 rows tall)
//       value text   -> 12 x 24 px per char (2x horizontal, 3x vertical)
//       size 1 CJK   -> 12 x 12 px glyph, 13px advance (12 rows tall)
//   * Screen extent: x = 0..127, y = 0..63.
//
// Design rule: no header at all. Each data page is split into two 64px columns.
// The label and its unit share one line above the number, and the number owns
// the full column width. Values are LEFT-aligned in their column, so a
// worst-case 5-digit value (60px @ 12px/char) still fits inside 64px with margin.
// On the climate page both columns keep a 1px inner margin: the temperature
// pair starts at x=1, the humidity label at x=65 (the humidity value still
// starts on the column boundary at x=64).
//
// Vertical budget (64px):
//   4..15   climate CJK label; 8..15 ASCII label; 8..15 climate units
//   20..43  value, 24px tall (5x7 glyphs scaled 2x horizontally, 3x vertically)
//   44..59  status row (AQI digit 44..59, status words 48..59)
//
// Max data lengths used to size the columns:
//   temperature "%.1f"  -> "-45.5"     5 chars = 60px @ 12px/char (column 0..63)
//   humidity    "%.0f"  -> "100"       3 chars = 36px @ 12px/char (column 64..127)
//   eCO2 / TVOC "%u"    -> "65535"     5 chars = 60px @ 12px/char (column 64px)
//   AQI digit           -> "5"         1 char  = 12px @ size 2
//   AQI status word     -> "很差/未知" 2 glyphs = 26px @ size 1
// ============================================================================

namespace Layout
{
    // ---- Screen bounds -----------------------------------------------------
    constexpr int SCREEN_WIDTH = 128; // OLED width: valid x range 0..127
    constexpr int SCREEN_HEIGHT = 64; // OLED height: valid y range 0..63

    // ---- Two-column data grid ----------------------------------------------
    // Both data pages use the same 2 x 64px columns:
    //   climate: [温度] [湿度]      air: [eCO2] [TVOC]
    constexpr int TILE1_X = 0;  // Left column origin X: "eCO2" label / eCO2 value / "AQI"
    constexpr int TILE2_X = 64; // Right column origin X: "TVOC" label / TVOC value
    constexpr int TILE_W = 64;  // Column width: values are <= 60px wide, must stay within 64px

    // Label band: a 12px CJK label (rows 4..15) or an 8px ASCII label
    // (rows 8..15). The unit (8px) sits on the same baseline (rows 8..15),
    // right after the label with a 1px gap (climate) / 2px gap (air).
    constexpr int CJK_LABEL_Y = 4;    // Top row of CJK label: "温度"(x=1), "湿度"(x=65) -> rows 4..15
    constexpr int ASCII_LABEL_Y = 8;  // Top row of ASCII label: "eCO2"(x=0), "TVOC"(x=64) -> rows 8..15
    constexpr int UNIT_Y = 8;         // Top row of unit on the air page: "ppm"(x=26), "ppb"(x=90) -> rows 8..15
    constexpr int LABEL_UNIT_GAP = 3; // Horizontal gap (px) from label right edge to unit left edge

    // ---- Page 0 "温湿度": two columns, 1px inner margins --------------------
    // Temperature column: label/value at x=1, unit at x=28 on its own row.
    // Humidity column: label at x=65, value on the column boundary at x=64.
    constexpr int CLIMATE_TEMP_LABEL_X = 1; // Left edge of "温度" -> 1..26
    constexpr int CLIMATE_TEMP_UNIT_X = 28; // Left edge of "°C"   -> 28..39
    constexpr int CLIMATE_TEMP_UNIT_Y = 8;  // Top row of "°C"    -> rows 8..15
    constexpr int CLIMATE_TEMP_VALUE_X = 1; // Left edge of "-45.5" (60px) -> 1..60
    constexpr int CLIMATE_HUM_LABEL_X = 65; // Left edge of "湿度" -> 65..90
    constexpr int CLIMATE_HUM_UNIT_X = 92;  // Left edge of "%"   -> 92..97
    constexpr int CLIMATE_HUM_UNIT_Y = 8;   // Top row of "%"    -> rows 8..15
    constexpr int CLIMATE_HUM_VALUE_X = 64; // Left edge of "100" (36px) -> 64..99
    // Comfort word (2 CJK glyphs = 26px) is centered on the left column axis
    // x=14 -> occupies 1..26, inside the temperature column.
    constexpr int CLIMATE_STATUS_CENTER_X = 14;
    constexpr int CLIMATE_STATUS_Y = 48; // Top row of climate comfort word -> rows 48..59

    // Air page units (firmware metrics):
    //   "eCO2" = 24px -> 24 + 2 = 26;  "TVOC" at TILE2_X -> 64 + 24 + 2 = 90
    constexpr int AIR_ECO2_UNIT_X = 26; // Left edge of "ppm" (left column, after "eCO2")
    constexpr int AIR_TVOC_UNIT_X = 90; // Left edge of "ppb" (right column, after "TVOC")

    // Number band: value text, 24px tall (21px ink), 12px per char.
    // Worst case 60px, column 64px.
    constexpr int VALUE_Y = 20; // Top row of value: "-45.5"(x=1), "100"(x=64), "65535"(x=0 or x=64) -> rows 20..43

    // ---- Page 2 "空气质量": AQI row ----------------------------------------
    // Row reads: "AQI" + digit, then a "质量" label + the status word.
    //   "AQI" = 18px -> digit at 28..39; "质量" (2 CJK glyphs) at 72..97;
    //   status word right-aligned with a 2px margin, worst case "很差/未知"
    //   (26px) at 100..125 -> 2px gap after "质量", no overlap.
    constexpr int AQI_LABEL_Y = 48;       // Top row of "AQI" label (x=0), 8px tall -> rows 48..55
    constexpr int AQI_DIGIT_X = 28;       // Left edge of AQI digit (1 char, 12px wide), right after "AQI"
    constexpr int AQI_DIGIT_Y = 44;       // Top row of AQI digit, size 2 -> rows 44..59
    constexpr int AQI_QUALITY_X = 72;     // Left edge of "质量" label, 2 CJK glyphs (26px) -> 72..97
    constexpr int AQI_STATUS_Y = 48;      // Top row of AQI status word, right-aligned -> rows 48..59
    constexpr int AQI_STATUS_RIGHT = 126; // Right edge X of AQI status word (2px margin from x=128)

    // ---- Page 3 "表情": full-screen expression bitmap (ui_face_bitmaps.h) ----
    // Intentionally headerless: the frames are standalone full-screen art.
}
