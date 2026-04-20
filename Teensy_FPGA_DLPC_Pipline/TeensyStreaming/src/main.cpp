#include <Arduino.h>
#include <SPI.h>
// #include "TeensyToFPGA.h"
// #include "TeensyToDLPC.h"

uint32_t index_val = 0;
uint32_t total_length = 921600;

uint16_t running_crc = 0xFFFF;
uint16_t received_crc = 0;
bool receiving_crc = false;
uint8_t crc_bytes[2];
uint8_t crc_index = 0;
char command = 'A';

#define CS_PIN 10
#define IRQ_PIN 2
#define CHUNK 512

char buffer[CHUNK];

int8_t currentOpCode = 0;
uint8_t expectedParams = 0;
uint8_t paramCounter = 0;
uint8_t paramBuffer[64];

void setup() {
  Serial.begin(115200);
  
  pinMode(CS_PIN, OUTPUT);
  pinMode(IRQ_PIN, INPUT);

  initDLPC(IRQ_PIN);

  digitalWrite(CS_PIN, HIGH);

  SPI.begin();
  // FPGA clk must be 4x faster than SPI clk
  // Teensy says it can go to 100 MHz SPI but like that's the hard max
  SPI.beginTransaction(SPISettings(20000000, MSBFIRST, SPI_MODE0)); 
}

enum State {
  WAITING_FOR_COMMAND,
  RECEIVING_IMAGE,
  WRITING_REG,
  WRITING_PARAMETERS
};

State currentState = WAITING_FOR_COMMAND;
static uint32_t sent = 0;
static bool first = true;

void loop() {
  switch (currentState) {
    case WAITING_FOR_COMMAND:
      if (Serial.available()) {
        command = Serial.read();
        if (command == 'I') {
          Serial.write(0xAB);
          sent = 0;
          first = true;
          running_crc = 0xFFFF;
          crc_index = 0;
          currentState = RECEIVING_IMAGE;
        } else if (command == 'W') {
          Serial.write(0xAC);
          currentState = WRITING_REG;
        } else {
          Serial.println("Unrecognized command! Please try again");
        }
      }
      break;
  
    case RECEIVING_IMAGE:
      if (Serial.available()) {
        // =========================
        // IMAGE SENDING
        // =========================
        uint32_t remaining = total_length - sent;
        uint32_t to_read = (remaining < CHUNK) ? remaining : CHUNK;
        
        int len = Serial.readBytes(buffer, to_read);

        running_crc = crc16_update(running_crc, buffer, len);
        
        digitalWrite(CS_PIN, LOW);

        send_header(first);
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

              uint8_t resp1 = crc_bytes[0];
              uint8_t resp2 = crc_bytes[1];
              
                // send same crc to FPGA
              digitalWrite(CS_PIN, LOW);
              resp1 = SPI.transfer((uint8_t)(running_crc & 0xFF));
              resp2 = SPI.transfer((uint8_t)((running_crc >> 8) & 0xFF));
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
              Serial.write(0xBB);
            }
            currentState = WAITING_FOR_COMMAND;
          }
        }
      }
      break;
    
    case WRITING_REG:
      if (Serial.available() >= 2) { 
        currentOpCode = Serial.read();
        expectedParams = Serial.read();
        paramCounter = 0;

        Serial.print("You selected opCode: ");
        Serial.print("0x");
        if (currentOpCode < 0x10) Serial.print("0"); // Leading zero for single digits
        Serial.println(currentOpCode, HEX);

        Serial.print("With ");
        Serial.print(expectedParams);
        Serial.println(" parameters (bytes)");
        
        if (expectedParams == 0) {
          // Execute immediately if no params
          writeDLPC(currentOpCode, NULL, 0);
          currentState = WAITING_FOR_COMMAND;
        } else {
          Serial.println("What command do you want to write?");
          currentState = WRITING_PARAMETERS;
        }
      }
      break;
    
    case WRITING_PARAMETERS:
      if (Serial.available()) {
        paramBuffer[paramCounter++] = Serial.read();

        Serial.print("Sending ");
        for (uint8_t i = 0; i < expectedParams; i++) {
          Serial.print(paramBuffer[i], HEX);
          Serial.print(", ");
        }
        Serial.println("to the DLPC");
        
        if (paramCounter >= expectedParams) {
            writeDLPC(currentOpCode, paramBuffer, expectedParams);
            
            Serial.write(0xAA);
            currentState = WAITING_FOR_COMMAND;
        }
      }
      break;
  }
}