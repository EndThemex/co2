// ============================================================================
// Page drawing implementations. Depends only on the Canvas abstraction,
// so the same code renders to OLED (firmware) and ASCII grid (PC simulator).
//
// Two pages:
//   0 "Climate"     : temperature + humidity + comfort status
//   1 "Air Quality" : eCO2 + TVOC + AQI
//
// Anti-overlap rule: numbers are RIGHT-ALIGNED into fixed slots sized for the
// longest possible value (see ui_layout.h), units live in their own column.
// ============================================================================

#include "ui_pages.h"

#include <cstdio>

namespace Ui
{
    using Layout::SCREEN_WIDTH;

    // ========================================================================
    // Text dictionaries (business text). Kept local to this TU.
    // ========================================================================
    static const char *aqiShort(uint8_t a)
    {
        switch (a)
        {
        case 1: return "Excellent";
        case 2: return "Good";
        case 3: return "Moderate";
        case 4: return "Poor";
        case 5: return "Unhealthy";
        default: return "Unknown";
        }
    }

    static const char *comfortText(float t, float h)
    {
        if (t < 18.0f) return "Cold";
        if (t > 28.0f) return "Hot";
        if (h < 30.0f) return "Dry";
        if (h > 70.0f) return "Humid";
        if (t >= 20.0f && t <= 26.0f && h >= 40.0f && h <= 60.0f) return "Comfort";
        return "OK";
    }

    // ========================================================================
    // Drawing helpers
    // ========================================================================
    // Right-aligned text: the string's right edge sits at `rightEdge`.
    static void draw_text_right(Canvas &c, int rightEdge, int y, int size,
                                const char *s)
    {
        c.draw_text(rightEdge - Canvas::text_width(s, size), y, size, s);
    }

    // Header: title left, "n/N" indicator right-aligned, separator line.
    static void draw_header(Canvas &c, const char *title, uint8_t pageIdx)
    {
        c.draw_text(0, Layout::HEADER_TEXT_Y, 1, title);

        char indicator[8];
        std::snprintf(indicator, sizeof(indicator), "%u/%u",
                      (unsigned)(pageIdx + 1), (unsigned)PAGE_COUNT);
        draw_text_right(c, SCREEN_WIDTH, Layout::HEADER_TEXT_Y, 1, indicator);

        c.draw_line(0, Layout::HEADER_LINE_Y, SCREEN_WIDTH - 1,
                    Layout::HEADER_LINE_Y);
    }

    // ========================================================================
    // Page 0: Climate — temperature & humidity
    // ========================================================================
    static void draw_climate(Canvas &c, const SensorData &d, uint8_t pageIdx)
    {
        draw_header(c, "Climate", pageIdx);

        // Temperature row: number right-aligned in [54..114], unit at 116.
        c.draw_text(0, Layout::ROW1_Y + Layout::LABEL_DY, 1, "Temp");

        char num[8];
        std::snprintf(num, sizeof(num), "%.1f", d.temperature);
        draw_text_right(c, Layout::CLIMATE_NUM_RIGHT, Layout::ROW1_Y, 2, num);

        c.draw_text(Layout::CLIMATE_UNIT_X, Layout::ROW1_Y + Layout::UNIT_DY,
                    1, "\xF8"
                       "C");

        // Humidity row: number right-aligned in [78..114], unit at 116.
        c.draw_text(0, Layout::ROW2_Y + Layout::LABEL_DY, 1, "Humidity");

        std::snprintf(num, sizeof(num), "%.0f", d.humidity);
        draw_text_right(c, Layout::CLIMATE_NUM_RIGHT, Layout::ROW2_Y, 2, num);

        c.draw_text(Layout::CLIMATE_UNIT_X, Layout::ROW2_Y + Layout::UNIT_DY,
                    1, "%");

        // Footer: comfort status (max "Comfort" = 42px).
        c.draw_text(0, Layout::CLIMATE_STATUS_Y, 1,
                    comfortText(d.temperature, d.humidity));
    }

    // ========================================================================
    // Page 1: Air Quality — eCO2, TVOC, AQI
    // ========================================================================
    static void draw_air(Canvas &c, const SensorData &d, uint8_t pageIdx)
    {
        draw_header(c, "Air Quality", pageIdx);

        char num[8];

        // eCO2 row: number right-aligned in [48..108], unit at 110.
        c.draw_text(0, Layout::ROW1_Y + Layout::LABEL_DY, 1, "eCO2");
        std::snprintf(num, sizeof(num), "%u", (unsigned)d.eco2);
        draw_text_right(c, Layout::AIR_NUM_RIGHT, Layout::ROW1_Y, 2, num);
        c.draw_text(Layout::AIR_UNIT_X, Layout::ROW1_Y + Layout::UNIT_DY,
                    1, "ppm");

        // TVOC row: number right-aligned in [48..108], unit at 110.
        c.draw_text(0, Layout::ROW2_Y + Layout::LABEL_DY, 1, "TVOC");
        std::snprintf(num, sizeof(num), "%u", (unsigned)d.tvoc);
        draw_text_right(c, Layout::AIR_NUM_RIGHT, Layout::ROW2_Y, 2, num);
        c.draw_text(Layout::AIR_UNIT_X, Layout::ROW2_Y + Layout::UNIT_DY,
                    1, "ppb");

        // AQI row: single digit ends at 64, status word right-aligned to 128.
        c.draw_text(0, Layout::ROW3_Y + Layout::LABEL_DY, 1, "AQI");

        char digit[2] = {'-', '\0'};
        if (d.aqi >= 1 && d.aqi <= 5)
            digit[0] = (char)('0' + d.aqi);
        draw_text_right(c, Layout::AQI_DIGIT_RIGHT, Layout::ROW3_Y, 2, digit);

        draw_text_right(c, Layout::AQI_STATUS_RIGHT,
                        Layout::ROW3_Y + Layout::LABEL_DY, 1,
                        aqiShort(d.aqi));
    }

    // ========================================================================
    // Page table
    // ========================================================================
    const Page PAGES[] = {
        {"Climate", draw_climate},
        {"Air Quality", draw_air},
    };

    const uint8_t PAGE_COUNT = (uint8_t)(sizeof(PAGES) / sizeof(PAGES[0]));

} // namespace Ui
