// ============================================================================
// Page drawing implementations. Depends only on the Canvas abstraction,
// so the same code renders to OLED (firmware) and ASCII grid (PC simulator).
//
// Three pages (titles rendered in Chinese via the FontCN whitelist):
//   0 "温湿度"   : temperature + humidity + comfort status
//   1 "空气质量" : eCO2 + TVOC + AQI
//   2 "表情"     : animated face reflecting air/comfort (see ui_face.h)
//
// Anti-overlap rule: each data page uses two 64px columns. Label + unit share
// one line above the number; the number is LEFT-aligned in its column and is
// sized for the longest possible value (see ui_layout.h).
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
        if (t < 18.0f)
            return Comfort::Cold;
        if (t > 28.0f)
            return Comfort::Hot;
        if (h < 30.0f)
            return Comfort::Dry;
        if (h > 70.0f)
            return Comfort::Humid;
        if (t >= 20.0f && t <= 26.0f && h >= 40.0f && h <= 60.0f)
            return Comfort::Comfort;
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
        case 1:
            return "优";
        case 2:
            return "良";
        case 3:
            return "中";
        case 4:
            return "差";
        case 5:
            return "很差";
        default:
            return "未知";
        }
    }

    static const char *comfortText(Comfort cf)
    {
        switch (cf)
        {
        case Comfort::Cold:
            return "偏冷";
        case Comfort::Hot:
            return "偏热";
        case Comfort::Dry:
            return "干燥";
        case Comfort::Humid:
            return "潮湿";
        case Comfort::Comfort:
            return "舒适";
        default:
            return "正常";
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

    // ========================================================================
    // Page 0: Climate — temperature & humidity
    // ========================================================================
    static void draw_climate(Canvas &c, const SensorData &d,
                             uint8_t /*pageIdx*/, uint32_t /*now_ms*/)
    {
        // Left column: temperature. Label + unit share the label line; the
        // 5-char worst case ("-45.5" = 60px) still fits the 64px column.
        c.draw_text(Layout::CLIMATE_TEMP_LABEL_X, Layout::CJK_LABEL_Y, 1, "温度");
        c.draw_text(Layout::CLIMATE_TEMP_UNIT_X, Layout::CLIMATE_TEMP_UNIT_Y, 1,
                    "\xF8"
                    "C");

        char num[8];
        std::snprintf(num, sizeof(num), "%.1f", d.temperature);
        c.draw_value(Layout::CLIMATE_TEMP_VALUE_X, Layout::VALUE_Y, num);

        // Right column: humidity. "100" = 36px, well inside 64px.
        c.draw_text(Layout::CLIMATE_HUM_LABEL_X, Layout::CJK_LABEL_Y, 1, "湿度");
        c.draw_text(Layout::CLIMATE_HUM_UNIT_X, Layout::CLIMATE_HUM_UNIT_Y, 1, "%");

        std::snprintf(num, sizeof(num), "%.0f", d.humidity);
        c.draw_value(Layout::CLIMATE_HUM_VALUE_X, Layout::VALUE_Y, num);

        // Status band: the comfort word, centered on the left column axis.
        const char *status = comfortText(comfort_category(d.temperature, d.humidity));
        const int sw = Canvas::text_width(status, 1);
        c.draw_text(Layout::CLIMATE_STATUS_CENTER_X - sw / 2, Layout::CLIMATE_STATUS_Y,
                    1, status);
    }

    // ========================================================================
    // Page 1: Air Quality — eCO2, TVOC, AQI
    // ========================================================================
    static void draw_air(Canvas &c, const SensorData &d,
                         uint8_t /*pageIdx*/, uint32_t /*now_ms*/)
    {
        char num[8];

        // Left column: eCO2. "65535" = 60px, inside the 64px column.
        c.draw_text(Layout::TILE1_X, Layout::ASCII_LABEL_Y, 1, "eCO2");
        c.draw_text(Layout::AIR_ECO2_UNIT_X, Layout::UNIT_Y, 1, "ppm");
        std::snprintf(num, sizeof(num), "%u", (unsigned)d.eco2);
        c.draw_value(Layout::TILE1_X, Layout::VALUE_Y, num);

        // Right column: TVOC.
        c.draw_text(Layout::TILE2_X, Layout::ASCII_LABEL_Y, 1, "TVOC");
        c.draw_text(Layout::AIR_TVOC_UNIT_X, Layout::UNIT_Y, 1, "ppb");
        std::snprintf(num, sizeof(num), "%u", (unsigned)d.tvoc);
        c.draw_value(Layout::TILE2_X, Layout::VALUE_Y, num);

        // Status band: the AQI row.
        c.draw_text(Layout::TILE1_X, Layout::AQI_LABEL_Y, 1, "AQI");

        char digit[2] = {'-', '\0'};
        if (d.aqi >= 1 && d.aqi <= 5)
            digit[0] = (char)('0' + d.aqi);
        c.draw_text(Layout::AQI_DIGIT_X, Layout::AQI_DIGIT_Y, 2, digit);

        c.draw_text(Layout::AQI_QUALITY_X, Layout::AQI_STATUS_Y, 1, "质量");
        draw_text_right(c, Layout::AQI_STATUS_RIGHT, Layout::AQI_STATUS_Y, 1,
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
