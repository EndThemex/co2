#pragma once
// ============================================================================
// Page declarations. Hardware-independent (depends only on Canvas + layout).
// ============================================================================

#include "ui_canvas.h"
#include "ui_layout.h"

#include <cstdint>

namespace Ui
{
    struct SensorData
    {
        float temperature;
        float humidity;
        uint16_t tvoc;
        uint16_t eco2;
        uint8_t aqi;
    };

    // Comfort category shared by the Climate footer text and the Face mood
    // logic (single source of truth for the thresholds).
    enum class Comfort : uint8_t
    {
        Cold,
        Hot,
        Dry,
        Humid,
        Comfort,
        Ok
    };
    Comfort comfort_category(float t, float h);

    struct Page
    {
        const char *title;
        // now_ms lets time-driven pages (the animated Mood face) render
        // deterministic frames; data pages ignore it.
        void (*draw)(Canvas &, const SensorData &, uint8_t pageIdx,
                     uint32_t now_ms);
    };

    extern const Page PAGES[];
    extern const uint8_t PAGE_COUNT;

} // namespace Ui
