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
//   AQI 1 && eCO2<=600 && TVOC<=150     -> Excited       excited <-> happy
//   AQI 1..2 && comfort in range        -> Happy         happy (idle winks)
//   AQI 3 (or generic "OK")             -> Neutral       2D wander + blink
//   comfort out of range (air ok)       -> Uncomfortable worried
//   AQI 4                               -> Sad           sad
//   AQI 5                               -> Angry         angry
//   TVOC >= 2000 (extreme overload)     -> Dizzy         disoriented
//
// Anti-flapping: a candidate mood must hold for CONFIRM_MS before it becomes
// the shown mood. Each switch plays an 800 ms transition frame, flavored by
// direction: into a WORSE mood -> "scared", otherwise -> "surprised".
//
// Idle life, all deterministic in now_ms (identical on firmware & simulator):
//   blink   : dual-period pseudo-random windows (~240 ms closed); while
//             looking up/down the matching blink variant is used
//   wander  : 6-step 2D loop rest -> left -> up -> rest -> right -> down,
//             2.2 s per step (ScanningEyes-style, extended)
//   wink    : Happy winks left/right on two long coprime periods
//   bounce  : Excited alternates excited/happy every 400 ms
//   bored   : Neutral held > 60 s flashes a bored face 2.2 s every 24 s
//
// update()  : feed latest sensor sample + timestamp (call once per sample).
// draw()    : pure renderer; caches the last frame pointer and skips the
//             blit while the frame is unchanged (OledCanvas::flush() then
//             also skips the I2C push), so high tick rates are nearly free.
//             Repaints automatically after any Canvas::clear() (page switch).
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
            Excited,
            Happy,
            Neutral,
            Uncomfortable,
            Sad,
            Angry,
            Dizzy
        };

        void update(const SensorData &s, uint32_t now_ms);

        // Renders the full-screen expression frame for the current mood.
        void draw(Canvas &c, uint32_t now_ms);

    } // namespace Face
} // namespace Ui
