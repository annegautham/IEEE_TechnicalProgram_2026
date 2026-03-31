#include <Arduino.h>
#include <SPI.h>
#include "helperFuncs.h"

uint32_t index_val = 0;
uint32_t total_length = 921600;

#define CS_PIN 10
#define CHUNK 512

uint8_t buffer[CHUNK];

void setup() {
  Serial.begin(115200);
  SPI.begin();

  pinMode(CS_PIN, OUTPUT);
  digitalWrite(CS_PIN, HIGH);

  SPI.beginTransaction(SPISettings(20000000, MSBFIRST, SPI_MODE0));

  index_val = make_index();
}

void loop() {
  static uint32_t sent = 0;
  static bool first = true;

  if (Serial.available()) {
    int len = Serial.readBytes(buffer, CHUNK);

    digitalWrite(CS_PIN, LOW);

    if (first) {
      send_header(true);
      first = false;
    } else {
      send_header(false);
    }

    SPI.transfer(buffer, len);

    sent += len;

    // FINAL PACKET
    if (sent >= total_length) {
      uint16_t crc = crc16(buffer, len);

      SPI.transfer((uint8_t*)&crc, 2);

      sent = 0;
      first = true;
    }

    digitalWrite(CS_PIN, HIGH);
  }
}