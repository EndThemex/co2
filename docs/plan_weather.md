# WiFi 联网天气显示 — 实现文档（ESPHome 固件）

> 目标：设备连接 WiFi 后，根据当前网络（出口 IP）定位所在城市，拉取该城市实时天气并
> 在 OLED 上新增一个天气页；**天气页只在 WiFi 已连接且天气数据就绪后才参与轮播**，
> 断网后立即隐藏。
>
> 适用固件：`esphome/co2.yaml`（Arduino/PlatformIO 主固件本次不改动）。

---

## 1. 需求拆解

| #   | 需求                       | 实现要点                                                                 |
| --- | -------------------------- | ------------------------------------------------------------------------ |
| R1  | 连接 WiFi 后才出现天气显示 | `wifi.on_connect` 触发定位+天气拉取；成功后把天气页加入轮播序列          |
| R2  | 根据当前网络获取对应城市   | 出口 IP → 地理定位 API（ip-api.com，中文城市名）→ 经纬度                 |
| R3  | 显示天气情况               | Open-Meteo 实时天气（免费、无 key、国内可访问）→ 温度/天气现象/湿度/风速 |
| R4  | 显示城市名（用户已选定）   | 城市名 UTF-8 直接上屏；中文字库按白名单扩充，缺字问题见 §6               |
| R5  | 断网时功能隐藏             | `wifi.on_disconnect` 立即切回普通页，`g_weather_ok=false`                |

## 2. 方案总览

```
开机
 └─ WiFi 连接（已有：secrets.yaml 凭证 + AP 配网 + Improv）
     └─ on_connect ──► ① IP 定位   GET http://ip-api.com/json/?lang=zh-CN&fields=...
                        （城市名/经纬度 → globals，失败退避重试）
         └─► ② 天气拉取  GET https://api.open-meteo.com/v1/forecast?latitude=..&longitude=..
             （当前温度/湿度/天气码/风速 → globals）
                 └─► ③ g_weather_ok = true → 天气页进入 4 页轮播
 └─ WiFi 断开 ──► on_disconnect：g_weather_ok=false，立即切回温湿度页
 └─ 每 10 分钟定时刷新天气（定位不重复请求，成功后缓存）
```

选型说明：

- **定位**：`ip-api.com` 免费版支持 `lang=zh-CN` 返回中文城市名（如 `杭州市`），
  无 key、45 次/分钟限速足够。**注意：免费版仅 HTTP**（HTTPS 是付费 Pro 版）。
- **天气**：Open-Meteo Forecast API，免费无 key、HTTPS、有 `current` 实时字段，
  `weather_code` 为 WMO 标准码，映射为固定中文措辞（见 §5）。
- 两者均可用 `http_request` 组件的 action 触发，JSON 用 lambda 内 `json::parse` 解析。

## 3. 数据源与请求格式

### 3.1 IP 定位（一次，缓存）

```
GET http://ip-api.com/json/?lang=zh-CN&fields=status,country,regionName,city,lat,lon
→ {"status":"success","country":"中国","regionName":"浙江省",
   "city":"杭州市","lat":30.29,"lon":120.16}
```

- 成功：写入 `g_city`（std::string）、`g_lat`、`g_lon`，随后立刻拉天气。
- `status":"fail"` 或超时：60s 后重试（最多 5 次）。
- 备选（HTTP 不可用时）：高德 IP 定位 API（HTTPS、需个人免费 key、中文、
  5000 次/日）；`https://ipwho.is/`（HTTPS 免 key，但城市名为英文）。

### 3.2 实时天气（每次 WiFi 连接 + 每 10 分钟）

```
GET https://api.open-meteo.com/v1/forecast
    ?latitude={g_lat}&longitude={g_lon}
    &current=temperature_2m,relative_humidity_2m,weather_code,wind_speed_10m
    &timezone=Asia%2FShanghai
→ {"current":{"temperature_2m":26.5,"relative_humidity_2m":65,
              "weather_code":2,"wind_speed_10m":3.2}}
```

- 需要 `http_request` 开 `insecure: true`（ESPHome 侧跳过 TLS 证书校验）；
  若所用 ESPHome 版本该选项名有变，改用其等价配置（`verify` / CA 选项）。
- 解析失败（非 200 / JSON 非法）：保留上一次数据，2 分钟后重试。

### 3.3 WMO weather_code → 中文措辞映射

| weather_code | 屏显文案                   |
| ------------ | -------------------------- |
| 0            | 晴                         |
| 1, 2         | 多云                       |
| 3            | 阴                         |
| 45, 48       | 雾                         |
| 51–57        | 毛毛雨                     |
| 61, 65       | 小雨 / 大雨（63 → 中雨）   |
| 66, 67       | 冻雨                       |
| 71, 75       | 小雪 / 大雪（73 → 中雪）   |
| 77           | 米雪                       |
| 80, 82       | 阵雨 / 强阵雨（81 → 阵雨） |
| 85, 86       | 阵雪                       |
| 95, 96, 99   | 雷阵雨                     |

实现为 lambda 内 `switch (code)` 返回 `const char*`，文案只用固定白名单字符。

## 4. ESPHome 配置设计（co2.yaml 增量）

以下为示意骨架，字段名以实际安装的 ESPHome 版本编译验证为准。

### 4.1 组件

```yaml
http_request:
  timeout: 10s
  useragent: esphome-co2-monitor
  follow_redirects: false
  capture_headers: false
  insecure: true # Open-Meteo 走 HTTPS，跳过证书校验

globals:
  - id: g_city # type: std::string
    type: std::string
    initial_value: " "
  - id: g_lat
    type: float
    initial_value: "0"
  - id: g_lon
    type: float
    initial_value: "0"
  - id: g_temp # 当前温度 °C
    type: float
    initial_value: "0"
  - id: g_hum
    type: float
    initial_value: "0"
  - id: g_wcode
    type: int
    initial_value: "-1"
  - id: g_wind
    type: float
    initial_value: "0"
  - id: g_weather_ok # 天气页开关（R1/R5 的唯一依据）
    type: bool
    initial_value: "false"
  - id: g_page_idx # 手动页序号（0..2 普通 / 0..3 含天气）
    type: int
    initial_value: "0"
```

### 4.2 触发（挂在已有 wifi 块上）

```yaml
wifi:
  ssid: !secret wifi_ssid
  password: !secret wifi_password
  # ... ap / captive_portal / improv_serial 均保持不动
  on_connect:
    then:
      - http_request.get:
          url: "http://ip-api.com/json/?lang=zh-CN&fields=status,country,regionName,city,lat,lon"
          on_response:
            then:
              - lambda: |-
                  if (x->status_code != 200) return;
                  auto root = json::parse(x->response_body);
                  if (root.isNull() || root["status"] != "success") return;
                  id(g_city) = root["city"].as<std::string>();
                  id(g_lat)  = root["lat"].as<float>();
                  id(g_lon)  = root["lon"].as<float>();
                  id(g_weather_ok) = false;   // 拉到新天气前先不露页
                  id(oled).show_page(id(page_climate));  // 重连时强制离开天气页
      # 定位成功后由 weather 间隔任务接管拉取（见 4.3）
```

> 注：`json::parse` 的返回类型/成员访问方式随 ESPHome 版本演进
> （`JsonDocument` / `JsonVariant`），编译报错时按当前版本 API 微调。

### 4.3 天气拉取 + 定时刷新

```yaml
interval:
  # 天气数据：连接后立即补一次，之后每 10 分钟
  - interval: 10min
    then:
      - if:
          condition:
            lambda: "return id(g_lat) != 0.0f || id(g_lon) != 0.0f;"
          then:
            - http_request.get:
                url: !lambda |-
                  return "https://api.open-meteo.com/v1/forecast?latitude="
                    + std::to_string(id(g_lat)) + "&longitude=" + std::to_string(id(g_lon))
                    + "&current=temperature_2m,relative_humidity_2m,weather_code,wind_speed_10m"
                    + "&timezone=Asia%2FShanghai";
                on_response:
                  then:
                    - lambda: |-
                        if (x->status_code != 200) return;
                        auto root = json::parse(x->response_body);
                        if (root.isNull() || !root["current"].is<JsonObject>()) return;
                        auto cur = root["current"];
                        id(g_temp) = cur["temperature_2m"].as<float>();
                        id(g_hum)  = cur["relative_humidity_2m"].as<float>();
                        id(g_wcode)= cur["weather_code"].as<int>();
                        id(g_wind) = cur["wind_speed_10m"].as<float>();
                        id(g_weather_ok) = true;   // 此刻天气页才会参与轮播
```

（首次连接不等 10 分钟：可在 `on_connect` 定位 lambda 成功分支里直接
`id(interval_weather).trigger()`，给 interval 加 `id:` 即可。）

### 4.4 断网隐藏（R5）

```yaml
wifi:
  on_disconnect:
    then:
      - lambda: |-
          id(g_weather_ok) = false;
          id(g_page_idx) = 0;
          id(oled).show_page(id(page_climate));
```

### 4.5 手动页序（替代现有 `display.page.show_next`）

现有 8s interval 改为按 `g_weather_ok` 决定总页数，保证
**未联网时天气页永远不出现**：

```yaml
interval:
  - interval: 8s
    then:
      - lambda: |-
          const int total = id(g_weather_ok) ? 4 : 3;
          id(g_page_idx) = (id(g_page_idx) + 1) % total;
          switch (id(g_page_idx)) {
            case 0: id(oled).show_page(id(page_climate)); break;
            case 1: id(oled).show_page(id(page_air));     break;
            case 2: id(oled).show_page(id(page_face));    break;
            case 3: id(oled).show_page(id(page_weather)); break;
          }
```

## 5. OLED 天气页布局（128×64，第 4 页）

沿用 `ui_layout.h` 的槽位思想：标签 12px、数字 16px、单位右列、行距 16px。

```
天气                            4/4
───────────────────────────────  ← y=13
杭州市                          ← y=15  城市名（12px 中文，最多约 8 字）
多云                26.5°C      ← y=31  天气词（左 12px）+ 温度（右对齐 16px，
                                      右缘 x=114，单位 °C x=116，同温湿度页）
湿度 65%          风速 3.2m/s   ← y=47  左湿度、右风速（size 1 ASCII + 中文标签）
```

- 页眉复用现有 `it.print(0,0,font_cn,"天气")` + 右对齐 `4/4` + 分隔线，
  与现有三页完全同构。
- 温度最坏 `"-12.5"`（60px）+ 天气词 2 字（26px）：26 + gap + 60 ≤ 114，无重叠。
- 城市名超长（如 `鄂尔多斯市` 65px 正常；极端 >8 字）：截断至屏宽或省略后缀“市”。
- 轮播页码随实际页数变化（3 页时显示 `n/3`，联网后 `n/4`），
  现有 `it.printf(... "%d/%d")` 处把分母从常量 3 改为
  `id(g_weather_ok) ? 4 : 3`。

## 6. 中文字库扩充（关键风险点）

现有字库白名单在 `co2.yaml` 的 `font_cn.glyphs`（源 TTF：
`esphome/fonts/fusion-pixel-12px-monospaced-zh_hans.ttf`）。需要新增：

1. **固定措辞字**（必须全部命中，否则屏上缺字）：
   `天气` + §3.3 全部映射文案用字 =
   `晴间多云阴雾毛小中大冻雪米阵雷伴冰雹风`
   —— 与现有白名单合并去重后追加到 `font_cn.glyphs`。
2. **城市名字**（尽力覆盖）：ESPHome 字体是编译期静态字集，无法动态加载。
   策略：
   - 把**常用汉字表前 1000 字**（覆盖绝大多数城市名）追加进 `glyphs`；
     每字位图约占 30–40B flash，1000 字 ≈ 40KB，4MB flash 可承受，
     编译后核对 `.bin` 体积增幅。
   - 用脚本去重合并，避免手抄出错：

     ```bash
     python - <<'EOF'
     base = "温湿度空气质量表情优良中差很未知偏冷热干燥潮舒适正常"
     fixed = "天气晴间多云阴雾毛小中大冻雪米阵雷伴冰雹风"
     top1000 = open("tools/hz_common_1000.txt", encoding="utf-8").read().strip()  # 需先准备
     print(len(set(base + fixed + top1000)))
     EOF
     ```

   - 缺字表现：ESPHome 渲染时该字为空白/占位（不崩溃）；遇到后把该字
     补进 `glyphs` 即可，OTA 重刷生效。
   - 注意 `font_cn` 的 `glyphs` 字符串写入 YAML 时保持 UTF-8、无重复字
     （重复字会导致 ESPHome 字体构建报错或浪费）。

## 7. 改动清单

| 文件                                | 改动                                                                                                                                                                                                                                    |
| ----------------------------------- | --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `esphome/co2.yaml`                  | ① `http_request` + `globals` 新增；② `wifi.on_connect/on_disconnect` 挂定位与隐藏逻辑；③ 新增天气拉取 interval；④ 8s 翻页 interval 改手动页序；⑤ 新增第 4 页 `page_weather` lambda；⑥ `font_cn.glyphs` 扩白名单；⑦ 原三页页码分母改动态 |
| `esphome/secrets.yaml`              | 无新增（ip-api / Open-Meteo 均免 key；若换高德定位则需加 `amap_key`）                                                                                                                                                                   |
| `tools/hz_common_1000.txt`          | 新增：常用字表（一次性生成，供 §6 合并脚本用）                                                                                                                                                                                          |
| Arduino 主固件 / `include/`、`src/` | **不改**                                                                                                                                                                                                                                |
| `esphome/README.md`                 | 补充天气功能说明与排障（联网后生效、缺字补字方法）                                                                                                                                                                                      |

可选增强（不做也不影响需求）：把 `g_city`、天气文案、温度注册为
`text_sensor` / `sensor` 发布到 Home Assistant。

## 8. 验证方案

1. **编译**：`cd esphome && esphome compile co2.yaml`
   —— 通过字体 glyph 数量、flash 占用、lambda API 兼容性三项检查。
2. **烧录 + 串口日志**（`esphome run co2.yaml` 或 esptool，见 esphome/README.md）：
   - [ ] WiFi 未连：轮播仅 3 页，`g_weather_ok=false`
   - [ ] WiFi 连上：日志打印定位城市（`ESP_LOGI` 输出 `g_city/g_lat/g_lon`）
   - [ ] 天气拉取成功：日志打印温度/天气码；第 4 页出现，内容与
         同时段手机天气 App 对照（温度误差 ±1°C 内，天气词一致）
   - [ ] 拔网线/关 AP：天气页在 8s 内消失并强制回温湿度页
   - [ ] 重连：重新定位并恢复天气页
   - [ ] 静置 10 分钟：日志出现第二次天气刷新
3. **城市名抽测**：分别在杭州市、生僻字城市（如`亳州市`）下查看，
   统计缺字并按 §6 补白名单。
4. **模拟器不适用**：天气页依赖 ESPHome lambda，`tools/ui_sim` 不覆盖；
   布局在实际屏幕上核对（或临时用 ESPHome `show_test_card` 对照）。

## 9. 风险与备选

| 风险                                              | 影响                      | 对策                                                                      |
| ------------------------------------------------- | ------------------------- | ------------------------------------------------------------------------- |
| ip-api 免费版仅 HTTP                              | 明文传输；个别网络封 HTTP | 备选高德 IP 定位（HTTPS+免费 key，中文）                                  |
| 城市名汉字不在字库                                | 屏上缺字                  | §6 白名单扩充 + 逐字补                                                    |
| Open-Meteo 国内偶发慢/不可达                      | 天气页不刷新              | `timeout: 10s` + 保留旧值；持续不可达则换和风天气（key）                  |
| ESPHome 版本 API 差异（json / http_request 选项） | 编译报错                  | 以 `esphome compile` 实测为准，骨架已标注需验证点                         |
| 字库体积增长                                      | flash 占用                | 每次编译核对 `firmware.ota.bin` 大小，>70% 告警时砍常用字表               |
| `on_connect` 在 DHCP 完成前触发 HTTP              | 定位请求失败              | 实测 ESPHome `on_connect` 已在获取 IP 后触发；若失败加 `delay(2s)` 再请求 |
