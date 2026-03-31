#include "helperFuncs.h"

void send_header(bool include_length) {
  uint8_t cmd = 0x04;

  SPI.transfer(cmd);

  // Row/Col index (little endian)
  SPI.transfer((uint8_t*)&index_val, 4);

  // Dummy byte
  SPI.transfer(0x00);

  if (include_length) {
    SPI.transfer((uint8_t*)&total_length, 4);
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