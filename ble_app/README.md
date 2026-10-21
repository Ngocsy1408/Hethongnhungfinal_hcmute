# 📱 App Bluetooth Hộp Đen Xe Máy

Web app (PWA) kết nối ESP32-S3 qua **Web Bluetooth**, thay cho việc gõ lệnh bằng Serial Bluetooth Terminal / nRF Connect.

| Tính năng | Lệnh firmware |
| :--- | :--- |
| Đồng hồ góc nghiêng Roll/Pitch, tốc độ GPS, số vệ tinh, link Google Maps, trạng thái xe — tự cập nhật mỗi 2 giây | `INFO` |
| Đổi số SOS (kiểm tra 9–15 chữ số trước khi gửi) | `SDT:<số>` |
| Đọc băng hộp đen 15 giây: biểu đồ Roll + lực G, bảng T−10s → T+5s, xuất CSV | `LOG` |
| Xóa hộp đen (bấm 2 lần để xác nhận) | `CLEAR` |
| Thử SOS (bấm 2 lần — **gửi SMS/gọi thật**) | `TEST_SOS` |
| Banner đỏ + rung + thông báo khi xe báo tai nạn; tự tải `LOG` khi ghi xong | (ESP32 tự đẩy) |
| Tự kết nối lại khi mất sóng; terminal gõ lệnh thô | — |

## Trình duyệt hỗ trợ

- ✅ **Chrome / Edge trên Android, Windows, macOS, Linux**
- ✅ iPhone/iPad: dùng app **Bluefy** (Safari không hỗ trợ Web Bluetooth)
- ❌ Safari, Firefox

Web Bluetooth chỉ chạy trên **HTTPS** hoặc `localhost`.

## Chạy

**Trên máy tính** (thử nhanh):

```bash
cd ble_app
python3 -m http.server 8000
# mở http://localhost:8000 bằng Chrome
```

**Trên điện thoại** cần HTTPS — cách đơn giản nhất là GitHub Pages:
Settings → Pages → Deploy from branch `main`, thư mục `/ (root)` → mở
`https://<user>.github.io/Hethongnhungfinal_hcmute/ble_app/` bằng Chrome Android → menu ⋮ → **Thêm vào màn hình chính** để dùng như app.

## Yêu cầu firmware

App cần bản `Esp32_board/ble_app_config.h` mới (đã sửa trong repo):

1. **`BLE_SendToPhone` chia gói 20 byte.** MTU mặc định của BLE chỉ chở 20 byte/notify, bản cũ gửi nguyên chuỗi nên `INFO`/`LOG` bị cắt cụt. Có thêm mutex để task Blackbox và callback BLE không gửi chen ngang nhau.
2. **`INFO` thêm 2 dòng** `Vi tri: lat,lon` và `Trang thai: <crash_state>` (app vẫn đọc được firmware cũ, chỉ thiếu bản đồ/trạng thái).

Các app terminal cũ (Serial Bluetooth Terminal, nRF Connect) vẫn dùng bình thường.

## Cấu trúc

```
ble_app/
├── index.html            # Giao diện
├── css/app.css           # Theme sáng/tối theo hệ thống
├── js/ble.js             # Kết nối Nordic UART Service, hàng đợi ghi tuần tự
├── js/parser.js          # Ghép gói 20 byte → dòng → sự kiện (status/log/…)
├── js/chart.js           # Biểu đồ SVG + tooltip
├── js/app.js             # Điều phối giao diện
├── sw.js, manifest.webmanifest, icon.svg   # PWA, mở được khi không có mạng
└── tests/parser.test.mjs # npm test
```

## Test

```bash
cd ble_app && npm test   # Node 18+, không cần cài thư viện
```
