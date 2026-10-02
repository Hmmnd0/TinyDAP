#include "keyboard.h"

#include "esp_log.h"

static const char *TAG = "keyboard";

#define REG_CFG        0x01
#define REG_INT_STAT   0x02
#define REG_KEY_LCK_EC 0x03
#define REG_KEY_EVENT  0x04
#define REG_KP_GPIO1   0x1D
#define REG_KP_GPIO2   0x1E
#define REG_KP_GPIO3   0x1F
#define CFG_KE_IEN     0x01

#define I2C_TIMEOUT_MS 50

static i2c_master_dev_handle_t s_dev;

/* Cardputer layout, 4 rows x 14 columns (M5Cardputer Keyboard.h). */
static const char KEYMAP[4][14] = {
    { '`', '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', KEY_BACKSPACE },
    { KEY_TAB, 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\\' },
    { KEY_FN, KEY_SHIFT, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', KEY_ENTER },
    { KEY_CTRL, KEY_OPT, KEY_ALT, 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', ' ' },
};

static esp_err_t write_reg(uint8_t reg, uint8_t val)
{
    uint8_t buf[2] = { reg, val };
    return i2c_master_transmit(s_dev, buf, sizeof buf, I2C_TIMEOUT_MS);
}

static esp_err_t read_reg(uint8_t reg, uint8_t *val)
{
    return i2c_master_transmit_receive(s_dev, &reg, 1, val, 1, I2C_TIMEOUT_MS);
}

esp_err_t keyboard_init(i2c_master_bus_handle_t bus, uint8_t addr)
{
    i2c_device_config_t cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = addr,
        .scl_speed_hz = 100000,
    };
    esp_err_t err = i2c_master_bus_add_device(bus, &cfg, &s_dev);
    if (err != ESP_OK) {
        return err;
    }

    /* 7 rows x 8 columns of keypad scanning; flush stale events. */
    err = write_reg(REG_KP_GPIO1, 0x7F);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "no response at 0x%02x: %s", addr, esp_err_to_name(err));
        return err;
    }
    write_reg(REG_KP_GPIO2, 0xFF);
    write_reg(REG_KP_GPIO3, 0x00);
    uint8_t ev;
    for (int i = 0; i < 16 && read_reg(REG_KEY_EVENT, &ev) == ESP_OK && ev; i++) {
    }
    write_reg(REG_INT_STAT, 0x03);
    uint8_t c = 0;
    read_reg(REG_CFG, &c);
    write_reg(REG_CFG, c | CFG_KE_IEN);
    ESP_LOGI(TAG, "ready");
    return ESP_OK;
}

int keyboard_read(key_event_t *out, int max)
{
    uint8_t count = 0;
    if (read_reg(REG_KEY_LCK_EC, &count) != ESP_OK) {
        return 0;
    }
    count &= 0x0F;

    int n = 0;
    for (int i = 0; i < count && n < max; i++) {
        uint8_t ev;
        if (read_reg(REG_KEY_EVENT, &ev) != ESP_OK || ev == 0) {
            break;
        }
        /* TCA8418 key number -> its (row, col), then M5's remap onto the
         * physical 4 x 14 layout. */
        int code = (ev & 0x7F) - 1;
        int tca_row = code / 10, tca_col = code % 10;
        int row = tca_col % 4;
        int col = tca_row * 2 + (tca_col > 3 ? 1 : 0);
        if (row >= 4 || col >= 14) {
            continue;
        }
        out[n].key = KEYMAP[row][col];
        out[n].pressed = ev & 0x80;
        n++;
    }
    write_reg(REG_INT_STAT, 0x01);
    return n;
}
