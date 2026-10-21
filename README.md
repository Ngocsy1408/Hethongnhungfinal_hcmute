# 🏍️ Smart Motorcycle Blackbox & Emergency SOS System
### Hệ Thống Hộp Đen Xe Máy Thông Minh & Cảnh Báo Tai Nạn Tự Động (STM32F401 + ESP32-S3 N16R8)

![STM32](https://img.shields.io/badge/Co--Processor-STM32F401CCU6-03234B?style=for-the-badge&logo=stmicroelectronics)
![ESP32-S3](https://img.shields.io/badge/Main--MCU-ESP32--S3_N16R8-E7352C?style=for-the-badge&logo=espressif)
![IMU](https://img.shields.io/badge/IMU_6--Axis-TDK_ICM42688-FF6F00?style=for-the-badge)
![GNSS](https://img.shields.io/badge/GNSS-u--blox_M10-0052CC?style=for-the-badge)
![4G_LTE](https://img.shields.io/badge/4G_LTE-SIMCom_A7680C-00C853?style=for-the-badge)
![Flash](https://img.shields.io/badge/Storage-Winbond_W25Q128_16MB-9C27B0?style=for-the-badge)
![RTOS](https://img.shields.io/badge/OS-FreeRTOS_Dual--Core-2196F3?style=for-the-badge)

Dự án **Hộp Đen Xe Máy Thông Minh (Smart Motorcycle Blackbox)** là hệ thống nhúng thời gian thực đa vi điều khiển, được thiết kế chuyên biệt để hoạt động trong môi trường rung động khắc nghiệt của xe gắn máy. Hệ thống thực hiện thu thập dữ liệu quán tính tốc độ cao (`100Hz`), lọc nhiễu động cơ đốt trong, ước lượng góc nghiêng chính xác khi xe ôm cua/phanh gấp, định vị vệ tinh GNSS, **ghi lại băng dữ liệu 15 giây trước và sau va chạm vào bộ nhớ Flash độc lập (`W25Q128`)**, đồng thời tự động **nhắn tin SMS kèm tọa độ Google Maps & gọi điện cứu hộ khẩn cấp qua mạng 4G LTE (`A7680C`)** tới số điện thoại người thân được cấu hình qua **Bluetooth (`BLE 5.0`)**.

---

## 🏗️ Kiến Trúc Tổng Thể Hệ Thống (System Architecture)

Hệ thống áp dụng kiến trúc phân tầng **Dual-MCU (Hai vi điều khiển song song)** kết hợp **Dual-Core FreeRTOS** để đảm bảo tính thời gian thực tuyệt đối (Hard Real-time), không bao giờ bị mất mẫu cảm biến trong lúc chờ xử lý mạng 4G hay ghi bộ nhớ Flash:

```text
┌───────────────────────────────────────────────────────────────────────────────────┐
│                    KHỐI ĐO LƯỜNG QUÁN TÍNH THỜI GIAN THỰC (100Hz)                 │
│                                                                                   │
│  [ Cảm biến 6 trục ICM-42688 ]                                                    │
│   ├─ Bộ lọc cứng AAF (42Hz) + UI DLPF bậc 2 (Chặn rung động cơ 60Hz - 250Hz)     │
│   └─ Giao tiếp SPI Mode 3 (10.5 MHz, Burst Read 14 Bytes)                         │
│                 │                                                                 │
│                 ▼                                                                 │
│  [ Vi điều khiển STM32F401CCU6 (ARM Cortex-M4F @ 84MHz) ]                         │
│   ├─ Bộ đếm vi mô phần cứng DWT Timer (Độ phân giải 1 µs)                         │
│   ├─ Thuật toán Adaptive Kalman Filter (Tự thích ứng ma trận R chống lực ly tâm)  │
│   ├─ Máy trạng thái hữu hạn (FSM) nhận diện Ngã xe & Va chạm mạnh                 │
│   └─ Đóng gói Binary Frame (45 Bytes) + Mã kiểm tra toàn vẹn CRC16-CCITT          │
└─────────────────┬─────────────────────────────────────────────────────────────────┘
                  │  UART1 (115200 bps, Chu kỳ 10ms / 100Hz)
                  │  [PA9 (TX) ──> GPIO 17 (RX1)] & [PA10 (RX) <── GPIO 16 (TX1)]
                  ▼
┌───────────────────────────────────────────────────────────────────────────────────┐
│              KHỐI XỬ LÝ TRUNG TÂM & VIỄN THÔNG (ESP32-S3 N16R8 @ 240MHz)          │
│                                                                                   │
│  ┌────────────────────────────────────┐   ┌────────────────────────────────────┐  │
│  │     CORE 1 (Real-Time Ingestion)   │   │   CORE 0 (Blackbox & Telemetry)    │  │
│  │ ────────────────────────────────── │   │ ────────────────────────────────── │  │
│  │ • Thu nhận gói 45-byte từ STM32    │   │ • Quản lý Bộ đệm vòng 10 giây      │  │
│  │ • Kiểm tra toàn vẹn CRC16-CCITT    │   │   trước tai nạn + 5 giây sau       │  │
│  │ • Tính gia tốc tổng hợp Peak G     │   │ • Ghi/Đọc băng 15s vào W25Q128     │  │
│  │ • Giải mã NMEA từ GPS u-blox M10   │   │ • Điều khiển SIM 4G A7680C (SMS/   │  │
│  │   (Vĩ độ, Kinh độ, Số vệ tinh,     │   │   Call SOS kèm Link Google Maps)   │  │
│  │    Vận tốc thực tế km/h)           │   │ • Phát sóng BLE 5.0 nhận lệnh đổi  │  │
│  │                                    │   │   SĐT SOS & lưu vĩnh viễn vào NVS  │  │
│  └────────────────────────────────────┘   └────────────────────────────────────┘  │
└────────┬───────────────────────────┬──────────────────────────┬───────────────────┘
         │ UART2 (115200 bps)         │ SPI2 (FSPI)              │ UART0 (115200 bps)
         ▼                           ▼                          ▼
 [ GPS u-blox M10 ]        [ Flash W25Q128 (16MB) ]    [ SIM 4G LTE A7680C ]
 (GPIO 18 RX / GPIO 8 TX)  (CS:10, MOSI:11, SCK:12,    (GPIO 4 RX / GPIO 5 TX)
                            MISO:13)
```

---

## 🌟 Điểm Nhấn Kỹ Thuật Chuyên Sâu (Key Engineering Highlights)

### 1. Bộ lọc phần cứng kép chống rung động cơ (Hardware Anti-Aliasing & UI DLPF)
Rung động cơ học từ piston và xích xe máy nằm trong dải tần `60Hz – 250Hz`. Nếu đọc thô thông thường, hiện tượng chồng phổ (*Aliasing*) sẽ làm sai lệch hoàn toàn gia tốc kế.
- Hệ thống can thiệp trực tiếp vào **Bank 1 & Bank 2** của cảm biến **ICM-42688** để kích hoạt bộ lọc **Anti-Aliasing Filter (AAF)** ở tần số cắt `~42Hz` kết hợp bộ lọc số **UI DLPF bậc 2** (`BW = ODR/4`).
- Mọi nhiễu rung cơ học tần số cao bị triệt tiêu ngay từ tầng phần cứng trước khi vào vi điều khiển.

### 2. Thuật toán Kalman Thích Ứng (Adaptive Kalman Filter - AKF)
Bộ lọc Kalman tuyến tính thông thường bị đánh lừa khi xe máy **ôm cua nghiêng người** hoặc **tăng tốc/phanh gấp** do lực ly tâm cộng hưởng với trọng lực.
- Hệ thống tính toán độ lệch của vector gia tốc tổng so với trọng trường chuẩn:
  $$e_{acc} = \left| \sqrt{a_x^2 + a_y^2 + a_z^2} - 1.0g \right|$$
- Ma trận hiệp phương sai nhiễu đo lường $R_{adaptive}$ được điều chỉnh động theo hàm bậc hai:
  $$R_{adaptive} = R_{base} \cdot \left(1 + \gamma \cdot e_{acc}^2\right)$$
- Khi xe chịu gia tốc ngoại lai lớn ($e_{acc} \gg 0$), $R_{adaptive}$ tăng vọt giúp bộ lọc tự động giảm trọng số của Gia tốc kế và ưu tiên tích phân từ Con quay hồi chuyển (Gyroscope) với bước thời gian $\Delta t$ chính xác tới từng micro-giây ($\mu s$) nhờ bộ đếm xung nhịp phần cứng **ARM Cortex-M4 DWT Cycle Counter**.

### 3. Cơ chế "Hộp Đen Máy Bay" 15 Giây (10s Pre-Crash + 5s Post-Crash Recorder)
Thay vì chỉ lưu một thời điểm đơn lẻ khi va chạm, hệ thống hoạt động giống hệt **Flight Data Recorder (Hộp đen máy bay)**:
- **Giai đoạn tuần tra bình thường:** Một bộ đệm vòng (*Circular Ring Buffer*) trong RAM liên tục lưu giữ trạng thái xe của **10 giây gần nhất (`T-10s` đến `T-1s`)**.
- **Khoảnh khắc xảy ra va chạm (`T = 0s`):**
  1. Đóng băng ngay lập tức 10 giây quá khứ trong bộ đệm vòng.
  2. Chụp trạng thái va chạm cực đại tại mốc `T = 0s`.
  3. Kích hoạt song song module **SIM 4G A7680C** gửi tin nhắn SMS khẩn cấp (kèm tọa độ Google Maps, vận tốc, lực va chạm $G_{max}$, góc nghiêng) và thực hiện cuộc gọi báo động.
  4. Tiếp tục thu thập thêm **5 giây diễn biến sau va chạm (`T+1s` đến `T+5s`)**.
- **Lưu trữ vĩnh viễn:** Toàn bộ chuỗi **16 mốc thời gian (`T-10s` $\rightarrow$ `T+5s`)** được ghi xuống chip nhớ **NOR Flash `W25Q128` (16MB)** qua giao tiếp SPI, cho phép phục dựng lại toàn bộ hiện trường trước, trong và sau tai nạn kể cả khi xe bị mất nguồn hoàn toàn.

### 4. Cấu hình Số Điện Thoại SOS Động & Trích xuất Hộp Đen qua Bluetooth BLE 5.0
- Tích hợp dịch vụ **Nordic UART Service (NUS)** qua Bluetooth Low Energy (`BLE 5.0`) với tên phát sóng **`HOP_DEN_XE_MAY`**.
- Cho phép người dùng sử dụng điện thoại thông minh (Android/iOS) để thay đổi số điện thoại người thân nhận tin nhắn SOS bất kỳ lúc nào mà không cần nạp lại code. Số điện thoại mới được lưu vĩnh viễn vào phân vùng **NVS (Non-Volatile Storage)** của ESP32-S3.

---

## 📂 Cấu Trúc Thư Mục Dự Án (Repository Structure)

```text
Motorcycle_Blackbox_Project/
│
├── 📁 Firmware_STM32F401/           # Dự án STM32CubeIDE (Cortex-M4 Bare-Metal / HAL Lite)
│   ├── Inc/
│   │   ├── stm32f4xx_hal.h          # Lớp trừu tượng thanh ghi phần cứng STM32F401 (RCC, GPIO, SPI1, USART1, DMA2)
│   │   ├── dwt_stm32.h              # Bộ đếm thời gian thực vi mô DWT Cycle Counter (1 µs resolution)
│   │   ├── icm42688_defs.h          # Bản đồ thanh ghi ICM-42688 (Bank 0, 1, 2) & cấu hình bộ lọc AAF 42Hz
│   │   ├── icm42688.h               # Khai báo cấu trúc & hàm giao tiếp cảm biến ICM-42688
│   │   ├── adaptive_kalman.h        # Khai báo cấu trúc thuật toán Adaptive Kalman Filter
│   │   └── telemetry_protocol.h     # Định nghĩa khung truyền nhị phân 45-byte & thuật toán CRC16-CCITT
│   └── Src/
│       ├── icm42688.c               # Driver SPI Mode 3, khởi tạo AAF/DLPF & Burst Read 14-byte
│       ├── adaptive_kalman.c        # Thực thi toán học bộ lọc Adaptive Kalman
│       └── main.c                   # Vòng lặp thời gian thực 100Hz, FSM phát hiện tai nạn & truyền UART
│
├── 📁 Firmware_ESP32S3/             # Dự án Arduino IDE / PlatformIO cho ESP32-S3 N16R8
│   ├── ESP32_Blackbox.ino           # Chương trình chính điều phối 2 nhân FreeRTOS (Core 0 & Core 1)
│   ├── config_types.h               # Định nghĩa Sơ đồ chân, Gói tin Telemetry & Bản ghi Hộp Đen W25Q128
│   ├── w25q128_flash.h              # Driver SPI quản lý bộ nhớ Flash W25Q128 (Ghi/Đọc băng 15s tai nạn)
│   ├── gps_m10_parser.h             # Bộ giải mã NMEA ($GNRMC, $GNGGA) cho GPS u-blox M10
│   ├── sim_a7680c.h                 # Tập lệnh AT điều khiển SIM 4G A7680C (SMS Google Maps & Voice Call)
│   └── ble_app_config.h             # Server Bluetooth BLE 5.0 nhận lệnh cấu hình SĐT & trích xuất Log
│
└── 📄 README.md                     # Tài liệu hướng dẫn kỹ thuật toàn diện của dự án
```

---

## 🔌 Sơ Đồ Đấu Nối Phần Cứng Chi Tiết (Hardware Pinout)

### 1. Sơ đồ đấu nối trên Vi điều khiển `STM32F401CCU6 (BlackPill)`
| Thiết bị kết nối | Chân Thiết bị | Chân STM32F401 | Giao thức / Chức năng | Ghi chú kỹ thuật |
| :--- | :---: | :---: | :--- | :--- |
| **Cảm biến ICM-42688** | `VCC / VDDIO` | **`3.3V`** | Nguồn nuôi | **Tuyệt đối không cấp 5V** |
| | `GND` | **`GND`** | Mass chung | Common Ground |
| | `NCS (CS)` | **`PA4`** | `GPIO_Output` | Chip Select (Active Low) |
| | `SCLK (SCK)` | **`PA5`** | `SPI1_SCK` (AF5) | SPI Mode 3 (`10.5 MHz`) |
| | `SDO (MISO)` | **`PA6`** | `SPI1_MISO` (AF5) | Dữ liệu từ ICM-42688 $\rightarrow$ STM32 |
| | `SDI (MOSI)` | **`PA7`** | `SPI1_MOSI` (AF5) | Lệnh cấu hình từ STM32 $\rightarrow$ ICM-42688 |
| **Giao tiếp ESP32-S3** | `GPIO 17 (RX1)` | **`PA9`** | `USART1_TX` (AF7) | Bắn gói tin 45-byte @ `115200 bps` |
| | `GPIO 16 (TX1)` | **`PA10`** | `USART1_RX` (AF7) | Nhận lệnh từ ESP32-S3 |
| | `GND` | **`GND`** | Mass chung | Đồng bộ mức điện áp logic |
| **Mạch nạp ST-Link V2**| `3.3V / GND` | **`3V3 / GND`** | Nguồn nạp | |
| | `SWDIO / SWCLK`| **`DIO / CLK`** | Cổng nạp SWD | |
| **Đèn trạng thái** | `Onboard LED` | **`PC13`** | `GPIO_Output` | Chớp `1s/lần` khi chạy bình thường |

---

### 2. Sơ đồ đấu nối trên Vi điều khiển Trung tâm `ESP32-S3 N16R8`
*(Lưu ý: Tránh hoàn toàn các chân `GPIO 35, 36, 37` dành riêng cho Octal PSRAM 8MB nội bộ)*

| Thiết bị ngoại vi | Chân Thiết bị | Chân ESP32-S3 N16R8 | Bộ ngoại vi | Ghi chú kỹ thuật |
| :--- | :---: | :---: | :---: | :--- |
| **1. STM32F401CCU6** | `PA9 (TX)` | **`GPIO 17`** | `UART1_RX` | Nhận gói tin Kalman `100Hz` (`115200 bps`) |
| | `PA10 (RX)` | **`GPIO 16`** | `UART1_TX` | Kênh truyền ngược |
| | `GND` | **`GND`** | Common GND | Bắt buộc nối Mass chung |
| **2. Flash W25Q128 (16MB)**| `VCC` | **`3.3V`** | Power Supply | Chỉ cấp điện áp `3.3V` |
| | `GND` | **`GND`** | Common GND | Mass chung |
| | `CS` | **`GPIO 10`** | `SPI2_CS` | Chip Select |
| | `DI (MOSI)` | **`GPIO 11`** | `SPI2_MOSI` | Dữ liệu ghi vào W25Q128 |
| | `CLK (SCK)` | **`GPIO 12`** | `SPI2_SCK` | Xung nhịp SPI |
| | `DO (MISO)` | **`GPIO 13`** | `SPI2_MISO` | Dữ liệu đọc từ W25Q128 |
| **3. GPS u-blox M10** | `TX` | **`GPIO 18`** | `UART2_RX` | Nhận chuỗi NMEA (`38400 bps`) |
| | `RX` | **`GPIO 8`** | `UART2_TX` | Cấu hình GPS |
| | `VCC / GND` | **`5V / GND`** | Power Supply | Đặt ăng-ten hướng lên trời |
| **4. SIM 4G A7680C** | `TXD` | **`GPIO 4`** | `UART0_RX` | Nhận phản hồi AT (`115200 bps`) |
| | `RXD` | **`GPIO 5`** | `UART0_TX` | Gửi tập lệnh AT điều khiển |
| | `VCC / GND` | **`5V (2A) / GND`** | Power Supply | Yêu cầu nguồn dòng đỉnh $\ge 2\text{A}$ |

---

## 📦 Đặc Tả Giao Thức Gói Tin Nhị Phân (Binary Telemetry Protocol)

Mỗi `10ms` (`100Hz`), STM32F401 đóng gói và truyền 1 khung dữ liệu nhị phân dài cố định **45 Bytes** (Little-Endian, `#pragma pack(1)`) sang ESP32-S3:

| Byte Offset | Kiểu dữ liệu | Tên trường | Đơn vị / Giá trị | Mô tả chi tiết |
| :---: | :---: | :--- | :---: | :--- |
| `0` | `uint8_t` | `magic_1` | `0xAA` | Byte đồng bộ đầu khung thứ nhất |
| `1` | `uint8_t` | `magic_2` | `0x55` | Byte đồng bộ đầu khung thứ hai |
| `2 - 5` | `uint32_t` | `seq_num` | `0 .. 2^32-1` | Số thứ tự gói tin liên tiếp |
| `6 - 9` | `uint32_t` | `timestamp_us`| Micro-giây ($\mu s$)| Thời gian thực từ bộ đếm DWT |
| `10 - 21` | `float[3]` | `accel[3]` | $g$ ($\pm 16g$) | Gia tốc tuyến tính 3 trục $A_x, A_y, A_z$ |
| `22 - 33` | `float[3]` | `gyro[3]` | $^\circ/s$ ($\pm 2000^\circ/s$)| Vận tốc góc 3 trục $G_x, G_y, G_z$ |
| `34 - 37` | `float` | `roll` | Độ ($^\circ$) | Góc nghiêng trái/phải sau lọc Adaptive Kalman |
| `38 - 41` | `float` | `pitch` | Độ ($^\circ$) | Góc chúi/ngửa xe sau lọc Adaptive Kalman |
| `42` | `uint8_t` | `crash_state` | `0, 1, 2, 3, 99`| `0`: Bình thường, `2`: Ngã xe, `3`: Va chạm, `99`: Lỗi IMU |
| `43 - 44` | `uint16_t` | `crc16` | `CRC16-CCITT` | Đa thức sinh `0x1021`, kiểm tra toàn vẹn từ byte `0..42` |

---

## 📱 Tập Lệnh Điều Khiển Qua Bluetooth (BLE Smartphone Commands)

Sử dụng ứng dụng **Serial Bluetooth Terminal** (chế độ *Bluetooth LE*) hoặc **nRF Connect** trên điện thoại, kết nối vào thiết bị **`HOP_DEN_XE_MAY`** và gửi các lệnh văn bản sau:

| Cú pháp lệnh gửi đi | Chức năng thực hiện | Phản hồi từ Hộp Đen về Điện thoại |
| :--- | :--- | :--- |
| **`SDT:0912345678`** | Cập nhật số điện thoại nhận tin nhắn & cuộc gọi cứu hộ SOS (Lưu vĩnh viễn vào bộ nhớ NVS Flash) | `✅ Da luu SDT SOS moi: 0912345678` |
| **`INFO`** | Kiểm tra nhanh trạng thái hiện tại của xe (SĐT SOS, Góc Roll/Pitch, Trạng thái bắt vệ tinh GPS, Vận tốc km/h) | Bảng thông số thời gian thực của xe |
| **`LOG`** | Trích xuất **Băng ghi Hộp Đen 15 giây (`T-10s` đến `T+5s`)** từ chip nhớ Flash **`W25Q128`** | Bảng phục dựng diễn biến 15 giây quanh vụ tai nạn |
| **`CLEAR`** | Xóa trắng lịch sử tai nạn lưu trong chip nhớ **`W25Q128`** | `✅ Da xoa trang lich su hop den W25Q128!` |
| **`TEST_SOS`** | Kích hoạt giả lập sự cố tai nạn ngay lập tức để kiểm tra ghi `W25Q128` và gửi tin nhắn `SIM 4G` | `🚨 Dang kich hoat thu nghiem SOS!` |

### Ví dụ dữ liệu trích xuất từ chip nhớ `W25Q128` khi gửi lệnh `LOG`:
```text
=== PHUC DUNG 15 GIAY TAI NAN (W25Q128) ===
[T-10s] Roll:  2.1° Pitch:  1.4° |  1.0G | 42.5km/h | OK
[T -9s] Roll:  3.0° Pitch:  1.5° |  1.0G | 44.0km/h | OK
...
[T -1s] Roll: 28.4° Pitch: -6.2° |  1.8G | 31.0km/h | OK
[T +0s] Roll: 67.5° Pitch: 18.2° |  4.9G |  0.0km/h | 💥 DAM!
[T +1s] Roll: 68.8° Pitch: 19.0° |  1.1G |  0.0km/h | ⚠️ NGA
...
[T +5s] Roll: 68.8° Pitch: 19.0° |  1.0G |  0.0km/h | ⚠️ NGA
===========================================
```

---

## 🛠️ Hướng Dẫn Biên Dịch & Nạp Chương Trình (Build & Flash)

### 1. Nạp Firmware cho `STM32F401CCU6`
1. Mở phần mềm **STM32CubeIDE** (tương thích hoàn toàn với bản `2.2.0` mới nhất).
2. Đưa các file `.h` vào thư mục `Inc/` và các file `.c` vào thư mục `Src/`.
3. Bấm biểu tượng **Build (🔨)** để biên dịch (`0 errors, 0 warnings`).
4. Kết nối mạch nạp **ST-Link V2** và bấm nút **Run (▶️)** để nạp xuống chip. Đèn LED `PC13` sẽ chớp nhịp tim đều đặn `1 giây/lần` báo hiệu cảm biến `ICM-42688` và đường truyền `UART1` đang hoạt động.

### 2. Nạp Firmware cho `ESP32-S3 N16R8`
1. Mở **Arduino IDE 2.3.x**, đảm bảo đã cài đặt gói board **esp32 by Espressif Systems**.
2. Cấu hình trong menu **Tools**:
   - **Board:** `ESP32S3 Dev Module`
   - **Flash Size:** `16MB (128Mb)`
   - **PSRAM:** `OPI PSRAM`
3. Mở file `ESP32_Blackbox.ino` (cùng 5 tab `.h` đi kèm) và nhấn **Upload (➡️)**.
4. Mở **Serial Monitor** ở tốc độ **`115200 baud`** để giám sát trực tiếp toàn bộ thông số của hệ thống.

---

## 📜 Giấy Phép & Tác Giả (License & Author)
Dự án được phát triển phục vụ mục đích nghiên cứu kỹ thuật hệ thống nhúng, xử lý tín hiệu số (DSP) và an toàn giao thông thông minh.
