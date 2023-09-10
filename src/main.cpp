#include <Arduino.h>
#include <WifiConfig.hpp>
#include <Adafruit_NeoPixel.h>
#include <HAfanHelper.hpp>
#include <arduino-timer.h>
#include <OneButton.h>
#include <SerialHandler.hpp>
#include "secrets.h"

#define RANDOM_PIN 1
#define NEOPIXEL_PIN 18
#define BTN_PIN 2
#define FAN_PIN 4
#define STEPS 8

bool debug = true;
WifiConfig wifiConfig(WIFI_SSID, WIFI_PASSWORD, "ESP32S2 Tower Fan", "tower-fan", AUTH_USER, AUTH_PASS, true, true, debug);
Adafruit_NeoPixel pixel(1, NEOPIXEL_PIN, NEO_GRB + NEO_KHZ800);
HAfanHelper fan(wifiConfig, "fan", FAN_PIN, STEPS, 0, 0, false, debug);
OneButton button(BTN_PIN);
Timer<1> timer;
uint8_t R=2, G=0, B=0;

void serialCb(const String&);

void setup() {
  if (debug) {
    Serial.begin(115200);
    delay(10);
  }

  pinMode(RANDOM_PIN, INPUT);
  randomSeed(analogRead(RANDOM_PIN));

  fan.setCb([](int level) {
    R = level == 0 ? 2 : 0;
    G = level;
  });

  button.attachClick([]() {
    IntConfig *state = fan.getConfig().getInt("state");
    IntConfig *level = fan.getConfig().getInt("level");

    if (!state->getIntVal()) {
      state->setValue(1);
      level->setValue(1);
    } else if (level->getIntVal() < STEPS) {
      level->setValue(level->getIntVal() + 1);
    } else {
      state->setValue(0);
      level->setValue(1);
    }
  });

  button.attachDoubleClick([]() {
    IntConfig *state = fan.getConfig().getInt("state");
    IntConfig *level = fan.getConfig().getInt("level");

    if (!state->getIntVal()) {
      state->setValue(HIGH);
    }

    if (level->getIntVal() < STEPS/2 || level->getIntVal() == STEPS) {
      level->setValue(STEPS/2);
    } else {
      level->setValue(STEPS);
    }
  });

  button.attachLongPressStart([]() {
    IntConfig *state = fan.getConfig().getInt("state");
    IntConfig *level = fan.getConfig().getInt("level");

    if (state->getIntVal()) {
      state->setValue(0);
      level->setValue(1);
    }
  });

  timer.every(25, [](void*) -> bool {
    B = wifiConfig.isWifiConnected() ? 0 : STEPS;
    int mR = map(R, 0, STEPS, 0, 255);
    int mG = map(G, 0, STEPS, 0, 255);
    int mB = map(B, 0, STEPS, 0, 255);
    pixel.setPixelColor(0, mR, mG, mB);
    pixel.show();
    return true;
  });

  wifiConfig.beginMQTT(
    MQTT_SERVER,
    1883,
    MQTT_USER,
    MQTT_PASS,
    "homeassistant/",
    MQTTConnectProps([]() {
      fan.onMqttConnect();
    }, [](const String& topic, const String& data) {
      fan.onMqttMessage(topic, data);
    })
  );

  pixel.begin();
  fan.begin();
}

void loop() {
  wifiConfig.loop();
  handleSerial(debug, serialCb);
  fan.tick();
  button.tick();
  timer.tick();
  delay(1);
}

void serialCb(const String& buffer) {
  if (buffer == "reset") {
    Serial.println("Resetting config");
    SavedConfiguration wconf = wifiConfig.getConfig();
    wconf.get("ssid")->setValue(WIFI_SSID);
    wconf.get("password")->setValue(WIFI_PASSWORD);
    wconf.get("auth_user")->setValue(AUTH_USER);
    wconf.get("auth_pass")->setValue(AUTH_PASS);
    ESP.restart();
  }
}
