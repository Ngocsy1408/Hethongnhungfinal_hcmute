# 🏍️ ĐỒ ÁN HỆ THỐNG NHÚNG: HỘP ĐEN XE MÁY THÔNG MINH & CỨU HỘ TỰ ĐỘNG 4G
### Smart Motorcycle Blackbox, Real-Time Adaptive Kalman Filter & Automated 4G SOS System (HCMUTE)

![STM32F401](https://img.shields.io/badge/Co--Processor-STM32F401CCU6_(Cortex--M4F)-03234B?style=for-the-badge&logo=stmicroelectronics)
![ESP32-S3](https://img.shields.io/badge/Main--MCU-ESP32--S3_N16R8_(Dual--Core)-E7352C?style=for-the-badge&logo=espressif)
![ICM42688](https://img.shields.io/badge/IMU_6--Axis-TDK_ICM--42688--P-FF6F00?style=for-the-badge)
![GNSS](https://img.shields.io/badge/GNSS_4--System-u--blox_M10_(115200_bps)-0052CC?style=for-the-badge)
![4G_LTE](https://img.shields.io/badge/4G_LTE-SIMCom_A7682S--LANS-00C853?style=for-the-badge)
![Flash](https://img.shields.io/badge/Storage-Winbond_W25Q128_(16MB)-9C27B0?style=for-the-badge)
![BLE](https://img.shields.io/badge/Wireless-Bluetooth_BLE_5.0-2196F3?style=for-the-badge&logo=bluetooth)

---

## 📌 MỤC LỤC BÁO CÁO & VẤN ĐÁP
1. [Kiến trúc tổng thể & Sơ đồ đấu nối phần cứng](#-phần-1-kiến-trúc-tổng-thể--sơ-đồ-đấu-nối-phần-cứng)
2. [Giải thích chi tiết nhiệm vụ từng File trong dự án](#-phần-2-giải-thích-chi-tiết-nhiệm-vụ-từng-file-trong-dự-án)
3. [Lý thuyết File `.h` và File `.c` — Tại sao phải chia tách module?](#-phần-3-file-h-là-gì-file-c-là-gì-tại-sao-phải-chia-ra-như-vậy)
4. [Phân biệt Function, Macro và `static inline` trong Source Code](#-phần-4-function-hàm-và-macro-là-gì-chỉ-rõ-trong-code-dự-án)
5. [Bảng tổng hợp các Thanh ghi & Giải mã chi tiết từng Bit phần cứng](#-phần-5-dự-án-sử-dụng-những-thanh-ghi-nào-giải-mã-chi-tiết-từng-bit)
6. [Bộ 10 Câu hỏi & Đáp án Vấn đáp Chuyên sâu Vi điều khiển](#-phần-6-bộ-câu-hỏi--đáp-án-vấn-đáp-chuyên-sâu-về-vi-điều-khiển)
7. [Tập lệnh điều khiển & Kết quả thực nghiệm tại HCMUTE](#-phần-7-tập-lệnh-điều-khiển--kết-quả-thực-nghiệm-tại-hcmute)

---

## 🏗️ PHẦN 1: KIẾN TRÚC TỔNG THỂ & SƠ ĐỒ ĐẤU NỐI PHẦN CỨNG

### 1.1. Sơ đồ khối phân tầng Dual-MCU (STM32F401CCU6 + ESP32-S3 N16R8)

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
                        UART1 RX (115200 bps, 100 Frames/s)
                        [STM32 PA9 (TX) ──> ESP32 GPIO 17 (U1RXD)]
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
│  │   (Cắt bỏ gói UBX nhị phân đầu dòng)  │  │   Google Maps qua SIM 4G A7682S     │  │
│  └───────────────────────────────────────┘  └─────────────────────────────────────┘  │
└───────────┬─────────────────────────────┬────────────────────────────┬───────────────┘
            │ SPI2 (10, 11, 12, 13)       │ UART2 RX (GPIO 18)         │ UART1 TX (GPIO 5)
            ▼                             ▼                            ▼
  [ NOR Flash W25Q128 (16MB) ]    [ GPS u-blox M10 ]         [ SIM 4G A7682S-LANS ]
  • Tự động đánh thức (0xAB)      • 115200 bps, 4 hệ vệ tinh  • 115200 bps (Mobifone 4G)
  • Lưu vĩnh viễn băng 15 giây    • GPS + Galileo + BeiDou    • Bắn SMS Link Google Maps
```

---

### 1.2. Bảng Đấu Nối Chân Phần Cứng (Pinout Mapping)

#### A. Phía `STM32F401CCU6 (BlackPill)`
| Thiết bị ngoại vi | Chân Thiết bị | Chân STM32F401 | Giao thức / Chức năng | Ghi chú |
| :--- | :---: | :---: | :--- | :--- |
| **ICM-42688** | `VCC / VDDIO` | **`3.3V`** | Nguồn nuôi `3.3V` | **Tuyệt đối không cấp 5V** |
| | `GND` | **`GND`** | Mass chung | |
| | `NCS (CS)` | **`PA4`** | `GPIO_Output` | Software Chip Select |
| | `SCLK (SCK)` | **`PA5`** | `SPI1_SCK` (AF5) | SPI Mode 3 (`10.5MHz`) |
| | `SDO (MISO)` | **`PA6`** | `SPI1_MISO` (AF5) | Dữ liệu từ IMU $\rightarrow$ STM32 |
| | `SDI (MOSI)` | **`PA7`** | `SPI1_MOSI` (AF5) | Lệnh cấu hình từ STM32 $\rightarrow$ IMU |
| **ESP32-S3** | `GPIO 17 (RX1)`| **`PA9`** | `USART1_TX` (AF7) | Truyền gói 45-byte (`115200 bps`) |
| | `GPIO 16` | **`PA10`** | `USART1_RX` (AF7) | |
| | `3V3 / GND` | **`3V3 / GND`**| Nguồn & Mass chung | Đồng bộ mức áp logic |
| **LED Trạng thái**| `Onboard LED` | **`PC13`** | `GPIO_Output` | Chớp nhịp tim `1s/lần` |

#### B. Phía `ESP32-S3 N16R8`
| Thiết bị ngoại vi | Chân Thiết bị | Chân ESP32-S3 N16R8 | Bộ ngoại vi | Ghi chú kỹ thuật |
| :--- | :---: | :---: | :--- | :--- |
| **1. STM32F401** | `PA9 (TX)` | **`GPIO 17`** | `UART1_RX` | Nhận luồng 100Hz (`115200 bps`) |
| **2. SIM A7682S-LANS**| `RXD` | **`GPIO 5` (hoặc `4`)**| `UART1_TX` | Phát lệnh `AT+CMGS` (`115200 bps`) |
| | `VCC / GND` | **`5V / GND`** | Power Supply | Nguồn dòng đỉnh $\ge 2\text{A}$ |
| **3. GPS u-blox M10**| `TX` | **`GPIO 18`** | `UART2_RX` | Nhận chuỗi NMEA (`115200 bps`) |
| | `RX` | **`GPIO 8`** | `UART2_TX` | Gửi lệnh kích hoạt `ubxEnableNMEA` |
| | `VCC / GND` | **`5V / GND`** | Power Supply | Mặt gốm ăng-ten hướng lên trời |
| **4. Flash W25Q128** | `VCC / GND` | **`3.3V / GND`** | Power Supply | **Chỉ cấp 3.3V** |
| | `CS` | **`GPIO 10`** | `SPI2_CS` | Chip Select |
| | `DI (MOSI)` | **`GPIO 11`** | `SPI2_MOSI` | Tự động đảo chiều `11 <-> 13` |
| | `CLK (SCK)` | **`GPIO 12`** | `SPI2_SCK` | Xung nhịp SPI |
| | `DO (MISO)` | **`GPIO 13`** | `SPI2_MISO` | Đọc dữ liệu từ Flash |

---

## 📂 PHẦN 2: GIẢI THÍCH CHI TIẾT NHIỆM VỤ TỪNG FILE TRONG DỰ ÁN

### 2.1. Khối Firmware trên `STM32F401CCU6` (9 Files)
1. **`stm32f4xx_hal.h`:** Tệp định nghĩa địa chỉ vật lý của toàn bộ các thanh ghi ngoại vi STM32F401 (`RCC`, `FLASH`, `GPIOA`, `GPIOC`, `SPI1`, `USART1`, `DMA2`, `SysTick`, `CoreDebug`, `DWT`) và các hàm `static inline` thao tác bit trực tiếp.
2. **`dwt_stm32.h`:** Kích hoạt và đọc bộ đếm chu kỳ xung nhịp phần cứng `DWT->CYCCNT` chạy ở `84MHz`, cung cấp hàm `DWT_GetMicroseconds()` có độ phân giải đúng `1 µs`.
3. **`icm42688_defs.h`:** Chứa bản đồ địa chỉ các thanh ghi thuộc `Bank 0`, `Bank 1`, `Bank 2` của cảm biến `ICM-42688` và các hệ số quy đổi vật lý (`ACCEL_SCALE_16G = 1/2048`, `GYRO_SCALE_2000DPS = 1/16.4`).
4. **`icm42688.h` & `icm42688.c`:** Bộ Driver điều khiển cảm biến qua `SPI1 Mode 3`. Thực hiện kiểm tra mã định danh `WHO_AM_I (0x47)`, bật chế độ `Low-Noise Mode`, kích hoạt bộ lọc chống chồng phổ phần cứng **`AAF 42Hz`** + **`UI DLPF bậc 2`**, và đọc đồng bộ 14 thanh ghi (`Burst Read`).
5. **`adaptive_kalman.h` & `adaptive_kalman.c`:** Thực thi thuật toán lọc **Adaptive Kalman Filter** 2 trạng thái (Góc nghiêng `angle` và Độ trôi `bias` của Gyro). Tự động tính toán sai số độ lớn gia tốc $|\|\mathbf{a}\| - 1.0g|$ để điều chỉnh động ma trận hiệp phương sai nhiễu đo lường $R_{adaptive}$, triệt tiêu sai số do lực ly tâm khi xe vào cua.
6. **`telemetry_protocol.h`:** Định nghĩa cấu trúc khung truyền nhị phân `TelemetryPacket_t` (`45 bytes`, đóng gói sát bằng `#pragma pack(push, 1)`) và hàm tính mã kiểm tra lỗi đường truyền **CRC16-CCITT (`0x1021`)**.
7. **`main.c`:** Bật khối tính toán dấu phẩy động `FPU` trước `main()`, thiết lập `PLL 84MHz`, điều phối vòng lặp thời gian thực `100Hz` (`10,000 µs`), nhận diện sự cố và truyền khung dữ liệu 45-byte sang ESP32-S3.

### 2.2. Khối Firmware trên `ESP32-S3 N16R8` (6 Files)
1. **`config_types.h`:** Định nghĩa sơ đồ chân GPIO, cấu trúc gói tin `TelemetryPacket_t` (đồng bộ 100% với STM32), cấu trúc bản ghi hộp đen 15 giây `BlackboxLogRecord_t` và cấu trúc `GPSData_t`.
2. **`w25q128_flash.h`:** Driver SPI điều khiển chip nhớ NOR Flash `W25Q128 (16MB)` — tự động đánh thức chip (`0xAB`), kiểm tra mã `JEDEC ID (0xEF 0x40 0x18)`, tự động đảo chân `DI/DO` nếu cắm ngược, xóa Sector `4KB` (`0x20`), ghi trang (`0x02`) và phục dựng bảng 16 mốc thời gian tai nạn (`T-10s` đến `T+5s`).
3. **`gps_m10_parser.h`:** Bộ giải mã chuỗi NMEA (`$GNRMC`, `$GNGGA`, `$GNGLL`) tốc độ `115200 bps` từ module `u-blox M10`. Tự động lọc bỏ các gói nhị phân `UBX` dính ở đầu câu bằng cơ chế bắt ký tự mốc `'$'`, quy đổi tọa độ `ddmm.mmmm` sang độ thập phân Google Maps `dd.dddddd` và vận tốc sang `km/h`.
4. **`sim_a7680c.h`:** Điều khiển module 4G `SIMCom A7682S-LANS`, tự động phát hiện chân `RXD` của SIM thông qua mức áp tĩnh và phát lệnh `AT+CMGS` gửi tin nhắn SMS cứu hộ kèm Link Google Maps.
5. **`ble_app_config.h`:** Khởi tạo Server Bluetooth `BLE 5.0` (Nordic UART Service) tên **`HOP_DEN_XE_MAY`**, chia nhỏ gói thông báo `20-byte/chunk`, dùng `Mutex` và `Worker Task` riêng để xử lý các lệnh `SDT:`, `ZERO`, `INFO`, `LOG`, `CLEAR`, `TEST_SOS` an toàn tuyệt đối.
6. **`sketch_oct21a.ino` (`ESP32_Blackbox.ino`):** Chương trình chính điều phối đa nhiệm 2 nhân CPU: **Core 1 (`loop`)** chuyên đọc luồng `100Hz` từ STM32, tự động lấy gốc `0.00°` (*Auto-Zero*) và đọc GPS M10; **Core 0 (`Task_Blackbox_Manager`)** quản lý bộ đệm vòng 10 giây trước tai nạn, thu 5 giây sau tai nạn, ghi `W25Q128` và kích hoạt `SIM 4G`.

---

## 🧩 PHẦN 3: FILE `.h` LÀ GÌ, FILE `.c` LÀ GÌ? TẠI SAO PHẢI CHIA RA NHƯ VẬY?

### 3.1. Phân biệt bản chất File `.h` (Header) và File `.c` / `.cpp` (Source)
* **File `.h` (Header File - "Bản thiết kế / Giao diện"):**
  - Chứa phần **Khai báo (Declarations)**: Khai báo kiểu dữ liệu cấu trúc (`typedef struct`, `enum`), các hằng số (`#define`), biến chia sẻ (`extern`), và **nguyên mẫu hàm (Function Prototypes — chỉ có tên hàm, kiểu dữ liệu đầu vào/đầu ra chứ chưa có ruột xử lý)** để thông báo cho các file khác biết module này có những hàm gì.
* **File `.c` / `.cpp` (Source File - "Phần thực thi thuật toán"):**
  - Chứa phần **Định nghĩa chi tiết (Definitions / Implementations)**: Nơi viết toàn bộ các câu lệnh tính toán bên trong hàm. Mỗi file `.c` được trình biên dịch (`arm-none-eabi-gcc`) dịch độc lập thành một file đối tượng (`.o` - Object file), sau đó trình liên kết (**Linker**) mới ghép các file `.o` lại thành file thực thi `.elf`.

### 3.2. Bốn lý do bắt buộc phải chia tách `.h` và `.c` trong Hệ thống nhúng
1. **Tính đóng gói & Trừu tượng hóa (Encapsulation):** File `main.c` chỉ cần `#include "icm42688.h"` và gọi `ICM42688_ReadData()`, không cần biết bên trong `icm42688.c` phải chuyển Bank thanh ghi phức tạp ra sao. Các hàm nội bộ như `static void WriteRegister()` được giấu kín bằng từ khóa `static`.
2. **Dễ dàng gỡ lỗi & Tái sử dụng (Reusability):** Khi cần mang thuật toán lọc nhiễu sang dự án khác, chỉ cần copy cặp file `adaptive_kalman.h` và `adaptive_kalman.c` mà không phải bóc tách từ `main.c`.
3. **Tăng tốc độ biên dịch (Incremental Build):** Khi sửa 1 dòng trong `main.c`, trình biên dịch chỉ dịch lại `main.c` (mất `0.3 giây`) thay vì phải dịch lại toàn bộ dự án.
4. **Chống lỗi khai báo trùng lặp nhờ `Include Guards` (`#ifndef ... #define ... #endif`):** Đảm bảo mỗi cấu trúc `struct` chỉ được nạp vào trình biên dịch đúng 1 lần duy nhất.

---

## ⚙️ PHẦN 4: FUNCTION (HÀM) VÀ MACRO LÀ GÌ? CHỈ RÕ TRONG CODE DỰ ÁN

### 4.1. Bảng so sánh lý thuyết giữa MACRO và FUNCTION

| Tiêu chí | **MACRO (`#define`)** | **FUNCTION (Hàm C/C++)** |
| :--- | :--- | :--- |
| **Cơ chế hoạt động** | Được xử lý bởi **Bộ tiền xử lý (Preprocessor)** trước khi biên dịch. Chỉ đơn thuần là **tìm và thay thế chuỗi văn bản (Text Substitution)** tại mọi vị trí gọi Macro. | Được biên dịch thành một khối lệnh mã máy lưu tại một **địa chỉ cố định trong bộ nhớ Flash**. Khi gọi hàm, CPU phải lưu ngữ cảnh vào `Stack` và nhảy (`BL`) tới địa chỉ hàm. |
| **Kiểm tra kiểu dữ liệu**| **Không kiểm tra kiểu dữ liệu** (Type-unsafe). | **Kiểm tra kiểu dữ liệu chặt chẽ** (Type-safe), dễ đặt Breakpoint gỡ lỗi. |
| **Tốc độ thực thi** | **Nhanh tuyệt đối** (không tốn chu kỳ lệnh nhảy hàm `Call/Return`). | Tốn vài chu kỳ máy để đẩy/lấy thanh ghi trên `Stack`, trừ khi được khai báo là **`static inline`**. |

### 4.2. Chỉ rõ đâu là MACRO, đâu là FUNCTION trong chính Code của Dự án

#### 🔹 Các MACRO được sử dụng trong dự án:
1. **Macro hằng số vật lý & cấu hình (Object-like Macro):**
   - `#define RAD_TO_DEG 57.29577951308232f` *(Hệ số đổi từ Radian sang Độ: $180/\pi$)*.
   - `#define ACCEL_SCALE_16G (1.0f / 2048.0f)` *(Hệ số đổi giá trị ADC 16-bit của ICM-42688 sang đơn vị $g$)*.
   - `#define SYNC_BYTE_1 0xAA` và `#define SYNC_BYTE_2 0x55` *(Byte đồng bộ đầu khung truyền)*.
2. **Macro ánh xạ thanh ghi phần cứng (Memory-Mapped Register Macro):**
   - `#define USART1_DR (*(volatile uint32_t *)0x40011004UL)` *(Ép kiểu địa chỉ vật lý `0x40011004` thành con trỏ 32-bit `volatile` và giải tham chiếu để đọc/ghi trực tiếp vào thanh ghi dữ liệu UART1)*.
3. **Macro hàm có tham số (Function-like Macro):**
   - `#define CS_LOW(dev)  HAL_GPIO_WritePin(dev->csPinBank, dev->csPin, GPIO_PIN_RESET)`
   - `#define CS_HIGH(dev) HAL_GPIO_WritePin(dev->csPinBank, dev->csPin, GPIO_PIN_SET)`

#### 🔹 Các FUNCTION (Hàm) được sử dụng trong dự án:
1. **Hàm biên dịch tiêu chuẩn (Standard Function):**
   - `float AKF_Update(AdaptiveKalman_t *kf, float newAngle, float newRate, float dt, float acc_magnitude)` *(Hàm tính toán bộ lọc Kalman thích ứng trong `adaptive_kalman.c`)*.
   - `bool ICM42688_Init(...)` và `bool ICM42688_ReadData(...)` *(Hàm khởi tạo và đọc cảm biến trong `icm42688.c`)*.
2. **Hàm nội tuyến (`static inline` Function — Kết hợp ưu điểm của cả Macro và Function):**
   - `static inline uint32_t DWT_GetMicroseconds(void)` *(trong `dwt_stm32.h`)*.
   - `static inline uint16_t Calculate_CRC16(const uint8_t *data, uint16_t length)` *(trong `telemetry_protocol.h`)*.
   - **Giải thích:** Từ khóa `static inline` yêu cầu trình biên dịch chèn thẳng mã máy của hàm vào vị trí gọi (nhanh như Macro, không tốn lệnh nhảy hàm) nhưng vẫn kiểm tra kiểu dữ liệu chặt chẽ như Function!

---

## 🔬 PHẦN 5: DỰ ÁN SỬ DỤNG NHỮNG THANH GHI NÀO? GIẢI MÃ CHI TIẾT TỪNG BIT

Trên vi điều khiển **`STM32F401CCU6`**, dự án **tự khai báo và điều khiển trực tiếp từng bit thanh ghi (Bare-Metal Register)**, giúp kích thước chương trình sau biên dịch chỉ nặng **`5.16 KB` Flash**:

### 5.1. Thanh ghi cấu hình chế độ chân `GPIOA->MODER` & `GPIOC->MODER`
```c
GPIOA->MODER |= (1UL << 8) | (2UL << 10) | (2UL << 12) | (2UL << 14) | (2UL << 18) | (2UL << 20);
GPIOC->MODER |= (1UL << 26);
```
* **Quy tắc phần cứng:** Thanh ghi `MODER` dài 32-bit chia cho 16 chân (`Pin 0..15`), nên **mỗi chân chiếm 2 bit** tại vị trí **`Bit = Số_chân × 2`**.
  - Giá trị `00` (`0`): Input | `01` (**`1UL`**): Output | `10` (**`2UL`**): Alternate Function (Ngoại vi) | `11` (`3UL`): Analog.
* **Giải mã từng cụm bit:**
  - `(1UL << 8)`: Chân **`PA4`** ($4 \times 2 = \text{Bit } 8$) nhận giá trị `01` (**Output**) làm chân `CS` chọn chip SPI.
  - `(2UL << 10)`: Chân **`PA5`** ($5 \times 2 = \text{Bit } 10$) nhận giá trị `10` (**Alternate Function**) làm chân `SPI1_SCK`.
  - `(2UL << 12)`: Chân **`PA6`** ($6 \times 2 = \text{Bit } 12$) nhận giá trị `10` (**Alternate Function**) làm chân `SPI1_MISO`.
  - `(2UL << 14)`: Chân **`PA7`** ($7 \times 2 = \text{Bit } 14$) nhận giá trị `10` (**Alternate Function**) làm chân `SPI1_MOSI`.
  - `(2UL << 18)`: Chân **`PA9`** ($9 \times 2 = \text{Bit } 18$) nhận giá trị `10` (**Alternate Function**) làm chân `USART1_TX`.
  - `(2UL << 20)`: Chân **`PA10`** ($10 \times 2 = \text{Bit } 20$) nhận giá trị `10` (**Alternate Function**) làm chân `USART1_RX`.
  - `(1UL << 26)`: Chân **`PC13`** ($13 \times 2 = \text{Bit } 26$) nhận giá trị `01` (**Output**) điều khiển đèn LED báo nhịp tim.

### 5.2. Thanh ghi định tuyến kênh ngoại vi `GPIOA->AFR[0]` & `GPIOA->AFR[1]`
```c
GPIOA->AFR[0] |= (5UL << 20) | (5UL << 24) | (5UL << 28);
GPIOA->AFR[1] |= (7UL << 4)  | (7UL << 8);
```
* **Quy tắc phần cứng:** Mỗi chân chiếm **4 bit** (chọn từ `AF0` đến `AF15`). `AFR[0]` quản lý chân `0..7` (`Bit = Số_chân × 4`), `AFR[1]` quản lý chân `8..15` (`Bit = (Số_chân - 8) × 4`).
* **Giải mã từng cụm bit:**
  - `(5UL << 20) | (5UL << 24) | (5UL << 28)`: Gán **`AF5` (`SPI1`)** cho các chân `PA5` (Bit 20), `PA6` (Bit 24), `PA7` (Bit 28).
  - `(7UL << 4) | (7UL << 8)`: Gán **`AF7` (`USART1`)** cho chân `PA9` (Bit 4 của `AFR[1]`) và `PA10` (Bit 8 của `AFR[1]`).

### 5.3. Thanh ghi nhân tần số xung nhịp hệ thống lên `84MHz` (`RCC_PLLCFGR` & `FLASH_ACR`)
```c
FLASH_ACR   = (1UL << 8) | (1UL << 9) | (1UL << 10) | 2UL;
RCC_PLLCFGR = (16UL << 0) | (336UL << 6) | (1UL << 16) | (7UL << 24);
```
* **Trên `FLASH_ACR`:**
  - `2UL` (`Bits 3:0 - LATENCY`): Cài đặt **2 chu kỳ chờ (2 Wait States)** để CPU đọc Flash an toàn ở `84MHz`.
  - `(1UL << 8)` (`PRFTEN`): Bật Prefetch Buffer | `(1UL << 9)` (`ICEN`): Bật Instruction Cache | `(1UL << 10)` (`DCEN`): Bật Data Cache.
* **Trên `RCC_PLLCFGR` (Nguồn vào từ dao động nội `HSI = 16 MHz`):**
  - `(16UL << 0)` (`Bits 5:0 - PLLM`): Bộ chia đầu vào $M = 16 \implies \frac{16\text{ MHz}}{16} = 1\text{ MHz}$.
  - `(336UL << 6)` (`Bits 14:6 - PLLN`): Bộ nhân VCO $N = 336 \implies 1\text{ MHz} \times 336 = 336\text{ MHz}$.
  - `(1UL << 16)` (`Bits 17:16 - PLLP`): Mã nhị phân `01` (số `1UL`) tương ứng hệ số **Chia 4** $\implies \text{SYSCLK} = \frac{336\text{ MHz}}{4} = \mathbf{84\text{ MHz}}$.

### 5.4. Thanh ghi điều khiển `SPI1_CR1` & `SPI1_SR`
```c
SPI1_CR1 = (1UL << 0) | (1UL << 1) | (1UL << 2) | (2UL << 3) | (1UL << 8) | (1UL << 9) | (1UL << 6);
```
* `(1UL << 0)` (`Bit 0 - CPHA = 1`) & `(1UL << 1)` (`Bit 1 - CPOL = 1`): Thiết lập chuẩn **SPI Mode 3** của cảm biến ICM-42688.
* `(1UL << 2)` (`Bit 2 - MSTR = 1`): Chế độ **Master**.
* `(2UL << 3)` (`Bits 5:3 - BR = 010`): Hệ số chia tốc độ = **Chia 8** ($\frac{84\text{ MHz}}{8} = \mathbf{10.5\text{ MHz}}$).
* `(1UL << 8)` (`SSI = 1`) & `(1UL << 9)` (`SSM = 1`): Quản lý chân chọn chip `CS (PA4)` bằng phần mềm.
* `(1UL << 6)` (`Bit 6 - SPE = 1`): Bật ngoại vi `SPI1`.
* **Trong `SPI1_SR`:** `Bit 1 (TXE)` = Đệm phát trống; `Bit 0 (RXNE)` = Đệm thu đầy; `Bit 7 (BSY)` = Bus SPI đang bận dịch bit.

### 5.5. Thanh ghi điều khiển `USART1_BRR`, `USART1_CR1` & `USART1_SR`
```c
USART1_BRR = 729UL;
USART1_CR1 = (1UL << 2) | (1UL << 3) | (1UL << 13);
```
* `USART1_BRR = 729UL`: Tính từ công thức $\text{BRR} = \frac{f_{APB2}}{\text{Baudrate}} = \frac{84,000,000}{115,200} = 729.16 \approx \mathbf{729}$.
* `USART1_CR1`: `Bit 2 (RE)` = Bật bộ thu; `Bit 3 (TE)` = Bật bộ phát; `Bit 13 (UE)` = Bật khối `USART1`.
* `USART1_SR & (1U << 7)`: Kiểm tra `Bit 7 (TXE - Transmit Data Register Empty)` trước khi nạp từng byte vào `USART1_DR`.

### 5.6. Thanh ghi FPU & Bộ đếm vi mô DWT (`SCB_CPACR`, `CoreDebug->DEMCR`, `DWT->CTRL`)
* `SCB_CPACR |= (0xFUL << 20)` (`0xE000ED88`): Ghi `1111` vào `Bits 23:20` để mở toàn quyền truy cập cho bộ đồng xử lý toán học dấu phẩy động **FPU (Coprocessor 10 & 11)**.
* `CoreDebug->DEMCR |= (1UL << 24)` (`0xE000EDFC`): Bật `Bit 24 (TRCENA)` cấp nguồn cho khối Debug/Trace ngay khi khởi động lạnh không có mạch nạp ST-Link.
* `DWT->CTRL |= (1UL << 0)` (`0xE0001000`): Bật `Bit 0 (CYCCNTENA)` để thanh ghi 32-bit `DWT->CYCCNT` đếm từng chu kỳ xung nhịp `84MHz`.

---

## 🎓 PHẦN 6: BỘ CÂU HỎI & ĐÁP ÁN VẤN ĐÁP CHUYÊN SÂU VỀ VI ĐIỀU KHIỂN

### ❓ Câu 1: Tại sao phải dùng từ khóa `volatile` cho các thanh ghi phần cứng và biến chia sẻ đa luồng?
> **Trả lời:** Từ khóa `volatile` ngăn trình biên dịch C (GCC Optimizer) lưu tạm giá trị biến vào thanh ghi nội của CPU, ép CPU luôn phải đọc/ghi trực tiếp tại địa chỉ RAM hoặc thanh ghi vật lý ở mỗi dòng lệnh. Nếu bỏ `volatile` ở `USART1_SR` hoặc biến cờ `sosTriggered`, trình biên dịch sẽ tưởng giá trị đó không đổi trong vòng lặp `while` và tối ưu hóa thành vòng lặp vô tận gây treo hệ thống.

### ❓ Câu 2: Tại sao phải dùng `#pragma pack(push, 1)` cho cấu trúc `TelemetryPacket_t`?
> **Trả lời:** Vi điều khiển 32-bit mặc định căn lề bộ nhớ (*Struct Padding*) theo bội số của 4 bytes. Trong `TelemetryPacket_t`, sau 2 bytes đầu (`magic_1, magic_2`) là biến 4 bytes (`seq_num`). Nếu không có `#pragma pack(push, 1)`, trình biên dịch sẽ tự động chèn thêm 2 bytes trống vào giữa, làm sai lệch vị trí các byte khi truyền nhị phân từ STM32 sang ESP32.

### ❓ Câu 3: Tại sao trên STM32F401 phải bật `FPU` bằng `__attribute__((constructor))` trước hàm `main()`?
> **Trả lời:** Khi biên dịch với cờ `-mfloat-abi=hard`, trình biên dịch sinh lệnh cất thanh ghi số thực (`VPUSH {d8-d15}`) ngay tại dòng mã máy đầu tiên ở đầu hàm `main()` (trước khi dòng lệnh đầu tiên bên trong `main()` kịp chạy). Nếu không bật `SCB_CPACR` trước `main()` thông qua `__attribute__((constructor))`, CPU vừa bước vào `main()` sẽ lập tức bị lỗi phần cứng `UsageFault / HardFault`.

### ❓ Câu 4: Tại sao khi đọc cảm biến ICM-42688 phải dùng `Burst Read 14 bytes` trong 1 lần kéo chân `CS` xuống thấp?
> **Trả lời:** Mỗi trục cảm biến gồm 2 thanh ghi 8-bit (`HIGH_BYTE` và `LOW_BYTE`). Đọc `Burst Read 14 bytes` liên tục giúp phần cứng cảm biến chốt khóa (*Shadow Latch*) toàn bộ 6 trục tại cùng một thời điểm lấy mẫu, tránh hiện tượng đọc nửa byte cao của mẫu cũ ghép với nửa byte thấp của mẫu mới, đồng thời giảm thời gian trễ trên bus SPI.

### ❓ Câu 5: Tại sao trên chip nhớ Flash `W25Q128`, trước khi ghi trang (`Page Program 0x02`) phải xóa Sector (`Sector Erase 0x20`)?
> **Trả lời:** Do đặc tính vật lý của bộ nhớ **NOR Flash**, lệnh ghi (`Page Program`) chỉ có thể kéo các bit từ mức `1` xuống mức `0`, không thể đẩy bit từ `0` lên `1`. Vì vậy bắt buộc phải gọi lệnh `Sector Erase (0x20)` để đưa toàn bộ vùng nhớ 4KB về trạng thái trắng `0xFF` (tất cả các bit đều bằng `1`) trước khi ghi dữ liệu mới.

### ❓ Câu 6: Tại sao trong bộ giải mã GPS (`gps_m10_parser.h`) lại phải bắt sự kiện `if (c == '$') nmeaLine = "$";`?
> **Trả lời:** Module GPS `u-blox M10` ở tốc độ `115200 bps` xuất đồng thời cả khung nhị phân `UBX` dính liền ngay trước đầu bản tin văn bản `$GNRMC` mà không có ký tự xuống dòng `\n`. Việc reset bộ đệm ngay khi gặp ký tự `'$'` giúp cắt bỏ hoàn toàn các byte nhị phân `UBX` phía trước, đảm bảo chuỗi NMEA luôn bắt đầu chuẩn xác bằng `$GNRMC` hoặc `$GNGGA`.

### ❓ Câu 7: Tại sao hệ thống tách riêng 2 bộ UART cứng trên ESP32-S3 (`UART1` cho STM32 RX + SIM TX, `UART2` cho GPS RX/TX)?
> **Trả lời:** Cả STM32 (`115200 bps`) chỉ dùng chiều Nhận (`RX = GPIO 17`) và module SIM `A7682S-LANS` (`115200 bps`) chỉ cần chiều Phát lệnh (`TX = GPIO 5`) khi bắn SMS. Việc ghép STM32 vào kênh `RX` và SIM vào kênh `TX` của cùng bộ `UART1`, đồng thời dành trọn bộ `UART2` cho `GPS M10` giúp cả 3 thiết bị hoạt động song song 24/24 mà không bao giờ phải đóng/mở (`end/begin`) chuyển chân UART lúc đang chạy.

---

## 📱 PHẦN 7: TẬP LỆNH ĐIỀU KHIỂN & KẾT QUẢ THỰC NGHIỆM TẠI HCMUTE

### 7.1. Bảng lệnh điều khiển qua Bluetooth BLE (`HOP_DEN_XE_MAY`) & Serial Monitor
| Cú pháp lệnh | Chức năng thực hiện |
| :--- | :--- |
| **`SDT:0342018857`** | Cập nhật số điện thoại nhận tin nhắn cứu hộ SOS (Lưu vĩnh viễn vào bộ nhớ NVS Flash). |
| **`ZERO`** | Đặt tư thế lắp đặt hiện tại của hộp đen làm mốc cân bằng `Roll = 0.00°, Pitch = 0.00°`. |
| **`INFO`** | Xem nhanh trạng thái góc nghiêng, số vệ tinh GPS, tọa độ và SĐT SOS hiện tại. |
| **`LOG`** | Trích xuất bảng phục dựng **15 giây trước và sau tai nạn (`T-10s` $\rightarrow$ `T+5s`)** từ chip nhớ `W25Q128`. |
| **`CLEAR`** | Xóa trắng lịch sử tai nạn trong chip nhớ `W25Q128`. |
| **`TEST_SOS`** | Kích hoạt giả lập tai nạn ngay lập tức: Bắn SMS 4G kèm Link Google Maps + Ghi 15s vào `W25Q128`. |

### 7.2. Dữ liệu thực nghiệm thu được tại ĐH Sư Phạm Kỹ Thuật TP.HCM (HCMUTE)
* **Bản tin vệ tinh thu thực tế từ `u-blox M10` (Khóa 12 vệ tinh, HDOP = 0.57):**
  ```text
  $GNRMC,041333.80,A,1050.23955,N,10646.65002,E,0.144,,241026,,,A,V*10
  $GNGGA,041333.80,1050.23955,N,10646.65002,E,1,12,0.57,18.3,M,-2.3,M,,*63
  ```
* **Tọa độ quy đổi chuẩn Google Maps:** `10.83733, 106.77750` (`https://maps.google.com/?q=10.837326,106.777500`).
