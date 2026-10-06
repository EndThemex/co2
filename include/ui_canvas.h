#pragma once
// ============================================================================
// Abstract Canvas interface. Hardware-independent.
//
// The same page-drawing code runs against two implementations:
//   - OledCanvas    : maps onto Adafruit_SSD1306 (firmware)
//   - ConsoleCanvas : maps onto an ASCII pixel grid (PC simulator)
// ============================================================================

#include <cstring>

class Canvas
{
public:
    virtual ~Canvas() = default;

    virtual void clear() = 0;
    virtual void draw_text(int x, int y, int size, const char *s) = 0;
    virtual void draw_line(int x0, int y0, int x1, int y1) = 0;
    virtual void flush() = 0;

    // Width of a string rendered at the given text size.
    // Adafruit GFX default font is 6px wide (5 glyph + 1 spacing) per char.
    static int text_width(const char *s, int size)
    {
        return (int)std::strlen(s) * size * 6;
    }
};
