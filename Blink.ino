#include <WiFi.h>
#include <Firebase_ESP_Client.h>
#include <ESP32Servo.h>

// Wi-Fi credentials
#define WIFI_SSID "Abhishek iPhone"
#define WIFI_PASSWORD "hacker55"

// Firebase credentials
#define API_KEY "AIzaSyB6W1DJLDT1mJ3X_DDR3AmKAPwyU27J2Ao"
#define DATABASE_URL "https://iot-sever-c8192-default-rtdb.europe-west1.firebasedatabase.app/"

// GPIOs
#define SERVO1_PIN 6
#define SERVO2_PIN 7
#define SOLENOID 5  // ✅ solenoid pin

// Firebase objects
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;
bool signupOK = false;

// Servo control
Servo servo1, servo2;
const int activeAngle = 90;
const int idleAngle = 0;

// Firebase paths
const String path1 = "/frets/0";
const String path2 = "/frets/4"; // strumming
const String path3 = "/frets/1";  // ✅ solenoid path

// Timing
unsigned long lastCheck = 0;
const unsigned long checkInterval = 200;

// State tracking
bool servo1State = false;
bool sweepingServo2 = false;
bool solenoidState = false;

void tokenStatusCallback(TokenInfo info) {
  if (info.status == token_status_ready) {
    Serial.println("Token Ready");
  } else {
    Serial.print("Token Status: ");
    Serial.println(info.status);
  }
}

void setup() {
  Serial.begin(115200);

  // Attach servos
  servo1.setPeriodHertz(50);
  servo2.setPeriodHertz(50);
  servo1.attach(SERVO1_PIN, 500, 2500);
  servo2.attach(SERVO2_PIN, 500, 2500);
  servo1.write(idleAngle);
  servo2.write(idleAngle);

  // ✅ Setup solenoid pin
  pinMode(SOLENOID, OUTPUT);
  digitalWrite(SOLENOID, LOW); // Default OFF

  // Initialize Wi-Fi
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to Wi-Fi");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(300);
  }
  Serial.println("\nWiFi connected.");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  // Firebase config
  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;
  config.token_status_callback = tokenStatusCallback;

  if (Firebase.signUp(&config, &auth, "", "")) {
    Serial.println("Anonymous Sign-Up Successful");
    signupOK = true;
  } else {
    Serial.printf("Sign-Up Failed: %s\n", config.signer.signupError.message.c_str());
  }

  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);
  Serial.println("Firebase Initialized");
}

void loop() {
  if (Firebase.ready() && signupOK && (millis() - lastCheck > checkInterval)) {
    lastCheck = millis();

    // Servo 1 (frets/0): move once
    if (Firebase.RTDB.getBool(&fbdo, path1)) {
      bool state = fbdo.boolData();
      if (state != servo1State) {
        servo1State = state;
        Serial.print("Servo 1 (frets/0): ");
        Serial.println(state ? "Moved to active" : "Reset to idle");
        servo1.write(state ? activeAngle : idleAngle);
      }
    }

    // Servo 2 (frets/1): sweep continuously
    if (Firebase.RTDB.getBool(&fbdo, path2)) {
      bool state = fbdo.boolData();
      if (state && !sweepingServo2) {
        sweepingServo2 = true;
        Serial.println("Servo 2 (frets/1): Start sweeping");
      } else if (!state && sweepingServo2) {
        sweepingServo2 = false;
        servo2.write(idleAngle);
        Serial.println("Servo 2 (frets/1): Stop sweeping");
      }
    }

    // ✅ Solenoid (frets/2)
    if (Firebase.RTDB.getBool(&fbdo, path3)) {
      bool state = fbdo.boolData();
      if (state != solenoidState) {
        solenoidState = state;
        digitalWrite(SOLENOID, state ? HIGH : LOW);
        Serial.print("Solenoid (frets/2): ");
        Serial.println(state ? "ON" : "OFF");
      }
    }
  }

  // Sweep servo2 only if enabled
  if (sweepingServo2) {
    for (int pos = idleAngle; pos <= activeAngle; pos++) {
      servo2.write(pos);
      delay(1);
      if (!sweepingServo2) break;
    }
    for (int pos = activeAngle; pos >= idleAngle; pos--) {
      servo2.write(pos);
      delay(1);
      if (!sweepingServo2) break;
    }
  }

  delay(5);
}