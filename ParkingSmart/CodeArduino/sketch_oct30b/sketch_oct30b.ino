/*
 * FILE: ESP32_FULL_STABLE.ino
 * FIX: Thêm Timeout HTTP, Watchdog chống treo, Gộp logic
 */

#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <SPI.h>
#include <MFRC522.h>

// ================= CONFIG =================
const char* WIFI_SSID = "trung keke"; 
const char* WIFI_PASS = "88888888";    
const char* SERVER_HOST = "172.31.106.251"; // <--- Đảm bảo IP đúng
const int   SERVER_PORT = 5000;
  
#define UNO_ADDR 0x08
#define MAX_SLOTS 4

// RFID Config (VSPI)
#define SS_PIN  5
#define RST_PIN 4

// ================= OBJECTS =================
MFRC522 rfid(SS_PIN, RST_PIN);
LiquidCrystal_I2C lcd(0x27, 16, 2);

// ================= VARIABLES =================
int slots[4] = {0, 0, 0, 0}; 
int lastSlots[4] = {-1, -1, -1, -1};

bool entryTriggered = false;
bool exitTriggered = false;
unsigned long lastHttpUpdate = 0;
unsigned long lastCommandCheck = 0;

// ================= DISPLAY HELPERS =================
void updateLCDGrid() {
  bool changed = false;
  for(int i=0; i<4; i++) if(slots[i] != lastSlots[i]) changed = true;
  if (!changed) return;

  for(int i=0; i<4; i++) lastSlots[i] = slots[i];

  lcd.setCursor(0, 0);
  lcd.printf("S1:[%c] S2:[%c]", slots[0]?'X':' ', slots[1]?'X':' ');
  lcd.setCursor(0, 1);
  lcd.printf("S3:[%c] S4:[%c]", slots[2]?'X':' ', slots[3]?'X':' ');
}

void lcdMessage(String line1, String line2) {
  lcd.clear();
  lcd.setCursor(0, 0); lcd.print(line1);
  lcd.setCursor(0, 1); lcd.print(line2);
}

// ================= HARDWARE CONTROL =================
void sendToUno(String cmd) {
  Wire.beginTransmission(UNO_ADDR);
  Wire.write((const uint8_t*)cmd.c_str(), cmd.length());
  Wire.endTransmission();
  delay(50); // Delay nhỏ để tránh nghẽn I2C
}

// ================= SERVER COMMUNICATION =================

// Hàm gọi Server chung (Dùng cho cả RFID và Camera)
String callServer(String type, String action, String uid = "") {
  if (WiFi.status() != WL_CONNECTED) return "WIFI_ERR";
  
  HTTPClient http;
  String url = "http://" + String(SERVER_HOST) + ":" + String(SERVER_PORT) + "/api/" + type + "/" + action;
  
  http.begin(url);
  http.addHeader("Content-Type", "application/json");
  http.setConnectTimeout(3000); // Timeout 3s để tránh treo

  String jsonStr = "{}";
  if (type == "rfid") {
    jsonStr = "{\"uid\": \"" + uid + "\"}";
  }

  int httpCode = http.POST(jsonStr);
  String decision = "error";

  if (httpCode > 0) {
    String payload = http.getString();
    StaticJsonDocument<512> doc;
    deserializeJson(doc, payload);
    
    decision = String((const char*)doc["action"]);
    const char* plate = doc["plate"];

    // Nếu Server trả về Payment Due hoặc Allow
    if (plate) {
      lcdMessage(String(plate), decision);
      delay(1500); // Giữ màn hình 1.5s
    }
  } else {
    Serial.printf("[HTTP] Error: %s\n", http.errorToString(httpCode).c_str());
  }
  http.end();
  return decision;
}

// Hàm nhận lệnh Manual Control từ Web
void checkServerCommand() {
  if (WiFi.status() != WL_CONNECTED) return;

  HTTPClient http;
  String url = "http://" + String(SERVER_HOST) + ":" + String(SERVER_PORT) + "/api/get_command";
  http.begin(url);
  http.setConnectTimeout(2000);
  
  int httpCode = http.GET();
  if (httpCode > 0) {
    String payload = http.getString();
    StaticJsonDocument<200> doc;
    deserializeJson(doc, payload);
    String cmd = String((const char*)doc["command"]);

    if (cmd != "none" && cmd != "null") {
      Serial.println("[MANUAL] Cmd: " + cmd);
      
      if (cmd == "OPEN_ENTRY") {
        lcdMessage("ADMIN CONTROL", "OPEN ENTRY...");
        sendToUno("OPEN_ENTRY");
        delay(3000);
        sendToUno("CLOSE_ENTRY");
        lastSlots[0] = -1; 
      } 
      else if (cmd == "OPEN_EXIT") {
        lcdMessage("ADMIN CONTROL", "OPEN EXIT...");
        sendToUno("OPEN_EXIT");
        delay(3000);
        sendToUno("CLOSE_EXIT");
        lastSlots[0] = -1;
      }
    }
  }
  http.end();
}

void sendSensorData() {
  if (WiFi.status() != WL_CONNECTED) return;
  HTTPClient http;
  String url = "http://" + String(SERVER_HOST) + ":" + String(SERVER_PORT) + "/api/update_data";
  http.begin(url);
  http.addHeader("Content-Type", "application/json");
  http.setConnectTimeout(2000);
  
  StaticJsonDocument<200> doc;
  doc["s1"] = slots[0]; doc["s2"] = slots[1];
  doc["s3"] = slots[2]; doc["s4"] = slots[3];
  
  String jsonStr;
  serializeJson(doc, jsonStr);
  http.POST(jsonStr);
  http.end();
}

// ================= MAIN TASK (ĐÃ FIX LỖI TREO RFID) =================
void TaskSystem(void *param) {
  Serial.println("--- Task Started ---");
  
  for (;;) {
    // 1. HỒI SỨC RFID (QUAN TRỌNG NHẤT)
    // Dòng này giúp RFID tự reset nếu bị treo do nhiễu điện
    rfid.PCD_Init(); 

    // 2. LOGIC RFID (Đưa lên đầu để ưu tiên bắt thẻ)
    // Chỉ đọc nếu có thẻ mới VÀ đọc được serial
    if (rfid.PICC_IsNewCardPresent() && rfid.PICC_ReadCardSerial()) {
        
        String uid = "";
        for (byte i = 0; i < rfid.uid.size; i++) {
           uid += String(rfid.uid.uidByte[i] < 0x10 ? " 0" : " ");
           uid += String(rfid.uid.uidByte[i], HEX);
        }
        uid.trim(); uid.toUpperCase();

        Serial.println("[RFID] UID: " + uid);
        
        // CHỐNG RUNG (Debounce): Không xử lý nếu vừa quẹt thẻ này trong 2s trước
        static String lastUID = "";
        static unsigned long lastTimeRead = 0;
        
        if (uid != lastUID || (millis() - lastTimeRead > 2000)) {
            lastUID = uid;
            lastTimeRead = millis();

            lcdMessage("THE RFID:", uid);
            
            // Mẹo: Nếu IR Exit (Bit 1) đang bị che -> Là xe ra. Không thì là xe vào.
            // (Lấy tạm trạng thái cũ để check nhanh)
            bool ir_exit = (slots[1] == 1) || (slots[3] == 1); // Tùy logic lắp đặt của bạn
            // Để đơn giản: Thử gọi Entry trước
            
            String action = "entry"; 
            String res = callServer("rfid", action, uid);
            
            // Nếu Server bảo "Thẻ này đang trong bãi rồi" -> Chuyển sang Exit
            if (res == "deny_entry" || res == "Card busy") {
               lcdMessage("CHECK OUT...", "WAIT");
               res = callServer("rfid", "exit", uid);
               action = "exit";
            }

            if (res == "allow_entry" || res == "allow_exit" || res == "payment_due") {
              lcdMessage("RFID OK", "MO CONG...");
              if (action == "entry") {
                sendToUno("OPEN_ENTRY"); delay(3000); sendToUno("CLOSE_ENTRY");
              } else {
                sendToUno("OPEN_EXIT"); delay(3000); sendToUno("CLOSE_EXIT");
              }
            } else {
              lcdMessage("LOI THE", "KHONG HOP LE");
              delay(1500);
            }
            lastSlots[0] = -1; // Reset màn hình
        }
        
        // Dừng thẻ để tránh đọc lại liên tục
        rfid.PICC_HaltA(); 
        rfid.PCD_StopCrypto1();
    }

    // 3. ĐỌC CẢM BIẾN TỪ UNO
    Wire.requestFrom(UNO_ADDR, 3);
    if (Wire.available() == 3) {
      byte sensorPacket = Wire.read();
      byte hi = Wire.read();
      byte lo = Wire.read();
      
      slots[0] = bitRead(sensorPacket, 2);
      slots[1] = bitRead(sensorPacket, 3);
      slots[2] = bitRead(sensorPacket, 4);
      slots[3] = bitRead(sensorPacket, 5);
      
      bool ir_entry = bitRead(sensorPacket, 0);
      bool ir_exit  = bitRead(sensorPacket, 1);

      // --- LOGIC CAMERA ---
      if (ir_entry && !entryTriggered) {
        entryTriggered = true;
        // Check Full
        if ((slots[0]+slots[1]+slots[2]+slots[3]) >= MAX_SLOTS) {
           lcdMessage("BAI DAY!", "FULL"); delay(2000); lastSlots[0]=-1;
        } else {
           lcdMessage("CAMERA...", "DANG SOI");
           String res = callServer("parking", "entry");
           if(res == "allow_entry") {
              lcdMessage("XE HOP LE", "MO CONG");
              sendToUno("OPEN_ENTRY"); delay(3000); sendToUno("CLOSE_ENTRY");
           } else {
              lcdMessage("KHONG NHAN DIEN", "QUET THE ->");
              delay(2000);
           }
           lastSlots[0] = -1;
        }
      }
      if (!ir_entry) entryTriggered = false;

      if (ir_exit && !exitTriggered) {
        exitTriggered = true;
        lcdMessage("CAMERA RA...", "DANG SOI");
        String res = callServer("parking", "exit");
        if(res == "allow_exit" || res == "payment_due") {
           lcdMessage("TAM BIET", "MO CONG");
           sendToUno("OPEN_EXIT"); delay(3000); sendToUno("CLOSE_EXIT");
        } else {
           lcdMessage("KHONG NHAN DIEN", "QUET THE ->");
           delay(2000);
        }
        lastSlots[0] = -1;
      }
      if (!ir_exit) exitTriggered = false;
    }

    // 4. CÁC TÁC VỤ PHỤ (Manual Check, Update Web, LCD)
    if (millis() - lastCommandCheck > 1000) { checkServerCommand(); lastCommandCheck = millis(); }
    if (millis() - lastHttpUpdate > 3000) { sendSensorData(); lastHttpUpdate = millis(); }
    if (!entryTriggered && !exitTriggered) updateLCDGrid();

    vTaskDelay(50 / portTICK_PERIOD_MS);
  }
}
// ================= SETUP =================
void setup() {
  Serial.begin(115200);
  delay(1000); // Đợi ổn định điện áp

  Serial.println("--- SYSTEM INIT ---");

  // 1. Init I2C & LCD
  Wire.begin();
  lcd.init(); lcd.backlight();
  lcd.print("INIT SYSTEM...");

  // 2. Init RFID
  SPI.begin();
  rfid.PCD_Init();
  Serial.println("RFID Ready");

  // 3. Init WiFi
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  lcd.setCursor(0,1); lcd.print("WIFI...");
  
  int retry = 0;
  while (WiFi.status() != WL_CONNECTED && retry < 20) {
    delay(500); Serial.print("."); retry++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    lcd.clear(); lcd.print("WIFI CONNECTED");
    Serial.println("\nIP: " + WiFi.localIP().toString());
  } else {
    lcd.clear(); lcd.print("WIFI FAILED");
  }
  delay(1000);

  // 4. Start Task
  xTaskCreate(TaskSystem, "System", 10000, NULL, 1, NULL); // Tăng stack lên 10000 cho an toàn
}

void loop() {}