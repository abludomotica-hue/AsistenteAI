#ifndef ES8311_INIT_C
#define ES8311_INIT_C

#include <stdint.h>
#include <stdbool.h>
#include "esp_log.h"
#include "driver/i2c_master.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define DEBUG_VERBOSE 0
#define ES8311_ADDR 0x18
static const char *TAG_ES = "ES8311";

static i2c_master_dev_handle_t es8311_dev_handle = NULL;

bool es8311_write_reg(uint8_t reg, uint8_t val) {
    if (!es8311_dev_handle) return false;
    uint8_t buffer[2] = {reg, val};
    esp_err_t err = i2c_master_transmit(es8311_dev_handle, buffer, 2, -1);
    vTaskDelay(pdMS_TO_TICKS(2));
    if(err != ESP_OK) {
        ESP_LOGE(TAG_ES, "ERROR I2C 0x%X escribiendo reg 0x%02X", err, reg);
        return false;
    }
    return true;
}

uint8_t es8311_read_reg(uint8_t reg) {
    if (!es8311_dev_handle) return 0xFF;
    uint8_t val = 0;
    i2c_master_transmit_receive(es8311_dev_handle, &reg, 1, &val, 1, -1);
    return val;
}

void ES8311_Init(i2c_master_bus_handle_t bus_handle) {
    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = ES8311_ADDR,
        .scl_speed_hz = 100000,
    };
    i2c_master_bus_add_device(bus_handle, &dev_cfg, &es8311_dev_handle);

    ESP_LOGI(TAG_ES, "Inicializando codec (secuencia ESP-ADF completa)...");

    // FASE 1
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

    es8311_write_reg(0x01, 0x3F); // CLK_MANAGER_REG01: all clocks on
    es8311_write_reg(0x06, 0x03); // CLK_MANAGER_REG06: bclk_div=4
    es8311_write_reg(0x07, 0x00); // CLK_MANAGER_REG07: lrck_h=0x00
    es8311_write_reg(0x08, 0xFF); // CLK_MANAGER_REG08: lrck_l=0xFF

    es8311_write_reg(0x09, 0x0C); // SDPIN_REG09 
    es8311_write_reg(0x0A, 0x0C); // SDPOUT_REG0A 

    es8311_write_reg(0x13, 0x10); // SYSTEM_REG13 
    es8311_write_reg(0x1B, 0x0A); // ADC_REG1B: HPF Stage 1 
    es8311_write_reg(0x1C, 0x6A); // ADC_REG1C: HPF Stage 2 

    // FASE 2
    es8311_write_reg(0x17, 0xBF); // ADC Volumen Máximo
    es8311_write_reg(0x32, 0x80); // DAC Volumen Moderado
    es8311_write_reg(0x0E, 0x02); // Analog PGA
    es8311_write_reg(0x12, 0x00); // DAC
    
    es8311_write_reg(0x14, 0x1A); // PGA gain = +24dB, DMIC OFF
    
    es8311_write_reg(0x0D, 0x01); // ADC + Mic Bias
    es8311_write_reg(0x15, 0x40); // ADC ramp rate
    es8311_write_reg(0x37, 0x48); // DAC ramp rate
    es8311_write_reg(0x45, 0x00); // GP CONTROL
    
    ESP_LOGI(TAG_ES, "Init completo y equilibrado (PA/DAC calibrados).");
}

static uint8_t s_current_volume_percent = 70; // 70% por defecto (Reg 0x32 = 0x80)

bool es8311_set_volume(uint8_t volume_percent) {
    if (!es8311_dev_handle) return false;
    if (volume_percent > 100) volume_percent = 100;
    // Mapeo perceptual seguro: 0% -> 0x00 (mute), 100% -> 0xBF (0 dB, nivel de fábrica sin recorte)
    uint8_t reg_val = (uint8_t)((volume_percent * 191) / 100);
    bool ok = es8311_write_reg(0x32, reg_val);
    if (ok) {
        s_current_volume_percent = volume_percent;
        ESP_LOGI(TAG_ES, "Volumen ajustado a %d%% (Reg 0x32 = 0x%02X)", volume_percent, reg_val);
    }
    return ok;
}

uint8_t es8311_get_volume(void) {
    return s_current_volume_percent;
}

#endif
