#pragma once
// ============================================================================
// OledCanvas: maps Canvas calls onto an Adafruit_SSD1306 display.
// ============================================================================

#include "ui_canvas.h"

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
};
