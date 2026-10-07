#pragma once
// ============================================================================
// Abstract Canvas interface. Hardware-independent.
//
// The same page-drawing code runs against two implementations:
//   - OledCanvas    : maps onto Adafruit_SSD1306 (firmware)
//   - ConsoleCanvas : maps onto an ASCII pixel grid (PC simulator)
// ============================================================================

#include <cstdint>

class Canvas
{
public:
    virtual ~Canvas() = default;

    virtual void clear() = 0;
    // Monotonic counter, bumped by every clear(). Stateful callers (Face
    // caches the last drawn frame) use it to detect that the canvas was
    // wiped — e.g. on a page switch — and must repaint even when the frame
    // content itself is unchanged.
    uint32_t clear_epoch() const { return clearEpoch_; }
    // Single lit pixel; the primitive all shape helpers below build on.
    virtual void draw_pixel(int x, int y) = 0;
    virtual void draw_text(int x, int y, int size, const char *s) = 0;
    virtual void draw_line(int x0, int y0, int x1, int y1) = 0;
    virtual void flush() = 0;

    // Width of a string rendered at the given text size. UTF-8 aware:
    //   ASCII / Latin-1 byte -> 6px advance (5 glyph + 1 spacing) per char,
    //   CJK code point       -> FontCN::ADVANCE (12px glyph + 1px spacing).
    // Trailing spacing of the last char is included, matching the advance
    // performed by draw_text(), so right-alignment stays consistent.
    static int text_width(const char *s, int size)
    {
        int w = 0;
        for (unsigned i = 0; s[i];)
            w += char_advance(utf8_next(s, i), size);
        return w;
    }

    // Decode the next UTF-8 code point from `s` at byte index `i` (which is
    // advanced past it). Invalid or truncated sequences fall back to Latin-1
    // (the raw byte value), keeping single-byte extended glyphs such as the
    // classic-font '°' (0xF8) rendering untouched.
    static uint32_t utf8_next(const char *s, unsigned &i)
    {
        const unsigned char c = (unsigned char)s[i];
        if (c < 0x80)
        {
            i += 1;
            return c;
        }
        auto cont = [](unsigned char b)
        { return (b & 0xC0) == 0x80; };
        if ((c & 0xE0) == 0xC0 && cont((unsigned char)s[i + 1]))
        {
            i += 2;
            return ((uint32_t)(c & 0x1F) << 6) |
                   ((uint32_t)s[i - 1] & 0x3F);
        }
        if ((c & 0xF0) == 0xE0 && cont((unsigned char)s[i + 1]) &&
            cont((unsigned char)s[i + 2]))
        {
            i += 3;
            return ((uint32_t)(c & 0x0F) << 12) |
                   (((uint32_t)s[i - 2] & 0x3F) << 6) |
                   ((uint32_t)s[i - 1] & 0x3F);
        }
        i += 1;
        return c;
    }

    // Horizontal advance of one code point at the given text size.
    static int char_advance(uint32_t cp, int size)
    {
        // >= 0x100: full-width CJK cell (12px glyph + 1px spacing, FontCN).
        return (cp >= 0x100 ? 13 : 6) * size;
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

protected:
    // Implementations must call this from clear().
    void note_clear() { ++clearEpoch_; }

private:
    uint32_t clearEpoch_ = 0;
};
