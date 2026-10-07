#pragma once
// ============================================================================
// Abstract Canvas interface. Hardware-independent.
//
// The same page-drawing code runs against two implementations:
//   - OledCanvas    : maps onto Adafruit_SSD1306 (firmware)
//   - ConsoleCanvas : maps onto an ASCII pixel grid (PC simulator)
// ============================================================================

#include <cstdint>
#include <cstring>

class Canvas
{
public:
    virtual ~Canvas() = default;

    virtual void clear() = 0;
    // Single lit pixel; the primitive all shape helpers below build on.
    virtual void draw_pixel(int x, int y) = 0;
    virtual void draw_text(int x, int y, int size, const char *s) = 0;
    virtual void draw_line(int x0, int y0, int x1, int y1) = 0;
    virtual void flush() = 0;

    // Width of a string rendered at the given text size.
    // Adafruit GFX default font is 6px wide (5 glyph + 1 spacing) per char.
    static int text_width(const char *s, int size)
    {
        return (int)std::strlen(s) * size * 6;
    }

    // Bitmap blit with Adafruit_GFX::drawBitmap() semantics (the format used
    // by the IrisOLED frames): row-major, byteWidth = (w+7)/8 bytes per row,
    // MSB = leftmost pixel. Implemented once here on top of draw_pixel(), so
    // the OLED firmware and the ASCII simulator rasterize identically.
    void draw_bitmap(int x, int y, int w, int h, const uint8_t *bits)
    {
        const int byte_width = (w + 7) / 8;
        for (int row = 0; row < h; ++row)
        {
            const uint8_t *line = bits + row * byte_width;
            const int py = y + row;
            for (int col = 0; col < w; ++col)
            {
                const uint8_t b = line[col / 8];
                if (b & (0x80 >> (col % 8)))
                    draw_pixel(x + col, py);
            }
        }
    }
};
