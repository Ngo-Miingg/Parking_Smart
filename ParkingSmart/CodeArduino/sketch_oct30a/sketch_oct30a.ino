
#include <Wire.h>
#include <Servo.h>

#define SLAVE_ADDRESS 0x08

// ===== PIN DEFINITION =====
#define SERVO_ENTRY_PIN 9
#define SERVO_EXIT_PIN 10
#define MQ135_PIN A0

// Các chân IR (INPUT_PULLUP)
const int irPins[6] = {2, 3, 4, 5, 6, 7}; 
// Index: 0=Entry, 1=Exit, 2=Slot1, 3=Slot2, 4=Slot3, 5=Slot4

Servo servoEntry;
Servo servoExit;

volatile byte sensorStates = 0;
volatile int mqValue = 0;

// Smoothing MQ135
const int MQ_SMOOTH = 10;
int mqBuffer[MQ_SMOOTH];
int mqIndex = 0;
long mqSum = 0;

void setup() {
  Serial.begin(9600);
  Wire.begin(SLAVE_ADDRESS);
  Wire.onRequest(requestEvent);
  Wire.onReceive(receiveEvent);

  // Setup IR Pins
  for (int i = 0; i < 6; i++) {
    pinMode(irPins[i], INPUT_PULLUP);
  }
  pinMode(MQ135_PIN, INPUT);

  // Setup Servo
  servoEntry.attach(SERVO_ENTRY_PIN);
  servoExit.attach(SERVO_EXIT_PIN);
  
  // === QUAN TRỌNG: KHỞI ĐỘNG Ở TRẠNG THÁI ĐÓNG (90 độ) ===
  servoEntry.write(90); 
  servoExit.write(90);
  
  Serial.println(F("--- UNO READY ---"));
}

void loop() {
  readSensors();
  delay(50); // Đọc mỗi 50ms
}

void readSensors() {
  byte current = 0;
  // Đọc 6 cảm biến, nếu LOW (có vật cản) thì set bit tương ứng
  for (int i = 0; i < 6; i++) {
    if (digitalRead(irPins[i]) == LOW) {
      bitSet(current, i);
    }
  }
  sensorStates = current;

  // Đọc MQ135
  int raw = analogRead(MQ135_PIN);
  mqSum = mqSum - mqBuffer[mqIndex];
  mqBuffer[mqIndex] = raw;
  mqSum = mqSum + raw;
  mqIndex = (mqIndex + 1) % MQ_SMOOTH;
  mqValue = mqSum / MQ_SMOOTH;
}

// Gửi dữ liệu cho ESP32
void requestEvent() {
  byte data[3];
  data[0] = sensorStates;
  data[1] = highByte(mqValue);
  data[2] = lowByte(mqValue);
  Wire.write(data, 3);
}

// Nhận lệnh từ ESP32
void receiveEvent(int numBytes) {
  String cmd = "";
  while (Wire.available()) {
    char c = Wire.read();
    cmd += c;
  }
  cmd.trim();
  
  // LOGIC SERVO MỚI: MỞ = 0, ĐÓNG = 90
  
  if (cmd == "OPEN_ENTRY") {
    servoEntry.write(0);      // <--- MỞ
    Serial.println("CMD: ENTRY OPEN (0 deg)");
  } 
  else if (cmd == "CLOSE_ENTRY") {
    servoEntry.write(90);     // <--- ĐÓNG
    Serial.println("CMD: ENTRY CLOSE (90 deg)");
  }
  else if (cmd == "OPEN_EXIT") {
    servoExit.write(0);       // <--- MỞ
    Serial.println("CMD: EXIT OPEN (0 deg)");
  }
  else if (cmd == "CLOSE_EXIT") {
    servoExit.write(90);      // <--- ĐÓNG
    Serial.println("CMD: EXIT CLOSE (90 deg)");
  }
}