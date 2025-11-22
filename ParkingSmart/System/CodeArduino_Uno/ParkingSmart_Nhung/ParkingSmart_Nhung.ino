#include <Servo.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

LiquidCrystal_I2C lcd(0x27, 16, 2);

// ==== PIN DEFINE ====
#define SENSOR_ENTRY 2
#define SENSOR_EXIT  3
#define SLOT1 4
#define SLOT2 5
#define SLOT3 6
#define SLOT4 7

#define SERVO_ENTRY_PIN 9
#define SERVO_EXIT_PIN 10

// ==== SERVO ====
Servo servoEntry;
Servo servoExit;

int entryState = HIGH;
int exitState = HIGH;
int slotState[4];
int lastSlotState[4] = {-1, -1, -1, -1}; // Mảng lưu trạng thái cũ để so sánh

// ==== LOGIC CỔNG ====
void openGate(Servo &gate) {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Gate Opening...");
  gate.write(0);     // mở
  delay(2000);
}

void closeGate(Servo &gate) {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Gate Closing...");
  gate.write(90);     // đóng
  delay(2000);
}

void updateLCD() {
  lcd.clear();
  
  lcd.setCursor(0, 0);
  lcd.print("S1:");
  lcd.print(slotState[0] == LOW ? "X" : "O");
  lcd.print(" S2:");
  lcd.print(slotState[1] == LOW ? "X" : "O");

  lcd.setCursor(0, 1);
  lcd.print("S3:");
  lcd.print(slotState[2] == LOW ? "X" : "O");
  lcd.print(" S4:");
  lcd.print(slotState[3] == LOW ? "X" : "O");
}

int getFreeSlots() {
  int freeCount = 0;
  for (int i = 0; i < 4; i++) {
    if (slotState[i] == HIGH) freeCount++;  // HIGH = trống
  }
  return freeCount;
}

void printSlotStatusSerial() {
  Serial.print("Slots: ");
  Serial.print(slotState[0] == LOW ? "[S1:X] " : "[S1:O] ");
  Serial.print(slotState[1] == LOW ? "[S2:X] " : "[S2:O] ");
  Serial.print(slotState[2] == LOW ? "[S3:X] " : "[S3:O] ");
  Serial.print(slotState[3] == LOW ? "[S4:X] " : "[S4:O] ");

  int freeSlots = getFreeSlots();
  Serial.print(" | Free: ");
  Serial.println(freeSlots);
}

void setup() {
  Serial.begin(9600);

  pinMode(SENSOR_ENTRY, INPUT_PULLUP);
  pinMode(SENSOR_EXIT, INPUT_PULLUP);

  pinMode(SLOT1, INPUT_PULLUP);
  pinMode(SLOT2, INPUT_PULLUP);
  pinMode(SLOT3, INPUT_PULLUP);
  pinMode(SLOT4, INPUT_PULLUP);

  servoEntry.attach(SERVO_ENTRY_PIN);
  servoExit.attach(SERVO_EXIT_PIN);

  closeGate(servoEntry);
  closeGate(servoExit);

  lcd.init();
  lcd.backlight();
  
  // Đọc trạng thái lần đầu
  slotState[0] = digitalRead(SLOT1);
  slotState[1] = digitalRead(SLOT2);
  slotState[2] = digitalRead(SLOT3);
  slotState[3] = digitalRead(SLOT4);
  
  updateLCD();
}

void loop() {
  // ==== 1. ĐỌC SLOT ====
  slotState[0] = digitalRead(SLOT1);
  slotState[1] = digitalRead(SLOT2);
  slotState[2] = digitalRead(SLOT3);
  slotState[3] = digitalRead(SLOT4);

  printSlotStatusSerial(); 

  // ==== 2. KIỂM TRA THAY ĐỔI ĐỂ CẬP NHẬT LCD ====
  // Logic này giúp LCD cập nhật ngay khi D4-D7 thay đổi mà không cần chờ D2/D3
  bool hasChanged = false;
  for(int i=0; i<4; i++){
    if(slotState[i] != lastSlotState[i]){
        hasChanged = true;
        lastSlotState[i] = slotState[i]; // Cập nhật lại trạng thái cũ
    }
  }

  if(hasChanged) {
      updateLCD(); // Chỉ cập nhật LCD khi trạng thái slot thực sự thay đổi
  }

  int freeSlots = getFreeSlots();

  // ==== 3. XỬ LÝ CẢM BIẾN VÀO ====
  int readEntry = digitalRead(SENSOR_ENTRY);

  if (readEntry == LOW && entryState == HIGH) {
    Serial.println("Xe vào -> Kiểm tra slot...");

    if (freeSlots == 0) {
      Serial.println("❌ Parking FULL – Entry Denied");
      lcd.clear();
      lcd.print("Parking FULL!");
      lcd.setCursor(0, 1);
      lcd.print("Entry Denied");
      delay(1500);
      updateLCD(); // Trả lại màn hình chính sau thông báo
    } else {
      Serial.println("✔ Slot available -> Opening Entry Gate");
      openGate(servoEntry);
      closeGate(servoEntry);
      updateLCD(); // Cập nhật lại màn hình chính sau khi đóng cổng
    }
  }
  entryState = readEntry;

  // ==== 4. XỬ LÝ CẢM BIẾN RA ====
  int readExit = digitalRead(SENSOR_EXIT);

  if (readExit == LOW && exitState == HIGH) {
    Serial.println("Xe ra -> Mo cong ra");
    openGate(servoExit);
    closeGate(servoExit);
    updateLCD(); // Cập nhật lại màn hình chính sau khi đóng cổng
  }
  exitState = readExit;

  delay(100); // Giảm delay xuống 100ms để phản hồi nhanh hơn
}