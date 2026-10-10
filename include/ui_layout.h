#pragma once
// ============================================================================
// UI layout constants. Hardware-independent (no Arduino dependency).
// Shared by the firmware (OLED canvas) and the PC simulator (console canvas).
//
// Design rule: every text item owns a fixed slot sized for the LONGEST value
// it can ever render, so numbers can never collide with labels or units.
//
// Font metrics:
//   size 1 ASCII (Adafruit GFX classic) -> 6 x 8 px per char
//   size 2 ASCII                        -> 12 x 16 px per char
//   size 1 CJK (FontCN whitelist)       -> 12 x 12 px glyph, 13px advance
//
// Max data lengths used to size the slots:
//   temperature "%.1f"  -> "-45.5"     5 chars = 60px @ size 2
//   humidity    "%.0f"  -> "100"       3 chars = 36px @ size 2
//   eCO2 / TVOC "%u"    -> "65535"     5 chars = 60px @ size 2
//   AQI digit           -> "5"         1 char  = 12px @ size 2
//   AQI status word     -> "很差/未知" 2 glyphs = 25px @ size 1
//   page indicator      -> "1/3"       3 chars = 18px @ size 1
// ============================================================================

namespace Layout
{
    constexpr int SCREEN_WIDTH = 128;
    constexpr int SCREEN_HEIGHT = 64;

    // ---- Shared header -----------------------------------------------------
    constexpr int HEADER_TEXT_Y = 0;  // title (12px CJK) & "n/N" indicator row
    constexpr int HEADER_LINE_Y = 13; // separator line under the header
    // "n/N" indicator is right-aligned to the screen edge (18px max).
    // Longest title "空气质量" = 51px -> ends at x=51, indicator starts >= 110.
    // No overlap.

    // ---- Value-row grid ----------------------------------------------------
    // Top edge of each size-2 number cell (16px tall, 16px pitch).
    // Within a row: size-1 CJK label (12px) sits at ROWn_Y + LABEL_DY
    // (vertically centered), size-1 unit (8px) sits at ROWn_Y + UNIT_DY
    // (bottom-aligned).
    constexpr int ROW1_Y = 15;
    constexpr int ROW2_Y = 31;
    constexpr int ROW3_Y = 47;
    constexpr int LABEL_DY = 2;
    constexpr int UNIT_DY = 8;

    // ---- Page 1 "温湿度": temperature & humidity ----------------------------
    // Unit column: "°C" (12px) / "%" (6px) both start here and end <= 128.
    constexpr int CLIMATE_UNIT_X = 116;
    // Numbers are right-aligned into the slot [54 .. 114]:
    // worst case "-45.5" (60px) starts at 54, clear of the 25px label column.
    constexpr int CLIMATE_NUM_RIGHT = 114;
    // Footer comfort status: CENTERED (2 glyphs = 26px -> x=51..76) and
    // flanked by divider lines, so the row reads as a deliberate status bar
    // instead of a lone corner label. Text 12px tall -> 50..61, below row 2
    // which ends at 46; divider sits at the text's vertical center (56).
    constexpr int CLIMATE_STATUS_Y = 50;
    constexpr int CLIMATE_STATUS_LINE_Y = 56;

    // ---- Page 2 "空气质量": eCO2 / TVOC / AQI --------------------------------
    // Unit column: "ppm" / "ppb" (18px) start here, end exactly at 128.
    constexpr int AIR_UNIT_X = 110;
    // Numbers are right-aligned into the slot [48 .. 108]:
    // worst case "65535" (60px) starts at 48, clear of the 24px label column.
    constexpr int AIR_NUM_RIGHT = 108;
    // AQI row: digit hugs its label ("AQI" = 18px -> digit at x=22..33),
    // so the value never floats in the middle of the screen; status word
    // right-aligned to the screen edge. Worst case "很差/未知" (26px) starts
    // at 102 -> >= 69px guaranteed gap after the digit, no overlap.
    constexpr int AQI_DIGIT_X = 22;
    constexpr int AQI_STATUS_RIGHT = SCREEN_WIDTH;

    // ---- Page 3 "表情": full-screen expression bitmap (ui_face_bitmaps.h) ----
    // Intentionally headerless: the frames are standalone full-screen art.
}
