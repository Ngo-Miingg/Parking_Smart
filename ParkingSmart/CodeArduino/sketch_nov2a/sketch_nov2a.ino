/*
 * FILE: ESP32_CAM_FINAL_NO_DELAY.ino
 * UPDATE: 
 * 1. Cố định Static IP (172.20.10.x)
 * 2. Fix lỗi chụp chậm (Xả buffer ảnh cũ)
 * 3. Tối ưu cấu hình Camera cho AI
 */

#include "esp_camera.h"
#include <WiFi.h>
#include <WebServer.h>

const char WIFI_SSID[] = "trung keke";     // Tên wifi giữ nguyên
const char WIFI_PASS[] = "88888888"; // Mật khẩu giữ nguyên

// --- CHỌN IP CHO CAMERA ---
// 1. Gateway: Khả năng cao là đuôi .1
// Bạn cần kiểm tra lại dòng "Default Gateway" xem có đúng là 172.31.106.1 không nhé
IPAddress gateway(172, 31, 106, 1);    

// 2. Local IP: 3 số đầu (172, 31, 106) LẤY THEO SỐ BẠN GỬI. 
// Số cuối tôi chọn là 40 (hoặc số khác tùy bạn, miễn khác 251 và 1)
IPAddress local_IP(172, 31, 106, 40); 

// 3. Subnet: Bạn cần xem dòng "Subnet Mask"
// Nếu máy tính hiện 255.255.255.0 thì điền như dưới:
IPAddress subnet(255, 255, 255, 0);

#define SERVER_PORT 80
#define FLASH_PIN   4  // Đèn Flash (GPIO 4)

// =================================================================
// 2. PIN DEFINITION (AI-THINKER MODEL)
// =================================================================
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27
#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

WebServer server(SERVER_PORT);

// =================================================================
// 3. HÀM XỬ LÝ CHỤP ẢNH (FIX DELAY)
// =================================================================
void handleCapture() {
  // --- BƯỚC 1: XẢ ẢNH CŨ (Ghost Frame) ---
  // Lấy ảnh đang tồn đọng trong buffer ra và hủy ngay lập tức
  camera_fb_t * fb = esp_camera_fb_get();
  if(fb){
    esp_camera_fb_return(fb); 
  }
  
  // --- BƯỚC 2: CHỤP ẢNH MỚI (Real-time) ---
  // Bật Flash nếu cần (Cẩn thận sụt nguồn nếu dùng cáp USB dỏm)
  // digitalWrite(FLASH_PIN, HIGH);
  // delay(50); 

  fb = esp_camera_fb_get(); // Lấy ảnh thực tế lúc này
  
  // Tắt Flash ngay
  // digitalWrite(FLASH_PIN, LOW);

  if (!fb) {
    server.send(500, "text/plain", "Camera Capture Failed");
    return;
  }

  // --- BƯỚC 3: GỬI VỀ SERVER PYTHON ---
  WiFiClient client = server.client();
  
  // Gửi Header HTTP thủ công để kiểm soát luồng
  String response = "HTTP/1.1 200 OK\r\n";
  response += "Content-Type: image/jpeg\r\n";
  response += "Content-Length: " + String(fb->len) + "\r\n";
  response += "Access-Control-Allow-Origin: *\r\n"; 
  response += "\r\n";
  
  server.sendContent(response);
  
  // Gửi dữ liệu ảnh (Binary)
  client.write(fb->buf, fb->len);

  // Giải phóng bộ nhớ
  esp_camera_fb_return(fb);
}

// =================================================================
// 4. SETUP
// =================================================================
void setup() {
  Serial.begin(115200);
  pinMode(FLASH_PIN, OUTPUT);
  digitalWrite(FLASH_PIN, LOW);

  Serial.println("\n🚀 ESP32-CAM STARTING...");

  // 1. Cấu hình Camera
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer   = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
  
  // Cấu hình tối ưu cho AI Server (Nhẹ & Nét)
  config.frame_size = FRAMESIZE_VGA; // 640x480
  config.jpeg_quality = 10;          // 10-12 là đẹp
  config.fb_count = 2;               // Dùng 2 buffer để mượt hơn
  config.grab_mode = CAMERA_GRAB_LATEST; // Luôn lấy ảnh mới nhất

  // Init Camera
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("❌ Camera Init Failed 0x%x\n", err);
    delay(3000);
    ESP.restart();
  }

  // 2. Cấu hình IP Tĩnh (Bắt buộc làm trước khi connect Wifi)
  if (!WiFi.config(local_IP, gateway, subnet)) {
    Serial.println("⚠️ Static IP Failed to configure");
  }

  // 3. Kết nối Wifi
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.print("🔗 Connecting WiFi");
  
  int retries = 0;
  while (WiFi.status() != WL_CONNECTED && retries < 40) {
    delay(500);
    Serial.print(".");
    retries++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n✅ WiFi Connected!");
    Serial.print("🌐 Link: http://");
    Serial.print(WiFi.localIP());
    Serial.println("/capture");
  } else {
    Serial.println("\n❌ WiFi Failed. Restarting...");
    delay(2000);
    ESP.restart();
  }

  // 4. Start Server
  server.on("/capture", HTTP_GET, handleCapture);
  server.begin();
  Serial.println("✅ HTTP Server Started");
}

void loop() {
  server.handleClient();
  
  // Tự động kết nối lại nếu rớt mạng
  if(WiFi.status() != WL_CONNECTED) {
     Serial.println("⚠️ WiFi lost, reconnecting...");
     WiFi.disconnect();
     WiFi.reconnect();
     delay(1000);
  }
}