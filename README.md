# Apple-Phone-Notification-Bridge-FreeRTOS

Overview: Iphone notification telemetry mirror using an ESP32 MCU, SPI-driven ILI9341 display, and Apple Notification Center Service (ANCS) over Bluetooth Low Energy (BLE)

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

# Video demo
https://drive.google.com/file/d/1ncuciQe3Naax4GUnjSa6SwU_7K7QUKud/view?usp=sharing

# Key Architecture
## Dual Core
The system is split across two FreeRTOS tasks pinned to separate cores, connected by a single thread-safe queue. 
BLE Task (Core 0)
- advertises, bonds
- subscribes to ANCS
- assembles notification structs
- drives buzzer tone
  
Display Task (Core 1)
- owns notifList (circular buffer)
- renders to touchscreen display
- handles touch input (clear, scrolling)

separate cores ensure that slower operations on core 1(display writes, touch polling) do not delay time sensitive BLE connection, preventing dropped links

## Dual-Role BLE Architecture (GAP vs. GATT)
GAP Role (Connection): The ESP32 acts as a peripheral by advertising itself, allowing iOS to initiate and establish the physical connection.

GATT Role (Data Access): ANCS data resides on the iPhone's GATT server, requiring the ESP32 to act as a client to read characteristics and subscribe to notifications over that same link.

The Connection Trick (pServer->getClient)
The Problem: Standard client APIs try to open a new connection, which fails because iOS already established one.

The Solution: Inside onConnect(), pClient = pServer->getClient(connInfo); wraps the existing connection into a client object, sharing a single physical BLE link for both roles.

## Queue instead of mutex/ shared list
The BLE task and Display task both need access to notification data, which can require a mutex to prevent race conditions. Instead, the design avoids shared mutable state entirely:

The BLE task never touches the notification list. It only assembles one complete Notification struct per event and pushes it onto notifQueue.
The Display task is the sole owner of notifList, notifCount, and notifHead, no other task reads or writes them.

Since only one task ever touches the persistent list, there's no shared state to protect, and no mutex is needed. xQueueSend/xQueueReceive handle the one point of actual cross-task interaction safely on their own, the queue is the only thing shared between the two tasks, and it's designed to be thread-safe by default.

# Hardware and Wiring
## Parts
- ESP32
- ILI9341+XPT2046 touch display screen
- passive buzzer

## Pin Mapping
<img width="822" height="751" alt="image" src="https://github.com/user-attachments/assets/898d3e29-ab69-44e0-81e3-91743b8a72be" />

# Setup
- Using Arduino IDE, download these libraries: NimBLE-Arduino, TFT_eSPI, XPT2046_Touchscreen
- Download the working esp32 ancs ino file
- For the TFT_eSPI library, look up the needed User_Setup.h config needed for the specific display (linked below)
- Ensure the ESP32 board is setup to work with the IDE
- Set the board setting (partition scheme = Large/ Huge App)
- Download the nRF Connect App, since the device may not show on regular bluetooth screen
- In the scanner section, look for ESP32-ANCS, tap connect, then accept pairing request

# Helpful sources
These are a list of the helpful info pages i consulted throughout the project:
https://developer.apple.com/library/archive/documentation/CoreBluetooth/Reference/AppleNotificationCenterServiceSpecification/Specification/Specification.html
https://randomnerdtutorials.com/esp32-freertos-queues-inter-task-arduino/
https://randomnerdtutorials.com/esp32-freertos-arduino-tasks/
https://lastminuteengineers.com/esp32-nimble-arduino-tutorial/
https://github.com/h2zero/esp-nimble-cpp/blob/master/examples/ANCS/main/main.cpp
https://www.instructables.com/IlluminANCS/
https://github.com/marcboeker/esp32-ble-ios-demo
