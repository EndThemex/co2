#pragma once
// ============================================================================
// OledCanvas: maps Canvas calls onto an Adafruit_SSD1306 display.
//
// flush() is diff-based: it keeps a copy of the 1 KB GDDRAM image actually
// pushed last time. Unchanged content short-circuits with zero I2C traffic;
// changed content re-pushes the whole buffer (the Adafruit library only
// exposes whole-frame output). Together with the per-frame pointer cache in
// Face::draw() this makes the 30 fps mood-page tick cost only a 1 KB memcmp
// whenever the face is standing still.
// ============================================================================

#include "ui_canvas.h"

#include <cstdint>

class Adafruit_SSD1306;

class OledCanvas : public Canvas
{
public:
    explicit OledCanvas(Adafruit_SSD1306 *display) : display_(display) {}

    void clear() override;
    void draw_pixel(int x, int y) override;
    void draw_text(int x, int y, int size, const char *s) override;
    void draw_line(int x0, int y0, int x1, int y1) override;
    void flush() override;

private:
    Adafruit_SSD1306 *display_;
    uint8_t prev_[128 * 8]; // GDDRAM image as last pushed
    bool prevValid_ = false;
};
