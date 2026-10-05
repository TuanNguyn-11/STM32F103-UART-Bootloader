# 🔧 Custom Bootloader trên STM32F103C8T6 qua UART

> **Đồ án môn Hệ Thống Nhúng — Nhóm 13**

Xây dựng chương trình **Bootloader tùy chỉnh** cho vi điều khiển **STM32F103C8T6** (Blue Pill), cho phép **nạp firmware mới từ máy tính qua giao tiếp UART** mà không cần sử dụng mạch nạp ST-Link.

---

## 📑 Mục lục

- [Tổng quan](#-tổng-quan)
- [Tính năng](#-tính-năng)
- [Kiến trúc hệ thống](#-kiến-trúc-hệ-thống)
- [Phần cứng sử dụng](#-phần-cứng-sử-dụng)
- [Cấu trúc thư mục](#-cấu-trúc-thư-mục)
- [Bố trí bộ nhớ Flash](#-bố-trí-bộ-nhớ-flash)
- [Giao thức truyền Firmware](#-giao-thức-truyền-firmware)
- [Hướng dẫn sử dụng](#-hướng-dẫn-sử-dụng)
- [Video Demo](#-video-demo)
- [Thành viên nhóm](#-thành-viên-nhóm)
- [Tài liệu tham khảo](#-tài-liệu-tham-khảo)

---

## 📌 Tổng quan

**Bootloader** là một chương trình nhỏ nằm ở vùng đầu bộ nhớ Flash của vi điều khiển, được chạy đầu tiên khi MCU khởi động. Nó có nhiệm vụ kiểm tra và nạp firmware ứng dụng (Application) vào vùng nhớ Flash, sau đó chuyển quyền điều khiển sang ứng dụng đó.

Trong đồ án này, nhóm xây dựng một **Custom Bootloader** giao tiếp qua **UART** kết hợp với **tool Python trên PC**, giúp nạp firmware mới mà **không cần mạch nạp chuyên dụng**.

### Quy trình hoạt động:

```
Khởi động MCU → Bootloader chạy → Chờ firmware từ PC qua UART
                                    ├── Nhận được firmware → Xóa App cũ → Ghi App mới → Kiểm tra Checksum → Nhảy vào App
                                    └── Không nhận được (timeout) → Kiểm tra App cũ hợp lệ → Nhảy vào App cũ
```

---

## ✨ Tính năng

| Tính năng | Mô tả |
|---|---|
| 🔄 Nạp firmware qua UART | Truyền file `.bin` từ PC qua cổng COM (USB-to-TTL) |
| ✅ Checksum Verification | Xác minh tính toàn vẹn dữ liệu bằng thuật toán checksum |
| 🛡️ Fallback App cũ | Tự động quay về firmware cũ nếu quá trình nạp thất bại |
| 🖥️ Tool Python GUI | Giao diện đồ họa (Tkinter) để chọn file firmware và cổng COM |
| ⚡ Chunk-based Transfer | Truyền firmware theo từng chunk 256 bytes, đảm bảo ổn định |
| 🔐 Flash Protection | Ghi/xóa Flash tuân thủ quy trình Unlock/Lock an toàn |

---

## 🏗️ Kiến trúc hệ thống

### Sơ đồ khối

```
┌──────────┐       USB        ┌─────────────┐      UART (TX/RX)     ┌──────────────────┐
│          │ ◄──────────────► │  USB-to-TTL  │ ◄──────────────────► │  STM32F103C8T6   │
│    PC    │                  │   (CH340G)   │                      │                  │
│          │                  │              │                      │  ┌────────────┐  │
│  Python  │                  └──────────────┘                      │  │ Bootloader │  │
│   Tool   │                                                        │  ├────────────┤  │
│          │                                                        │  │    App     │  │
└──────────┘                                                        │  └────────────┘  │
                                                                    └──────────────────┘
```

### Kết nối phần cứng

| USB-to-TTL (CH340G) | STM32F103C8T6 |
|:---:|:---:|
| TX | PA10 (RX) |
| RX | PA9 (TX) |
| 3V3 | 3V3 |
| GND | GND |

---

## 🔩 Phần cứng sử dụng

| Linh kiện | Mô tả |
|---|---|
| **STM32F103C8T6** (Blue Pill) | Vi điều khiển ARM Cortex-M3, Flash 64KB, SRAM 20KB |
| **Module USB-to-TTL CH340G** | Chuyển đổi USB ↔ UART (TTL 3.3V) |
| **LED** | Kết nối tại chân PA0 — chỉ thị trạng thái Bootloader |
| **Dây nối Dupont** | Kết nối giữa các module |

---

## 📂 Cấu trúc thư mục

```
NHOM13_BOOTLOADER/
├── BootLoader/
│   ├── BL/                         # Project STM32CubeMX - Chương trình Bootloader
│   │   ├── Core/
│   │   │   ├── Inc/                # Header files
│   │   │   └── Src/
│   │   │       └── main.c          # Hàm main - gọi Bootloader_UART()
│   │   ├── Drivers/                # HAL Drivers (STM32)
│   │   ├── MDK-ARM/                # Project Keil µVision
│   │   └── BL.ioc                  # File cấu hình STM32CubeMX
│   │
│   ├── Blink_LED/                  # Project STM32CubeMX - App mẫu (Blink LED)
│   │   ├── Core/
│   │   ├── Drivers/
│   │   ├── MDK-ARM/
│   │   └── Blink_LED.ioc
│   │
│   ├── File BIN/                   # File firmware đã biên dịch
│   │   ├── Blink_LED.bin           # File .bin để nạp qua Bootloader
│   │   └── Blink_LED.hex           # File .hex
│   │
│   └── Library/                    # Thư viện Bootloader tự xây dựng
│       ├── BootLoader.c            # Source code Bootloader (ghi/xóa Flash, checksum, nhảy App)
│       ├── BootLoader.h            # Header file
│       └── Interface_Send_FW.py    # Tool Python nạp firmware từ PC
│
├── Schematic/
│   ├── SoDoKhoi.png                # Sơ đồ khối hệ thống
│   └── schematic.png               # Sơ đồ nguyên lý mạch
│
├── Video/                          # Video demo
│   ├── 1. Build chương trình Bootloader.mp4
│   ├── 2. Nạp chương trình Bootloader.mp4
│   ├── 3. Tool python nạp firmware mới.mp4
│   └── Link Youtube.txt
│
├── HTN_Bootloader.pdf              # Báo cáo đồ án (PDF)
└── Lienhe.txt                      # Thông tin liên hệ nhóm
```

---

## 💾 Bố trí bộ nhớ Flash

STM32F103C8T6 có **64KB Flash** (từ `0x08000000` đến `0x0800FFFF`), được chia thành **64 page** (mỗi page 1KB).

```
┌───────────────────────────────────────────┐ 0x08000000
│                                           │
│             BOOTLOADER                    │
│          (~50KB - 50 pages)               │
│                                           │
│   - Vector Table (Bootloader)             │
│   - Hàm main(), Bootloader_UART()         │
│   - Flash_Erase(), Flash_Write()          │
│   - Checksum, Is_App_Valid()              │
│   - Enter_To_App()                        │
│                                           │
├───────────────────────────────────────────┤ 0x0800C800
│                                           │
│           APPLICATION (App)               │
│          (~14KB - 14 pages)               │
│                                           │
│   - Vector Table (App)                    │
│   - Chương trình ứng dụng                 │
│     (VD: Blink LED)                       │
│                                           │
└───────────────────────────────────────────┘ 0x0800FFFF
```

| Vùng nhớ | Địa chỉ bắt đầu | Kích thước |
|---|---|---|
| Bootloader | `0x08000000` | ~50 KB |
| Application | `0x0800C800` | ~14 KB |

---

## 📡 Giao thức truyền Firmware

### Cấu trúc gói tin

```
┌─────────────────────────────────────────────────────────────┐
│                        HEADER (8 bytes)                     │
├─────────────────────────────┬───────────────────────────────┤
│   Firmware Length (4 bytes) │    Checksum (4 bytes)         │
│        Little-Endian        │      Little-Endian            │
├─────────────────────────────┴───────────────────────────────┤
│                      FIRMWARE DATA                          │
├─────────────────────────────────────────────────────────────┤
│  Chunk 0 (256 bytes) │ Chunk 1 (256 bytes) │ ... │ Chunk N │
└─────────────────────────────────────────────────────────────┘
```

### Luồng giao tiếp UART

```
       PC (Python)                          MCU (Bootloader)
           │                                       │
           │         ◄── "BL_READY\r\n" ──────────│  ① MCU sẵn sàng
           │                                       │
           │── Header (8 bytes: Len + Checksum) ──►│  ② Gửi header
           │                                       │
           │── Chunk 0 (256 bytes) ───────────────►│  ③ Gửi firmware
           │── Chunk 1 (256 bytes) ───────────────►│     theo chunks
           │── ...                                 │
           │── Chunk N (≤ 256 bytes) ─────────────►│
           │                                       │
           │         ◄── "FW_OK\r\n" ─────────────│  ④ Checksum OK
           │         hoặc "CRC_FAIL\r\n" ─────────│     → Nhảy vào App
           │         hoặc "FW_SIZE_ERR\r\n" ───────│     → Lỗi kích thước
           │         hoặc "RX_ERR\r\n" ────────────│     → Lỗi nhận dữ liệu
```

### Thông số UART

| Thông số | Giá trị |
|---|---|
| Baudrate | 115200 |
| Data bits | 8 |
| Parity | None |
| Stop bits | 1 |
| Timeout | 5000 ms |

---

## 🚀 Hướng dẫn sử dụng

### Yêu cầu

- **Phần mềm:**
  - [STM32CubeMX](https://www.st.com/en/development-tools/stm32cubemx.html) — Cấu hình ngoại vi MCU
  - [Keil µVision 5](https://www.keil.com/) — Biên dịch và debug chương trình
  - [STM32 Flash Loader Demonstrator](https://www.st.com/en/development-tools/flasher-stm32.html) — Nạp Bootloader lần đầu
  - [Python 3.x](https://www.python.org/) — Chạy tool nạp firmware
- **Phần cứng:**
  - Board STM32F103C8T6 (Blue Pill)
  - Module USB-to-TTL (CH340G / CP2102 / FT232)

### Bước 1: Nạp Bootloader vào MCU (lần đầu tiên)

1. Kết nối board STM32 với máy tính qua **USB-to-TTL**
2. Chuyển jumper **BOOT0 = 1** trên board (chế độ System Memory)
3. Mở **STM32 Flash Loader Demonstrator**
4. Chọn đúng cổng COM → **Next**
5. Chọn file `BL.hex` (trong thư mục `BootLoader/BL/MDK-ARM/`) → **Nạp**
6. Sau khi nạp xong, chuyển **BOOT0 = 0** → **Reset board**

### Bước 2: Cài đặt thư viện Python

```bash
pip install pyserial
```

### Bước 3: Nạp firmware ứng dụng mới qua Tool Python

1. Kết nối board STM32 với PC qua **USB-to-TTL**
2. **Reset board** để Bootloader khởi chạy
3. Chạy tool Python:

```bash
python Interface_Send_FW.py
```

4. Trong giao diện GUI:
   - **Chọn file firmware** `.bin` cần nạp (VD: `Blink_LED.bin`)
   - **Nhập cổng COM** (VD: `COM5`)
5. Chờ phản hồi từ MCU:
   - ✅ `FW_OK` — Nạp thành công, MCU tự động chạy App mới
   - ❌ `CRC_FAIL` — Checksum không khớp, cần nạp lại
   - ❌ `RX_ERR` — Lỗi nhận dữ liệu
   - ❌ `FW_SIZE_ERR` — Kích thước firmware không hợp lệ

### Bước 4: Build firmware ứng dụng riêng (tùy chọn)

Nếu muốn tạo App riêng để nạp qua Bootloader:

1. Tạo project mới trong **STM32CubeMX** cho STM32F103C8T6
2. Trong file `.ioc`, thay đổi địa chỉ Flash:
   - **Flash Origin** = `0x0800C800`
   - **Flash Length** = `0x3800` (14 KB)
3. Trong code, thêm dòng sau vào đầu hàm `main()`:
   ```c
   SCB->VTOR = 0x0800C800; // Đặt lại Vector Table cho App
   ```
4. Build project → Lấy file `.bin` để nạp qua Bootloader

---

## 🎬 Video Demo

Xem video demo đầy đủ tại playlist YouTube:

🔗 **[Playlist YouTube — Nhóm 13 Bootloader](https://www.youtube.com/playlist?list=PLvvRVMs_Fpy4zuxtDvjuQdRlxRjHrFLW4)**

| Video | Nội dung |
|---|---|
| Video 1 | Build chương trình Bootloader bằng Keil µVision |
| Video 2 | Nạp chương trình Bootloader vào MCU bằng Flash Demonstrator |
| Video 3 | Sử dụng Tool Python để nạp firmware mới qua UART |

---

## 👥 Thành viên nhóm

| Họ và tên | MSSV | Công việc |
|---|---|---|
| **Phan Ngọc Tuấn Nguyên** | 23119178 | Tổng hợp lý thuyết Bootloader, kiến trúc bộ nhớ Flash STM32F103C8T6, cấu trúc gói tin |
| **Phạm Trọng An Nam** | 23119174 | Xây dựng hàm ghi/xóa Flash, Checksum, `App_Is_Valid`, `Enter_To_App` |
| **Nguyễn Trọng Nhân** | 23119182 | Xây dựng tool Python nạp Firmware, quay video demo |
| **Hà Quang Minh** | 23119169 | Xây dựng chương trình Bootloader, nạp Bootloader bằng Flash Demonstrator, nạp Firmware qua tool Python |

---

## 📚 Tài liệu tham khảo

- [STM32F103C8T6 Reference Manual (RM0008)](https://www.st.com/resource/en/reference_manual/rm0008-stm32f101xx-stm32f102xx-stm32f103xx-stm32f105xx-and-stm32f107xx-advanced-armbased-32bit-mcus-stmicroelectronics.pdf)
- [STM32F103C8T6 Datasheet](https://www.st.com/resource/en/datasheet/stm32f103c8.pdf)
- [AN2606 — STM32 System Memory Boot Mode](https://www.st.com/resource/en/application_note/an2606-stm32-microcontroller-system-memory-boot-mode-stmicroelectronics.pdf)
- [STM32 HAL Library Documentation](https://www.st.com/en/embedded-software/stm32cubef1.html)

---

## 📄 Giấy phép

Đồ án phục vụ mục đích học tập tại trường Đại học. Mã nguồn được chia sẻ cho mục đích tham khảo và nghiên cứu.

---

<p align="center">
  <b>Nhóm 13 — Môn Hệ Thống Nhúng</b><br>
  <i>⭐ Nếu project hữu ích, hãy cho một Star!</i>
</p>
