#pragma once
// ============================================================================
// UI layout constants. Hardware-independent (no Arduino dependency).
// Shared by the firmware (OLED canvas) and the PC simulator (console canvas).
//
// Design rule: every text item owns a fixed slot sized for the LONGEST value
// it can ever render, so numbers can never collide with labels or units.
//
// Font metrics (Adafruit GFX classic font, monospace):
//   size 1 -> 6 x 8 px per char
//   size 2 -> 12 x 16 px per char
//
// Max data lengths used to size the slots:
//   temperature "%.1f"  -> "-45.5"     5 chars = 60px @ size 2
//   humidity    "%.0f"  -> "100"       3 chars = 36px @ size 2
//   eCO2 / TVOC "%u"    -> "65535"     5 chars = 60px @ size 2
//   AQI digit           -> "5"         1 char  = 12px @ size 2
//   AQI status word     -> "Unhealthy" 9 chars = 54px @ size 1
//   page indicator      -> "1/2"       3 chars = 18px @ size 1
// ============================================================================

namespace Layout
{
    constexpr int SCREEN_WIDTH  = 128;
    constexpr int SCREEN_HEIGHT = 64;

    // ---- Shared header -----------------------------------------------------
    constexpr int HEADER_TEXT_Y = 0; // title & "n/N" indicator row
    constexpr int HEADER_LINE_Y = 8; // separator line under the header
    // "n/N" indicator is right-aligned to the screen edge (18px max).
    // Longest title "Air Quality" = 66px -> 110..65 always free. No overlap.

    // ---- Value-row grid ----------------------------------------------------
    // Top edge of each size-2 number cell (16px tall, 18px pitch).
    // Within a row: size-1 label sits at ROWn_Y + LABEL_DY (vertically
    // centered), size-1 unit sits at ROWn_Y + UNIT_DY (bottom-aligned).
    constexpr int ROW1_Y = 11;
    constexpr int ROW2_Y = 29;
    constexpr int ROW3_Y = 47;
    constexpr int LABEL_DY = 4;
    constexpr int UNIT_DY = 8;

    // ---- Page 1 "Climate": temperature & humidity ---------------------------
    // Unit column: "°C" (12px) / "%" (6px) both start here and end <= 128.
    constexpr int CLIMATE_UNIT_X = 116;
    // Numbers are right-aligned into the slot [54 .. 114]:
    // worst case "-45.5" (60px) starts at 54, clear of the 48px label column.
    constexpr int CLIMATE_NUM_RIGHT = 114;
    // Footer comfort status (size 1, left-aligned at x=0, max 7 chars = 42px).
    constexpr int CLIMATE_STATUS_Y = 53;

    // ---- Page 2 "Air Quality": eCO2 / TVOC / AQI ----------------------------
    // Unit column: "ppm" / "ppb" (18px) start here, end exactly at 128.
    constexpr int AIR_UNIT_X = 110;
    // Numbers are right-aligned into the slot [48 .. 108]:
    // worst case "65535" (60px) starts at 48, clear of the 24px label column.
    constexpr int AIR_NUM_RIGHT = 108;
    // AQI row: digit ends at 64; status word (<=54px) right-aligned to the
    // screen edge starts at >= 74 -> at least 10px of guaranteed gap.
    constexpr int AQI_DIGIT_RIGHT = 64;
    constexpr int AQI_STATUS_RIGHT = SCREEN_WIDTH;

    // ---- Page 3 "Mood": full-screen expression bitmap (ui_face_bitmaps.h) ----
    // Intentionally headerless: the frames are standalone full-screen art.
}
