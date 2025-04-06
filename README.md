📦 MediBox - Smart Medication Reminder & Environment Monitor
A smart IoT device that reminds users to take medicine and monitors storage conditions (temperature & humidity).

MediBox Demo (Replace with actual project image)

🔍 Features
✅ Multiple Alarms – Set reminders for medication times
✅ Temperature & Humidity Monitoring – Ensures safe storage conditions (24°C-32°C, 65%-80% humidity)
✅ Visual & Audio Alerts – LED & buzzer for alarms and warnings
✅ OLED Display – Shows time, alarms, and environment status
✅ WiFi Sync – Automatically updates time via NTP
✅ Snooze Function – 5-minute delay option

🛠 Hardware Components
Microcontroller: ESP32

Display: SSD1306 OLED (128x64)

Sensor: DHT22 (Temperature & Humidity)

Input: 4x Tactile Buttons (Up, Down, OK, Cancel)

Output: Buzzer & LED (for alerts)

⚙ Setup & Installation
1. Prerequisites
Arduino IDE 

ESP32 Board Support

2. Required Libraries
Install via Arduino Library Manager:

Adafruit_GFX

Adafruit_SSD1306

DHTesp

WiFi

TimeLib



3. Wiring Guide
Component	ESP32 Pin
OLED SDA	GPIO 21
OLED SCL	GPIO 22
DHT22	GPIO 12
Buzzer	GPIO 18
LED	GPIO 15
Button OK	GPIO 32
Button Cancel	GPIO 34
Button Up	GPIO 35
Button Down	GPIO 33
📋 Usage Instructions
Power On → Device connects to WiFi (Wokwi-GUEST by default).

Set Alarms:

Press OK → Navigate menu → Set alarm times.

View Alarms:

Check active alarms in the "View Alarms" menu.

Environment Warnings:

If temperature/humidity is unsafe, LED turns on and warnings appear.

📷 Demo
Inside the folder
