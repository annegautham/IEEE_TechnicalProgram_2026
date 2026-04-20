#ifndef TeensyToDLPCH
#define TeensyToDLPCH

#include <Arduino.h>
#include <i2c_device.h>
#include <imx_rt1060_i2c_driver.h>

// Definitions
#define DLPC_ADDR 0x36

// --- REGISTER DICTIONARY (OpCodes) ---
enum DLPC_Reg {
    REG_OPERATING_MODE   = 0x05,
    REG_TEST_PATTERN_SEL = 0x0B,
    REG_SPLASH_SEL       = 0x0D,
    REG_IMG_ORIENTATION  = 0x14,
    REG_IMG_CURTAIN      = 0x16,
    REG_IMG_FREEZE       = 0x1A,
    REG_SPLASH_EXC       = 0x35,
    REG_LED_ENABLE       = 0x52,
    REG_LED_CURRENT      = 0x54,
    REG_SYSTEM_STATUS    = 0xD1,
    REG_CONTROLLER_ID    = 0xD4,
    REG_EXT_PRINT_CONFIG = 0xA8,
    REG_EXT_PRINT_CCTRL  = 0xC1,
    REG_PVIDEO_EN        = 0xC3,
    REG_FPGA_BUF_SEL     = 0xC5,
    REG_FPGA_CTRL        = 0xCA,
    REG_FPGA_CRC         = 0xCE
};

// --- MODE DICTIONARY ---
enum DLPC_Mode {
    MODE_TPG      = 0x01, // Test Pattern Generator
    MODE_SPLASH   = 0x02,
    MODE_EXTERNAL = 0x06,
    MODE_STANDBY  = 0xFF
};

// --- TEST PATTERN DICTIONARY ---
enum DLPC_Pattern {
    PAT_SOLID        = 0x00,
    PAT_HORIZ_RAMP   = 0x01,
    PAT_VERT_RAMP     = 0x02,
    PAT_CHECKERBOARD = 0x07
};

// Function Prototypes
bool writeDLPC(uint8_t opCode, uint8_t* params, uint8_t len);
void setOperatingMode(uint8_t mode);
void setStandby();
void writeFPGABuffer(uint8_t buf);
void initDLPC(int irqPin);

#endif