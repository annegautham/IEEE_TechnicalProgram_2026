#include "TeensyToFPGA.h"

void send_header(bool &isFirst) {
  uint8_t cmd = 0x04;

  SPI.transfer(cmd);

  // Row/Col index (little endian)
  uint8_t* indexPtr = (uint8_t*)&index_val;
  for(int i=0; i<4; i++) {
    SPI.transfer(indexPtr[i]);
  }

  // Dummy byte
  SPI.transfer(0x00);

  if (isFirst) {
    uint8_t* lengthPtr = (uint8_t*)&total_length;
    for(int i=0; i<4; i++) {
      SPI.transfer(lengthPtr[i]);
    }

    isFirst = false;
  }
}

uint16_t crc16(uint8_t *data, uint32_t len) {
  uint16_t crc = 0xFFFF;

  for (uint32_t i = 0; i < len; i++) {
    crc ^= (uint16_t)data[i] << 8;
    for (int j = 0; j < 8; j++) {
      if (crc & 0x8000)
        crc = (crc << 1) ^ 0x8005;
      else
        crc <<= 1;
    }
  }
  return crc;
}

uint16_t crc16_update(uint16_t crc, uint8_t *data, uint32_t len) {
  for (uint32_t i = 0; i < len; i++) {
    crc ^= (uint16_t)data[i] << 8;
    for (int j = 0; j < 8; j++) {
      if (crc & 0x8000)
        crc = (crc << 1) ^ 0x8005;
      else
        crc <<= 1;
    }
  }
  return crc;
}