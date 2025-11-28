<div align="center">

# 🚗 HỆ THỐNG QUẢN LÝ BÃI ĐỖ XE THÔNG MINH <br> VỚI NHẬN DIỆN BIỂN SỐ (AI & IoT)

[![Python](https://img.shields.io/badge/Python-3.7%2B-blue?logo=python&logoColor=white)](https://www.python.org/)
[![Flask](https://img.shields.io/badge/Flask-Web%20Server-lightgrey?logo=flask)](https://flask.palletsprojects.com/)
[![EasyOCR](https://img.shields.io/badge/AI-EasyOCR-yellow)](https://github.com/JaidedAI/EasyOCR)
[![Hardware](https://img.shields.io/badge/Hardware-ESP32%20%7C%20Arduino-red?logo=arduino)](https://www.arduino.cc/)

<p align="center">
  <img src="ParkingSmart/LogoDaiNam.png" alt="DaiNam University Logo" width="180"/>
  &nbsp;&nbsp;&nbsp;&nbsp;&nbsp;
  <img src="ParkingSmart/LogoIoT.png" alt="AIoTLab Logo" width="150"/>
</p>

**Đồ án môn học - Khoa Công nghệ Thông tin - Đại học Đại Nam**

</div>

---

## 📖 Giới Thiệu
**Smart Parking System** là giải pháp tự động hóa quy trình quản lý bãi đỗ xe sử dụng công nghệ **AI (Nhận diện biển số)** kết hợp với **IoT (Cảm biến & Vi điều khiển)**.

Hệ thống giúp giám sát xe ra vào theo thời gian thực, tự động mở barie khi nhận diện đúng biển số hoặc thẻ từ, và cung cấp giao diện Web Admin trực quan để quản lý doanh thu và lịch sử. Dữ liệu được lưu trữ an toàn và truy xuất nhanh chóng qua **SQLite**.

---

## 🌟 Tính Năng Nổi Bật

| Tính Năng | Mô Tả |
| :--- | :--- |
| 📷 **Nhận diện AI** | Tự động đọc biển số xe với độ chính xác cao sử dụng **EasyOCR** & **YOLO**. |
| 🅿️ **Quản lý Slot** | Giám sát trạng thái **6 vị trí đỗ xe** (Trống/Có xe) qua cảm biến hồng ngoại. |
| 🌐 **Web Dashboard** | Giao diện hiện đại, Responsive, hiển thị Camera live và thông số môi trường. |
| 📊 **Thống kê & Báo cáo** | Lưu trữ lịch sử ra/vào, tính toán phí gửi xe và báo cáo doanh thu. |
| 🚧 **Điều khiển tự động** | Servo Barie tự động đóng mở dựa trên kết quả xử lý từ Server. |

---

## 🛠️ Kiến Trúc Hệ Thống

### 1. Sơ Đồ Kết Nối Phần Cứng
<p align="center">
  <img src="ParkingSmart/SoDoKetNoi.png" width="800" alt="Sơ đồ mạch"/>
</p>

### 2. Giao Diện Quản Lý (Web Admin)
<p align="center">
  <img src="ParkingSmart/GiaoDienQuanLy.png" width="800" alt="Giao diện Web"/>
</p>

---

## ⚙️ Yêu Cầu Kỹ Thuật

### 🖥️ Phần Mềm (Software)
* **Ngôn ngữ:** Python 3.7+
* **Web Framework:** Flask, Flask-SocketIO
* **AI/Xử lý ảnh:** EasyOCR, OpenCV, PyTorch, NumPy
* **Cơ sở dữ liệu:** SQLite

### 🔌 Phần Cứng (Hardware)
* **Vi xử lý:** ESP32-WROOM (Master), Arduino Uno R3 (Slave), ESP32-CAM (Camera).
* **Cảm biến:** Cảm biến hồng ngoại (IR Sensor), Đầu đọc thẻ RFID RC522.
* **Cơ cấu chấp hành:** Servo SG90 (Barie), Màn hình LCD 1602 (I2C).
* **Nguồn:** Adapter 5V-4A (Bắt buộc).

---

## 🚀 Hướng Dẫn Cài Đặt & Sử Dụng

### Bước 1: Cài đặt Môi trường Python
Đảm bảo máy tính đã cài đặt Python. Sau đó cài các thư viện dependency:

```bash
# Clone dự án (nếu có) hoặc tải source code về
# Cài đặt thư viện
pip install Flask EasyOCR opencv-python numpy Pillow torch torchvision
```

### Bước 2: Nạp Code Phần Cứng (Firmware)

Sử dụng **Arduino IDE** để nạp code cho các bo mạch:

1.  **Arduino Uno:** Nạp file `UNO_R3_Slave.ino` (Quản lý cảm biến & Servo).
2.  **ESP32 Main:** Nạp file `ESP32_Master.ino` (Kết nối Wifi, RFID, LCD & giao tiếp Server).
3.  **ESP32-CAM:** Nạp file `ESP32_CAM.ino` (Lưu ý sửa IP tĩnh trong code trùng với dải mạng của bạn).

### Bước 3: Cấu hình Logic (Mapping)

Hệ thống sử dụng logic giao tiếp I2C và điều khiển Servo như sau:

| Chân | Chức năng | Trạng thái Logic |
| :--- | :--- | :--- |
| **D2, D3** | Cảm biến Cổng Vào/Ra | `LOW`: Có xe - `HIGH`: Trống |
| **D4 - D7** | Cảm biến Slot 1-4 | `LOW`: Có xe - `HIGH`: Trống |
| **D9, D10** | Servo Barie | `0°`: Mở - `90°`: Đóng |

### Bước 4: Khởi Chạy

1.  Kết nối toàn bộ phần cứng (Lưu ý nguồn điện).
2.  Chạy Server Python:
    ```bash
    python app.py
    ```
3.  Truy cập Web: `http://localhost:5000` hoặc `http://<IP_MAY_TINH>:5000` (trên điện thoại).

-----

## ⚠️ LƯU Ý QUAN TRỌNG (TROUBLESHOOTING)

> [!WARNING]
> **VẤN ĐỀ NGUỒN ĐIỆN (POWER SUPPLY)**
>
>   * Dự án sử dụng nhiều linh kiện tiêu thụ dòng lớn (ESP32 Wifi, Servo, Camera).
>   * **KHÔNG** cắm tất cả vào nguồn USB Laptop, sẽ gây sụt áp dẫn đến treo ESP32 hoặc Servo không quay.
>   * **BẮT BUỘC:** Cấp nguồn ngoài tối thiểu **5V - 4A** vào các đường ray nguồn (Power Rails) của Breadboard.
>   * Nhớ nối chung chân **GND** của nguồn ngoài, ESP32 và Arduino lại với nhau.

> [!TIP]
> **ESP32-CAM:** Để giảm độ trễ (delay), module Camera cần được cấu hình **IP Tĩnh** và nạp code tối ưu bộ đệm (Frame Buffer). Không cắm Camera qua Breadboard lung lay, hãy dùng dây cái-cái cắm trực tiếp.

-----

## 📂 Tài Nguyên Dự Án

  * **Video Demo & Poster:** [Xem tại Google Drive](https://drive.google.com/drive/folders/1gjgWLPGixKoOhLTOEyunc6heffVYvRjw?usp=sharing)
-----

## 👨‍💻 Tác Giả & Bản Quyền

<div align="center">

**© 2025 Nhóm 2 - Lớp CNTT_17-01** **Khoa Công nghệ Thông tin - Đại học Đại Nam**

Thực hiện bởi: **Ngô Văn Minh** và các thành viên nhóm.  
📧 Email: mt0u0tm@gmail.com

</div>

---
