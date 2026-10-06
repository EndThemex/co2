# UI 抽离 + 控制台模拟渲染器 —— 实施计划

> 目标：把 `src/main.cpp` 中所有屏幕布局/绘制相关代码抽离到独立模块，并通过抽象 `Canvas` 接口让同一份 UI 代码既能驱动 SSD1306 OLED，也能在 PC 控制台以 ASCII 字符预览。

## 1. 范围与非目标

**在做**
- 抽离布局常量（`Layout::*`、`SCREEN_WIDTH/HEIGHT`）。
- 抽离绘制原语、页面声明与页面函数（`drawHeader/drawRow/4 个页面/PAGES[]`）。
- 抽象 `Canvas` 接口，提供 OLED / 控制台两套实现。
- 提供 PC 端的 ASCII 模拟渲染器（`tools/ui_sim`）。

**不在做（保留在 main.cpp）**
- 引脚宏、I2C、`readAHT30`、ENS160 驱动、`setup/loop`、分页定时器。
- 文本字典（`getAQILevelShort/getCO2Hint/getTOVHint/getComfort`）—— 属于业务文本，不属于布局。

## 2. 目标文件结构

```
co2/
├── include/
│   ├── ui_layout.h        布局常量（不依赖 Arduino）
│   ├── ui_canvas.h        抽象 Canvas 接口（不依赖 Arduino）
│   └── ui_pages.h         Row/Align/Page 声明 + 页面函数原型
├── src/
│   ├── main.cpp           精简后的主程序
│   ├── ui_pages.cpp       页面绘制实现（仅依赖 Canvas 抽象）
│   ├── canvas_oled.h
│   └── canvas_oled.cpp    把 Canvas 调用映射到 Adafruit_SSD1306
└── tools/
    ├── ui_sim.cpp         模拟器入口
    ├── canvas_console.h
    ├── canvas_console.cpp 把 Canvas 调用映射到 ASCII 控制台
    └── Makefile           独立编译该模拟器
```

## 3. 关键设计

### 3.1 `include/ui_layout.h`
- 原样搬迁 `Layout` 命名空间全部常量。
- 搬迁 `SCREEN_WIDTH/HEIGHT`。
- 不引入 Arduino 头。

### 3.2 `include/ui_canvas.h`
```cpp
class Canvas {
public:
    virtual ~Canvas() = default;
    virtual void clear() = 0;
    virtual void draw_text(int x, int y, int size, const char* s) = 0;
    virtual void draw_line(int x0, int y0, int x1, int y1) = 0;
    virtual void flush() = 0;

    static int text_width(const char* s, int size) {
        return (int)strlen(s) * size * 6;
    }
};
```
- 不引入 Arduino 头。
- 所有页面函数统一接收 `Canvas&`。

### 3.3 `include/ui_pages.h` / `src/ui_pages.cpp`
- `enum class Align { Left, Right, Center }`。
- `struct Row { uint8_t x, y, w, size; Align align; const char* text; };`
- `struct SensorData { float temperature, humidity; uint16_t tvoc, eco2; uint8_t aqi; };`
- 函数：
  - `void draw_header(Canvas& c, const char* title, uint8_t pageIdx, uint8_t pageCount);`
  - `void draw_row(Canvas& c, const Row& r);`
  - `void draw_dashboard(Canvas& c, const SensorData& d);`
  - `void draw_co2(Canvas& c, const SensorData& d);`
  - `void draw_tvoc(Canvas& c, const SensorData& d);`
  - `void draw_comfort(Canvas& c, const SensorData& d);`
- 页面分发表：
```cpp
struct Page { const char* title; void (*draw)(Canvas&, const SensorData&); };
extern const Page PAGES[];
extern const uint8_t PAGE_COUNT;
```
- `PAGE_COUNT = sizeof(PAGES)/sizeof(PAGES[0])`。
- 所有原本直接 `display.xxx` 的语句改为 `c.xxx`。
- 文本字典（`getAQILevelShort/...`）作为 `static` 函数留在本文件内，供页面函数调用；保证 `ui_pages.cpp` 不依赖 `main.cpp`。

### 3.4 `src/canvas_oled.{h,cpp}`
```cpp
class OledCanvas : public Canvas {
    Adafruit_SSD1306* d_;
public:
    explicit OledCanvas(Adafruit_SSD1306* d) : d_(d) {}
    void clear()        override { d_->clearDisplay(); }
    void draw_text(int x,int y,int size,const char* s) override {
        d_->setTextSize(size); d_->setCursor(x,y); d_->print(s);
    }
    void draw_line(int x0,int y0,int x1,int y1) override {
        d_->drawLine(x0,y0,x1,y1, SSD1306_WHITE);
    }
    void flush()        override { d_->display(); }
};
```

### 3.5 `tools/canvas_console.{h,cpp}`（ASCII 渲染）
- 内部维护两张缓冲区：
  - 像素缓冲 `pix[H][W]`（H=16 行, W=64 列，比例 4×2 = OLED 128×64）。
  - 文字叠层 `txt[H][W]`（空格初值）。
- `clear()`：两张缓冲清空。
- `draw_text(x, y, size, s)`：
  - 起始格 `cx = x/2, cy = y/4`；每个 ASCII 字符宽 = `size*3` 格（基于 5×7 字体 + 间距 ≈ 6px@size=1 → 3 格@2px/格）。
  - 将每个字符从简易 5×7 位图（内置 mini5x7 数组）逐格写到 `txt[cy][cx..]`；超出区域裁掉。
- `draw_line(x0,y0,x1,y1)`：Bresenham 整数算法，将每个亮像素写到 `pix[y/4][x/2]`。
- 特殊处理：当一条线为水平时（`y0==y1`），整行用 `─` 替代 `#`，让标题分隔线/底部分隔线更接近"线"的视觉。
- `flush()`：ANSI 清屏 `\x1b[2J\x1b[H` + 顶部/底部分别输出 `+---+...+` 边框，再逐行合并 `txt > pix` 字符输出。

### 3.6 `tools/ui_sim.cpp`
- 准备一组固定的 `SensorData`（例如 `T=23.4, H=55, eCO2=742, TVOC=187, AQI=2`）。
- 提供两种运行模式：
  - 默认：连续打印 4 页，每页之间暂停等用户回车。
  - 命令行 `--all`：一次性打印全部 4 页（便于脚本/CI 抓快照）。
- 标题用 `"Preview"`（区别于真机上的真实标题）。

### 3.7 `tools/Makefile`
```makefile
CXX      ?= g++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Iinclude
SRCS      = ui_sim.cpp canvas_console.cpp ../src/ui_pages.cpp
TARGET    = ui_sim
all: $(TARGET)
$(TARGET): $(SRCS)
	$(CXX) $(CXXFLAGS) $^ -o $@
clean:
	rm -f $(TARGET)
run: $(TARGET)
	./$(TARGET)
```

## 4. main.cpp 改动要点
- 删除所有布局/页面绘制代码。
- 增加 `#include "ui_canvas.h"`、`#include "ui_pages.h"`、`#include "canvas_oled.h"`。
- 用全局 `OledCanvas oled(&display);` 替换原先的 `display.xxx`。
- 替换 `updateDisplay()` 实现为：
```cpp
void updateDisplay() {
    if (currentPage >= Ui::PAGE_COUNT) currentPage = 0;
    SensorData s{ temperature, humidity, tvoc, eco2, aqi };
    oled.clear();
    Ui::PAGES[currentPage].draw(oled, s);
    oled.flush();
}
```
- `setup()` 中 `display.begin(...)` 行为不变。

## 5. 验证清单
- [ ] PlatformIO 仍能成功编译 `airm2m_core_esp32c3` 环境。
- [ ] `tools/Makefile` 编译 `ui_sim` 无 warning（除故意 `-Wall` 触发的标准库提示）。
- [ ] 模拟器输出能看到：标题栏 + 分隔线 + 页码 `n/4`，每个页面文字位置与真机一致。
- [ ] 4 页全部渲染无越界、无字符残影。

## 6. 风险与对策
- **Page/Row 数组的全局 `static` 局部变量**：原 `main.cpp` 中部分 `Row[]` 在函数内声明。改为把 `Row` 移到 `ui_pages.cpp` 内是允许的，但需保证函数体内 `auto` 仍可工作（`ui_pages.cpp` 是非 Arduino 实现，完全支持）。
- **`strlen` 链接**：模拟器与真机都需要 `<cstring>`，在 `ui_canvas.h` 中显式 `#include <cstring>`。
- **文字叠层 vs 像素层冲突**：当分隔线穿过文本底部时，文字优先显示（`flush()` 时 `txt>0?txt:pix`），这样和 OLED 实际渲染顺序（先画线、再 print 文字）一致。
- **`Page` 函数字段类型一致性**：原 `void(*draw)()`，改造后为 `void(*draw)(Canvas&, const SensorData&)`，配套在两个 `draw_*` 中分别传入 `Canvas&`。