# Apple-Phone-Notification-Bridge-FreeRTOS

Iphone notification telemetry mirror using an ESP32 MCU, SPI-driven ILI9341 display, and Apple Notification Center Service (ANCS) over Bluetooth Low Energy (BLE)

# Problem Statement
As a student or a busy person, sometimes you want to focus on a task at hand and not get distracted by apps or scrolling on your phone, while still staying informed on notifications in case there is something important to respond to. A solution to this is to get rid of having access to the phone and instead replace it with a notification bridge. This way all forms of notifications, (calls, emails, messages, etc) can be received on the bridge device instead of your phone, ensuring maximum focus by not letting you touch your phone and avoid enticing social media apps.

# Features
- Live iOS notification mirroring via ANCS (title/ sender, message, app identifier)
- Secure BLE pairing/bonding (GAP peripheral + GATT client dual role)
- Scrollable notification history (circular buffer, up to 15 stored)
- Touch-driven scroll and clear-all
- Category-differentiated audio alerts via PWM tone mapping
- Dual-core FreeRTOS architecture (BLE on Core 0, rendering/touch on Core 1)

<img width="304" height="391" alt="IMG_4408" src="https://github.com/user-attachments/assets/c4edb909-2aef-4e5a-9c82-de7821f4483a" />

# Key Architecture

# Hardware and Wiring
## Parts
- ESP32
- ILI9341+XPT2046 touch display screen
- passive buzzer

## Pin Mapping

# Setup
- Using Arduino IDE, download these libraries: NimBLE-Arduino, TFT_eSPI, XPT2046_Touchscreen
- For the TFT_eSPI library, look up the needed User_Setup.h config needed for the specific display (linked below)
- Ensure the ESP32 board is setup to work with the IDE
- Set the board setting (partition scheme = Large/ Huge App)
- Download the nRF Connect App, since the device may not show on regular bluetooth screen
- In the scanner section, look for ESP32-ANCS, tap connect, then accept pairing request

# Video demo
https://drive.google.com/file/d/1ncuciQe3Naax4GUnjSa6SwU_7K7QUKud/view?usp=sharing

# Helpful sources
These are a list of the helpful info pages i consulted throughout the project:
https://developer.apple.com/library/archive/documentation/CoreBluetooth/Reference/AppleNotificationCenterServiceSpecification/Specification/Specification.html
https://randomnerdtutorials.com/esp32-freertos-queues-inter-task-arduino/
https://randomnerdtutorials.com/esp32-freertos-arduino-tasks/
https://lastminuteengineers.com/esp32-nimble-arduino-tutorial/
https://github.com/h2zero/esp-nimble-cpp/blob/master/examples/ANCS/main/main.cpp
https://www.instructables.com/IlluminANCS/
https://github.com/marcboeker/esp32-ble-ios-demo


