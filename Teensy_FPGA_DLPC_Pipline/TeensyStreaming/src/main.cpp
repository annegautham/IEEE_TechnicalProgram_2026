#include <Arduino.h>
#include <SPI.h>
#include "helperFuncs.h"

uint32_t index_val = 0;
uint32_t total_length = 921600;

uint16_t running_crc = 0xFFFF;
uint16_t received_crc = 0;
bool receiving_crc = false;
uint8_t crc_bytes[2];
uint8_t crc_index = 0;

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
    // =========================
    // IMAGE SENDING
    // =========================
    uint32_t remaining = total_length - sent;
    uint32_t to_read = (remaining < CHUNK) ? remaining : CHUNK;
    
    int len = Serial.readBytes(buffer, to_read);

    // Update CRC
    running_crc = crc16_update(running_crc, buffer, len);

    digitalWrite(CS_PIN, LOW);

    if (first) {
      send_header(true);
      first = false;
    } else {
      send_header(false);
    }

    SPI.transfer(buffer, len);

    digitalWrite(CS_PIN, HIGH);

    sent += len;

    // ACK chunk
    Serial.write(0xAA);

    // =========================
    // once last chunk is sent we check crc and send the same crc to fpga
    // =========================
    if (sent >= total_length) {
      while (Serial.available() && crc_index < 2) {
        crc_bytes[crc_index++] = Serial.read();
      }

      if (crc_index == 2) {
        received_crc = crc_bytes[0] | (crc_bytes[1] << 8);

        if (received_crc == running_crc) {
          Serial.write(0xCC);  // good
          // send same crc to FPGA
          digitalWrite(CS_PIN, LOW);

          uint8_t resp1 = SPI.transfer((uint8_t)(running_crc & 0xFF));
          uint8_t resp2 = SPI.transfer((uint8_t)((running_crc >> 8) & 0xFF));

          digitalWrite(CS_PIN, HIGH);

          // Combine FPGA response (if it's 16-bit)
          uint16_t fpga_crc = resp1 | (resp2 << 8);

          // Send FPGA result back to Python
          if (fpga_crc == running_crc) {
            Serial.write(0xDD);  // FPGA agrees
          } else {
            Serial.write(0xFF);  // FPGA mismatch
          }
        } else {
          Serial.write(0xEE);  // bad
        }

        // reset for new frame
        sent = 0;
        first = true;
        running_crc = 0xFFFF;
        crc_index = 0;
      }

      return;
    }
  }
}