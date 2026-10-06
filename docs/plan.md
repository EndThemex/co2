# ESP32-C3 Mini + ENS160 + AHT30 + OLED SSD1315 空气质量检测仪完整实现方案

## 一、项目概述

本项目基于 ESP32-C3 Mini 开发板，搭配 ENS160 空气质量传感器、AHT30 温湿度传感器和 SSD1315 OLED 显示屏，实现室内空气质量（AQI、TVOC、eCO₂）及环境温湿度的实时监测与显示。

**监测指标：**

- AQI（空气质量指数，1-5级）
- TVOC（总挥发性有机物，单位 ppb）
- eCO₂（等效二氧化碳，单位 ppm）
- 温度（℃）
- 相对湿度（%RH）

## 二、硬件清单

| 组件                  | 说明                          |
| --------------------- | ----------------------------- |
| ESP32-C3 Mini 开发板  | 主控，支持 Wi-Fi + BLE 5.0    |
| ENS160 空气质量传感器 | I²C 接口，输出 AQI/TVOC/eCO₂  |
| AHT30 温湿度传感器    | I²C 接口，精度 ±0.3℃ / ±2% RH |
| SSD1315 OLED 显示屏   | 0.96英寸，128×64，I²C 接口    |
| 杜邦线                | 公对公/公对母若干             |
| USB-C 数据线          | 供电与烧录                    |

## 三、引脚连接

**所有设备均通过 I²C 总线连接，共用 SDA 和 SCL 引脚**。

| ESP32-C3 Mini 引脚 | 连接设备                        | 说明                 |
| ------------------ | ------------------------------- | -------------------- |
| **3.3V**           | ENS160 VIN、AHT30 VIN、OLED VCC | 电源（**严禁接5V**） |
| **GND**            | ENS160 GND、AHT30 GND、OLED GND | 共地                 |
| **GPIO8**          | 所有设备 SDA                    | I²C 数据线           |
| **GPIO9**          | 所有设备 SCL                    | I²C 时钟线           |

**接线示意图：**

```
ESP32-C3 Mini          ENS160      AHT30       OLED SSD1315
   3.3V  ────────────── VIN ────── VIN ────── VCC
   GND   ────────────── GND ────── GND ────── GND
   GPIO8 ────────────── SDA ────── SDA ────── SDA
   GPIO9 ────────────── SCL ────── SCL ────── SCL
```

**注意事项：**

1. 所有传感器和显示屏均为 I²C 设备，共用 SDA/SCL 总线
2. 供电必须使用 **3.3V**，5V 会烧毁传感器
3. 接线时确保引脚接触良好，避免虚接导致设备无法识别

## 四、软件开发环境

### 4.1 开发环境

- **Arduino IDE** 或 **VS Code + PlatformIO**
- 安装 ESP32-C3 开发板支持包

### 4.2 需要安装的库

| 库名称                                               | 用途              | 安装方式                           |
| ---------------------------------------------------- | ----------------- | ---------------------------------- |
| `SparkFun Indoor Air Quality Sensor - ENS160`        | ENS160 传感器驱动 | PlatformIO 从 GitHub `v1.1.0` 拉取 |
| `Adafruit AHTX0`                                     | AHT30 传感器驱动  | Arduino IDE → 库管理 → 搜索安装    |
| `Adafruit BusIO`                                     | I²C 通信底层库    | 安装上述库时自动依赖               |
| `ESP8266_and_ESP32_OLED_driver_for_SSD1306_displays` | SSD1315 OLED 驱动 | 库管理搜索安装                     |
| `Adafruit GFX Library`                               | 图形库            | 库管理搜索安装                     |

> **库变更说明**：原计划使用 `Adafruit ENS160` 库，但该库已被 Adafruit 弃用，PlatformIO Registry 与 GitHub 主仓均无法获取。实际改用 **SparkFun Indoor Air Quality Sensor - ENS160**（API：`setOperatingMode()` / `setTempCompensationCelsius()` / `setRHCompensationFloat()` / `checkDataStatus()` / `getAQI()` / `getTVOC()` / `getECO2()`）。两者均基于 ScioSense ENS160 寄存器协议，功能等价。

### 4.3 ESP32-C3 特殊配置

ESP32-C3 只有一个 I²C 硬件接口，使用某些 OLED 库时可能报错 `'Wire1' was not declared`。解决方法：在代码**第一行**加入：

```cpp
#define SOC_HP_I2C_NUM 2
```

## 五、完整代码

完整代码在 [`src/main.cpp`](src/main.cpp)，单文件包含 4 个分页（Dashboard / CO2 / TVOC / Comfort）的全部 UI 绘制逻辑。

主要结构：

| 部分                                          | 内容                                                |
| --------------------------------------------- | --------------------------------------------------- |
| 顶部定义                                      | I²C 引脚、OLED 地址、AHT30 命令、屏幕尺寸、分页参数 |
| `readAHT30()`                                 | 直接通过 I²C 读 AHT30 寄存器，转换成 °C / %RH       |
| `aqiShort / co2Hint / tvocHint / comfortText` | 文字字典辅助函数                                    |
| `drawDashboard()`                             | 左列 AQI 大数字 + 右列 eCO2 大数字 + 底行 T/H       |
| `drawCO2Page()`                               | eCO2 居中超大数字 + 提示 + AQI                      |
| `drawTVOCPage()`                              | TVOC 居中超大数字 + 提示 + AQI                      |
| `drawComfortPage()`                           | 温湿度大数字 + Comfort 状态文字                     |
| `setup()`                                     | 串口、I²C、OLED、ENS160 初始化                      |
| `loop()`                                      | 每秒读传感器、刷新当前页、5 秒翻页                  |

页面切换由 `currentPage` 与 `lastPageSwitchMs` 控制，每 `PAGE_INTERVAL_MS` (5000ms) 翻一页。

> **库变更说明**：ENS160 实际使用 SparkFun 驱动（API：`setOperatingMode()` / `setTempCompensationCelsius()` / `setRHCompensationFloat()` / `checkDataStatus()` / `getAQI()` / `getTVOC()` / `getECO2()`），而非计划中的 Adafruit ENS160（已被官方弃用）。AHT30 通过直读寄存器方式实现，避免引入额外依赖。

## 六、ENS160 校准说明

ENS160 传感器需要一定的预热和校准时间：

1. **初次上电**：需要连续通电 **1 小时** 进行初始校准
2. **完全校准**：连续通电 **24 小时** 后，校准数据会写入传感器内部非易失性存储器，之后每次启动只需约 **3 分钟** 即可就绪
3. **温湿度补偿**：ENS160 的 eCO₂ 和 TVOC 测量依赖 AHT30 提供的温湿度数据进行补偿，**必须**在每次读取前调用 `ens160.setTempAndHumidity(temperature, humidity)`

> **注意**：ENS160 内部加热板会使模块自身温度升高，AHT30 测得的温度可能比环境温度高几度。如需精确环境温度，可考虑增加独立温度传感器或手动修正偏移量。

## 七、扩展功能建议

### 7.1 Wi-Fi 数据上报

ESP32-C3 内置 Wi-Fi，可将数据上报至：

- **Home Assistant**（通过 ESPHome 或 MQTT）
- **ThingsBoard** 等物联网平台
- 自建 Web 服务器

### 7.2 数据存储

- 使用 SD 卡模块记录历史数据
- 通过 NVS（非易失性存储）保存配置参数

### 7.3 报警功能

- AQI 超标时通过 GPIO 控制蜂鸣器或 LED 指示灯
- 通过 Wi-Fi 发送推送通知

## 八、常见问题排查

| 问题                      | 可能原因              | 解决方法                                                                                |
| ------------------------- | --------------------- | --------------------------------------------------------------------------------------- |
| OLED 不显示               | I²C 地址错误          | 检查 OLED 地址（常见 0x3C 或 0x3D）                                                     |
| ENS160 无数据             | 未完成初始化序列      | 检查代码中的 RESET→IDLE→CLEAR→STANDARD 序列                                             |
| 传感器无法识别            | 接线问题或供电不足    | 检查 3.3V 供电和 I²C 接线，使用 I²C 扫描程序检测                                        |
| 编译报错 Wire1            | ESP32-C3 只有一个 I2C | 在代码第一行添加 `#define SOC_HP_I2C_NUM 2`                                             |
| 读数不稳定                | I²C 总线干扰          | 缩短杜邦线长度，降低 I²C 时钟频率（如 50kHz）                                           |
| 烧录成功但串口无输出      | 未启用 USB-CDC        | 在 `platformio.ini` 添加 `-DARDUINO_USB_CDC_ON_BOOT=1 -DARDUINO_USB_MODE=1`（详见 8.1） |
| 温度偏高 5-15℃ / 湿度偏低 | ENS160 热板烘烤 AHT30 | 物理上分开 AHT30 与 ENS160 ≥2cm，或加隔热（详见 8.2）                                   |

### 8.1 串口无输出：ESP32-C3 USB-CDC 启用

ESP32-C3 Arduino 内核默认 `Serial` 走 UART0（GPIO20/21），**不经过板载 USB**。如果开发板只有 USB-C 接口（无外部 USB-UART 桥），监视器看不到任何输出。

解决方法：在 `platformio.ini` 的 `[env]` 段添加：

```ini
build_flags =
    -DARDUINO_USB_CDC_ON_BOOT=1
    -DARDUINO_USB_MODE=1
```

重新 `pio run -t upload` 后 `pio device monitor` 即可看到输出。

> 注意：开启 USB-CDC 后 UART0 (GPIO20/21) 不再可用。本项目 I²C 在 GPIO8/9，不受影响。

### 8.2 AHT30 温湿度偏差（温度偏高、湿度偏低）

**现象**：温度比实际室温高 5~15℃，湿度比实际偏低 10~20%。

**根因**：ENS160 内部金属氧化物热板需要加热到约 200~400℃，会把所在 PCB 区域温度抬高 5~15℃。如果 AHT30 焊在同一片 PCB 上或紧邻 ENS160，AHT30 测的就是"被烘烤过的局部空气"，不是真实环境。

**校验方法**：先让设备**连续通电 24 小时**——ENS160 完成首次基线校准，期间偏差可能更大。24h 后若仍偏高，确认是物理摆放问题。

**解决方法（按推荐顺序）**：

1. **物理隔离**：把 AHT30 模块移到独立 PCB / 杜邦线远端（≥2cm 间距），中间用泡棉或铝箔挡住 ENS160 散热。
2. **改善通风**：不要装入密封外壳，开几个通风孔让热空气流通。
3. **不建议软件补偿**：偏差随摆放和通风变化，硬编码偏移会引入更多问题。

### I²C 地址扫描代码（调试用）

```cpp
#include <Wire.h>
void setup() {
    Serial.begin(115200);
    Wire.begin(8, 9);
    for (uint8_t addr = 1; addr < 127; addr++) {
        Wire.beginTransmission(addr);
        if (Wire.endTransmission() == 0) {
            Serial.printf("Device found at 0x%02X\n", addr);
        }
    }
}
void loop() {}
```

---
