/**
 * 室内空气质量监测仪 — 主程序
 * 硬件: ESP32-C3 (LuAT airm2m_core_esp32c3) + ENS160 + AHT30 + SSD1306 128x64 OLED
 *
 * Author: EndTheme
 */

#define SOC_HP_I2C_NUM 2 // ESP32-C3 专用：解决 I2C 编译问题

#include <Arduino.h>
#include <Wire.h>
#include <SparkFun_ENS160.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#include "ui_layout.h"
#include "ui_pages.h"
#include "ui_face.h"
#include "canvas_oled.h"

// ====== 引脚定义 ======
#define I2C_SDA 8
#define I2C_SCL 9
#define AHT30_ADDRESS 0x38
#define AHT30_TRIGGER_CMD 0xAC

// ====== OLED 屏幕定义 ======
#define OLED_ADDR 0x3C
#define OLED_RESET -1

// ====== 温湿度校准（双轨 + 分段偏移）======
// ENS160 工作时加热丝会让模块升温 3~6°C，AHT30 测到的是"模块微环境"温度。
// 原则：原始值直接喂给 ENS160 补偿（同一热环境，反而更准）；
//      显示值按原始值所在区间取对应偏移（自热温升随环境条件变化，单点偏移不够准）。
// 偏移量需用参考温度计在不同温湿度段实测后调整；表尾一段 upper 用 INFINITY 兜底。
struct OffsetSegment
{
    float upper;  // 原始值区间上限（含）
    float offset; // 该区间的显示偏移
};

// 温度偏移表：按 AHT30 原始温度分段 (°C)
static const OffsetSegment TEMP_OFFSET_TABLE[] = {
    {15.0f, -3.5f},   // 原始温度 ≤15°C
    {25.0f, -5.0f},   // 15~25°C
    {INFINITY, -6.0f} // >25°C
};

// 湿度偏移表：按 AHT30 原始湿度分段 (%RH)，温度虚高会使 RH 系统性偏低
static const OffsetSegment HUM_OFFSET_TABLE[] = {
    {40.0f, 16.0f},  // 原始湿度 ≤40%
    {70.0f, 10.0f},  // 40~70%
    {INFINITY, 8.0f} // >70%
};

// 在分段偏移表中查找 value 所在区间的偏移值
template <size_t N>
static float lookupOffset(const OffsetSegment (&table)[N], float value)
{
    for (size_t i = 0; i < N; i++)
    {
        if (value <= table[i].upper)
            return table[i].offset;
    }
    return table[N - 1].offset;
}

// ====== 界面分页参数 ======
#define PAGE_INTERVAL_MS 8000  // 每页停留时间（ms）
#define SENSOR_PERIOD_MS 1000  // 传感器读取/情绪评估周期（ms）
#define MOOD_FRAME_MS 33       // 表情页动画帧间隔（~30fps；差分刷新使静止帧
                               // 仅有 1KB memcmp 开销，I2C 只在实际画面
                               // 变化时才传输）
#define DATA_FRAME_MS 500      // 数据页重绘间隔

// ============================================================================
// 对象声明
// ============================================================================
SparkFun_ENS160 ens160;
Adafruit_SSD1306 display(Layout::SCREEN_WIDTH, Layout::SCREEN_HEIGHT, &Wire, OLED_RESET);
OledCanvas oled(&display);

// ============================================================================
// 数据存储
// ============================================================================
float temperature = 0, humidity = 0;         // AHT30 原始值 → ENS160 补偿
float temperatureDisp = 0, humidityDisp = 0; // 校准值 → OLED 显示
uint16_t tvoc = 0, eco2 = 0;
uint8_t aqi = 0;

// ============================================================================
// 分页状态
// ============================================================================
static uint8_t currentPage = 0;
static uint32_t lastPageSwitchMs = 0;
static uint32_t lastSensorMs = 0;
static uint32_t lastFrameMs = 0;

// ============================================================================
// AHT30 直读
// ============================================================================
bool readAHT30(float *temperature, float *humidity)
{
    Wire.beginTransmission(AHT30_ADDRESS);
    Wire.write(AHT30_TRIGGER_CMD);
    Wire.write(0x33);
    Wire.write(0x00);
    if (Wire.endTransmission() != 0)
        return false;

    delay(80);

    Wire.requestFrom(AHT30_ADDRESS, 7);
    if (Wire.available() < 7)
        return false;

    uint8_t data[7];
    for (int i = 0; i < 7; i++)
        data[i] = Wire.read();

    if (data[0] & 0x80)
        return false;

    uint32_t rawHumidity = ((uint32_t)data[1] << 12) |
                           ((uint32_t)data[2] << 4) |
                           ((uint32_t)data[3] >> 4);
    uint32_t rawTemperature = (((uint32_t)data[3] & 0x0F) << 16) |
                              ((uint32_t)data[4] << 8) |
                              (uint32_t)data[5];

    *humidity = (rawHumidity * 100.0) / 1048576.0;
    *temperature = ((rawTemperature * 200.0) / 1048576.0) - 50.0;
    return true;
}

// ============================================================================
// 显示刷新
// ============================================================================
void updateDisplay(uint32_t now)
{
    if (currentPage >= Ui::PAGE_COUNT)
        currentPage = 0;

    Ui::SensorData s{temperatureDisp, humidityDisp, tvoc, eco2, aqi};
    oled.clear();
    Ui::PAGES[currentPage].draw(oled, s, currentPage, now);
    oled.flush();
}

// ============================================================================
// 初始化
// ============================================================================
void setup()
{
    Serial.begin(115200);
    delay(1000);
    Serial.println("\n=== Air Quality Monitor Starting ===");

    Wire.begin(I2C_SDA, I2C_SCL);
    // 400kHz: SSD1306 全屏刷新（1KB 缓冲）在 100kHz 下约 100ms，会拖慢
    // 表情页动画帧率；AHT30/ENS160 均支持 400kHz。
    Wire.setClock(400000);

    if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR))
    {
        Serial.println("OLED not detected!");
    }
    else
    {
        Serial.println("OLED OK");
        display.clearDisplay();
        display.display();
    }

    if (!ens160.begin())
    {
        Serial.println("ENS160 not detected!");
    }
    else
    {
        Serial.println("ENS160 OK");
        ens160.setOperatingMode(SFE_ENS160_RESET);
        delay(10);
        ens160.setOperatingMode(SFE_ENS160_IDLE);
        delay(10);
        ens160.setOperatingMode(SFE_ENS160_STANDARD);
    }

    Serial.println("Ready...");
    delay(1000);
    lastPageSwitchMs = millis();
}

// ============================================================================
// 主循环（非阻塞：传感器 1Hz、表情动画 ~30fps、分页 8s 轮播）
// 表情帧与屏幕内容均为差分刷新：静止帧仅 memcmp，实际变化才推 I2C。
// ============================================================================
void loop()
{
    uint32_t now = millis();

    // ---- 传感器采样 + 情绪评估（1Hz）----
    if (now - lastSensorMs >= SENSOR_PERIOD_MS)
    {
        lastSensorMs = now;

        if (readAHT30(&temperature, &humidity))
        {
            // 原始值喂 ENS160 补偿（微环境真实温湿度）
            ens160.setTempCompensationCelsius(temperature);
            ens160.setRHCompensationFloat(humidity);

            // 校准值仅供显示：按原始值所在区间取对应偏移
            temperatureDisp = temperature + lookupOffset(TEMP_OFFSET_TABLE, temperature);
            humidityDisp = constrain(humidity + lookupOffset(HUM_OFFSET_TABLE, humidity), 0.0f, 100.0f);
        }

        if (ens160.checkDataStatus())
        {
            aqi = ens160.getAQI();
            tvoc = ens160.getTVOC();
            eco2 = ens160.getECO2();
        }

        Serial.printf("Page:%u | T:%.2f (disp %.2f) C | H:%.2f (disp %.2f) %% | eCO2:%u ppm | TVOC:%u ppb | AQI:%u\n",
                      currentPage, temperature, temperatureDisp, humidity, humidityDisp, eco2, tvoc, aqi);

        Ui::SensorData s{temperatureDisp, humidityDisp, tvoc, eco2, aqi};
        Ui::Face::update(s, now);
    }

    // ---- 分页轮播 ----
    if (now - lastPageSwitchMs >= PAGE_INTERVAL_MS)
    {
        currentPage = (currentPage + 1) % Ui::PAGE_COUNT;
        lastPageSwitchMs = now;
    }

    // ---- 按页面类型重绘：表情页按动画帧率，数据页低频刷新 ----
    uint32_t frameMs = (currentPage == Ui::PAGE_COUNT - 1) ? MOOD_FRAME_MS
                                                           : DATA_FRAME_MS;
    if (now - lastFrameMs >= frameMs)
    {
        lastFrameMs = now;
        updateDisplay(now);
    }

    delay(5);
}
