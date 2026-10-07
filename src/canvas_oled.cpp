// ============================================================================
// OledCanvas implementation.
// ============================================================================

#include "canvas_oled.h"

#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

void OledCanvas::clear()
{
    display_->clearDisplay();
}

void OledCanvas::draw_pixel(int x, int y)
{
    display_->drawPixel(x, y, SSD1306_WHITE);
}

void OledCanvas::draw_text(int x, int y, int size, const char *s)
{
    display_->setTextSize(size);
    display_->setTextColor(SSD1306_WHITE);
    display_->setCursor(x, y);
    display_->print(s);
}

void OledCanvas::draw_line(int x0, int y0, int x1, int y1)
{
    display_->drawLine(x0, y0, x1, y1, SSD1306_WHITE);
}

void OledCanvas::flush()
{
    display_->display();
}
