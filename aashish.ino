#include <WiFiS3.h>
#include <ThingSpeak.h>
#include <DHT.h>
#include <ArduinoHttpClient.h>

// ======================= 1. SETTINGS =======================
const char* ssid = "John";
const char* password = "12345678";

// PC Address (Check your PC's IP again just in case!)
const char* serverAddress = "172.20.10.5"; 
const int serverPort = 5000;

// ThingSpeak
unsigned long channelID = 3231615;
const char* writeAPIKey = "ZXJU6OK8BHHW5RLX";

// Hardware Pins
#define DHT_PIN 2
#define RED_LED 12      // Heater
#define GREEN_LED 11    // Fan
#define BTN_HOT 8       // Button A
#define BTN_COLD 9      // Button B

// ======================= 2. VARIABLES =======================
DHT dht(DHT_PIN, DHT11);
WiFiClient wifi;
HttpClient client = HttpClient(wifi, serverAddress, serverPort);

// AI Model Storage
float m = 0.0;
float c = 22.0;

// Button State Tracking (To fix the repeating issue)
bool lastHotState = HIGH;
bool lastColdState = HIGH;
unsigned long lastDebounceTime = 0;

// Timers
unsigned long lastCloudUpdate = 0;
unsigned long lastAIUpdate = 0;

void setup() {
  Serial.begin(9600);
  
  pinMode(RED_LED, OUTPUT);
  pinMode(GREEN_LED, OUTPUT);
  
  // INPUT_PULLUP means: HIGH = Not Pressed, LOW = Pressed
  pinMode(BTN_HOT, INPUT_PULLUP);
  pinMode(BTN_COLD, INPUT_PULLUP);

  dht.begin();
  
  // Quick WiFi Connect
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    WiFi.begin(ssid, password);
    delay(500);
    Serial.print(".");
  }
  Serial.println(" Connected!");
  
  ThingSpeak.begin(wifi);
  
  // Get initial intelligence
  updateModelFromPC();
}

void loop() {
  // ================= SECTION A: BUTTONS (Instant Response) =================
  // We read the buttons every single loop for maximum speed
  bool currentHotState = digitalRead(BTN_HOT);
  bool currentColdState = digitalRead(BTN_COLD);

  // Check Button A (HOT) - Only trigger on "Press Down" (HIGH -> LOW)
  if (lastHotState == HIGH && currentHotState == LOW) {
    if (millis() - lastDebounceTime > 300) { // Ignore double-clicks faster than 300ms
       Serial.println(">>> User reported: TOO HOT");
       sendFeedback("hot");
       lastDebounceTime = millis();
    }
  }
  lastHotState = currentHotState;

  // Check Button B (COLD)
  if (lastColdState == HIGH && currentColdState == LOW) {
    if (millis() - lastDebounceTime > 300) {
       Serial.println(">>> User reported: TOO COLD");
       sendFeedback("cold");
       lastDebounceTime = millis();
    }
  }


    if (digitalRead(BTN_COLD) == HIGH) {
    digitalWrite(RED_LED, HIGH);  // Heater ON
    digitalWrite(GREEN_LED, LOW);
    delay(5000);
    digitalWrite(RED_LED, LOW);  // Heater ON
    digitalWrite(GREEN_LED, LOW);
    } else if (digitalRead(BTN_HOT) == HIGH) {
    digitalWrite(RED_LED, LOW);
    digitalWrite(GREEN_LED, HIGH); // Fan ON
    delay(5000);
    digitalWrite(RED_LED, LOW);  // Heater ON
    digitalWrite(GREEN_LED, LOW);
    }
  
  lastColdState = currentColdState;

  // ================= SECTION B: AI CONTROL (The "Smart" Part) =================
  float currentTemp = dht.readTemperature();
  float humidity = dht.readHumidity();
  
  // Calculate Target: y = mx + c
  // (Using minutes since start as 'x')
  float timeX = millis() / 60000.0; 
  float targetTemp = (m * timeX) + c; 



  // ================= SECTION C: BACKGROUND TASKS (Slow Stuff) =================
  
  // 1. Send to ThingSpeak (Every 20 seconds)
  if (millis() - lastCloudUpdate > 20000) {
    ThingSpeak.setField(1, currentTemp);
    ThingSpeak.setField(2, humidity);
    ThingSpeak.setField(3, targetTemp);
    ThingSpeak.writeFields(channelID, writeAPIKey);
    lastCloudUpdate = millis();
    Serial.println("(Background) Data sent to ThingSpeak.");
  }

  // 2. Update AI from PC (Every 1 Hour)
  if (millis() - lastAIUpdate > 3600000) { 
    updateModelFromPC();
    lastAIUpdate = millis();
  }
}

// ======================= HELPER FUNCTIONS =======================

void sendFeedback(String type) {
  // 1. Get current temp to send to PC
  float t = dht.readTemperature();
  if (isnan(t)) t = 20.0; // Default safety
  
  // 2. Format message: "hot,24.5"
  String message = type + "," + String(t);
  
  // 3. Send
  Serial.print("Sending to PC: "); Serial.println(message);
  client.post("/feedback", "text/plain", message);
  
  // 4. Instantly learn new habits
  updateModelFromPC();
}

void updateModelFromPC() {
  client.get("/update");
  int statusCode = client.responseStatusCode();
  
  if (statusCode == 200) {
    String response = client.responseBody(); // Format: "m,c"
    int split = response.indexOf(',');
    if (split > 0) {
      m = response.substring(0, split).toFloat();
      c = response.substring(split + 1).toFloat();
      Serial.print("AI Updated! New Formula: y = "); 
      Serial.print(m); Serial.print("x + "); Serial.println(c);
    }
  }
}