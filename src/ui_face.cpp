// ============================================================================
// Face implementation. See ui_face.h for the mood/timing design.
//
// All animation is deterministic in now_ms:
//   blink   : (now % 4000) < 240 || (now % 7300) < 240  -> pseudo-random feel
//   wander  : normal/look_left/look_right, one step per 2200 ms
// ============================================================================

#include "ui_face.h"

#include "ui_face_bitmaps.h"

namespace Ui
{
    namespace Face
    {
        namespace
        {
            // ---- Timing constants ------------------------------------------
            constexpr uint32_t CONFIRM_MS = 3000;  // candidate must hold this long
            constexpr uint32_t SURPRISE_MS = 800;  // transition on mood switch
            constexpr uint32_t BLINK_LEN = 240;    // closed-eye duration
            constexpr uint32_t WANDER_STEP_MS = 2200;

            // ---- Hysteresis state -------------------------------------------
            Mood shownMood = Mood::Sleepy;  // what the face displays
            Mood targetMood = Mood::Sleepy; // latest sensor-derived candidate
            uint32_t targetSince = 0;       // when the candidate first appeared
            uint32_t surpriseUntil = 0;     // transition frame window

            Mood classify(const SensorData &d)
            {
                if (d.aqi == 0)
                    return Mood::Sleepy; // ENS160 warming up / no valid data
                if (d.aqi >= 5)
                    return Mood::Dizzy;
                if (d.aqi == 4)
                    return Mood::Sad;

                Comfort cf = comfort_category(d.temperature, d.humidity);
                if (cf == Comfort::Cold || cf == Comfort::Hot ||
                    cf == Comfort::Dry || cf == Comfort::Humid)
                    return Mood::Uncomfortable;

                if (d.aqi <= 2)
                    return Mood::Happy; // includes "Comfort"
                return Mood::Neutral;   // AQI 3 / generic OK
            }

            bool blinking(uint32_t now)
            {
                return (now % 4000) < BLINK_LEN || (now % 7300) < BLINK_LEN;
            }

            // Neutral idle frame: blink windows swap in the closed-eyes frame,
            // otherwise glance left/right on a fixed 4-step loop.
            const uint8_t *neutral_frame(uint32_t now)
            {
                if (blinking(now))
                    return Bmp::IRIS_BLINK;
                switch ((now / WANDER_STEP_MS) % 4)
                {
                case 1: return Bmp::IRIS_LOOK_LEFT;
                case 3: return Bmp::IRIS_LOOK_RIGHT;
                default: return Bmp::IRIS_NORMAL;
                }
            }

            const uint8_t *frame_for(Mood m, uint32_t now)
            {
                switch (m)
                {
                case Mood::Sleepy: return Bmp::IRIS_SLEEPY;
                case Mood::Happy: return Bmp::IRIS_HAPPY;
                case Mood::Neutral: return neutral_frame(now);
                case Mood::Uncomfortable: return Bmp::IRIS_WORRIED;
                case Mood::Sad: return Bmp::IRIS_SAD;
                case Mood::Dizzy: return Bmp::IRIS_DISORIENTED;
                }
                return Bmp::IRIS_NORMAL;
            }

        } // namespace

        void update(const SensorData &s, uint32_t now_ms)
        {
            Mood m = classify(s);
            if (m != targetMood)
            {
                targetMood = m;
                targetSince = now_ms;
            }
            else if (m != shownMood && now_ms - targetSince >= CONFIRM_MS)
            {
                shownMood = m;
                surpriseUntil = now_ms + SURPRISE_MS;
            }
        }

        void draw(Canvas &c, uint32_t now_ms)
        {
            // Surprise overlay while the transition window is open.
            const uint8_t *frame = (now_ms < surpriseUntil)
                                       ? Bmp::IRIS_SURPRISED
                                       : frame_for(shownMood, now_ms);
            c.draw_bitmap(0, 0, 128, 64, frame);
        }

    } // namespace Face
} // namespace Ui
