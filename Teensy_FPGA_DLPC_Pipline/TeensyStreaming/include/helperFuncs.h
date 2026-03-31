#ifndef HELPER_FUNCS_H
#define HELPER_FUNCS_H

#include <Arduino.h>
#include <SPI.h>

// extern variables (defined elsewhere)
extern uint32_t index_val;
extern uint32_t total_length;

// function declarations
void send_header(bool include_length);
uint16_t crc16(uint8_t *data, uint32_t len);

#endif