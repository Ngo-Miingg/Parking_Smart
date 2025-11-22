<h1 align="center">🚗 HỆ THỐNG QUẢN LÝ BÃI ĐỖ XE VỚI NHẬN DIỆN BIỂN SỐ </h1>

<div align="center">

<p align="center">
</p>
<p align="center">
<img src="ParkingSmart\LogoDaiNam.png" alt="DaiNam University Logo" width="200"/>
<img src="ParkingSmart\LogoIoT.png" alt="AIoTLab Logo" width="170"/>
</p>

</div>


<p align="left">
  Hệ thống quản lý bãi đỗ xe tích hợp nhận diện biển số xe tự động, với giao diện web hiện đại để quản lý bãi đỗ một cách dễ dàng và hiệu quả. Dữ liệu điểm danh được lưu trữ trong lite SQL 
</p>

---
## 🌟 Sơ Đồ Kết Nối Mạch
</p>
<p align="center">
<img src="ParkingSmart\SoDoKetNoi.jpg" width="700"/>
</p>


---

## 🌟 WEB QUẢN LÝ (Minh họa)
</p>
<p align="center">
<img src="ParkingSmart\GiaoDienQuanLyMinhHoa.jpg" width="700"/>

</p>


---
## 🌟 Tính Năng
✅ Nhận diện biển số xe tự động với EasyOCR  
✅ Quản lý **6 vị trí đỗ xe**  
✅ Giao diện web **hiện đại & dễ sử dụng**  
✅ **Lưu trữ lịch sử** đỗ xe  
✅ Thống kê số lượng xe đang đỗ và chỗ trống  
✅ Hiển thị **thời gian vào/ra** của xe  

---

## 📌 Yêu Cầu Hệ Thống
### 🖥️ Phần Mềm
- **Python 3.7+**
- Các thư viện Python cần thiết:
  ```bash
  pip install Flask EasyOCR OpenCV-Python NumPy Pillow
  Flask==2.3.3
  Pillow==10.2.0
  numpy==1.26.4
  opencv-python==4.9.0.80
  torch==2.2.0
  torchvision==0.17.0
  easyocr==1.7.1 
  ```

---

## 🚀 Hướng Dẫn Cài Đặt & Chạy Hệ Thống

### 1️⃣ Cài Đặt Môi Trường
1. Đảm bảo bạn đã cài đặt **Python 3.7+**. Nếu chưa, hãy tải về từ [Python.org](https://www.python.org/).
2. Cài đặt các thư viện cần thiết bằng lệnh:
   ```bash
   pip install Flask EasyOCR OpenCV-Python NumPy Pillow
   ```

---

### 2️⃣ Thư viện và khai báo phần cứng
```python
#include <Servo.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

LiquidCrystal_I2C lcd(0x27, 16, 2);

```
👉 Dùng 3 thư viện:

Servo.h – điều khiển servo barie

LiquidCrystal_I2C – điều khiển màn LCD 16x2 qua I2C

Wire.h – giao tiếp I2C

---

### 3️⃣ Khai báo chân cảm biến
```python
#define SENSOR_ENTRY 2
#define SENSOR_EXIT  3
#define SLOT1 4
#define SLOT2 5
#define SLOT3 6
#define SLOT4 7
#define SERVO_ENTRY_PIN 9
#define SERVO_EXIT_PIN 10

```
```bash
| Chân | Chức năng                   |
| ---- | --------------------------- |
| 2    | Cảm biến vào (entry sensor) |
| 3    | Cảm biến ra (exit sensor)   |
| 4–7  | Cảm biến chỗ đỗ S1–S4       |
| 9    | Servo cổng vào              |
| 10   | Servo cổng ra               |

```

---

### 4️⃣ Servo điều khiển barie
```python
void openGate(Servo &gate) {
  gate.write(0); // mở
}
void closeGate(Servo &gate) {
  gate.write(90); // đóng
}

```
```bash
Mở: quay 0°

Đóng: quay 90°

Có delay để gate thực sự chuyển động

In LCD để báo trạng thái

Chú ý: servo vào và ra sử dụng cùng một hàm, truyền bằng tham chiếu Servo &gate.
```

---

### 5️⃣ Đọc trạng thái slot + cập nhật LCD
```python
slotState[i] == LOW ? "X" : "O";
```
Low = có xe -> X
Hight = trống xe -> O
```python
S1:X S2:O
S3:O S4:X
```

---

### 6️⃣ Hàm đếm số bãi trống
```python
if (slotState[i] == HIGH) freeCount++;
```
Hight = trống -> thêm xe vào


---
---

### 7️⃣ Gửi log ra Serial Monitor
```python
Serial.print("[S1:X] ");
```
In đầy đủ trạng thái của 4 slot và số chỗ trống
```python
Slots: [S1:X] [S2:O] [S3:O] [S4:X] | Free: 2

```
```
### 8️⃣  Setup() 
```pythonpinMode(..., INPUT_PULLUP);
```
Các cảm biến đều được bật PULLUP → bình thường HIGH → kích hoạt LOW.
```python
servoEntry.attach(9);
servoExit.attach(10);
closeGate(servoEntry);
closeGate(servoExit);

```
→ Barie vào & ra đều đóng khi khởi động.
```python
lcd.init();
lcd.backlight();
updateLCD();

```
Hiển thị trạng thái slot ngay từ đầu.

---

## ⚠️ Lưu Ý
- **ESP32-CAM không cắm như sơ đồ mạch** mà **cắm thẳng vào laptop** và lưu ý bắt buộc phải **cấp thêm nguồn điện tối thiểu 5V-4A thì dự án mới có thể hoạt động tốt** nếu **dòng điện yếu thì dự án sẽ hoạt động bị gián đoạn và bị treo**.

---

## 🎯 Mục Tiêu
- **Tự động hóa quy trình quản lý bãi đỗ xe** bằng công nghệ nhận diện biển số xe.
- **Nâng cao hiệu suất và giảm thiểu lỗi thủ công** trong quản lý xe ra vào.
- **Dễ dàng tích hợp với hệ thống IoT** để giám sát từ xa.
---
## Video thực hành 
https://drive.google.com/file/d/14Q-VhjpNjrfpyCArisUyTweeGcRDRu_1/view?usp=sharing
---
## Poster dự án
https://drive.google.com/file/d/1P5lU7P8-RpwqRjRRhmVsK77MVMZaAN1q/view?usp=sharing
---
## 📝 Bản quyền

© 2025 Ngô Văn Minh-Nhóm 2-CNTT_17-01, Khoa Công nghệ Thông tin, Đại học Đại Nam. Mọi quyền được bảo lưu.
<div align="center">
Được thực hiện bởi 💻 Nhóm 2-CNTT_17-01 tại Đại học Đại Nam

Email cá nhân : mt0u0tm@gmail.com
