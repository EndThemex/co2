# 空气质量监测仪 — 固件编译与烧录指南

基于 ESPHome 的空气质量监测仪固件，接入 Home Assistant。

- **硬件**: ESP32-C3（LuAT airm2m_core_esp32c3）+ AHT30 + ENS160 + SSD1306 128x64 OLED
- **配置文件**: `co2.yaml`
- **I2C 接线**: SDA=GPIO8, SCL=GPIO9

## 目录结构

```
esphome/
├── co2.yaml                # ESPHome 主配置
├── secrets.yaml.example    # 凭证模板（入库）
├── secrets.yaml            # WiFi / API 密钥（不入库，自己创建）
└── fonts/                  # 本地字体文件（已包含在仓库中）
```

## 1. 环境准备

安装 ESPHome（需要 Python 3.9+）：

```bash
pip install esphome
```

### 配置 secrets.yaml

首次编译前需在 `esphome/` 目录下创建 `secrets.yaml`（可复制模板 `secrets.yaml.example` 后修改）：

```yaml
wifi_ssid: "你的WiFi名称"
wifi_password: "你的WiFi密码"
api_key: "64位随机密钥"   # 可用 esphome 运行时自动生成，或用 openssl rand -hex 32 生成
```

## 2. 编译固件

```bash
cd esphome
esphome compile co2.yaml
```

> 首次编译会下载工具链和依赖，耗时较长；国内网络建议提前配置 pip / PlatformIO 镜像。

编译产物位于：

```
.esphome/build/co2-monitor/build/
```

| 文件 | 说明 |
|---|---|
| `firmware.factory.bin` | **合并固件（推荐）**：已包含 bootloader + 分区表 + otadata + app，单文件从 `0x0` 烧录即可 |
| `firmware.ota.bin` | 仅 OTA 在线升级用 |
| `bootloader.bin` / `partition-table.bin` / `ota_data_initial.bin` / `co2-monitor.bin` | 分区文件，需按偏移分别烧录 |

分区偏移（ESP32-C3，Flash 4MB，DIO / 80MHz）：

| 地址 | 文件 |
|---|---|
| `0x0` | `bootloader/bootloader.bin` |
| `0x8000` | `partition_table/partition-table.bin` |
| `0x9000` | `ota_data_initial.bin` |
| `0x10000` | `co2-monitor.bin` |

## 3. 烧录

设备为 ESP32-C3，通过板载 USB 串口烧录。烧录前按住 **BOOT 键**再上电/复位，进入下载模式（部分板子自动进入可跳过）。

### 方式 A：esptool 命令行（推荐）

```bash
# 烧录合并固件（一条命令搞定）
esptool --chip esp32c3 --port COM5 --baud 921600 write_flash 0x0 firmware.factory.bin

# 或按分区烧录（使用项目自带 flash_args）
esptool --chip esp32c3 --port COM5 write_flash @flash_args
```

> Windows 下 `COM5` 换成实际的串口号（设备管理器中查看）。

### 方式 B：Flash Download Tool（乐鑫官方 GUI）

1. 下载 [Flash Download Tool](https://www.espressif.com.cn/zh-hans/support/download/other-tools)
2. 芯片类型选 **ESP32-C3**，工作模式 **UART**
3. 最简单：只勾选 `firmware.factory.bin`，地址填 `0x0`
   或按上表勾选 4 个分区文件并填对应地址
4. 波特率建议 921600，点 **START** 开始烧录

### 方式 C：ESPHome Web（浏览器，无需安装工具）

打开 https://web.esphome.io ，连接 USB 后选择 `firmware.factory.bin` 即可（需 Chrome/Edge）。

## 4. 烧录后配网

串口烧录会清除 NVS，WiFi 配置需重新输入。固件内置两种配网方式：

- **Improv（推荐）**：设备连上 USB 后，用 Chrome/Edge 打开 https://improv-wifi.com ，按提示选择串口并输入 WiFi 即可，无需重新刷机
- **AP 热点**：设备连不上 WiFi 时会自动创建热点 `co2-monitor Setup`（密码 `12345678`），连接后进入 `192.168.4.1` 配置页

## 5. 后续升级（OTA）

设备已配网后，日常修改直接 OTA 更新，无需 USB：

```bash
esphome run co2.yaml
```

或通过 Home Assistant 的 ESPHome 集成无线更新。

## 常见问题

- **找不到串口**：ESP32-C3 使用 USB Serial/JTAG，需要较新的系统驱动；部分线材仅供电无数据，请更换数据线
- **烧录失败 / 无法进入下载模式**：按住 BOOT 键重新上电，再点烧录
- **编译报错找不到字体**：确认 `fonts/` 目录下三个 TTF 文件存在
