# Hethongnhungfinal_hcmute
Đồ án môn học Hệ Thống Nhúng Trường Đại học Công Nghệ Kỹ Thuật TP HCM
# 🏍️ Motorcycle Blackbox - IMU Sensor Fusion & Telemetry Node (STM32 + ICM-42688)

![MCU](https://img.shields.io/badge/MCU-STM32F401CCU6-blue.svg)
![Sensor](https://img.shields.io/badge/IMU-TDK_ICM42688-orange.svg)
![Interface](https://img.shields.io/badge/Interface-SPI_%7C_UART_DMA-green.svg)
![Algorithm](https://img.shields.io/badge/Filter-Adaptive_Kalman-red.svg)

Module xử lý quán tính thời gian thực (Real-time Inertial Measurement Unit) dành cho hệ thống **Hộp đen xe máy (Motorcycle Blackbox)**. Hệ thống sử dụng vi điều khiển **STM32F401CCU6** kết hợp cùng cảm biến 6 trục cao cấp **ICM-42688** để lọc nhiễu động cơ, ước lượng góc nghiêng chính xác trong điều kiện động học phức tạp, phát hiện tai nạn/ngã xe và đóng gói dữ liệu nhị phân đẩy sang **ESP32** qua cơ chế **UART DMA**.

---

## 🌟 Điểm nổi bật về kỹ thuật (Key Engineering Features)

Môi trường xe máy gây ra hai thách thức lớn cho cảm biến IMU: **nhiễu rung tần số cao từ động cơ đốt trong** và **gia tốc ngoại lai (lực ly tâm khi ôm cua, phanh gấp)**. Dự án này giải quyết triệt để các vấn đề trên thông qua 4 tầng xử lý:

1. **Bộ lọc phần cứng kép (Hardware Anti-Aliasing & DLPF):**
   - Khai thác trực tiếp bộ lọc **Anti-Aliasing Filter (AAF)** ở tần số cắt `~42Hz` và **UI DLPF bậc 2** bên trong thanh ghi Bank 1 & Bank 2 của ICM-42688.
   - Triệt tiêu hoàn toàn hài bậc cao của rung động cơ (`60Hz - 250Hz`) ngay từ tầng ADC cảm biến trước khi đưa vào vi điều khiển.
2. **Thuật toán Kalman Thích ứng (Adaptive Kalman Filter - AKF):**
   - Khắc phục nhược điểm bị đánh lừa bởi lực ly tâm khi xe nghiêng vào cua của bộ lọc Kalman thông thường.
   - Tự động điều chỉnh ma trận hiệp phương sai nhiễu đo lường ($R_{adaptive}$) dựa trên độ lệch của độ lớn vector gia tốc tổng ($\|\mathbf{a}\| - 1.0g$).
3. **Định thời vi mô bằng phần cứng (Hardware DWT Microsecond Timer):**
   - Sử dụng bộ đếm chu kỳ xung nhịp nhân ARM Cortex-M4 (`DWT->CYCCNT` @ 84MHz) thay vì `HAL_GetTick()`, giúp phép tính tích phân vận tốc góc $\Delta t$ đạt độ chính xác tới từng micro-giây ($\mu s$).
4. **Truyền thông Zero-Copy UART DMA & Bảo toàn dữ liệu (CRC16-CCITT):**
   - Đóng gói dữ liệu theo cấu trúc nhị phân (`#pragma pack(1)`) dài **41 bytes/frame** ở tốc độ **100Hz**.
   - Kiểm tra lỗi đường truyền bằng thuật toán **CRC16-CCITT (0x1021)**, chống nhiễu điện từ (EMI) từ hệ thống đánh lửa bugi xe máy.
   - Sử dụng `DMA2_Stream7` kết hợp cơ chế *Atomic Double-Buffer Snapshot* giúp CPU hoàn toàn không bị trễ (0% CPU blocking overhead).

---

## 📂 Cấu trúc thư mục (Project Structure)

```text
Core/
├── Inc/
│   ├── icm42688_defs.h       # Định nghĩa bản đồ thanh ghi (Bank 0, 1, 2) & hằng số vật lý ICM-42688
│   ├── icm42688.h            # API giao tiếp SPI và cấu trúc dữ liệu IMU
│   ├── dwt_stm32.h           # Thư viện lấy mẫu thời gian thực độ phân giải Microsecond (DWT)
│   ├── adaptive_kalman.h     # Khai báo cấu trúc và hàm xử lý Adaptive Kalman Filter
│   └── telemetry_protocol.h  # Định nghĩa gói tin Binary Frame, FSM trạng thái tai nạn & hàm CRC16
│
└── Src/
    ├── icm42688.c            # Driver SPI Mode 3, cấu hình bộ lọc phần cứng AAF/DLPF & Burst Read 14-byte
    ├── adaptive_kalman.c     # Thực thi thuật toán lọc Kalman thích ứng theo gia tốc động
    ├── stm32f4xx_it.c        # Trình phục vụ ngắt phần cứng (DMA2_Stream7 & USART1 IRQ)
    └── main.c                # Khởi tạo ngoại vi, vòng lặp điều khiển 100Hz, FSM phát hiện tai nạn & DMA TX
```

---

## 🔌 Sơ đồ đấu nối phần cứng (Hardware Pinout)

Hệ thống được thiết kế cho board **STM32F401CCU6 (BlackPill)** chạy ở xung nhịp tối đa **84MHz** (HSE 25MHz):

| Thiết bị | Chân Thiết bị | Chân STM32F401 | Giao thức / Chức năng | Ghi chú |
| :--- | :---: | :---: | :--- | :--- |
| **ICM-42688** | `VCC / VDDIO` | `3.3V` | Power Supply | **Tuyệt đối không cấp 5V** |
| | `GND` | `GND` | Common Ground | Mass chung |
| | `NCS (CS)` | `PA4` | `GPIO_Output` | Software Chip Select (Active Low) |
| | `SCLK (SCK)` | `PA5` | `SPI1_SCK` (AF5) | SPI Mode 3 (~10.5 MHz) |
| | `SDO (MISO)` | `PA6` | `SPI1_MISO` (AF5) | Dữ liệu từ IMU -> STM32 |
| | `SDI (MOSI)` | `PA7` | `SPI1_MOSI` (AF5) | Lệnh cấu hình từ STM32 -> IMU |
| **ESP32** | `RX` | `PA9` | `USART1_TX` (AF7) | Truyền gói tin Telemetry (DMA) |
| | `TX` | `PA10` | `USART1_RX` (AF7) | Nhận phản hồi từ ESP32 |
| | `GND` | `GND` | Common Ground | Đồng bộ mức áp logic |
| **Onboard LED**| `LED` | `PC13` | `GPIO_Output` | Nháy nhanh nếu lỗi kết nối SPI IMU |

---

## 📦 Cấu trúc khung truyền dữ liệu (Telemetry Packet Frame)

Gói tin nhị phân được truyền xuống ESP32 qua UART ở tốc độ `115200 bps` (hoặc `921600 bps`) với chu kỳ `10ms (100Hz)`. Tổng kích thước cố định: **41 Bytes** (Little-Endian).

| Byte Offset | Kiểu dữ liệu | Tên trường | Mô tả |
| :---: | :---: | :--- | :--- |
| `0` | `uint8_t` | `magic_1` | Byte đồng bộ đầu khung 1 (`0xAA`) |
| `1` | `uint8_t` | `magic_2` | Byte đồng bộ đầu khung 2 (`0x55`) |
| `2 - 5` | `uint32_t` | `seq_num` | Số thứ tự gói tin (Dùng để phát hiện mất gói bên ESP32) |
| `6 - 9` | `uint32_t` | `timestamp_us`| Thời gian lấy mẫu tính bằng micro-giây ($\mu s$) |
| `10 - 21` | `float[3]` | `accel[3]` | Gia tốc 3 trục $A_x, A_y, A_z$ (Đơn vị: $g$, dải đo $\pm 16g$) |
| `22 - 33` | `float[3]` | `gyro[3]` | Vận tốc góc 3 trục $G_x, G_y, G_z$ (Đơn vị: $^\circ/s$, dải đo $\pm 2000^\circ/s$) |
| `34 - 37` | `float` | `roll` | Góc nghiêng trái/phải sau lọc Adaptive Kalman (Đơn vị: Độ) |
| `38 - 41` | `float` | `pitch` | Góc chúi/ngửa xe sau lọc Adaptive Kalman (Đơn vị: Độ) |
| `42` | `uint8_t` | `crash_state` | Trạng thái xe: `0`: Bình thường, `1`: Phanh gấp, `2`: Đổ xe, `3`: Va chạm mạnh |
| `43 - 44` | `uint16_t` | `crc16` | Mã kiểm tra toàn vẹn dữ liệu **CRC16-CCITT** (Tính từ byte `0` đến `42`) |

*(Tổng kích thước struct `TelemetryPacket_t`: **45 bytes**)*

---

## 🧠 Máy trạng thái nhận diện sự cố (Crash Detection FSM)

Hệ thống phân biệt rõ trạng thái **nghiêng xe ôm cua (Cornering)** và **ngã xe thực sự (Fallen)** bằng cách kết hợp góc nghiêng, độ lớn vector gia tốc và vận tốc góc:
- **`CRASH_STATE_COLLISION (0x03)`:** Xung gia tốc tổng $\|\mathbf{a}\| > 4.5g$ (Va chạm mạnh trực diện).
- **`CRASH_STATE_FALLEN (0x02)`:** Góc $|\text{Roll}| > 55^\circ$ hoặc $|\text{Pitch}| > 60^\circ$ **ĐỒNG THỜI** vận tốc góc tổng $\|\boldsymbol{\omega}\| < 15^\circ/s$ duy trì liên tục trên `1.5 giây` (Xe đã đổ và nằm bất động).
- **`CRASH_STATE_NORMAL (0x00)`:** Trạng thái vận hành bình thường.

---

## 🚀 Hướng dẫn Build & Nạp chương trình

1. Mở dự án bằng **STM32CubeIDE** hoặc **Keil MDK-ARM**.
2. Đảm bảo đã bật cờ hỗ trợ tập lệnh dấu phẩy động phần cứng (**FPU - Floating Point Unit**) trên Cortex-M4 (`-mfpu=fpv4-sp-d16 -mfloat-abi=hard`).
3. Biên dịch (Build) và nạp chương trình qua mạch nạp **ST-Link V2**.
4. Nếu đèn LED `PC13` tắt (không nhấp nháy liên tục), cảm biến ICM-42688 đã vượt qua bước kiểm tra `WHO_AM_I (0x47)` và bắt đầu bơm dữ liệu DMA sang ESP32.
