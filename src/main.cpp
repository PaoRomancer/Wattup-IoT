/* Comment this out to disable prints and save space */
#define BLYNK_PRINT Serial

/* Fill in information from Blynk Device Info here */
#define BLYNK_TEMPLATE_ID ""
#define BLYNK_TEMPLATE_NAME ""
#define BLYNK_AUTH_TOKEN ""

#include <WiFi.h>
#include <WiFiClient.h>
#include <BlynkSimpleEsp32.h>
#include <PZEM004Tv30.h>
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <HTTPClient.h>

// Constants
#define GOOGLE_SCRIPT_URL "https://script.google.com/macros/s/AKfycbxqD5WXZZWk4IP9idFP2GqZfkLylZQgaTWaSTuCk9XOnC3anktgFsISrHsogTMwWZSl/exec"
#define OLED_ADDRESS 0x3C
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define BUZZER_PIN 18
#define RELAY_PIN 23
#define VOLTAGE_HIGH_LIMIT 240.0
#define VOLTAGE_LOW_LIMIT 210.0
#define CURRENT_HIGH_LIMIT 10.0

// WiFi credentials
char ssid[] = "";
char pass[] = "";

class EmailService {
private:
  const char* scriptUrl;
  
public:
  EmailService(const char* url) : scriptUrl(url) {}
  
  void sendEmail(String message) {
    HTTPClient http;
    http.begin(scriptUrl);
    http.addHeader("Content-Type", "application/json");

    String jsonPayload = "{\"value1\":\"" + message + "\"}";
    int httpResponseCode = http.POST(jsonPayload);

    Serial.print("Response Code: ");
    Serial.println(httpResponseCode);

    if (httpResponseCode == 302) {
      String redirectUrl = http.getLocation();
      Serial.println("Redirected to: " + redirectUrl);
    } else if (httpResponseCode == 200) {
      Serial.println("Email sent successfully!");
    } else {
      Serial.println("Failed to send email!");
    }

    http.end();
  }
};

class Display {
private:
  Adafruit_SSD1306 display;
  
public:
  Display() : display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1) {}
  
  bool begin() {
    if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS)) {
      Serial.println(F("SSD1306 display not found!"));
      return false;
    }
    
    display.setTextSize(2);
    display.setTextColor(SSD1306_WHITE);
    return true;
  }
  
  void showWiFiConnecting() {
    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(0, 20);
    display.println("Connecting to");
    display.println("WiFi...");
    display.display();
  }
  
  void showWiFiConnected() {
    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(0, 20);
    display.println("Connection");
    display.println("Successful!");
    display.display();
    delay(2000);  // Show the message for 2 seconds
    display.setTextSize(2);  // Reset to original text size
  }
  
  void showTestMode(bool isLowVoltage) {
    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(0, 10);
    display.println("*** TEST MODE ***");
    display.println();
    if (isLowVoltage) {
      display.println("SIMULATING");
      display.println("VOLTAGE DROP!");
    } else {
      display.println("NORMAL VOLTAGE");
    }
    display.display();
  }
  
  void updateDisplay(float cost, float energy) {
    display.clearDisplay();
    display.setCursor(0, 0);
    display.println("Cost|Units");
    display.print(cost, 2);
    display.print("|");
    display.println(energy, 2);
    display.print("baht");
    display.print("|");
    display.print("kWh");
    display.display();
  }
};

class AlertSystem {
private:
  EmailService& emailService;
  int buzzerPin;
  int relayPin;
  float voltageHighLimit;
  float voltageLowLimit;
  float currentHighLimit;
  
  bool isVoltageHigh = false;
  bool isVoltageLow = false;
  bool isCurrentHigh = false;
  
  unsigned long voltageDropStartTime = 0;
  unsigned long voltageHighStartTime = 0;
  unsigned long currentHighStartTime = 0;
  unsigned long alertDuration = 600000; // 10 min (10 * 60 * 1000 ms)

  // For test mode
  bool testMode = false;
  
public:
  AlertSystem(EmailService& email, int buzzer, int relay, 
              float vHigh, float vLow, float cHigh) 
    : emailService(email), buzzerPin(buzzer), relayPin(relay),
      voltageHighLimit(vHigh), voltageLowLimit(vLow), currentHighLimit(cHigh) {}
  
  void begin() {
    pinMode(buzzerPin, OUTPUT);
    digitalWrite(buzzerPin, LOW);
    pinMode(relayPin, OUTPUT);
    digitalWrite(relayPin, HIGH);
  }
  
  void setTestMode(bool mode) {
    testMode = mode;
    // Reset states when entering/exiting test mode
    if (testMode) {
      resetStates();
    }
  }
  
  bool isInTestMode() {
    return testMode;
  }
  
  void resetStates() {
    isVoltageHigh = false;
    isVoltageLow = false;
    isCurrentHigh = false;
    voltageDropStartTime = 0;
    voltageHighStartTime = 0;
    currentHighStartTime = 0;
    digitalWrite(relayPin, HIGH); // Reset relay to ON
    digitalWrite(buzzerPin, LOW); // Turn off buzzer
  }
  
  // Test function for immediate reaction
  void testVoltageDropImmediate(float voltage, float current) {
    if (voltage < voltageLowLimit) {
      if (!isVoltageLow) {
        isVoltageLow = true;
        Serial.println("⚠️ TEST MODE: Voltage DROP detected. Cutting power immediately...");
        digitalWrite(relayPin, LOW); // Cut power immediately
        digitalWrite(buzzerPin, HIGH); // Turn on buzzer
        emailService.sendEmail("🚨 TEST MODE: Voltage DROP detected. Power cut immediately.");
      }
    } else {
      if (isVoltageLow) {
        isVoltageLow = false;
        Serial.println("✅ TEST MODE: Voltage back to normal. Restoring power.");
        digitalWrite(relayPin, HIGH); // Restore power
        digitalWrite(buzzerPin, LOW); // Turn off buzzer
      }
    }
  }
  
  void checkAlert(float voltage, float current) {
    // If in test mode, use immediate reaction
    if (testMode) {
      testVoltageDropImmediate(voltage, current);
      return;
    }
    
    // Regular alert system (with 10-minute delay)
    // Check for low voltage
    if (voltage < voltageLowLimit) {
      if (!isVoltageLow) {
        isVoltageLow = true;
        voltageDropStartTime = millis();
        Serial.println("⚠️ Voltage DROP detected. Waiting for 10 minutes...");
      }
    } else if (voltage >= voltageLowLimit) {
      if (isVoltageLow) {
        isVoltageLow = false;
        voltageDropStartTime = 0;
        Serial.println("✅ Voltage back to normal.");
      }
    }

    // Check for high voltage
    if (voltage > voltageHighLimit) {
      if (!isVoltageHigh) {
        isVoltageHigh = true;
        voltageHighStartTime = millis();
        Serial.println("⚠️ Voltage OVER detected. Waiting for 10 minutes...");
      }
    } else if (voltage <= voltageHighLimit) {
      if (isVoltageHigh) {
        isVoltageHigh = false;
        voltageHighStartTime = 0;
        Serial.println("✅ Voltage back to normal.");
      }
    }

    // Check for high current
    if (current > currentHighLimit) {
      if (!isCurrentHigh) {
        isCurrentHigh = true;
        currentHighStartTime = millis();
        Serial.println("⚠️ Overcurrent detected. Waiting for 10 minutes...");
      }
    } else if (current <= currentHighLimit) {
      if (isCurrentHigh) {
        isCurrentHigh = false;
        currentHighStartTime = 0;
        Serial.println("✅ Current back to normal.");
      }
    }

    // If low voltage lasts more than 10 minutes
    if (isVoltageLow && (millis() - voltageDropStartTime >= alertDuration)) {
      String message = "🚨 Voltage DROP lasted over 10 minutes. Cutting power...";
      Serial.println(message);
      emailService.sendEmail(message);
      digitalWrite(relayPin, LOW);
      digitalWrite(buzzerPin, HIGH);
    }

    // If high voltage lasts more than 10 minutes
    if (isVoltageHigh && (millis() - voltageHighStartTime >= alertDuration)) {
      String message = "🚨 Voltage OVER lasted over 10 minutes. Cutting power...";
      Serial.println(message);
      emailService.sendEmail(message);
      digitalWrite(relayPin, LOW);
      digitalWrite(buzzerPin, HIGH);
    }

    // If high current lasts more than 10 minutes
    if (isCurrentHigh && (millis() - currentHighStartTime >= alertDuration)) {
      String message = "🚨 Overcurrent lasted over 10 minutes. Cutting power...";
      Serial.println(message);
      emailService.sendEmail(message);
      digitalWrite(relayPin, LOW);
      digitalWrite(buzzerPin, HIGH);
    }
  }
};

class EnergyMonitor {
private:
  PZEM004Tv30 pzem;
  Display& display;
  BlynkTimer timer;
  EmailService& emailService;
  AlertSystem& alertSystem;
  
  float voltage = 0;
  float current = 0;
  float power = 0;
  float energy = 0;
  float pf = 0;
  float cost = 0;
  float ft_rate = 0.30;
  
  unsigned long lastEmailMillis = 0;
  unsigned long emailInterval = 600000; // 10 minutes
  
  // For test mode
  unsigned long testStartTime = 0;
  bool testLowVoltagePhase = false;
  
public:
  EnergyMonitor(EmailService& email, AlertSystem& alert, Display& disp) 
    : pzem(Serial2, 16, 17), emailService(email), alertSystem(alert), display(disp) {}
  
  bool begin() {
    updateBlynkFTRate();
    return true;
  }
  
  void updateFTRate(float newFT) {
    ft_rate = newFT;
    Serial.print("Updated FT Rate: ");
    Serial.println(ft_rate);
    updateBlynkFTRate();
  }
  
  private:
  void updateBlynkFTRate() {
    Blynk.virtualWrite(V5, String(ft_rate, 2));
    
    // Change color of FT Rate in Label (V5)
    if (ft_rate < 0) {
      Blynk.setProperty(V5, "color", "#e7f7cd");  
    } else {
      Blynk.setProperty(V5, "color", "#f5bcbc");  
    }
  }
  
  public:
  void enableTestMode() {
    alertSystem.setTestMode(true);
    testStartTime = millis();
    testLowVoltagePhase = false;
    Serial.println("⚠️ TEST MODE ENABLED - Simulating voltage fluctuations");
  }
  
  void disableTestMode() {
    alertSystem.setTestMode(false);
    Serial.println("✅ TEST MODE DISABLED - Back to normal operation");
  }
  
  float getTestVoltage() {
    // Alternate between normal and low voltage every 5 seconds
    unsigned long elapsed = millis() - testStartTime;
    if (elapsed % 10000 < 5000) { // First 5 seconds of each 10-second cycle
      if (testLowVoltagePhase) {
        testLowVoltagePhase = false;
        Serial.println("TEST: Voltage returning to normal (230V)");
      }
      return 230.0; // Normal voltage
    } else {
      if (!testLowVoltagePhase) {
        testLowVoltagePhase = true;
        Serial.println("TEST: Voltage dropping (200V)");
      }
      return 200.0; // Low voltage (below VOLTAGE_LOW_LIMIT of 210V)
    }
  }
  
  void update() {
    // Normal operation mode - read from sensor
    /*
    voltage = pzem.voltage();
    current = pzem.current();
    power = pzem.power();
    energy = pzem.energy();
    pf = pzem.pf();
    */
    
    // Test mode code - uncomment this section and comment out the above section to test
    /*
    if (alertSystem.isInTestMode()) {
      voltage = getTestVoltage();
      current = 2.5; // Constant test current
      power = voltage * current;
      // Don't change energy in test mode to avoid resetting actual energy consumption
      pf = 0.95; // Constant test power factor
      display.showTestMode(testLowVoltagePhase);
    } else {
      voltage = pzem.voltage();
      current = pzem.current();
      power = pzem.power();
      energy = pzem.energy();
      pf = pzem.pf();
    }
    */
    
    // Default to reading from sensor (this can be commented out when testing)
    voltage = pzem.voltage();
    current = pzem.current();
    power = pzem.power();
    energy = pzem.energy();
    pf = pzem.pf();
    
    cost = energy * (ft_rate + 3.25);
    
    // Check for alerts
    alertSystem.checkAlert(voltage, current);
    
    // Update Blynk
    Blynk.virtualWrite(V0, (double)voltage);
    Blynk.virtualWrite(V1, (double)current);
    Blynk.virtualWrite(V2, (double)power);
    Blynk.virtualWrite(V3, (double)energy);
    Blynk.virtualWrite(V6, (double)cost);
    
    // Update display (skip if in test mode, as display is handled separately)
    if (!alertSystem.isInTestMode()) {
      display.updateDisplay(cost, energy);
    }
    
    // Print values to serial monitor
    Serial.print("Voltage: ");
    Serial.print(voltage);
    Serial.print(" V | ");
    Serial.print("Current: ");
    Serial.print(current);
    Serial.print(" A | ");
    Serial.print("Power: ");
    Serial.print(power);
    Serial.print(" W | ");
    Serial.print("Energy: ");
    Serial.print(energy);
    Serial.print(" kWh | ");
    Serial.print("Power Factor: ");
    Serial.println(pf);
    
    // Send email reminder every 10 minutes (skip in test mode)
    if (!alertSystem.isInTestMode()) {
      unsigned long currentMillis = millis();
      if (currentMillis - lastEmailMillis >= emailInterval) {
        lastEmailMillis = currentMillis;
        String costStr = String(cost);
        emailService.sendEmail("📢 Reminder for current electricity bill payment (แจ้งเตือนชำระค่าไฟฟ้าเดือนปัจจุบัน) 💵 ยอดที่ต้องชำระ " + costStr + " บาท");
        Serial.println("Email reminder sent at 10-minute interval");
      }
    }
  }
  
  float getFTRate() {
    return ft_rate;
  }
};

// Global instances
EmailService emailService(GOOGLE_SCRIPT_URL);
AlertSystem alertSystem(emailService, BUZZER_PIN, RELAY_PIN, VOLTAGE_HIGH_LIMIT, VOLTAGE_LOW_LIMIT, CURRENT_HIGH_LIMIT);
Display display;
EnergyMonitor energyMonitor(emailService, alertSystem, display);

// Test mode control
#define TEST_MODE_PIN 5 // Digital pin to activate test mode (connect to GND to activate)

// Blynk handler for FT rate updates
BLYNK_WRITE(V4) {
  String input = param.asStr();
  Serial.print("Raw Input: ");
  Serial.println(input);

  float newFT = atof(input.c_str());
  Serial.print("Converted FT: ");
  Serial.println(newFT);

  energyMonitor.updateFTRate(newFT);
  
  // Send message update to Terminal in Blynk App
  Blynk.virtualWrite(V4, "\nFT updated to: " + String(newFT, 2) + " THB/kWh\n");
}

// Add another virtual pin for enabling/disabling test mode 
BLYNK_WRITE(V7) {
  int testModeStatus = param.asInt();
  if (testModeStatus == 1) {
    energyMonitor.enableTestMode();
    Blynk.virtualWrite(V4, "\nTEST MODE ENABLED\n");
  } else {
    energyMonitor.disableTestMode();
    Blynk.virtualWrite(V4, "\nTEST MODE DISABLED\n");
  }
}

void setup() {
  Serial.begin(115200);
  
  // Initialize I2C
  Wire.begin(21, 22);
  
  // Initialize components
  if (!display.begin()) {
    Serial.println("Failed to initialize display!");
  }
  
  alertSystem.begin();
  
  // Test mode pin setup
  pinMode(TEST_MODE_PIN, INPUT_PULLUP);
  
  // Show connecting message
  display.showWiFiConnecting();
  
  // Connect to WiFi and Blynk
  WiFi.begin(ssid, pass);
  
  // Wait for connection with timeout
  int timeout = 0;
  while (WiFi.status() != WL_CONNECTED && timeout < 20) {
    delay(500);
    Serial.print(".");
    timeout++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("WiFi connected");
    display.showWiFiConnected();
  } else {
    Serial.println("WiFi connection failed");
  }
  
  Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);
  
  // Initialize energy monitor
  energyMonitor.begin();
  
  Serial.println("System ready. To activate test mode, comment/uncomment the relevant sections in the update() method.");
}

void loop() {
  Blynk.run();
  
  // Check physical pin for test mode (alternative to Blynk control)
  if (digitalRead(TEST_MODE_PIN) == LOW) {
    if (!alertSystem.isInTestMode()) {
      energyMonitor.enableTestMode();
    }
  } else {
    if (alertSystem.isInTestMode()) {
      energyMonitor.disableTestMode();
    }
  }
  
  energyMonitor.update();
  delay(5000);
}

/*วิธีการใช้งานโหมดทดสอบ
มีส่วนของโค้ดปกติที่อ่านค่าจากเซนเซอร์
มีส่วนโค้ดทดสอบที่ถูกคอมเมนต์ไว้ที่คุณสามารถเปิดใช้งานได้


วิธีการเปิดใช้งานโหมดทดสอบ:
คอมเมนต์ส่วนที่อ่านค่าจากเซ็นเซอร์จริง
เปิดคอมเมนต์ส่วนที่ใช้ในการทดสอบ
มี 2 วิธีในการเปิดใช้งานโหมดทดสอบ:
a. ผ่านแอป Blynk (เพิ่มปุ่มบนวิดเจ็ต V7 - switch)
b. ผ่านขา GPIO ของบอร์ด (กำหนดไว้ที่ขา 5 ต่อลงกราวด์เพื่อเปิดใช้งาน)



สิ่งที่เพิ่มเติม
เพิ่มการแสดงผลบนหน้าจอ OLED เมื่ออยู่ในโหมดทดสอบ
เพิ่มคลาสและเมธอดสำหรับจัดการกับโหมดทดสอบโดยเฉพาะ
ระบบจะส่งอีเมลแจ้งเตือนเมื่อมีการตัดไฟในโหมดทดสอบ
มีการแสดงข้อความบนซีเรียลมอนิเตอร์เพื่อให้ทราบสถานะการทดสอบ*/
