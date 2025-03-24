# WattsUp: Smart Energy Monitoring System

## 🌟 Features
✅ 1. Real-time Energy Monitoring
- Measure Voltage
- Measure Current
- Measure Energy Consumption
- Measure Power
- Measure Power Factor (pf)

📲 2. Data Transmission to Blynk App
- Display real-time energy values on the Blynk app
- Update the electricity tariff (FT) through the Terminal on Blynk

Change the color in Blynk according to the FT value (Green if negative, Red if positive)

Send energy and cost values to Blynk Virtual Pins

📧 3. Alert and Automatic Power Cutoff System
Detect overvoltage or undervoltage conditions

Detect overcurrent conditions

Send email alerts when energy consumption exceeds the limit (via Google Apps Script)

Automatically cut off power through the relay if abnormal values persist for more than 10 minutes

💾 4. Data Logging to Google Sheets
Send energy and cost data to Google Sheets

Use Google Apps Script for automatic data updates

🖥️ 5. OLED Display Output
Display real-time values of Energy, Cost, and Units

Update every 5 seconds

🚨 6. Buzzer for Alerts
Trigger an alert when voltage or current is abnormal

Sound the buzzer when electricity cost exceeds the set limit

🔋 7. Surge and Overcurrent Protection
Automatically cut off power when voltage or current exceeds the limit

Resume power when values return to normal



## Hardware
 - ESP8266 NodeMCU
 - [OLED 256×64 SSD1332](https://www.buydisplay.com/default/serial-oled-module-price-3-2-inch-display-256x64-screens-white-on-black)
 - LDR(Light Dependent Resistor)
 - Push Button
 - IR LED and LED (normal led for indicate when IR is emitting)
 - NPN Transistor BC338 - for increse voltage level for IR LED
