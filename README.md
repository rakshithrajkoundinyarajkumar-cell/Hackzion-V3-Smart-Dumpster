# Hackzion-V3-Smart-Dumpster



EcoTrack is an intelligent dumpster waste management system that combines IoT, real-time GPS tracking, and smart route optimization to improve garbage collection efficiency and live camera feed for certain feats.



Features include:

Real-Time Bin Monitoring

Uses IoT sensors (ESP32) to track bin fill levels

Data fetched from ThingSpeak

Live status: EMPTY / PARTIALLY FULL / FULL

GPS-Based Bin Tracking

Phone acts as a field device

Continuously updates bin location using browser GPS

Real-time syncing with backend

Live Map Dashboard

Interactive map powered by Leaflet

Shows:

Bin locations

Fill levels

Priority status

Updates every few seconds


Smart Route Optimization

Generates optimal pickup route based on:

Fill percentage

Urgency scoring

Prioritizes high-fill bins automatically

Live Camera feed + sonic buzzer trigger
Uses live feed from the camera to detect potential scavenging animals and sets off a ultra sonic buzzer to fend them away.

Tech Stack
Backend

Node.js

Express.js

Frontend

HTML, CSS, JavaScript (Vanilla)



Database

JSON-based lightweight storage



IoT

ESP32

Ultrasonic sensor






