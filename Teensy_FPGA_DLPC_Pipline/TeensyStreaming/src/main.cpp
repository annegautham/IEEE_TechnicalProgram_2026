#include <Arduino.h>
#include <SPI.h>
#include <helperFuncs.h>

uint32_t index_val = 0;
uint32_t total_length = 921600;

uint16_t running_crc = 0xFFFF;
uint16_t received_crc = 0;
bool receiving_crc = false;
uint8_t crc_bytes[2];
uint8_t crc_index = 0;

#define CS_PIN 10
#define CHUNK 512

bool haveFPGA = false;

char buffer[CHUNK];

void setup() {
  Serial.begin(115200);
  if (haveFPGA == true) {
    SPI.begin();
  }
  

  pinMode(CS_PIN, OUTPUT);
  digitalWrite(CS_PIN, HIGH);

  if (haveFPGA == true) {
    // 40-50 MHz ideally, testing at 20 MHz
    // Teensy says it can go to 100 MHz SPI but like that's the hard max
    // also the FPGA has a internal clock of 100 MHz so shouldn't go only 2x more
    SPI.beginTransaction(SPISettings(20000000, MSBFIRST, SPI_MODE0)); 
  }
  
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

    running_crc = crc16_update(running_crc, buffer, len);

    digitalWrite(CS_PIN, LOW);

    if (first) {
      if (haveFPGA == true) {
        send_header(true);
      }
      first = false;
    } else {
      if (haveFPGA == true) {
        send_header(false);
      }
    }

    if (haveFPGA == true) {
      SPI.transfer(buffer, len);
    }
    

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

          uint8_t resp1 = crc_bytes[0];
          uint8_t resp2 = crc_bytes[1];
          
          if (haveFPGA == true) {
            // send same crc to FPGA
            digitalWrite(CS_PIN, LOW);
            resp1 = SPI.transfer((uint8_t)(running_crc & 0xFF));
            resp2 = SPI.transfer((uint8_t)((running_crc >> 8) & 0xFF));
            digitalWrite(CS_PIN, HIGH);
          }

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
          Serial.write(0xBB);
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