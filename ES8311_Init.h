#ifndef ES8311_INIT_H
#define ES8311_INIT_H

#include <Wire.h>


// Dirección I2C típica del ES8311 (7-bit)
#define ES8311_ADDR 0x18

bool es8311_write_reg(uint8_t reg, uint8_t val) {
    Wire.beginTransmission(ES8311_ADDR);
    Wire.write(reg);
    Wire.write(val);
    uint8_t err = Wire.endTransmission();
    delay(2);
    if(err != 0) {
        Serial.printf("[ES8311] ERROR I2C %d escribiendo reg 0x%02X\n", err, reg);
        Serial.flush();
        return false;
    }
    return true;
}

uint8_t es8311_read_reg(uint8_t reg) {
    Wire.beginTransmission(ES8311_ADDR);
    Wire.write(reg);
    Wire.endTransmission(false);
    Wire.requestFrom((uint8_t)ES8311_ADDR, (uint8_t)1);
    if (Wire.available()) {
        return Wire.read();
    }
    return 0xFF; // Error
}

void ES8311_DumpRegs() {
    Serial.println("\n--- VOLCADO DE REGISTROS ES8311 ---");
    // Chip ID
    Serial.printf("  ChipID1 (0xFD): 0x%02X\n", es8311_read_reg(0xFD));
    Serial.printf("  ChipID2 (0xFE): 0x%02X\n", es8311_read_reg(0xFE));
    Serial.printf("  Version (0xFF): 0x%02X\n", es8311_read_reg(0xFF));
    // Relojes
    Serial.printf("  Reset   (0x00): 0x%02X\n", es8311_read_reg(0x00));
    Serial.printf("  CLK_01  (0x01): 0x%02X\n", es8311_read_reg(0x01));
    Serial.printf("  CLK_02  (0x02): 0x%02X\n", es8311_read_reg(0x02));
    Serial.printf("  CLK_03  (0x03): 0x%02X\n", es8311_read_reg(0x03));
    Serial.printf("  CLK_04  (0x04): 0x%02X\n", es8311_read_reg(0x04));
    Serial.printf("  CLK_05  (0x05): 0x%02X\n", es8311_read_reg(0x05));
    Serial.printf("  CLK_06  (0x06): 0x%02X\n", es8311_read_reg(0x06));
    Serial.printf("  CLK_07  (0x07): 0x%02X\n", es8311_read_reg(0x07));
    Serial.printf("  CLK_08  (0x08): 0x%02X\n", es8311_read_reg(0x08));
    // SDP (formato I2S)
    Serial.printf("  SDP_IN  (0x09): 0x%02X\n", es8311_read_reg(0x09));
    Serial.printf("  SDP_OUT (0x0A): 0x%02X\n", es8311_read_reg(0x0A));
    // Sistema
    Serial.printf("  SYS_0B  (0x0B): 0x%02X\n", es8311_read_reg(0x0B));
    Serial.printf("  SYS_0C  (0x0C): 0x%02X\n", es8311_read_reg(0x0C));
    Serial.printf("  SYS_0D  (0x0D): 0x%02X\n", es8311_read_reg(0x0D));
    Serial.printf("  SYS_0E  (0x0E): 0x%02X\n", es8311_read_reg(0x0E));
    Serial.printf("  SYS_10  (0x10): 0x%02X\n", es8311_read_reg(0x10));
    Serial.printf("  SYS_11  (0x11): 0x%02X\n", es8311_read_reg(0x11));
    Serial.printf("  SYS_12  (0x12): 0x%02X\n", es8311_read_reg(0x12));
    Serial.printf("  SYS_13  (0x13): 0x%02X\n", es8311_read_reg(0x13));
    Serial.printf("  SYS_14  (0x14): 0x%02X\n", es8311_read_reg(0x14));
    // ADC
    Serial.printf("  ADC_15  (0x15): 0x%02X\n", es8311_read_reg(0x15));
    Serial.printf("  ADC_16  (0x16): 0x%02X\n", es8311_read_reg(0x16));
    Serial.printf("  ADC_17  (0x17): 0x%02X\n", es8311_read_reg(0x17));
    Serial.printf("  ADC_1B  (0x1B): 0x%02X\n", es8311_read_reg(0x1B));
    Serial.printf("  ADC_1C  (0x1C): 0x%02X\n", es8311_read_reg(0x1C));
    // DAC
    Serial.printf("  DAC_31  (0x31): 0x%02X\n", es8311_read_reg(0x31));
    Serial.printf("  DAC_32  (0x32): 0x%02X\n", es8311_read_reg(0x32));
    Serial.printf("  DAC_37  (0x37): 0x%02X\n", es8311_read_reg(0x37));
    // GP
    Serial.printf("  GP_45   (0x45): 0x%02X\n", es8311_read_reg(0x45));
    Serial.println("--- FIN VOLCADO ---\n");
    Serial.flush();
}

void ES8311_Init() {
    Serial.println("\n--- ESCÁNER I2C (Buscando Codec) ---"); Serial.flush();
    bool found = false;
    for(byte address = 1; address < 127; address++ ) {
      Wire.beginTransmission(address);
      if (Wire.endTransmission() == 0) {
        Serial.printf("[I2C] Dispositivo encontrado en 0x%02X\n", address); Serial.flush();
        found = true;
      }
    }
    if(!found) {
        Serial.println("[I2C] ¡NO SE ENCONTRARON DISPOSITIVOS I2C!"); Serial.flush();
    }
    Serial.println("------------------------------------\n"); Serial.flush();

    Serial.println("[ES8311] Inicializando codec (secuencia ESP-ADF completa)..."); Serial.flush();

    // ============================================================
    // FASE 1: es8311_codec_init() - Exacta de ESP-ADF
    // ============================================================
    es8311_write_reg(0x01, 0x30); // CLK_MANAGER_REG01
    es8311_write_reg(0x02, 0x00); // CLK_MANAGER_REG02
    es8311_write_reg(0x03, 0x10); // CLK_MANAGER_REG03 (adc_osr=0x10)
    es8311_write_reg(0x16, 0x24); // ADC_REG16
    es8311_write_reg(0x04, 0x10); // CLK_MANAGER_REG04 (dac_osr=0x10)
    es8311_write_reg(0x05, 0x00); // CLK_MANAGER_REG05
    es8311_write_reg(0x0B, 0x00); // SYSTEM_REG0B
    es8311_write_reg(0x0C, 0x00); // SYSTEM_REG0C
    es8311_write_reg(0x10, 0x1F); // SYSTEM_REG10
    es8311_write_reg(0x11, 0x7F); // SYSTEM_REG11
    es8311_write_reg(0x00, 0x80); // RESET_REG00: CSM ON, Slave Mode

    // Habilitar todos los relojes, MCLK desde pin externo
    es8311_write_reg(0x01, 0x3F); // CLK_MANAGER_REG01: all clocks on
    // Bit 7 = 0 → MCLK desde pin externo (ya es 0)

    // Coeficientes de reloj para MCLK=4.096MHz, Fs=16kHz
    // (pre_div=1, pre_multi=1, bclk_div=4, lrck=255)
    // Reg 0x02 ya es 0x00 (correcto para pre_div=1, pre_multi=1)
    es8311_write_reg(0x06, 0x03); // CLK_MANAGER_REG06: bclk_div=4 (4-1=3), no invert
    es8311_write_reg(0x07, 0x00); // CLK_MANAGER_REG07: lrck_h=0x00
    es8311_write_reg(0x08, 0xFF); // CLK_MANAGER_REG08: lrck_l=0xFF (total=255)

    // Configuración I2S: 16-bit, Formato Philips Standard
    // 0x0C = bits[4:2]=011 (16-bit), bits[1:0]=00 (I2S std)
    es8311_write_reg(0x09, 0x0C); // SDPIN_REG09 (DAC input format)
    es8311_write_reg(0x0A, 0x0C); // SDPOUT_REG0A (ADC output format, bit6=0 = NOT muted)

    // *** REGISTROS FALTANTES CRÍTICOS (líneas 474-476 de ESP-ADF) ***
    es8311_write_reg(0x13, 0x10); // SYSTEM_REG13 (config DAC)
    es8311_write_reg(0x1B, 0x0A); // ADC_REG1B: HPF Stage 1 (¡FILTRO DC!)
    es8311_write_reg(0x1C, 0x6A); // ADC_REG1C: HPF Stage 2 (¡FILTRO DC!)

    // ============================================================
    // FASE 2: es8311_start(ES_MODULE_ADC_DAC) - Exacta de ESP-ADF
    // ============================================================
    es8311_write_reg(0x17, 0xBF); // ADC Volumen Máximo
    es8311_write_reg(0x32, 0x80); // DAC Volumen Moderado (Previene caídas de tensión/Brownouts por USB)
    es8311_write_reg(0x0E, 0x02); // Enciende Analog PGA
    es8311_write_reg(0x12, 0x00); // Enciende DAC
    
    // Ganancia Micrófono Analógico (+24dB, DMIC apagado)
    // Ahora es seguro porque el HPF (0x1B/0x1C) filtra el DC
    es8311_write_reg(0x14, 0x1A); // PGA gain = +24dB, DMIC OFF
    
    es8311_write_reg(0x0D, 0x01); // Enciende ADC + Mic Bias
    es8311_write_reg(0x15, 0x40); // ADC ramp rate
    es8311_write_reg(0x37, 0x48); // DAC ramp rate
    es8311_write_reg(0x45, 0x00); // GP CONTROL
    
    Serial.println("[ES8311] Init completo. Volcando registros..."); Serial.flush();
    ES8311_DumpRegs();
}

#endif

