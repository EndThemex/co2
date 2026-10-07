#pragma once
// ============================================================================
// Face: mood evaluation + full-screen expression renderer. HW-independent.
//
// Expression art is ported from the IrisOLED library (MIT, see
// ui_face_bitmaps.h): each frame is a standalone 128x64 dithered-face bitmap
// drawn full screen.
//
// Mood mapping (when to switch, which frame):
//   no data (AQI==0, sensor warming up) -> Sleepy        sleepy
//   AQI 1..2 && comfort in range        -> Happy         happy
//   AQI 3 (or generic "OK")             -> Neutral       normal / look / blink
//   comfort out of range (air ok)       -> Uncomfortable worried
//   AQI 4                               -> Sad           sad
//   AQI 5                               -> Dizzy         disoriented
//
// Anti-flapping: a candidate mood must hold for CONFIRM_MS before it becomes
// the shown mood; each switch plays an 800 ms "surprised" transition.
//
// Idle life on the Neutral face, following IrisOLED's official examples:
//   blink  : normal -> blink -> normal, ~240 ms closed (dual-period pseudo-
//            random windows), per the Blink example
//   wander : normal -> look_left -> normal -> look_right, 2.2 s per step,
//            per the ScanningEyes example
//
// update()  : feed latest sensor sample + timestamp (call once per sample).
// draw()    : pure renderer; all animation is a deterministic function of
//             now_ms, so the OLED firmware and the PC simulator produce
//             identical frames.
// ============================================================================

#include "ui_canvas.h"
#include "ui_pages.h"

#include <cstdint>

namespace Ui
{
    namespace Face
    {
        enum class Mood : uint8_t
        {
            Sleepy,
            Happy,
            Neutral,
            Uncomfortable,
            Sad,
            Dizzy
        };

        void update(const SensorData &s, uint32_t now_ms);

        // Renders the full-screen expression frame for the current mood.
        void draw(Canvas &c, uint32_t now_ms);

    } // namespace Face
} // namespace Ui
