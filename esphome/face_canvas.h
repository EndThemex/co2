#pragma once
// ============================================================================
// ESPHome Canvas 适配器：把 ESPHome 的 display::Display 包装成固件版的
// Ui::Canvas 接口，让表情引擎（ui_face.cpp，硬件无关）零改动跑在
// ESPHome 的 display 页面 lambda 里。
//
// 用法（co2.yaml 的 page_face lambda）：
//   static EsphomeCanvas canvas; // static：clear_epoch 跨周期单调递增
//   canvas.begin(it);
//   canvas.clear();              // 标记缓冲被清空 → Face 每周期整帧重绘
//   Ui::Face::update(data, millis());
//   Ui::Face::draw(canvas, millis());
//
// 说明：
//   - ESPHome 每个 update 周期会先 auto_clear 清空显存再调页面 lambda，
//     这里的 clear() 再补一次 fill + note_clear()，语义与固件版
//     oled.clear() 一致，保证 Face::draw 的"帧未变则跳过"缓存不会把
//     空白缓冲误判为已绘制内容。
//   - draw_text / draw_line / flush 表情页用不到：数据页由 YAML lambda
//     自绘；推屏由 ESPHome 在 lambda 返回后自动完成。
//   - 本文件经 esphome.includes 拷入固件 src/ 并被自动 include 进
//     main.cpp，"esphome.h" 有 #pragma once，重复包含安全。
// ============================================================================

#include "esphome.h"

#include "ui_canvas.h"

#include "ui_face.h" // Ui::Face / Ui::SensorData（页面 lambda 使用）

class EsphomeCanvas : public Canvas
{
public:
    void begin(esphome::display::Display &d) { d_ = &d; }

    void clear() override
    {
        if (d_ != nullptr)
            d_->clear(); // fill(COLOR_OFF)；auto_clear 下幂等
        note_clear();
    }

    void draw_pixel(int x, int y) override
    {
        if (d_ != nullptr)
            d_->draw_pixel_at(x, y);
    }

    void draw_text(int, int, int, const char *) override {} // 未使用
    void draw_line(int, int, int, int) override {}          // 未使用
    void flush() override {}                                // ESPHome 自动推屏

private:
    esphome::display::Display *d_ = nullptr;
};
