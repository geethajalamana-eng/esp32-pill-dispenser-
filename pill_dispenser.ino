// Automatic Pill Dispenser - ESP32
// Dispenses a pill at each scheduled time, alerts the user with a buzzer,
// OLED message and Telegram notification, and waits for a button press.

#include <Wire.h>
#include "RTClib.h"
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Stepper.h>
#include <ESP32Servo.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>

// ---- Fill these in before uploading (never commit real values) ----
const char* ssid     = "YOUR_WIFI_NAME";
const char* password = "YOUR_WIFI_PASSWORD";
#define BOT_TOKEN "YOUR_TELEGRAM_BOT_TOKEN"
#define CHAT_ID   "YOUR_TELEGRAM_CHAT_ID"

// ---- Pins ----
#define SERVO_PIN 13
#define BUZZER    25
#define BUTTON    27   // wired with internal pull-up, pressed = LOW
// Stepper (ULN2003) IN1-IN4 -> GPIO 16, 18, 17, 19
// OLED + DS3231 share the I2C bus: SDA = 21, SCL = 22

#define STEPS_PER_REV 2048
#define SCREEN_WIDTH  128
#define SCREEN_HEIGHT 64

WiFiClientSecure client;
UniversalTelegramBot bot(BOT_TOKEN, client);
Stepper stepper(STEPS_PER_REV, 16, 18, 17, 19);
Servo doorServo;
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
RTC_DS3231 rtc;

// 9 compartments = 3 days x 3 doses
const int totalAlarms = 9;
int alarmHour[totalAlarms]   = {0};
int alarmMinute[totalAlarms] = {0};
int alarmIndex  = 0;   // next free slot when setting alarms
int currentSlot = 0;   // next alarm to fire
int lastMinute  = -1;  // stops the same minute firing twice
bool firstPill  = true;
unsigned long lastTelegramCheck = 0;

// Returns true once when the button is pressed (with simple debounce)
bool checkButton() {
  if (digitalRead(BUTTON) == LOW) {
    delay(50);
    if (digitalRead(BUTTON) == LOW) {
      Serial.println("Button pressed");
      return true;
    }
  }
  return false;
}

void showMessage(const char* text) {
  display.clearDisplay();
  display.setCursor(0, 20);
  display.println(text);
  display.display();
}

void setup() {
  Serial.begin(115200);
  Wire.begin(21, 22);

  stepper.setSpeed(10);
  pinMode(BUZZER, OUTPUT);
  pinMode(BUTTON, INPUT_PULLUP);

  doorServo.setPeriodHertz(50);
  doorServo.attach(SERVO_PIN, 500, 2400);
  doorServo.write(90);  // door closed

  if (!rtc.begin()) {
    Serial.println("RTC not found!");
    while (1);
  }
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("OLED not found!");
    while (1);
  }
  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(WHITE);

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected");
  client.setInsecure();  // skips certificate check; fine for a prototype

  Serial.println("System ready");
}

void loop() {
  // Check for new Telegram commands every 2 seconds
  if (millis() - lastTelegramCheck > 2000) {
    handleTelegram();
    lastTelegramCheck = millis();
  }

  DateTime now = rtc.now();

  // Time matches the next alarm -> dispense and alert
  if (currentSlot < totalAlarms &&
      now.hour() == alarmHour[currentSlot] &&
      now.minute() == alarmMinute[currentSlot] &&
      now.minute() != lastMinute) {
    Serial.println("Dispensing pill");
    dispensePill();
    alertUser();
    lastMinute = now.minute();
    currentSlot++;
  }

  // All 9 doses done -> start over
  if (currentSlot >= totalAlarms) {
    Serial.println("All pills done, resetting");
    currentSlot = 0;
    alarmIndex  = 0;
  }

  delay(500);
}

// Open the gate, let the pill drop, close it again
void operateServo() {
  doorServo.write(0);   // open
  delay(2000);
  doorServo.write(90);  // close
  delay(1000);
}

void dispensePill() {
  static int stepCount = 0;

  if (firstPill) {
    firstPill = false;  // first compartment is already at the gate
  } else {
    // Roughly 40 degrees per dose (9 compartments); 224/228 steps
    // alternate to cancel rounding error over a full turn
    if (stepCount % 9 == 0) stepper.step(224);
    else                    stepper.step(228);
    stepCount++;
    delay(500);
  }

  operateServo();

  DateTime now = rtc.now();
  String message = "Take your Pills! ";
  message += String(now.hour()) + ":" + String(now.minute());
  bot.sendMessage(CHAT_ID, message, "");
}

// Telegram command:  /setalarm HH:MM   (up to 9 alarms)
void handleTelegram() {
  int numNewMessages = bot.getUpdates(bot.last_message_received + 1);

  while (numNewMessages) {
    for (int i = 0; i < numNewMessages; i++) {
      String text    = bot.messages[i].text;
      String chat_id = bot.messages[i].chat_id;

      if (chat_id != CHAT_ID) continue;  // ignore anyone else

      if (text.startsWith("/setalarm")) {
        int h, m;
        if (sscanf(text.c_str(), "/setalarm %d:%d", &h, &m) == 2) {
          if (alarmIndex < totalAlarms) {
            alarmHour[alarmIndex]   = h;
            alarmMinute[alarmIndex] = m;
            bot.sendMessage(chat_id, "Alarm stored at slot " + String(alarmIndex), "");
            alarmIndex++;
          } else {
            bot.sendMessage(chat_id, "All alarms already set!", "");
          }
        } else {
          bot.sendMessage(chat_id, "Use: /setalarm HH:MM", "");
        }
      }
    }
    numNewMessages = bot.getUpdates(bot.last_message_received + 1);
  }
}

// Buzz for 5 s, then repeat every 5 s until the button is pressed.
// Returns true if the button was pressed during the beeping.
bool beepFor5Seconds() {
  unsigned long start = millis();
  while (millis() - start < 5000) {
    digitalWrite(BUZZER, HIGH);
    delay(200);
    if (checkButton()) { digitalWrite(BUZZER, LOW); return true; }
    digitalWrite(BUZZER, LOW);
    delay(200);
    if (checkButton()) return true;
  }
  return false;
}

void alertUser() {
  showMessage("Take Pills");
  bool pillTaken = beepFor5Seconds();

  while (!pillTaken) {
    // quiet 5 s window to press the button
    unsigned long waitStart = millis();
    while (millis() - waitStart < 5000) {
      if (checkButton()) { pillTaken = true; break; }
    }
    if (pillTaken) break;

    showMessage("Pill Not\nTaken");
    pillTaken = beepFor5Seconds();
  }

  digitalWrite(BUZZER, LOW);
  showMessage("Pill Taken");
  delay(3000);
  display.clearDisplay();
  display.display();
}
