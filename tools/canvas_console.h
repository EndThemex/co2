#pragma once
// ============================================================================
// ConsoleCanvas: map Canvas calls onto a pixel-by-pixel ASCII grid.
// ----------------------------------------------------------------------------
// One character represents exactly ONE OLED pixel:
//   width  = 128 chars (= SCREEN_WIDTH)
//   height =  64 chars (= SCREEN_HEIGHT)
// No text overlay, no downsampling.  Text is rendered by "lighting" the
// 5x7 font glyph pixels, lines are drawn pixel by pixel with Bresenham.
// ============================================================================

#include "ui_canvas.h"
#include "ui_layout.h"

class ConsoleCanvas : public Canvas
{
public:
    static constexpr int W = Layout::SCREEN_WIDTH;  // 128
    static constexpr int H = Layout::SCREEN_HEIGHT; // 64

    ConsoleCanvas();

    void clear() override;
    void draw_text(int x, int y, int size, const char *s) override;
    void draw_line(int x0, int y0, int x1, int y1) override;
    void flush() override;

private:
    char buf_[H][W];

    // 5x7 ASCII font (0x20..0x7E), 5 bytes per char, 5 bits used (MSB = top).
    static const unsigned char *font5x7(char c);

    // Light one OLED pixel; clipped to the visible area.
    void set_pixel(int px, int py);

    // Plot a single glyph pixel into buf_ at (px,py) after applying `size`.
    void put_glyph_pixel(int px, int py, char ch, int size);
};