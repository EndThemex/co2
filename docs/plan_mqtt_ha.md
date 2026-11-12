# 主固件 MQTT 接入 Home Assistant — 详细设计

> 状态：设计稿（代码尚未实现）  
> 适用固件：根目录 Arduino/PlatformIO 主固件（`src/main.cpp`）  
> 不涉及：`esphome/` 方案保持不变  
> 相关文档：[README](../README.md) · [plan.md](plan.md) · [plan_weather.md](plan_weather.md) · [esphome/README.md](../esphome/README.md)

---

## 1. 目标与非目标

### 1.1 目标

| #   | 需求                 | 验收口径                                                     |
| --- | -------------------- | ------------------------------------------------------------ |
| R1  | 数据上报到 HA        | 温湿度、eCO₂、TVOC、AQI、舒适度、AQI 等级出现在 HA 实体中     |
| R2  | 接受 HA 下发的配置   | 页面开关、每页停留时长、统计窗口可就地修改并立即生效          |
| R3  | 配置持久化           | 断电重启、断网重启后仍按最后一次配置轮播                      |
| R4  | 不改变现有显示行为   | 未联网时与当前固件一致：三页 8s 轮播、表情 30fps、差分刷新     |
| R5  | 为扩展页面留出结构   | 统计页、天气页作为独立槽位接入，不破坏现有两数据页布局        |
| R6  | ESPHome 版不受影响   | 本次只改根目录固件与共享的纯 UI 文件                          |

### 1.2 非目标

- 不做 MQTT over TLS、不做公网直连（局域网明文，省 C3 的 TLS 堆峰值）。
- 不做 OTA（后续可单独加 ArduinoOTA，不影响本设计）。
- 不支持任意页面排序；页面顺序固定为槽位顺序，用开关决定是否参与。
- 不把 ESPHome 协议（原生 API）搬到 Arduino 固件里。

---

## 2. 选型结论

**主固件 = WiFi + MQTT 客户端 + HA MQTT Discovery；配置落 NVS。**

上报与下发共用同一条 MQTT 链路：状态用 publish，配置用 subscribe。HA 侧由设备主动发布 discovery 报文，自动生成 `sensor` / `switch` / `number` / `select` 实体，不需要在 HA 里写 YAML 或自动化。

| 备选方案                  | 不选的原因                                                       |
| ------------------------- | ---------------------------------------------------------------- |
| ESPHome 原生 API          | 没有维护良好的 Arduino 客户端，等于重写 ESPHome                  |
| HA REST API / Webhook     | 无自动发现；配置下发依赖 HA 自动化或 `rest_command`；token 存设备 |
| 设备 Web Server + RESTful | HA 端要手写 YAML，上报靠轮询，仍无自动发现                       |
| MQTT（本方案）            | 只多一个 Mosquitto 服务，双向、发现、retained、LWT 全部原生支持   |

---

## 3. 总体架构

```
┌──────────────────── 主固件（Arduino / PlatformIO） ────────────────────┐
│                                                                        │
│  ENS160 / AHT30 ──1Hz──► 数据状态 ──► 页面调度 ──► OLED（差分刷新）      │
│                             │                ▲                         │
│                             │                │                         │
│                      ┌──────▼──────┐   ┌─────┴─────┐                    │
│                      │ MQTT Bridge │──►│  AppConfig │◄── NVS（JSON）    │
│                      └──────┬──────┘   └───────────┘                    │
│                             │                                          │
└─────────────────────────────┼──────────────────────────────────────────┘
                              │  publish: co2/<id>/state
                              │  subscribe: co2/<id>/config/.../set
                              ▼
                    ┌───────────────────┐
                    │ Mosquitto Broker  │  HA 加载项 / 局域网 1883
                    └─────────┬─────────┘
                              │
                    ┌─────────▼─────────┐
                    │ Home Assistant    │  MQTT 集成 + 自动发现
                    │ 设备卡片 / 实体   │
                    └───────────────────┘
```

要点：

- 网络层与显示层完全解耦：MQTT 断开只影响上报/配置，不影响本地显示。
- 配置只有一个真源：`AppConfig` 内存结构；NVS 与 MQTT 状态主题都是它的副本。
- 页面调度只读取 `AppConfig` 和“页面数据是否就绪”，不直接读 MQTT。

---

## 4. HA 侧准备（一次性）

你的环境目前没有 broker，按下面顺序装：

1. **安装加载项**：设置 → 加载项 → 加载项商店 → 搜索 `Mosquitto broker` → 安装 → 启动，勾选“开机启动”和“看门狗”。
2. **创建账号**：设置 → 人员 → 用户 → 添加用户（如 `mqtt-co2`），设置密码；该密码同时作为 MQTT 用户名/密码。
   - 若你的 Mosquitto 版本提供 `Logins` 配置项，也可直接在其中添加 `用户名:密码`。
3. **启用 MQTT 集成**：设置 → 设备与服务 → 添加集成 → MQTT。
   - Broker：`core-mosquitto`（仅 HA 内部可用）
   - 端口：`1883`
   - 用户名/密码：上一步的账号
4. **给设备留一个固定地址**：设备不能使用 `core-mosquitto` 这个主机名，必须填 HA 主机的局域网 IP（如 `192.168.1.10`）；建议在路由器给 HA 做 DHCP 固定租约。
5. **确认端口可达**：从同网段电脑执行 `telnet 192.168.1.10 1883`（或 `Test-NetConnection`），能连通再烧录固件。

> 安全边界：明文 MQTT 只适合受信任局域网。不要把 1883 映射到公网；broker 账号只授予本设备所需主题。

---

## 5. 固件改造总览

| 文件                          | 类型 | 内容                                                                 |
| ----------------------------- | ---- | -------------------------------------------------------------------- |
| `platformio.ini`              | 修改 | 增加 `PubSubClient`、`ArduinoJson` 依赖                              |
| `include/secrets.example.h`   | 新增 | WiFi / MQTT 凭证模板（入库）                                          |
| `include/secrets.h`           | 新增 | 实际凭证（本地，加入 `.gitignore`）                                   |
| `src/app_config.h/.cpp`       | 新增 | 配置结构体、默认值、校验、NVS 读写                                    |
| `src/net_wifi.h/.cpp`         | 新增 | 非阻塞 WiFi 连接与重连                                                |
| `src/net_mqtt.h/.cpp`         | 新增 | MQTT 连接、LWT、discovery、状态发布、配置订阅                         |
| `src/stats.h/.cpp`            | 新增 | 1 分钟采样环形缓冲、窗口聚合                                          |
| `src/weather.h/.cpp`          | 新增 | ip-api 定位 + Open-Meteo 拉取（独立任务）                             |
| `src/main.cpp`                | 修改 | setup/loop 接入网络与配置；页面调度改为槽位制                          |
| `include/ui_layout.h`         | 修改 | 统计页、天气页坐标常量                                                |
| `src/ui_pages.cpp`、`include/ui_pages.h` | 修改 | 新增 `Ui::draw_stats()`、`Ui::draw_weather()`（纯 Canvas，不含网络） |
| `tools/gen_cn_font.py`        | 修改 | 追加新页面汉字白名单并重新生成 `include/font_cn.h`                    |
| `tools/verify_layout.py`      | 修改 | 把统计页、天气页纳入最坏宽度校验                                      |

注意：ESPHome 配置只 `includes` 了 `ui_pages.cpp` / `ui_face.cpp` 等纯 UI 文件，新增的 `stats.cpp`、`weather.cpp`、`net_*.cpp` 不会被 ESPHome 编译，双方互不影响。

### 5.1 模块接口

| 模块         | 对外接口（示意）                                                                 |
| ------------ | -------------------------------------------------------------------------------- |
| `AppConfig`  | `load()`、`saveSoon()`、`applyJson()`、`pageCfg(slot)`                           |
| `NetWifi`    | `begin()`、`loop()`、`connected()`                                              |
| `NetMqtt`    | `begin()`、`loop()`、`publishState()`、`publishConfigState()`、`onConfigChanged()` |
| `Stats`      | `sampleIfDue(now, sensor)`、`setWindow()`、`view()`                              |
| `Weather`    | `begin()`、`ready()`、`view()`                                                   |
| `Ui`         | `draw_stats(Canvas&, const StatsView&, uint32_t now)`、`draw_weather(Canvas&, const WeatherView&, uint32_t now)` |

`StatsView` / `WeatherView` 只包含普通字段（无 Arduino 类型），定义在 `ui_pages.h`，因此 `ui_pages.cpp` 仍然可以被 ESPHome 原样编译。


---

## 6. 配置模型与 NVS

### 6.1 内存结构

```cpp
// src/app_config.h
constexpr uint8_t SLOT_COUNT = 5;
constexpr uint8_t CFG_VERSION = 1;

enum class Slot : uint8_t { Climate = 0, Air, Stats, Weather, Face };

struct PageCfg {
    bool on;
    uint16_t sec;   // 3..60
};

struct AppConfig {
    uint8_t ver;
    PageCfg pages[SLOT_COUNT];
    uint8_t statsWindow;  // 0 = 1h, 1 = 24h
};
```

默认值：

| 槽位     | 默认开关 | 默认停留 |
| -------- | ----------- | -------- |
| Climate  | 开       | 8s       |
| Air      | 开       | 8s       |
| Stats    | 关       | 10s      |
| Weather  | 关       | 8s       |
| Face     | 开       | 8s       |

`statsWindow` 默认 `1h`。

### 6.2 NVS 存储

- 命名空间 `co2cfg`，键 `cfg`。
- 值为 JSON 字符串（≤ 512 字节），便于后续加字段时向后兼容；解析失败或版本不符则回落默认值。

持久化示例：

```json
{"ver":1,"pages":{"climate":{"on":true,"sec":8},"air":{"on":true,"sec":8},"stats":{"on":false,"sec":10},"weather":{"on":false,"sec":8},"face":{"on":true,"sec":8}},"stats_window":"1h"}
```

规则：

- 每次配置变更后 **延迟 1s 落盘**（`millis()` 去抖），避免 HA 连续拖动数值时反复擦写 NVS。
- 页面停留时长限制在 3–60s，越界值夹取后回写状态主题，让 HA 控件自动回正。
- 若所有页面都被关闭，强制保留 Climate 为开启，并在配置状态主题中体现修正后的结果。

### 6.3 应用时序

1. `setup()` 先加载 NVS 配置，再用它初始化页面调度，最后启动 WiFi。
2. 收到 MQTT 配置命令 → 校验 → 更新 `AppConfig` → 重新计算当前页是否仍有效 → 发布配置状态 → 1s 后落盘。
3. 当前页被关闭或被禁用时，立即跳到下一个有效页，不等待原定时器到期。

---

## 7. MQTT 主题与报文

### 7.1 连接参数

| 项         | 值                                                     |
| ---------- | ------------------------------------------------------ |
| Client ID  | `co2-<id>`                                             |
| `<id>`     | MAC 后 3 字节的 6 位小写十六进制（如 `ab12cd`）          |
| Keepalive  | 30s                                                    |
| Buffer     | 1024 字节（discovery 报文可能到 400–600 字节）          |
| LWT        | `co2/<id>/status` → `offline`，retained，QoS 0          |
| 上线       | 连接成功后向同一主题发布 `online`，retained             |

### 7.2 主题清单

| 方向     | 主题                                              | 载荷                       | Retain |
| ----- | ------------------------------------------------- | -------------------------- | ------ |
| 设备→HA | `co2/<id>/status`                                 | `online` / `offline`       | 是     |
| 设备→HA | `co2/<id>/state`                                  | 传感器快照 JSON            | 否     |
| 设备→HA | `co2/<id>/config/state`                           | 完整配置 JSON              | 是     |
| 设备→HA | `co2/<id>/config/page/<slot>/enabled`             | `ON` / `OFF`               | 是     |
| 设备→HA | `co2/<id>/config/page/<slot>/duration`            | 秒数（如 `8`）             | 是     |
| 设备→HA | `co2/<id>/config/stats_window`                    | `1h` / `24h`               | 是     |
| HA→设备 | `co2/<id>/config/page/<slot>/enabled/set`         | `ON` / `OFF`               | 否     |
| HA→设备 | `co2/<id>/config/page/<slot>/duration/set`        | 秒数                       | 否     |
| HA→设备 | `co2/<id>/config/stats_window/set`                | `1h` / `24h`               | 否     |
| 设备→HA | `homeassistant/<component>/co2_<id>_<key>/config` | discovery JSON             | 是     |

`<slot>` 取值：`climate` / `air` / `stats` / `weather` / `face`。

> 命令主题刻意 **不启用 retained**：避免 HA 重启或设备重连时重放过期指令；配置回显由 retained 的 `.../enabled`、`.../duration` 与 `config/state` 提供。

### 7.3 状态快照

`co2/<id>/state`，每 10s 发布一次（预热期缺省字段，不发布 `0`）：

```json
{
  "temperature": 21.4,
  "humidity": 48,
  "eco2": 620,
  "tvoc": 130,
  "aqi": 2,
  "comfort": "comfort",
  "aqi_text": "good",
  "rssi": -58,
  "uptime": 3600
}
```

- `comfort` 与 `aqi_text` 用稳定的英文枚举值，HA 侧直接显示；OLED 仍用本地中文映射。
- 温度/湿度为校准后显示值，与 OLED 一致；原始值不出设备。
- `rssi`、`uptime` 为可选诊断项。

### 7.4 配置状态

`co2/<id>/config/state`（retained）：

```json
{
  "ver": 1,
  "pages": {
    "climate": {"on": true,  "sec": 8},
    "air":     {"on": true,  "sec": 8},
    "stats":   {"on": false, "sec": 10},
    "weather": {"on": false, "sec": 8},
    "face":    {"on": true,  "sec": 8}
  },
  "stats_window": "1h"
}
```

---

## 8. HA 自动发现实体清单

设备上线后按表发布 discovery 报文（retained）。所有实体共享同一个 `device` 块：

```json
"device": {
  "identifiers": ["co2_<id>"],
  "name": "空气质量监测仪",
  "manufacturer": "EndTheme",
  "model": "co2-monitor"
}
```

### 8.1 实体总表

| 名称           | component | object_id                   | 状态主题                                   | 命令主题                                   |
| -------------- | --------- | --------------------------- | ------------------------------------------ | ------------------------------------------ |
| 温度           | sensor    | `co2_<id>_temperature`      | `co2/<id>/state` + 模板                    | —                                          |
| 湿度           | sensor    | `co2_<id>_humidity`         | 同上                                       | —                                          |
| eCO₂           | sensor    | `co2_<id>_eco2`             | 同上                                       | —                                          |
| TVOC           | sensor    | `co2_<id>_tvoc`             | 同上                                       | —                                          |
| AQI            | sensor    | `co2_<id>_aqi`              | 同上                                       | —                                          |
| 舒适度         | sensor    | `co2_<id>_comfort`          | 同上                                       | —                                          |
| AQI 等级       | sensor    | `co2_<id>_aqi_text`         | 同上                                       | —                                          |
| 信号强度       | sensor    | `co2_<id>_rssi`             | 同上（诊断）                               | —                                          |
| 运行时长       | sensor    | `co2_<id>_uptime`           | 同上（诊断）                               | —                                          |
| 温湿度页开关   | switch    | `co2_<id>_page_climate`     | `.../config/page/climate/enabled`          | `.../enabled/set`                          |
| 空气质量页开关 | switch    | `co2_<id>_page_air`         | `.../config/page/air/enabled`              | `.../enabled/set`                          |
| 统计页开关     | switch    | `co2_<id>_page_stats`       | `.../config/page/stats/enabled`            | `.../enabled/set`                          |
| 天气页开关     | switch    | `co2_<id>_page_weather`     | `.../config/page/weather/enabled`          | `.../enabled/set`                          |
| 表情页开关     | switch    | `co2_<id>_page_face`        | `.../config/page/face/enabled`             | `.../enabled/set`                          |
| 温湿度页停留   | number    | `co2_<id>_dur_climate`      | `.../config/page/climate/duration`         | `.../duration/set`                         |
| 空气质量页停留 | number    | `co2_<id>_dur_air`          | `.../config/page/air/duration`             | `.../duration/set`                         |
| 统计页停留     | number    | `co2_<id>_dur_stats`        | `.../config/page/stats/duration`           | `.../duration/set`                         |
| 天气页停留     | number    | `co2_<id>_dur_weather`      | `.../config/page/weather/duration`         | `.../duration/set`                         |
| 表情页停留     | number    | `co2_<id>_dur_face`         | `.../config/page/face/duration`            | `.../duration/set`                         |
| 统计窗口       | select    | `co2_<id>_stats_window`     | `.../config/stats_window`                  | `.../stats_window/set`                     |

每个实体都要带 `availability_topic: co2/<id>/status`。

### 8.2 报文示例

温度：

```json
{
  "name": "温度",
  "unique_id": "co2_ab12cd_temperature",
  "state_topic": "co2/ab12cd/state",
  "value_template": "{{ value_json.temperature }}",
  "unit_of_measurement": "°C",
  "device_class": "temperature",
  "state_class": "measurement",
  "availability_topic": "co2/ab12cd/status",
  "device": {
    "identifiers": ["co2_ab12cd"],
    "name": "空气质量监测仪",
    "manufacturer": "EndTheme",
    "model": "co2-monitor"
  }
}
```

页面开关：

```json
{
  "name": "统计页",
  "unique_id": "co2_ab12cd_page_stats",
  "state_topic": "co2/ab12cd/config/page/stats/enabled",
  "command_topic": "co2/ab12cd/config/page/stats/enabled/set",
  "availability_topic": "co2/ab12cd/status",
  "device": { "identifiers": ["co2_ab12cd"] }
}
```

停留时长：

```json
{
  "name": "统计页停留",
  "unique_id": "co2_ab12cd_dur_stats",
  "state_topic": "co2/ab12cd/config/page/stats/duration",
  "command_topic": "co2/ab12cd/config/page/stats/duration/set",
  "min": 3,
  "max": 60,
  "step": 1,
  "unit_of_measurement": "s",
  "mode": "box",
  "availability_topic": "co2/ab12cd/status",
  "device": { "identifiers": ["co2_ab12cd"] }
}
```

> 舒适度、AQI 等级用 `sensor` + `value_template` 返回字符串（无单位、无 `state_class`）；HA 的 MQTT `sensor` 允许文本状态。若某版本表现异常，改为发布英文枚举到 `sensor` 并加 `icon` 即可，不影响固件协议。

---

## 9. 运行时行为

### 9.1 启动顺序

1. I²C / OLED / ENS160 初始化（与现有逻辑一致）。
2. `AppConfig::load()`：读取 NVS，失败则默认值。
3. 页面调度初始化到第一个有效页，开始正常显示（**不等待 WiFi**）。
4. `Net::wifiBegin()`：非阻塞发起连接。
5. WiFi 获得 IP 后：`Net::mqttBegin()`，连接成功后发布 discovery、配置状态，再进入周期上报。

### 9.2 WiFi

- `WiFi.mode(WIFI_STA)`、`WiFi.setAutoReconnect(true)`、`WiFi.setSleep(false)`（保证 MQTT 响应，代价是功耗略高）。
- 断线后 15s 重试一次，指数退避至 60s 封顶；重连成功后重置退避。
- 不阻塞主循环，不在 `setup()` 里 `while (!WiFi.isConnected())`。

### 9.3 MQTT

- 断开后 5s 重试一次，连续失败退避到 60s；每次重连都要重新订阅配置主题并发布 `online`。
- `PubSubClient::connect()` 本身是阻塞的（通常 1–3s）。为减少表情页卡顿：
  - 重连尝试只在“非表情页”进行；
  - 或接受一次短暂卡顿，但把退避做足，避免频繁尝试。
- 配置命令在回调里只做解析与内存更新，NVS 落盘延迟处理，避免在回调里阻塞网络。

### 9.4 页面调度

```cpp
static Slot cur = Slot::Climate;

static bool ready(Slot s) {
    if (s == Slot::Weather) return Weather::ready();  // 联网 + 有数据 + 未过期
    return true;
}

static void advance() {
    for (uint8_t i = 0; i < SLOT_COUNT; ++i) {
        cur = (Slot)((cur + 1) % SLOT_COUNT);
        if (cfg.pages[(int)cur].on && ready(cur)) return;
    }
    cur = Slot::Climate;   // 兜底
}
```

- 停留时间从 `cfg.pages[cur].sec` 读取，变化立即生效。
- 表情页仍按 33ms 重绘，其余页面 500ms。
- 配置变更后：若当前页已关闭或不再就绪，立即 `advance()` 并重置计时。
- 天气页在数据过期（>30min）或 WiFi 断开时 `ready()==false`，自动隐藏。

### 9.5 断网行为

- 显示、配置、统计完全按本地状态运行。
- MQTT 状态主题由 LWT 标记为 `offline`，HA 实体显示不可用。
- 恢复后先发布完整状态与配置，避免 HA 显示旧值。

---

## 10. 统计页设计

### 10.1 采样

- 每 60s 一条样本，只在 AHT30/ENS160 数据有效时写入。
- 环形缓冲 1440 条（24h）：

```cpp
struct Sample {
    uint16_t eco2;
    uint16_t tvoc;
    int16_t  temp;    // 0.1°C
    uint8_t  hum;
    uint8_t  aqi;
};                    // 约 9–10 字节
```

RAM 占用约 13–14KB，占 C3 可用堆的比例可接受。

### 10.2 聚合

- 窗口：1h → 最近 60 条；24h → 最近 1440 条。
- 计算 eCO₂、TVOC 的平均/最高/最低；样本不足时显示 `--`。
- 有新样本或窗口切换时重算一次并缓存，绘制时只读缓存。

### 10.3 布局（128×64，先按现有双栏规则预留）

```
y=0   统计                         1h（右对齐，size 1）
y=16  平均  [eCO2 均值]            平均  [TVOC 均值]
y=32  最高  [eCO2 最高]            最高  [TVOC 最高]
y=48  最低  [eCO2 最低]            最低  [TVOC 最低]
```

- 标签 12px 中文（“平均”“最高”“最低”各 26px），数值 size 1（6px/字符）右对齐。
- 5 位数值 30px + 标签 26px = 56px，仍在 64px 栏内，留 8px 间隙；落地前必须用 `verify_layout.py` 按真实字宽复核。
- 无样本时在右侧显示 `--`，不放 0。

### 10.4 字库

新增：`统计 平均 最高 最低 采样`。  
ESPHome 版字库不动。

---

## 11. 天气页设计

### 11.1 数据源

沿用 `docs/plan_weather.md` 的思路，但 **不显示城市名**（主固件中文字模为编译期白名单，动态城市名会迫使把上千汉字打进 flash）：

1. `GET http://ip-api.com/json/?lang=en&fields=status,lat,lon` → 经纬度。
2. `GET https://api.open-meteo.com/v1/forecast?latitude=..&longitude=..&current=temperature_2m,relative_humidity_2m,weather_code,wind_speed_10m&timezone=Asia%2FShanghai` → 天气码/温度/湿度/风速。

经纬度仅缓存内存；定位失败时退避重试，必要时回落到 `secrets.h` 中的默认坐标。

### 11.2 非阻塞实现

HTTP 与 TLS 会阻塞 1–3s，不能放进主循环。使用独立 FreeRTOS 任务：

```cpp
xTaskCreate(weatherTask, "weather", 8192, nullptr, 1, nullptr);
```

- 任务每秒检查一次：WiFi 是否连接、距上次成功是否满 10min、上次失败是否满 2min。
- 结果写入共享结构，用互斥锁或临界区保护；主循环只读取副本。
- `WiFiClientSecure` + `setInsecure()`；`HTTPClient` 超时 8s。
- 失败保留上一次数据；连续失败不清空页面，只标记过期。
- 数据超过 30min 未更新 → 天气页不参与轮播。

### 11.3 天气码映射

| weather_code | 显示词 | weather_code | 显示词 |
| ------------ | ------ | ------------ | ------ |
| 0            | 晴     | 51–57        | 小雨   |
| 1–2          | 多云   | 61/63/65     | 小雨/中雨/大雨 |
| 3            | 阴     | 66–67        | 冻雨   |
| 45/48        | 雾     | 71/73/75     | 小雪/中雪/大雪 |
| 80–82        | 阵雨   | 85–86        | 小雪   |
| 95–99        | 雷雨   | 其他         | 未知   |

### 11.4 布局

```
y=0   天气
y=16  多云（12px，x=0）        26.5（size 2，右缘 114）+ °C（x=116，size 1）
y=36  湿度 65%（12px + size 1）  风速 3.2m/s（12px + size 1）
```

- 温度最坏 `-12.5` 4 字符 ×12px = 48px，从 x=66 起；天气词最长 3 字（39px），左侧仍留 27px 间隙。
- 第三行左右各占 64px：左侧约 47px，右侧约 62px，中间留 17px。
- 天气页只显示天气词，不显示城市名。

### 11.5 字库

新增：`天气 晴 多 云 阴 雾 小 中 大 阵 雷 雨 雪 冻 风`（`中`、`湿`、`度` 已在白名单）。

---

## 12. 字库与布局约束

1. 任何新中文字符必须加入 `tools/gen_cn_font.py` 的 `CHARS` 白名单，然后执行：

```bash
python tools/gen_cn_font.py
```

2. 统计页、天气页的坐标常量放进 `include/ui_layout.h`，遵循现有“64px 双栏 + 最坏值预留”规则，不得把坐标散落在绘制函数里。
3. `tools/verify_layout.py` 需要把新页面纳入检查，否则布局校验只覆盖旧三页，等于没校验。
4. 新增页面绘制只依赖 `Canvas`，保证 PC 模拟器仍可预览；网络相关逻辑不得进入 `ui_pages.cpp`。

---

## 13. 资源预算

| 项目             | 估算                                      | 说明                                      |
| ---------------- | ----------------------------------------- | ----------------------------------------- |
| 统计环形缓冲     | 约 13–14KB RAM                            | 1440 × 9–10B                              |
| MQTT 缓冲        | 1KB RAM                                   | 需容纳 discovery 报文                     |
| 配置 JSON        | 2 × 512B RAM                              | 读写各一份临时缓冲                        |
| 天气任务栈       | 8KB（堆）                                 | TLS + JSON 解析                           |
| TLS 瞬态         | 约 40KB 堆峰值                            | 每 10min 一次，需实测碎片情况              |
| 新增字模         | 约 24 字 × 30–40B ≈ 1KB                   | 可忽略                                    |
| MQTT/JSON 库     | 数十 KB flash                             | 以实际编译产物为准                        |

C3 的关键风险是 TLS 与 WiFi 共存时的堆碎片；若实测不稳，按 §16 退化为“HA 推送天气”。

---

## 14. 分阶段实施

### Phase 1 — 网络骨架

- `secrets.h` + `platformio.ini` 依赖。
- `net_wifi` / `net_mqtt`：连接、LWT、状态上报、discovery 发布。
- `main.cpp`：setup/loop 接入，UI 与页面逻辑不动。
- 验收：HA 出现 9 个传感器实体；`mosquitto_sub` 能看到 `state` 与 `status`。

### Phase 2 — 配置闭环

- `app_config`：结构体、校验、NVS、JSON 序列化。
- HA 开关/数值/选择实体；订阅配置主题。
- 页面调度改为槽位制，按开关与停留时间轮播。
- 验收：HA 改开关/时长立即生效；断电重启保持；全关时回退温湿度页。

### Phase 3 — 统计页

- 环形缓冲与聚合。
- `ui_layout.h` 坐标、`Ui::draw_stats()`、字库扩充、布局校验。
- 验收：1h/24h 窗口切换正确；无样本显示 `--`；最坏值不重叠。

### Phase 4 — 天气页

- `weather` 独立任务、定位与天气拉取、过期隐藏。
- `Ui::draw_weather()`、字库扩充、布局校验。
- 验收：联网后出现天气页；断网/过期自动隐藏；温度与手机天气误差 ≤1°C。

---

## 15. 验证方案

### 15.1 编译与静态检查

```bash
pio run
python tools/gen_cn_font.py
python tools/verify_layout.py
```

同时确认 `include/font_cn.h` 中新增汉字齐全，且 `firmware.bin` 体积增幅可接受。

### 15.2 串口日志

- WiFi 获取 IP、MQTT 连接成功、订阅成功、discovery 发布条数。
- 配置命令解析结果与落盘时机。
- 天气任务成功/失败与耗时。

### 15.3 MQTT 侧

```bash
mosquitto_sub -h 192.168.1.10 -p 1883 -u mqtt-co2 -P '密码' -t 'co2/#' -v
mosquitto_pub -h 192.168.1.10 -p 1883 -u mqtt-co2 -P '密码' \
  -t 'co2/ab12cd/config/page/stats/enabled/set' -m 'ON'
```

检查项：

- [ ] `status` 上线后为 `online`，断电后 HA 显示不可用。
- [ ] `state` 每 10s 一条，字段与 OLED 显示一致。
- [ ] 配置命令生效并回显，NVS 持久化。
- [ ] discovery 报文全部 retained，HA 重启后实体仍在。

### 15.4 场景测试

- [ ] 断网启动：三页 8s 轮播与当前固件一致。
- [ ] 联网后关闭表情页：轮播立即跳过表情页。
- [ ] 页面停留改为 3s/60s：节奏正确。
- [ ] 统计页无样本、1h、24h 三种状态显示正确。
- [ ] 天气页断网 30min 后自动消失，恢复后重新出现。
- [ ] 连续开关页面 20 次：NVS 不因高频写入卡顿，最终配置正确。

---

## 16. 风险与备选

| 风险                                   | 影响                          | 对策                                                       |
| -------------------------------------- | ------------------------------ | ---------------------------------------------------------- |
| MQTT 未连接时 `connect()` 阻塞          | 表情页短暂卡顿                | 仅非表情页尝试重连 + 5s→60s 退避                            |
| TLS 与 WiFi 共存导致堆碎片              | 天气拉取偶发失败               | 任务内串行请求 + 8s 超时；持续失败改为 HA 通过 MQTT 推送天气 |
| HA 局域网 IP 变化                       | 设备连不上 broker              | 路由器 DHCP 固定租约；或后续加设备端可配置主机名            |
| ESP32-C3 仅支持 2.4GHz                  | 5GHz-only 网络无法连接        | 使用 2.4GHz SSID，或开双频合一                              |
| 新页面最坏值超出 64px 栏                | 文本重叠                       | 扩展 `verify_layout.py`，按真实字宽复核                     |
| 明文 MQTT                               | 同网段可嗅探                   | 仅局域网、独立账号、必要时网络隔离                          |
| 配置命令误发 retained                   | 重启后套用旧指令               | 命令主题保持非 retained，状态主题 retained                  |

**TLS 兜底方案**：天气改为由 HA 自动化把天气码/温度/湿度发布到 `co2/<id>/weather/set`，设备只解析小 JSON，不再发起 HTTPS。该方案不改变页面与配置模型，仅替换 `weather.cpp` 的数据来源。
