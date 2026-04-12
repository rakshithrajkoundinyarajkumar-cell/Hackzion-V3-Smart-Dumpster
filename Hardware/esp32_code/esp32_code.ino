#include <WiFi.h>
#include <HTTPClient.h>
#include <ESP32Servo.h>

#define TRIG_PIN 5
#define ECHO_PIN 18
#define SERVO_PIN 13

// ---- WIFI ----
const char* ssid     = " ";
const char* password = " ";
String apiKey        = " ";

Servo lidServo;

// ---- CONFIG ----
float BIN_HEIGHT = 18.0;   // increased for correct empty detection
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
float CLOSE_THRESHOLD = 80.0;
float OPEN_THRESHOLD  = 70.0;

void setup() {
  Serial.begin(115200);

  WiFi.begin(ssid, password);
  Serial.print("Connecting WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println(" Connected!");

  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  lidServo.attach(SERVO_PIN);
  lidServo.write(OPEN_ANGLE);
}

// ---- ULTRASONIC ----
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

// ---- SMOOTHING ----
float smoothDistance() {
  float sum = 0;
  for (int i = 0; i < NUM_SAMPLES; i++) {
    sum += readDistance();
    delay(20);
  }
  return sum / NUM_SAMPLES;
}

// ---- ML (WITH CALIBRATION) ----
float predictML(float d0, float d1) {
  float delta = d0 - d1;

  float raw = w0*d0 + w1*d1 + w2*delta + b;

  // auto calibrate at empty
  if (!calibrated) {
    EMPTY_OFFSET = raw;
    calibrated = true;
    Serial.print("Calibrated Offset: ");
    Serial.println(EMPTY_OFFSET);
  }

  float fill = raw - EMPTY_OFFSET;

  // clamp
  if (fill < 0) fill = 0;
  if (fill > 100) fill = 100;

  return fill;
}

// ---- SEND ----
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

// ---- LOOP ----
void loop() {

  float d0 = smoothDistance();
  float d1 = previous_distance;

  // safety clamp
  if (d0 > BIN_HEIGHT) d0 = BIN_HEIGHT;

  // ---- FORMULA ----
  float formulaFill = ((BIN_HEIGHT - d0) / BIN_HEIGHT) * 100;
  formulaFill = constrain(formulaFill, 0, 100);

  // ---- ML ----
  float mlFill = predictML(d0, d1);

  // ---- HYBRID ----
  float finalFill;

  // critical empty condition
  if (d0 >= BIN_HEIGHT - 0.5) {
    finalFill = 0;
  } else {
    finalFill = 0.9 * formulaFill + 0.1 * mlFill;
  }

  Serial.print("Distance: ");
  Serial.print(d0);
  Serial.print(" | Fill %: ");
  Serial.println(finalFill);

  // ---- SERVO ----
  if (finalFill >= CLOSE_THRESHOLD) {
    lidServo.write(CLOSE_ANGLE);
    Serial.println("LID CLOSED (FULL)");
  }
  else if (finalFill <= OPEN_THRESHOLD) {
    lidServo.write(OPEN_ANGLE);
    Serial.println("LID OPEN (AVAILABLE)");
  }

  previous_distance = d0;

  // ---- STATUS ----
  String status = "EMPTY";
  if (finalFill >= CLOSE_THRESHOLD) status = "FULL";
  else if (finalFill > 30) status = "PARTIALLY FULL";

  // ---- SEND ----
  sendToThingSpeak(d0, finalFill, status);

  delay(15000);  // IMPORTANT for ThingSpeak
}