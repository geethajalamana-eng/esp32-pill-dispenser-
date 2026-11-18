# ESP32 Automatic Pill Dispenser

An IoT-based pill dispenser built on the ESP32. It releases a pill at each scheduled time, alerts the user with a buzzer and OLED message, sends a Telegram notification, and keeps reminding until the user presses a button.

Mini project, B.Tech Electronics and Communication Engineering, Government Engineering College Palakkad (KTU), 2026.

## Features
- Schedule set remotely through a Telegram bot (`/setalarm HH:MM`)
- Accurate timekeeping with a DS3231 RTC, which keeps time through power loss
- Rotating carousel with 9 compartments (3 days x 3 doses), driven by a stepper motor
- Servo-operated gate releases one pill into the tray
- Buzzer and OLED alerts; the buzzer repeats until the push button is pressed
- Telegram message when a dose is dispensed

## Hardware
| Part | Role |
|---|---|
| ESP32 | Main controller, Wi-Fi |
| DS3231 RTC | Real-time clock (I2C) |
| 28BYJ-48 stepper + ULN2003 driver | Rotates the pill carousel |
| SG90 servo | Opens and closes the dispensing gate |
| SSD1306 OLED | Shows messages (I2C) |
| Buzzer, push button | Alert and acknowledgement |
| 5 V, 2 A supply | Powers everything |

## Pin connections
| Component | ESP32 pin |
|---|---|
| OLED + RTC (I2C) | SDA = GPIO 21, SCL = GPIO 22 |
| Stepper driver IN1-IN4 | GPIO 16, 18, 17, 19 |
| Servo signal | GPIO 13 |
| Buzzer | GPIO 25 |
| Push button | GPIO 27 (internal pull-up) |

## How it works
1. On power-up the ESP32 connects to Wi-Fi and the gate is set to the closed position.
2. The user sends `/setalarm HH:MM` on Telegram for each dose (up to 9).
3. The ESP32 reads the RTC continuously. When the time matches the next alarm, the stepper moves the carousel to the next compartment and the servo opens the gate for 2 seconds.
4. A Telegram message is sent, "Take Pills" appears on the OLED, and the buzzer sounds.
5. If the button is not pressed, the OLED shows "Pill Not Taken" and the buzzer repeats until it is.

Stepper rotation: 2048 steps per revolution, so 40 degrees (one compartment) is about 227 steps. The code alternates 224 and 228 steps to limit accumulated rounding error.

## Setup
1. Install the Arduino IDE with ESP32 board support.
2. Install these libraries: RTClib, Adafruit GFX, Adafruit SSD1306, ESP32Servo, UniversalTelegramBot (and ArduinoJson, which it needs).
3. Create a Telegram bot with @BotFather and note the bot token. Get your chat ID from a bot such as @userinfobot.
4. Open `pill_dispenser.ino` and fill in your Wi-Fi name, password, bot token and chat ID.
5. Upload to the ESP32 and open the Serial Monitor at 115200 baud.

Do not commit your real Wi-Fi password or bot token to a public repository.

## Limitations
- Storage is limited to the 9 compartments of the prototype.
- Needs mains power and an internet connection for Telegram alerts.
- Prototype body is made of foam board and cardboard.

## Future scope
- Weight or IR sensor to confirm the pill was actually taken
- Mobile app instead of only Telegram
- Voice assistant for elderly users
- Battery backup for power failures
- More compact and sturdier mechanical design

## Author
Amritha J, ECE, Government Engineering College Palakkad
