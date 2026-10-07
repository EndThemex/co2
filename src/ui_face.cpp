// ============================================================================
// Face implementation. See ui_face.h for the mood/timing design.
//
// All animation is deterministic in now_ms:
//   blink  : (now % 4000) < 240 || (now % 7300) < 240  -> pseudo-random feel
//   wander : 6-step 2D loop, one step per 2200 ms
//   wink   : happy winks on 11 s / 13.7 s coprime windows
//   bounce : excited alternates with happy every 400 ms
//   bored  : after 60 s of stable neutral, 2.2 s flash every 24 s
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
            constexpr uint32_t CONFIRM_MS = 3000;   // candidate must hold this long
            constexpr uint32_t TRANSITION_MS = 800; // transition frame window
            constexpr uint32_t BLINK_LEN = 240;     // closed-eye duration
            constexpr uint32_t WANDER_STEP_MS = 2200;
            constexpr uint32_t BORE_DELAY_MS = 60000;  // stable-neutral before boredom
            constexpr uint32_t BORE_PERIOD_MS = 24000; // boredom cadence
            constexpr uint32_t BORE_LEN_MS = 2200;     // boredom flash length

            // ---- Mood state -------------------------------------------------
            Mood shownMood = Mood::Sleepy;  // what the face displays
            Mood targetMood = Mood::Sleepy; // latest sensor-derived candidate
            uint32_t targetSince = 0;       // when the candidate first appeared
            uint32_t shownSince = 0;        // when shownMood was adopted
            uint32_t transitionUntil = 0;   // transition frame window
            const uint8_t *transitionFrame = Bmp::IRIS_SURPRISED;

            // Severity ranking for transition flavor: adopting a strictly
            // worse mood startles with "scared", anything else "surprised".
            uint8_t rank(Mood m)
            {
                switch (m)
                {
                case Mood::Excited: return 0;
                case Mood::Happy: return 1;
                case Mood::Neutral: return 2;
                case Mood::Uncomfortable: return 3;
                case Mood::Sad: return 4;
                case Mood::Angry: return 5;
                case Mood::Dizzy: return 6;
                case Mood::Sleepy: return 2;
                }
                return 2;
            }

            Mood classify(const SensorData &d)
            {
                if (d.aqi == 0)
                    return Mood::Sleepy; // ENS160 warming up / no valid data
                if (d.tvoc >= 2000)
                    return Mood::Dizzy; // extreme overload dominates
                if (d.aqi >= 5)
                    return Mood::Angry;
                if (d.aqi == 4)
                    return Mood::Sad;

                Comfort cf = comfort_category(d.temperature, d.humidity);
                if (cf != Comfort::Comfort && cf != Comfort::Ok)
                    return Mood::Uncomfortable;

                if (d.aqi == 1 && d.eco2 <= 600 && d.tvoc <= 150)
                    return Mood::Excited; // textbook fresh air
                if (d.aqi <= 2)
                    return Mood::Happy;
                return Mood::Neutral; // AQI 3 / generic OK
            }

            bool blinking(uint32_t now)
            {
                return (now % 4000) < BLINK_LEN || (now % 7300) < BLINK_LEN;
            }

            // Neutral idle frame: 6-step 2D wander (rest -> left -> up ->
            // rest -> right -> down); blink windows swap in the matching
            // eye-variant while looking up/down. Long stable stretches flash
            // a bored face periodically.
            const uint8_t *neutral_frame(uint32_t now)
            {
                if (now - shownSince > BORE_DELAY_MS &&
                    now % BORE_PERIOD_MS < BORE_LEN_MS)
                    return Bmp::IRIS_BORED;
                const uint32_t phase = (now / WANDER_STEP_MS) % 6;
                if (blinking(now))
                {
                    switch (phase)
                    {
                    case 2: return Bmp::IRIS_BLINK_UP;
                    case 5: return Bmp::IRIS_BLINK_DOWN;
                    default: return Bmp::IRIS_BLINK;
                    }
                }
                switch (phase)
                {
                case 1: return Bmp::IRIS_LOOK_LEFT;
                case 2: return Bmp::IRIS_LOOK_UP;
                case 4: return Bmp::IRIS_LOOK_RIGHT;
                case 5: return Bmp::IRIS_LOOK_DOWN;
                default: return Bmp::IRIS_NORMAL;
                }
            }

            // Happy idle frame: quick winks on two long coprime periods so
            // they feel spontaneous rather than metronomic.
            const uint8_t *happy_frame(uint32_t now)
            {
                if (now % 11000 < BLINK_LEN)
                    return Bmp::IRIS_WINK_RIGHT;
                if (now % 13700 < BLINK_LEN)
                    return Bmp::IRIS_WINK_LEFT;
                return Bmp::IRIS_HAPPY;
            }

            const uint8_t *frame_for(Mood m, uint32_t now)
            {
                switch (m)
                {
                case Mood::Sleepy: return Bmp::IRIS_SLEEPY;
                case Mood::Excited: // cheerful bounce
                    return ((now / 400) % 2) ? Bmp::IRIS_HAPPY : Bmp::IRIS_EXCITED;
                case Mood::Happy: return happy_frame(now);
                case Mood::Neutral: return neutral_frame(now);
                case Mood::Uncomfortable: return Bmp::IRIS_WORRIED;
                case Mood::Sad: return Bmp::IRIS_SAD;
                case Mood::Angry: return Bmp::IRIS_ANGRY;
                case Mood::Dizzy: return Bmp::IRIS_DISORIENTED;
                }
                return Bmp::IRIS_NORMAL;
            }

            // ---- Repaint cache ---------------------------------------------
            // Identical consecutive frames skip the full 8192-pixel blit;
            // the epoch guard forces a repaint after any Canvas::clear()
            // (page switches wipe the buffer).
            const uint8_t *lastDrawn = nullptr;
            uint32_t lastEpoch = 0;

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
                transitionFrame = (rank(m) > rank(shownMood)) ? Bmp::IRIS_SCARED
                                                              : Bmp::IRIS_SURPRISED;
                shownMood = m;
                shownSince = now_ms;
                transitionUntil = now_ms + TRANSITION_MS;
            }
        }

        void draw(Canvas &c, uint32_t now_ms)
        {
            // Transition frame while the window is open, else the mood frame.
            const uint8_t *frame = (now_ms < transitionUntil)
                                       ? transitionFrame
                                       : frame_for(shownMood, now_ms);
            if (frame != lastDrawn || c.clear_epoch() != lastEpoch)
            {
                c.draw_bitmap(0, 0, 128, 64, frame);
                lastDrawn = frame;
                lastEpoch = c.clear_epoch();
            }
        }

    } // namespace Face
} // namespace Ui
