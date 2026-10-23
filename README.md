# 🏍️ Smart Motorcycle Blackbox & Automated 4G SOS Telemetry System
### Hệ Thống Hộp Đen Xe Máy Thông Minh, Lọc Nhiễu Quán Tính Thời Gian Thực & Cứu Hộ Tự Động Đa Vi Điều Khiển

![STM32F401](https://img.shields.io/badge/Co--Processor-STM32F401CCU6_(Cortex--M4F)-03234B?style=for-the-badge&logo=stmicroelectronics)
![ESP32-S3](https://img.shields.io/badge/Main--Controller-ESP32--S3_N16R8_(Dual--Core)-E7352C?style=for-the-badge&logo=espressif)
![ICM42688](https://img.shields.io/badge/IMU_6--Axis-TDK_ICM--42688--P-FF6F00?style=for-the-badge)
![GNSS](https://img.shields.io/badge/GNSS_4--System-u--blox_M10_(115200_bps)-0052CC?style=for-the-badge)
![4G_LTE](https://img.shields.io/badge/4G_LTE_Cat--1-SIMCom_A7682S--LANS-00C853?style=for-the-badge)
![Flash](https://img.shields.io/badge/Blackbox_Storage-Winbond_W25Q128_(16MB)-9C27B0?style=for-the-badge)
![BLE](https://img.shields.io/badge/Wireless_Config-Bluetooth_BLE_5.0-2196F3?style=for-the-badge&logo=bluetooth)

---

## 📖 1. Giới Thiệu Tổng Quan (Overview)

**Smart Motorcycle Blackbox** là hệ thống hộp đen hành trình và cảnh báo tai nạn tự động chuẩn công nghiệp được thiết kế chuyên biệt cho xe gắn máy. Khác với các hệ thống đo góc nghiêng cơ bản vốn dễ bị tê liệt bởi **nhiễu rung tần số cao từ động cơ đốt trong** và **lực ly tâm khi xe nghiêng vào cua**, dự án này giải quyết bài toán bằng kiến trúc **phân tách phần cứng song song (Dual-MCU Architecture)**:

1. **Node Xử Lý Quán Tính Thời Gian Thực (`STM32F401CCU6` + `ICM-42688`):**
   - Chạy độc lập ở xung nhịp `84MHz` với tập lệnh phần cứng dấu phẩy động (**FPU**).
   - Kích hoạt bộ lọc chống chồng phổ phần cứng (**Hardware Anti-Aliasing Filter - AAF 42Hz**) và bộ lọc thông thấp **UI DLPF bậc 2** ngay bên trong cảm biến `ICM-42688`.
   - Thực thi thuật toán **Lọc Kalman Thích Ứng (Adaptive Kalman Filter - AKF)** ở tần số cố định **`100Hz` (`10ms`)** với độ phân giải thời gian cấp micro-giây ($\mu s$) từ bộ đếm phần cứng **DWT Cycle Counter**.
   - Đóng gói dữ liệu nhị phân `45 Bytes/frame` kèm mã kiểm tra toàn vẹn **`CRC16-CCITT`** truyền sang vi điều khiển trung tâm.

2. **Node Trung Tâm Hộp Đen & Viễn Thông Đa Nhiệm (`ESP32-S3 N16R8` + `FreeRTOS`):**
   - **Tự động hiệu chuẩn góc lắp đặt (`Auto-Zero Mounting Calibration`):** Tự động lấy vị trí đặt mạch ban đầu làm mốc cân bằng `0.00°`, cho phép gắn hộp đen ở mọi tư thế trong cốp xe.
   - **Cơ chế "Hộp Đen Máy Bay" 15 Giây (`10s Pre-Crash + 5s Post-Crash Recorder`):** Liên tục duy trì bộ đệm vòng (*Circular Ring Buffer*) lưu 10 giây quá khứ gần nhất trong RAM. Khi xảy ra va chạm (`T = 0s`), hệ thống đóng băng 10 giây trước đó, thu tiếp 5 giây diễn biến sau tai nạn và khắc vĩnh viễn toàn bộ **16 mốc thời gian (`T-10s` đến `T+5s`)** vào chip nhớ **NOR Flash `W25Q128` (16MB)**.
   - **Định vị vệ tinh đa hệ (`u-blox M10`) & Cứu hộ 4G LTE (`A7682S-LANS`):** Thu nhận đồng thời 4 hệ thống vệ tinh (*GPS, Galileo, BeiDou, QZSS*) ở tốc độ `115200 bps` và tự động bắn tin nhắn **SMS khẩn cấp kèm đường link bản đồ Google Maps**, vận tốc, góc nghiêng và lực va chạm cực đại ($G_{max}$) về số điện thoại người thân.
   - **Cấu hình không dây qua Bluetooth (`BLE 5.0`):** Đổi số điện thoại nhận tin nhắn cứu hộ (`SDT:...`), đặt lại góc cân bằng (`ZERO`) và trích xuất dữ liệu hộp đen (`LOG`) trực tiếp từ điện thoại thông minh, lưu vĩnh viễn vào bộ nhớ **NVS Flash**.

---

## 🏗️ 2. Sơ Đồ Kiến Trúc Hệ Thống (System Architecture)

```text
┌──────────────────────────────────────────────────────────────────────────────────────┐
│                 TẦNG 1: KHỐI ĐO LƯỜNG & LỌC NHIỄU QUÁN TÍNH (100Hz)                  │
│                                                                                      │
│  [ Cảm biến IMU 6 trục TDK ICM-42688-P ]                                             │
│   ├─ Dải đo: Accel ±16G (Chống bão hòa khi va chạm) | Gyro ±2000 dps                 │
│   ├─ Bộ lọc cứng: Anti-Aliasing Filter (AAF 42Hz) + UI DLPF Order 2 (Chặn rung máy)  │
│   └─ Giao tiếp: SPI1 Mode 3 (10.5 MHz, Burst Read 14 Bytes đồng bộ pha)              │
│                                        │                                             │
│                                        ▼                                             │
│  [ Vi điều khiển STM32F401CCU6 (ARM Cortex-M4F @ 84MHz, Bare-Metal Register) ]       │
│   ├─ Khởi động độc lập: Constructor FPU Enable (SCB_CPACR) & Cold-Boot DWT Timer     │
│   ├─ Bộ lọc: Adaptive Kalman Filter (Tự điều chỉnh R_adaptive theo ||a|| - 1.0g)     │
│   └─ Đóng gói: Khung nhị phân 45 Bytes (#pragma pack(1)) + Checksum CRC16-CCITT      │
└────────────────────────────────────────┬─────────────────────────────────────────────┘
                                         │
                        UART1 (115200 bps, 8N1, 100 Frames/s)
                        [PA9 (TX) ──> GPIO 17 (RX1)]
                        [PA10(RX) <── GPIO 16 (TX1)]
                                         │
                                         ▼
┌──────────────────────────────────────────────────────────────────────────────────────┐
│            TẦNG 2: KHỐI ĐIỀU PHỐI TRUNG TÂM (ESP32-S3 N16R8 @ 240MHz)                │
│                                                                                      │
│  ┌───────────────────────────────────────┐  ┌─────────────────────────────────────┐  │
│  │       CORE 1 (Real-Time Loop)         │  │     CORE 0 (Task_Blackbox_Manager)  │  │
│  │ ───────────────────────────────────── │  │ ─────────────────────────────────── │  │
│  │ • Thu nhận gói 45-byte từ STM32F401   │  │ • Nhịp 1Hz: Cập nhật Bộ đệm vòng    │  │
│  │ • Kiểm tra mã bảo mật CRC16-CCITT     │  │   10 giây trước va chạm (T-10..T-1) │  │
│  │ • Tự động lấy mốc 0.00° (Auto-Zero)   │  │ • Khi tai nạn (T=0s): Đóng băng 10s │  │
│  │ • Phát hiện Ngã xe (>55° trong 1.5s)  │  │   quá khứ + thu tiếp 5s sau (T+1..5)│  │
│  │   & Va chạm mạnh (Peak Impact > 3.5G) │  │ • Lưu 16 mốc vào Flash W25Q128      │  │
│  │ • Giải mã NMEA 115200 bps từ GPS M10  │  │ • Phát lệnh bắn SMS SOS kèm tọa độ  │  │
│  │   ($GNRMC, $GNGGA -> Lat, Lon, km/h)  │  │   Google Maps qua SIM 4G A7682S     │  │
│  └───────────────────────────────────────┘  └─────────────────────────────────────┘  │
└───────────┬─────────────────────────────┬────────────────────────────┬───────────────┘
            │ SPI2 (10, 11, 12, 13)       │ UART2 (115200 bps)         │ BLE 5.0 (NUS)
            ▼                             ▼                            ▼
  [ NOR Flash W25Q128 (16MB) ]   [ GPS M10 & SIM 4G A7682S-LANS ]  [ Điện thoại / PC ]
  • Tự động đánh thức (0xAB)     • GPS M10: GPIO 18 / GPIO 8       • Đổi SĐT (SDT:...)
  • Tự động đảo chân DI/DO       • SIM 4G : GPIO 4  / GPIO 5       • Trích xuất (LOG)
  • Lưu vĩnh viễn băng 15 giây   • Tự động dò xung & chiều dây     • Cân bằng (ZERO)
```

---

## 🧠 3. Cơ Sở Lý Thuyết & Thuật Toán Cốt Lõi (Core Algorithms)

### 3.1. Thuật toán Kalman Thích Ứng (Adaptive Kalman Filter)
Khi xe máy di chuyển thực tế, gia tốc kế đo tổng hợp cả trọng lực $\mathbf{g}$ và gia tốc quán tính ngoại lai $\mathbf{a}_{ext}$ (lực ly tâm khi vào cua, phanh gấp, ổ gà). Nếu dùng bộ lọc Kalman cố định, góc nghiêng ước lượng sẽ bị trôi theo lực ly tâm.

Hệ thống tính độ lệch chuẩn hóa của độ lớn vector gia tốc:
$$e_{acc} = \left| \sqrt{a_x^2 + a_y^2 + a_z^2} - 1.0g \right|$$

Từ đó, phương sai nhiễu đo lường $R_{adaptive}$ được điều chỉnh động ở mỗi chu kỳ $k$ ($\Delta t = 10\text{ ms}$):
$$R_{adaptive}(k) = R_{base} \cdot \left( 1 + 10 \cdot e_{acc}^2 \right)$$

- **Khi xe đứng yên hoặc chạy thẳng đều ($e_{acc} \approx 0$):** $R_{adaptive} \approx R_{base}$, hệ số Kalman Gain $K$ lớn giúp dùng Gia tốc kế để triệt tiêu hoàn toàn độ trôi (*Bias Drift*) của Gyroscope.
- **Khi xe ôm cua gắt, tăng tốc hoặc xóc mạnh ($e_{acc} \gg 0$):** $R_{adaptive}$ tăng theo hàm mũ $\rightarrow$ Hệ số $K \to 0$, bộ lọc lập tức bỏ qua sai số tạm thời của Gia tốc kế và tin tưởng hoàn toàn vào phép tích phân vận tốc góc từ Gyroscope.

### 3.2. Tự động quy chiếu góc lắp đặt (Auto-Zero Mounting Calibration)
Khi bắt cố định hộp đen vào khung xe máy, cảm biến có thể nằm nghiêng một góc ban đầu $(\phi_0, \theta_0)$. Ngay tại gói tin hợp lệ đầu tiên (hoặc khi nhận lệnh `ZERO`), hệ thống ghi nhận góc tham chiếu và chuẩn hóa miền góc về $[-180^\circ, +180^\circ]$:
$$\text{Roll}_{rel} = \text{wrap}_{180}(\text{Roll}_{Kalman} - \phi_0), \quad \text{Pitch}_{rel} = \text{wrap}_{180}(\text{Pitch}_{Kalman} - \theta_0)$$

---

## 📂 4. Cấu Trúc Mã Nguồn (Project Directory Structure)

```text
Motorcycle_Blackbox_Project/
│
├── 📁 Firmware_STM32F401/           # Dự án trên STM32CubeIDE 2.2.0 (STM32F401CCU6)
│   ├── Inc/
│   │   ├── stm32f4xx_hal.h          # Định nghĩa thanh ghi phần cứng (RCC, GPIO, SPI1, USART1, DMA2, CoreDebug)
│   │   ├── dwt_stm32.h              # Bộ đếm chu kỳ xung nhịp DWT Timer (Độ phân giải 1 micro-giây)
│   │   ├── icm42688_defs.h          # Bản đồ thanh ghi ICM-42688 (Bank 0, 1, 2) & thông số lọc AAF 42Hz
│   │   ├── icm42688.h               # Cấu trúc dữ liệu & nguyên mẫu hàm giao tiếp IMU
│   │   ├── adaptive_kalman.h        # Cấu trúc bộ lọc Adaptive Kalman Filter
│   │   └── telemetry_protocol.h     # Định nghĩa khung nhị phân 45-byte & hàm tính CRC16-CCITT
│   └── Src/
│       ├── icm42688.c               # Driver SPI Mode 3, cấu hình AAF/DLPF & Burst Read 14-byte
│       ├── adaptive_kalman.c        # Giải thuật toán Adaptive Kalman 1D cho trục Roll & Pitch
│       └── main.c                   # Khởi tạo FPU trước main(), vòng lặp 100Hz & truyền khung UART1
│
├── 📁 Firmware_ESP32S3/             # Dự án trên Arduino IDE 2.3.4 (ESP32-S3 N16R8)
│   ├── ESP32_Blackbox.ino           # Chương trình chính điều phối Core 0 & Core 1, Auto-Zero & FSM
│   ├── config_types.h               # Khai báo Sơ đồ chân, Gói tin 45-byte & Cấu trúc Hộp Đen 15 giây
│   ├── w25q128_flash.h              # Driver SPI đọc/ghi băng 15s trên W25Q128 (Tự động đảo chân DI/DO)
│   ├── gps_m10_parser.h             # Giải mã tọa độ thập phân & tốc độ km/h từ GPS u-blox M10
│   ├── sim_a7680c.h                 # Điều khiển SIM 4G A7682S-LANS & GPS M10 (Tự động dò xung & chiều dây)
│   └── ble_app_config.h             # Dịch vụ Bluetooth BLE 5.0, lưu NVS Flash & xử lý tập lệnh điều khiển
│
└── 📄 README.md                     # Tài liệu kỹ thuật chi tiết của dự án
```

---

## 🔌 5. Sơ Đồ Đấu Nối Phần Cứng (Hardware Wiring & Pinout)

### 5.1. Kết nối trên `STM32F401CCU6 (BlackPill)`
| Thiết bị | Chân Thiết bị | Chân STM32F401 | Chức năng | Lưu ý |
| :--- | :---: | :---: | :--- | :--- |
| **ICM-42688** | `VCC / VDDIO` | **`3.3V`** | Nguồn nuôi `3.3V` | **Cấm cấp 5V** |
| | `GND` | **`GND`** | Mass chung | |
| | `NCS (CS)` | **`PA4`** | `GPIO_Output` | Chip Select |
| | `SCLK (SCK)` | **`PA5`** | `SPI1_SCK` (AF5) | SPI Mode 3 (`10.5MHz`) |
| | `SDO (MISO)` | **`PA6`** | `SPI1_MISO` (AF5) | |
| | `SDI (MOSI)` | **`PA7`** | `SPI1_MOSI` (AF5) | |
| **ESP32-S3** | `GPIO 17 (RX1)`| **`PA9`** | `USART1_TX` (AF7) | Truyền gói 45-byte (`115200`) |
| | `GPIO 16 (TX1)`| **`PA10`** | `USART1_RX` (AF7) | |
| | `3V3 / GND` | **`3V3 / GND`**| Nguồn & Mass chung | Đồng bộ áp logic |
| **LED Nhịp tim**| `LED Blue` | **`PC13`** | `GPIO_Output` | Chớp `1s/lần` khi chạy tốt |

### 5.2. Kết nối trên `ESP32-S3 N16R8`
*(Không sử dụng các chân `GPIO 35, 36, 37` dành riêng cho Octal PSRAM 8MB nội bộ)*

| Thiết bị ngoại vi | Chân Thiết bị | Chân ESP32-S3 N16R8 | Giao thức / Tốc độ | Cơ chế thông minh tích hợp |
| :--- | :---: | :---: | :--- | :--- |
| **1. STM32F401** | `PA9 (TX)` | **`GPIO 17`** | `UART1_RX` (`115200`) | Kiểm tra CRC16 & Auto-Zero góc `0°` |
| | `PA10 (RX)` | **`GPIO 16`** | `UART1_TX` (`115200`) | |
| | `GND` | **`GND`** | Common Ground | |
| **2. Flash W25Q128** | `VCC` | **`3.3V`** | Nguồn `3.3V` | **Chỉ cấp 3.3V** |
| | `GND` | **`GND`** | Common Ground | |
| | `CS` | **`GPIO 10`** | `SPI2_CS` | Tự động đánh thức (`0xAB`) & |
| | `DI (MOSI)` | **`GPIO 11`** | `SPI2_MOSI` | Tự động đảo chiều chân `11 <-> 13` |
| | `CLK (SCK)` | **`GPIO 12`** | `SPI2_SCK` | nếu cắm ngược dây `DI / DO` |
| | `DO (MISO)` | **`GPIO 13`** | `SPI2_MISO` | |
| **3. GPS u-blox M10**| `TX` | **`GPIO 18`** | `UART2_RX` (`115200`) | Tự động đo xung tìm chân `TX` |
| | `RX` | **`GPIO 8`** | `UART2_TX` (`115200`) | giữa `GPIO 18` và `GPIO 8` |
| | `VCC / GND` | **`5V / GND`**| Nguồn nuôi GPS | |
| **4. SIM A7682S-LANS**| `TXD` | **`GPIO 4`** | `UART2_RX` (`115200`) | Tự động dò chiều dây `4 <-> 5` & |
| | `RXD` | **`GPIO 5`** | `UART2_TX` (`115200`) | lọc bỏ nhiễu byte `0x00` |
| | `VCC / GND` | **`5V / GND`**| Nguồn dòng đỉnh 2A | |

---

## 📦 6. Đặc Tả Cấu Trúc Khung Truyền & Bộ Nhớ Hộp Đen

### 6.1. Khung truyền UART `TelemetryPacket_t` (45 Bytes @ 100Hz)
| Byte Offset | Kiểu dữ liệu | Tên trường | Mô tả |
| :---: | :---: | :--- | :--- |
| `0 - 1` | `uint8_t[2]` | `magic_1, magic_2` | Chuỗi đồng bộ đầu khung cố định: `0xAA 0x55` |
| `2 - 5` | `uint32_t` | `seq_num` | Số thứ tự gói tin tăng dần liên tục |
| `6 - 9` | `uint32_t` | `timestamp_us` | Thời gian vi mô từ bộ đếm DWT ($\mu s$) |
| `10 - 21` | `float[3]` | `accel[3]` | Gia tốc 3 trục $A_x, A_y, A_z$ ($g$) |
| `22 - 33` | `float[3]` | `gyro[3]` | Vận tốc góc 3 trục $G_x, G_y, G_z$ ($^\circ/s$) |
| `34 - 37` | `float` | `roll` | Góc nghiêng trái/phải sau lọc Adaptive Kalman ($^\circ$) |
| `38 - 41` | `float` | `pitch` | Góc chúi/ngửa sau lọc Adaptive Kalman ($^\circ$) |
| `42` | `uint8_t` | `crash_state` | `0`: Bình thường, `2`: Ngã xe, `3`: Va chạm mạnh, `99`: Lỗi IMU |
| `43 - 44` | `uint16_t` | `crc16` | Mã kiểm tra **CRC16-CCITT** (`Poly = 0x1021, Init = 0xFFFF`) |

### 6.2. Cấu trúc Băng Ghi 15 Giây Tai Nạn trên `W25Q128` (`Sector 1: 0x001000`)
Mỗi vụ tai nạn lưu **16 bản ghi liên tiếp** (mỗi bản ghi căn lề `64 Bytes` để không vượt ranh giới Page `256 Bytes` của Flash):
- **Bản ghi `0 .. 9` (`rel_sec = -10s .. -1s`):** 10 giây diễn biến **TRƯỚC** khi xảy ra tai nạn (Trích xuất từ bộ đệm vòng trong RAM).
- **Bản ghi `10` (`rel_sec = 0s`):** Khoảnh khắc **VA CHẠM / ĐỔ XE** (`💥 DAM!`).
- **Bản ghi `11 .. 15` (`rel_sec = +1s .. +5s`):** 5 giây diễn biến **SAU** khi xảy ra tai nạn (`⚠️ NGA`).

---

## 📱 7. Hướng Dẫn Điều Khiển Qua Bluetooth BLE & Serial Monitor

Kết nối điện thoại vào Bluetooth LE tên **`HOP_DEN_XE_MAY`** (hoặc gõ trực tiếp vào ô `Message` trên **Serial Monitor `115200 baud`** của máy tính):

| Lệnh điều khiển | Chức năng thực hiện | Ví dụ / Kết quả phản hồi |
| :--- | :--- | :--- |
| **`SDT:0889602028`** | Thay đổi số điện thoại nhận tin nhắn cứu hộ SOS và lưu vĩnh viễn vào bộ nhớ Flash NVS | `✅ ĐÃ CẬP NHẬT SĐT SOS MỚI: 0889602028` |
| **`ZERO`** | Lấy tư thế đặt hộp đen hiện tại làm mốc cân bằng chuẩn `Roll = 0.00°, Pitch = 0.00°` | `✅ Đã đặt lại góc hiện tại làm mốc 0.00°!` |
| **`INFO`** | Xem nhanh thông số góc nghiêng, trạng thái GPS và SĐT SOS hiện tại | Hiển thị bảng tóm tắt trạng thái xe |
| **`LOG`** | Đọc và phục dựng **Băng ghi Hộp Đen 15 giây (`T-10s` $\rightarrow$ `T+5s`)** từ chip nhớ `W25Q128` | Xuất bảng phục dựng 16 mốc thời gian tai nạn |
| **`CLEAR`** | Xóa trắng lịch sử tai nạn trong chip nhớ `W25Q128` | `✅ [W25Q128] Đã xóa trắng lịch sử hộp đen!` |
| **`TEST_SOS`** *(hoặc `test sos`)* | Kích hoạt giả lập tai nạn ngay lập tức: Bắn tin nhắn SMS 4G + Lưu 15 giây vào `W25Q128` | Bắn SMS kèm Link Google Maps về SĐT đã cài |

---

## 📊 8. Kết Quả Thực Nghiệm Hệ Thống (Experimental Logs)

### 8.1. Log giám sát thời gian thực (STM32 100Hz + GPS u-blox M10 Fix 8 vệ tinh)
```text
[# 53230] Roll: -4.05° | Pitch: -4.77° | BINH THUONG || [GPS] Fix:1 Sat: 8 Lat:10.88206 Lon:106.76079 ( 0.4km/h) || SDT:0889602028
[# 53250] Roll: -4.07° | Pitch: -4.78° | BINH THUONG || [GPS] Fix:1 Sat: 8 Lat:10.88206 Lon:106.76079 ( 0.4km/h) || SDT:0889602028
```

### 8.2. Nội dung tin nhắn SMS cứu hộ gửi về điện thoại qua `A7682S-LANS` (Mobifone 4G)
```text
[HOP DEN SOS] NGA XE BAT DONG! | Luc: 2.4G | Goc Roll=68.5, Pitch=-12.1 | Toc do: 0.0km/h | Map: https://maps.google.com/?q=10.882060,106.760790
```

### 8.3. Bảng phục dựng hiện trường 15 giây trích xuất từ chip nhớ `W25Q128` (Lệnh `LOG`)
```text
=== PHUC DUNG 15 GIAY TAI NAN (W25Q128) ===
[T-10s] Roll:  0.2° Pitch: -0.1° |  1.0G | GPS:10.88206,106.76079 (28.5km/h) | OK
[T -9s] Roll:  1.1° Pitch: -0.3° |  1.0G | GPS:10.88206,106.76079 (30.2km/h) | OK
...
[T -1s] Roll: 34.0° Pitch: -8.5° |  1.9G | GPS:10.88206,106.76079 (18.4km/h) | OK
[T +0s] Roll: 68.5° Pitch:-12.1° |  4.6G | GPS:10.88206,106.76079 ( 0.0km/h) | 💥 DAM!
[T +1s] Roll: 69.2° Pitch:-12.3° |  1.1G | GPS:10.88206,106.76079 ( 0.0km/h) | ⚠️ NGA
...
[T +5s] Roll: 69.2° Pitch:-12.3° |  1.0G | GPS:10.88206,106.76079 ( 0.0km/h) | ⚠️ NGA
===========================================
```
