// ============================================================================
// Page drawing implementations. Depends only on the Canvas abstraction,
// so the same code renders to OLED (firmware) and ASCII grid (PC simulator).
//
// Three pages (titles rendered in Chinese via the FontCN whitelist):
//   0 "温湿度"   : temperature + humidity + comfort status
//   1 "空气质量" : eCO2 + TVOC + AQI
//   2 "表情"     : animated face reflecting air/comfort (see ui_face.h)
//
// Anti-overlap rule: numbers are RIGHT-ALIGNED into fixed slots sized for the
// longest possible value (see ui_layout.h), units live in their own column.
// ============================================================================

#include "ui_pages.h"

#include "ui_face.h"

#include <cstdio>

namespace Ui
{
    using Layout::SCREEN_WIDTH;

    // ========================================================================
    // Comfort classification (single source of truth for the thresholds).
    // ========================================================================
    Comfort comfort_category(float t, float h)
    {
        if (t < 18.0f) return Comfort::Cold;
        if (t > 28.0f) return Comfort::Hot;
        if (h < 30.0f) return Comfort::Dry;
        if (h > 70.0f) return Comfort::Humid;
        if (t >= 20.0f && t <= 26.0f && h >= 40.0f && h <= 60.0f) return Comfort::Comfort;
        return Comfort::Ok;
    }

    // ========================================================================
    // Text dictionaries (business text, UTF-8 Chinese). Kept local to this TU.
    // Every Chinese glyph used here must be whitelisted in tools/gen_cn_font.py.
    // ========================================================================
    static const char *aqiShort(uint8_t a)
    {
        switch (a)
        {
        case 1: return "优";
        case 2: return "良";
        case 3: return "中";
        case 4: return "差";
        case 5: return "很差";
        default: return "未知";
        }
    }

    static const char *comfortText(Comfort cf)
    {
        switch (cf)
        {
        case Comfort::Cold: return "偏冷";
        case Comfort::Hot: return "偏热";
        case Comfort::Dry: return "干燥";
        case Comfort::Humid: return "潮湿";
        case Comfort::Comfort: return "舒适";
        default: return "正常";
        }
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
    static void draw_climate(Canvas &c, const SensorData &d, uint8_t pageIdx,
                             uint32_t /*now_ms*/)
    {
        draw_header(c, "温湿度", pageIdx);

        // Temperature row: number right-aligned in [54..114], unit at 116.
        c.draw_text(0, Layout::ROW1_Y + Layout::LABEL_DY, 1, "温度");

        char num[8];
        std::snprintf(num, sizeof(num), "%.1f", d.temperature);
        draw_text_right(c, Layout::CLIMATE_NUM_RIGHT, Layout::ROW1_Y, 2, num);

        c.draw_text(Layout::CLIMATE_UNIT_X, Layout::ROW1_Y + Layout::UNIT_DY,
                    1, "\xF8"
                       "C");

        // Humidity row: number right-aligned in [78..114], unit at 116.
        c.draw_text(0, Layout::ROW2_Y + Layout::LABEL_DY, 1, "湿度");

        std::snprintf(num, sizeof(num), "%.0f", d.humidity);
        draw_text_right(c, Layout::CLIMATE_NUM_RIGHT, Layout::ROW2_Y, 2, num);

        c.draw_text(Layout::CLIMATE_UNIT_X, Layout::ROW2_Y + Layout::UNIT_DY,
                    1, "%");

        // Footer: comfort status (max "潮湿" = 25px).
        c.draw_text(0, Layout::CLIMATE_STATUS_Y, 1,
                    comfortText(comfort_category(d.temperature, d.humidity)));
    }

    // ========================================================================
    // Page 1: Air Quality — eCO2, TVOC, AQI
    // ========================================================================
    static void draw_air(Canvas &c, const SensorData &d, uint8_t pageIdx,
                         uint32_t /*now_ms*/)
    {
        draw_header(c, "空气质量", pageIdx);

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
    // Page 2: Mood — full-screen IrisOLED-style expression (no header/caption:
    // the frames are standalone full-screen art, ink spans nearly all rows).
    // ========================================================================
    static void draw_mood(Canvas &c, const SensorData & /*d*/, uint8_t /*pageIdx*/,
                          uint32_t now_ms)
    {
        Face::draw(c, now_ms);
    }

    // ========================================================================
    // Page table (titles are UTF-8 Chinese; the Mood page itself is
    // headerless full-screen art, its title is only used by the simulator).
    // ========================================================================
    const Page PAGES[] = {
        {"温湿度", draw_climate},
        {"空气质量", draw_air},
        {"表情", draw_mood},
    };

    const uint8_t PAGE_COUNT = (uint8_t)(sizeof(PAGES) / sizeof(PAGES[0]));

} // namespace Ui
