// bare_ble_mouse.ino
// Throwaway sketch for step 0 bring-up only. Confirms Bluefruit BLE HID
// mouse advertising, pairing, and a manual click work on real hardware and
// real host platforms, before any core/firmware/ code exists.

#include <bluefruit.h>

BLEDis bledis;
BLEHidAdafruit blehid;

const int BUTTON_PIN = 7;  // any convenient GPIO with an onboard/jumper button;
                            // exact pin doesn't matter for this throwaway test

void setup() {
  Serial.begin(115200);

  pinMode(BUTTON_PIN, INPUT_PULLUP);

  Bluefruit.begin();
  Bluefruit.setTxPower(4);
  Bluefruit.setName("Autoclicker Bringup Test");

  bledis.setManufacturer("Bringup Test");
  bledis.setModel("Step 0");
  bledis.begin();

  blehid.begin();

  Bluefruit.Advertising.addFlags(BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE);
  Bluefruit.Advertising.addTxPower();
  Bluefruit.Advertising.addAppearance(BLE_APPEARANCE_HID_MOUSE);
  Bluefruit.Advertising.addService(blehid);
  Bluefruit.Advertising.addName();
  Bluefruit.Advertising.restartOnDisconnect(true);
  Bluefruit.Advertising.setInterval(32, 244);
  Bluefruit.Advertising.setFastTimeout(30);
  Bluefruit.Advertising.start(0);
}

void loop() {
  static bool wasDown = false;
  bool isDown = (digitalRead(BUTTON_PIN) == LOW);

  if (isDown && !wasDown) {
    blehid.mouseButtonPress(MOUSE_BUTTON_LEFT);
  } else if (!isDown && wasDown) {
    blehid.mouseButtonRelease();
  }
  wasDown = isDown;
}
