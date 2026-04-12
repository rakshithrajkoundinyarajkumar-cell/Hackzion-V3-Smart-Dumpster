#include <WiFi.h>
#include <HTTPClient.h>
#include <ESP32Servo.h>
#include <WebServer.h>        // ← ADDED

#define TRIG_PIN 5
#define ECHO_PIN 18
#define SERVO_PIN 13
#define BUZZER_PIN 22         // ← ADDED — connect buzzer here

// ---- WIFI ----
const char* ssid     = " ";
const char* password = " ";
String apiKey        = " ";

Servo lidServo;
WebServer server(80);         // ← ADDED

// ---- CONFIG ----
float BIN_HEIGHT = 18.0;
const int NUM_SAMPLES = 5;

// ---- ML WEIGHTS ----
float w0 = -1.1208;
float w1 = -0.8789;
float w2 = -0.8790;
float b  = 100.0;

float previous_distance = 0;

// ---- CALIBRATION ----
float EMPTY_OFFSET = 0;
bool calibrated = false;

// ---- SERVO SETTINGS ----
int OPEN_ANGLE = 90;
int CLOSE_ANGLE = 0;

// ---- THRESHOLDS ----
float CLOSE_THRESHOLD = 70.0;
float OPEN_THRESHOLD  = 70.0;

// =========================================================
//  ADDED — BUZZER FUNCTIONS
// =========================================================
void buzzAnimalAlert() {
  for (int i = 0; i < 3; i++) {
    digitalWrite(BUZZER_PIN, HIGH);
    delay(200);
    digitalWrite(BUZZER_PIN, LOW);
    delay(150);
  }
}

// =========================================================
//  ADDED — HTTP ENDPOINT HANDLERS
// =========================================================
void handleAlert() {
  String label = server.arg("label");
  String conf  = server.arg("conf");

  Serial.println("[ANIMAL ALERT] Label: " + label + " | Conf: " + conf);

  if (label == "Animals") {
    buzzAnimalAlert();
    server.send(200, "text/plain", "BUZZER_TRIGGERED");
  } else {
    server.send(200, "text/plain", "NO_ACTION");
  }
}

void handleRoot() {
  server.send(200, "text/plain", "EcoTrack ESP32 — Online");
}

// =========================================================
//  EXISTING FUNCTIONS — UNCHANGED
// =========================================================
float readDistance() {
  long duration;
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);
  duration = pulseIn(ECHO_PIN, HIGH, 30000);
  if (duration == 0) return previous_distance;
  return duration * 0.034 / 2;
}

float smoothDistance() {
  float sum = 0;
  for (int i = 0; i < NUM_SAMPLES; i++) {
    sum += readDistance();
    delay(20);
  }
  return sum / NUM_SAMPLES;
}

float predictML(float d0, float d1) {
  float delta = d0 - d1;
  float raw = w0*d0 + w1*d1 + w2*delta + b;
  if (!calibrated) {
    EMPTY_OFFSET = raw;
    calibrated = true;
    Serial.print("Calibrated Offset: ");
    Serial.println(EMPTY_OFFSET);
  }
  float fill = raw - EMPTY_OFFSET;
  if (fill < 0) fill = 0;
  if (fill > 100) fill = 100;
  return fill;
}

void sendToThingSpeak(float distance, float fillPercent, String status) {
  if (WiFi.status() != WL_CONNECTED) return;
  HTTPClient http;
  String url = "https://api.thingspeak.com/update?api_key=" + apiKey +
               "&field1=" + String(distance, 2) +
               "&field2=" + String(fillPercent, 2) +
               "&field3=" + status;
  http.begin(url);
  http.GET();
  http.end();
}

// =========================================================
//  SETUP
// =========================================================
void setup() {
  Serial.begin(115200);

  pinMode(BUZZER_PIN, OUTPUT);          // ← ADDED
  digitalWrite(BUZZER_PIN, LOW);        // ← ADDED

  WiFi.begin(ssid, password);
  Serial.print("Connecting WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println(" Connected!");
  Serial.println("ESP32 IP: " + WiFi.localIP().toString()); // ← ADDED — note this IP

  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  lidServo.attach(SERVO_PIN);
  lidServo.write(OPEN_ANGLE);

  // ---- ADDED — start web server ----
  server.on("/", handleRoot);
  server.on("/alert", handleAlert);
  server.begin();
  Serial.println("HTTP server started — waiting for animal alerts");
}

// =========================================================
//  LOOP
// =========================================================
void loop() {

  server.handleClient();                // ← ADDED — must be first line in loop

  float d0 = smoothDistance();
  float d1 = previous_distance;

  if (d0 > BIN_HEIGHT) d0 = BIN_HEIGHT;

  float formulaFill = ((BIN_HEIGHT - d0) / BIN_HEIGHT) * 100;
  formulaFill = constrain(formulaFill, 0, 100);

  float mlFill = predictML(d0, d1);

  float finalFill;
  if (d0 >= BIN_HEIGHT - 0.5) {
    finalFill = 0;
  } else {
    finalFill = 0.9 * formulaFill + 0.1 * mlFill;
  }

  Serial.print("Distance: ");
  Serial.print(d0);
  Serial.print(" | Fill %: ");
  Serial.println(finalFill);

  if (finalFill >= CLOSE_THRESHOLD) {
    lidServo.write(CLOSE_ANGLE);
    Serial.println("LID CLOSED (FULL)");
  }
  else if (finalFill <= OPEN_THRESHOLD) {
    lidServo.write(OPEN_ANGLE);
    Serial.println("LID OPEN (AVAILABLE)");
  }

  previous_distance = d0;

  String status = "EMPTY";
  if (finalFill >= CLOSE_THRESHOLD) status = "FULL";
  else if (finalFill > 30) status = "PARTIALLY FULL";

  sendToThingSpeak(d0, finalFill, status);
// Replace delay(15000) at the bottom of loop() with:
unsigned long loopEnd = millis() + 15000;
while (millis() < loopEnd) {
  server.handleClient();   // keep responding to alerts during the wait
  delay(10);
}
}