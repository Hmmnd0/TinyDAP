#include "es8311.h"

#include "esp_log.h"

static const char *TAG = "es8311";

#define I2C_TIMEOUT_MS 100

static i2c_master_dev_handle_t s_dev;

static esp_err_t write_reg(uint8_t reg, uint8_t val)
{
    uint8_t buf[2] = { reg, val };
    return i2c_master_transmit(s_dev, buf, sizeof buf, I2C_TIMEOUT_MS);
}

static esp_err_t read_reg(uint8_t reg, uint8_t *val)
{
    return i2c_master_transmit_receive(s_dev, &reg, 1, val, 1, I2C_TIMEOUT_MS);
}

esp_err_t es8311_init(i2c_master_bus_handle_t bus, uint8_t addr)
{
    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = addr,
        .scl_speed_hz = 100000,
    };
    esp_err_t err = i2c_master_bus_add_device(bus, &dev_cfg, &s_dev);
    if (err != ESP_OK) {
        return err;
    }

    uint8_t id1 = 0, id2 = 0;
    err = read_reg(0xFD, &id1);
    if (err == ESP_OK) {
        err = read_reg(0xFE, &id2);
    }
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "no response at 0x%02x: %s", addr, esp_err_to_name(err));
        return err;
    }
    ESP_LOGI(TAG, "chip id %02x%02x%s", id1, id2,
             (id1 == 0x83 && id2 == 0x11) ? "" : " (expected 8311)");

    /* Playback sequence from M5Unified's Cardputer-Adv speaker setup (MIT),
     * plus an explicit 16-bit word length. */
    static const uint8_t init_seq[][2] = {
        { 0x00, 0x80 },  /* RESET: chip state machine on, slave mode */
        { 0x01, 0xB5 },  /* CLK_MANAGER: MCLK sourced from BCLK */
        { 0x02, 0x18 },  /* CLK_MANAGER: pre-multiply x8 (32fs BCLK -> 256fs) */
        { 0x09, 0x0C },  /* SDP_IN: I2S format, 16-bit word */
        { 0x0D, 0x01 },  /* SYSTEM: power up analog */
        { 0x12, 0x00 },  /* SYSTEM: power up DAC */
        { 0x13, 0x10 },  /* SYSTEM: enable headphone/output drive */
        { 0x37, 0x08 },  /* DAC: bypass equalizer */
    };
    for (size_t i = 0; i < sizeof init_seq / sizeof init_seq[0]; i++) {
        err = write_reg(init_seq[i][0], init_seq[i][1]);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "write reg 0x%02x failed: %s", init_seq[i][0], esp_err_to_name(err));
            return err;
        }
    }
    return ESP_OK;
}

esp_err_t es8311_set_volume_db(int db)
{
    if (db > 0) {
        db = 0;
    } else if (db < -95) {
        db = -95;
    }
    /* REG32: 0xBF = 0 dB, 0.5 dB per step. */
    return write_reg(0x32, (uint8_t)(0xBF + 2 * db));
}
