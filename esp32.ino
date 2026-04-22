//////////////////////////////////////////////////////////////
// ESP32 Mic Indicator Firmware (Full Script)
// - Uses ezButton for perfect debounce
// - Short press: TOGGLE
// - Long press (>1s): UNMUTE while held, MUTE on release
// - CRC8 framed serial protocol
// - Heartbeat timeout
// - NeoPixel LED status
//////////////////////////////////////////////////////////////

#include <Adafruit_NeoPixel.h>
#include <ezButton.h>

#define LED_PIN 5
#define LED_COUNT 8
#define BUTTON_PIN 15   // Works on your board

Adafruit_NeoPixel strip(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);
ezButton button(BUTTON_PIN);

// ---------------- STATE ----------------
String micStatus = "UNKNOWN";
unsigned long lastHeartbeat = 0;
unsigned long heartbeatTimeout = 3000;

String rxBuffer = "";

// ---------------- LONG PRESS ----------------
const unsigned long LONG_PRESS_TIME = 500;
unsigned long pressStart = 0;
bool longPressActive = false;

// ---------------- CRC8 ----------------
uint8_t crc8(String data) {
  uint8_t c = 0;
  for (int i = 0; i < data.length(); i++)
    c ^= data[i];
  return c;
}

// ---------------- LED HELPERS ----------------
void setColor(uint8_t r, uint8_t g, uint8_t b) {
  for (int i = 0; i < LED_COUNT; i++)
    strip.setPixelColor(i, strip.Color(r, g, b));
  strip.show();
}

void setMuted()    { setColor(0, 255, 0); }
void setUnmuted()  { setColor(255, 0, 0); }
void setUnknown()  { setColor(0, 0, 255); }

// ---------------- SEND FRAME ----------------
void sendFrame(String type, String payload) {
  String body = type + "|" + payload;
  uint8_t c = crc8(body);
  Serial.print("^" + body + "|0x" + String(c, HEX) + "$");
}

// ---------------- PROCESS FRAME ----------------
void processFrame(String frame) {
  if (frame.length() < 5) return;
  if (frame[0] != '^' || frame[frame.length()-1] != '$') return;

  frame = frame.substring(1, frame.length()-1);

  int p1 = frame.indexOf('|');
  int p2 = frame.lastIndexOf('|');
  if (p1 < 0 || p2 < 0 || p2 <= p1) return;

  String type = frame.substring(0, p1);
  String payload = frame.substring(p1+1, p2);
  String crcStr = frame.substring(p2+1);

  uint8_t expected = crc8(type + "|" + payload);
  uint8_t received = strtol(crcStr.substring(2).c_str(), NULL, 16);

  if (expected != received) return;

  if (type == "S") {
    micStatus = payload;
    lastHeartbeat = millis();
  }
  else if (type == "H") {
    lastHeartbeat = millis();
  }
}

// ---------------- BUTTON LOGIC (ezButton) ----------------
void handleButton() {
  button.loop();

  // PRESSED
  if (button.isPressed()) {
    pressStart = millis();
    longPressActive = false;
  }

  // HELD
  if (button.getState() == LOW && !longPressActive) {
    if (millis() - pressStart > LONG_PRESS_TIME) {
      longPressActive = true;
      if (micStatus == "MUTED") {
        sendFrame("C", "UNMUTE");
      }
    }
  }

  // RELEASED
  if (button.isReleased()) {
    if (longPressActive) {
      if (micStatus == "UNMUTED") {
        sendFrame("C", "MUTE");
      }
    } else {
      sendFrame("C", "TOGGLE");
    }
  }
}

// ---------------- SETUP ----------------
void setup() {
  Serial.begin(115200);
  button.setDebounceTime(30);

  strip.begin();
  strip.show();
  setUnknown();

  Serial.println("ESP32 READY");
}

// ---------------- LOOP ----------------
void loop() {

  // Read serial
  while (Serial.available()) {
    char c = Serial.read();
    rxBuffer += c;

    if (c == '$') {
      processFrame(rxBuffer);
      rxBuffer = "";
    }
  }

  // Heartbeat timeout
  if (millis() - lastHeartbeat > heartbeatTimeout) {
    micStatus = "UNKNOWN";
  }

  // LED update
  if (micStatus == "MUTED") setMuted();
  else if (micStatus == "UNMUTED") setUnmuted();
  else setUnknown();

  // Button logic
  handleButton();
}
