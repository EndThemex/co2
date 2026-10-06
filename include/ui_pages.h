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

    struct Page
    {
        const char *title;
        void (*draw)(Canvas &, const SensorData &, uint8_t pageIdx);
    };

    extern const Page PAGES[];
    extern const uint8_t PAGE_COUNT;

} // namespace Ui
