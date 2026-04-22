#include "TeensyToDLPC.h"

bool writeDLPC(uint8_t opCode, uint8_t* params, uint8_t len) {
    Wire.beginTransmission(DLPC_ADDR);
    Wire.write(opCode);
    for (uint8_t i = 0; i < len; i++) {
        Wire.write(params[i]);
    }
    return (Wire.endTransmission() == 0);
}

void setOperatingMode(DLPC_Mode mode) {
    uint8_t params[] = { mode };
    writeDLPC(0x05, params, 1);
}

void setStandby() {
    setOperatingMode(MODE_STANDBY);
}

void initDLPC(int irqPin) {
    Serial.println("Waiting for DLPC1438 boot...");
    while(digitalRead(irqPin) == HIGH) {
        delay(10);
    }
    Serial.println("HOSTIRQ pin pulled high! Starting i2c");
    
    Wire.begin();
    Wire.setClock(100000); // 100kHz from TI
    Serial.println("DLPC Communication Initialized.");

    Serial.println("Configuring settings");
    setStandby();
    // frame 1 steps 1-3
    // error injection, enable crc16, reset FPGA, allow reset
    uint8_t val = 0x0F;
    writeDLPC(REG_FPGA_CTRL, &val, 1); // The '&' passes the memory address
    // active buffer = 0
    val = 0x00;
    writeDLPC(REG_FPGA_BUF_SEL, &val, 1);
    // SECTION 3.3.6.1, TPG CONFIG LINEAR DEGAMMA AND TURN ON LED 1 I THINK, THE SOFTWARE GUIDE IS WRITTEN WEIRDLY
    uint8_t extConfig[] = {0x00, 0x01};
    writeDLPC(REG_EXT_PRINT_CONFIG, extConfig, 2);

    Serial.println("Done configuring!");
    
}