import cv2
import numpy as np
import requests
import time
import os
os.environ["TF_CPP_MIN_LOG_LEVEL"] = "3"
os.environ["TF_ENABLE_ONEDNN_OPTS"] = "0"
os.environ["TF_USE_LEGACY_KERAS"] = "1"  # ← critical

import tensorflow as tf
import tf_keras as keras  # ← legacy Keras 2, handles old h5 models
from tf_keras.layers import DepthwiseConv2D

# ====== CONFIG ======
IP_WEBCAM_URL = "http://192.0.0.4:8080/video"
ESP32_IP      = "http://10.131.133.51"
CONFIDENCE    = 0.85
COOLDOWN_SEC  = 5
LABELS        = ["Animals", "Empty"]

# ====== PATCH DepthwiseConv2D groups bug ======
original_from_config = DepthwiseConv2D.from_config.__func__

@classmethod
def patched_from_config(cls, config):
    config.pop("groups", None)
    return original_from_config(cls, config)

DepthwiseConv2D.from_config = patched_from_config

# ====== LOAD MODEL ======
print("Loading model...")
model = keras.models.load_model("keras_model.h5", compile=False)
print("Model loaded successfully!")
print(f"Input : {model.input_shape}")
print(f"Output: {model.output_shape}")

# ====== PREPROCESS ======
def preprocess(frame):
    img = cv2.resize(frame, (224, 224))
    img = cv2.cvtColor(img, cv2.COLOR_BGR2RGB)
    img = img.astype(np.float32) / 255.0
    img = np.expand_dims(img, axis=0)
    return img

# ====== ALERT ESP32 ======
def alert_esp32(label, confidence):
    try:
        url = f"{ESP32_IP}/alert?label={label}&conf={confidence:.2f}"
        requests.get(url, timeout=2)
        print(f"[ALERT SENT] {label} ({confidence*100:.1f}%)")
    except Exception as e:
        print(f"[ESP32 UNREACHABLE] {e}")

# ====== MAIN LOOP ======
print(f"\nConnecting to IP Webcam: {IP_WEBCAM_URL}")
cap = cv2.VideoCapture(IP_WEBCAM_URL)

if not cap.isOpened():
    print("ERROR: Cannot open stream — check IP Webcam URL")
    exit()

last_alert_time = 0
print("Stream connected! Press Q to quit.\n")

while True:
    ret, frame = cap.read()
    if not ret:
        print("No frame — retrying...")
        time.sleep(1)
        continue

    inp         = preprocess(frame)
    predictions = model.predict(inp, verbose=0)[0]
    class_idx   = int(np.argmax(predictions))
    confidence  = float(predictions[class_idx])
    label       = LABELS[class_idx]

    # Main label
    color = (0, 0, 255) if label == "Animals" else (0, 255, 0)
    cv2.putText(frame, f"{label}: {confidence*100:.1f}%",
                (10, 40), cv2.FONT_HERSHEY_SIMPLEX, 1.2, color, 2)

    # All probabilities
    for i, (lbl, prob) in enumerate(zip(LABELS, predictions)):
        cv2.putText(frame, f"{lbl}: {prob*100:.1f}%",
                    (10, 80 + i * 30),
                    cv2.FONT_HERSHEY_SIMPLEX, 0.7, (255, 255, 255), 1)

    cv2.imshow("EcoTrack Animal Detector", frame)

    # Trigger buzzer
    now = time.time()
    if label == "Animals" and confidence >= CONFIDENCE:
        if now - last_alert_time > COOLDOWN_SEC:
            alert_esp32(label, confidence)
            last_alert_time = now

    if cv2.waitKey(1) & 0xFF == ord('q'):
        break

cap.release()
cv2.destroyAllWindows()