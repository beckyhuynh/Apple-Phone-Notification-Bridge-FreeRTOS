# Apple-Phone-Notification-Bridge-FreeRTOS

Iphone notification telemetry mirror using an ESP32 MCU, SPI-driven ILI9341 display, and Apple Notification Center Service (ANCS) over Bluetooth Low Energy (BLE)

# Problem Statement
As a student or a busy person, sometimes you want to focus on a task at hand and not get distracted by apps or scrolling on your phone, while still staying informed on notifications in case there is an important call out to respond to. A solution to this is to get rid of having access to the phone and instead replace it with a notification bridge. This way all forms of notifications, (calls, emails, messages, etc) can be received on the bridge device instead of your phone, ensuring maximum focus by not letting you touch your phone and avoid enticing social media apps.

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


# Video demo
https://drive.google.com/file/d/1ncuciQe3Naax4GUnjSa6SwU_7K7QUKud/view?usp=sharing
