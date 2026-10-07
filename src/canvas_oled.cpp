// ============================================================================
// OledCanvas implementation.
// ============================================================================

#include "canvas_oled.h"

#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#include <cstring>

#include "font_cn.h"

void OledCanvas::clear()
{
    display_->clearDisplay();
    note_clear();
}

void OledCanvas::draw_pixel(int x, int y)
{
    display_->drawPixel(x, y, SSD1306_WHITE);
}

void OledCanvas::draw_text(int x, int y, int size, const char *s)
{
    display_->setTextSize(size);
    display_->setTextColor(SSD1306_WHITE);

    int cx = x;
    for (unsigned i = 0; s[i];)
    {
        const uint32_t cp = Canvas::utf8_next(s, i);
        if (cp < 0x100)
        {
            // ASCII / Latin-1 byte (incl. '°' 0xF8): classic GFX font.
            display_->setCursor(cx, y);
            display_->print((char)cp);
        }
        else
        {
            // CJK: 12x12 bitmap glyph from the generated whitelist.
            if (const FontCN::Glyph *g = FontCN::find(cp))
            {
                if (size == 1)
                {
                    draw_bitmap(cx, y, FontCN::W, FontCN::H, g->bits);
                }
                else
                {
                    for (int row = 0; row < FontCN::H; ++row)
                        for (int col = 0; col < FontCN::W; ++col)
                            if (g->bits[row * 2 + col / 8] &
                                (0x80 >> (col % 8)))
                                for (int dy = 0; dy < size; ++dy)
                                    for (int dx = 0; dx < size; ++dx)
                                        draw_pixel(cx + col * size + dx,
                                                   y + row * size + dy);
                }
            }
        }
        cx += Canvas::char_advance(cp, size);
    }
}

void OledCanvas::draw_line(int x0, int y0, int x1, int y1)
{
    display_->drawLine(x0, y0, x1, y1, SSD1306_WHITE);
}

void OledCanvas::flush()
{
    constexpr size_t BUF_N = 128 * 8;
    uint8_t *buf = display_->getBuffer();

    if (prevValid_ && memcmp(buf, prev_, BUF_N) == 0)
        return; // pixel-identical to what GDDRAM already shows: no I2C

    display_->display(); // Adafruit's whole-buffer push
    memcpy(prev_, buf, BUF_N);
    prevValid_ = true;
}
