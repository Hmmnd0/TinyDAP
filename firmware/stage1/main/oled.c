#include "oled.h"

#include "board_devkitc.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_check.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "oled";
static spi_device_handle_t s_dev;

static void send(bool data, const uint8_t *buf, size_t len)
{
    gpio_set_level(BOARD_OLED_DC, data);
    spi_transaction_t t = { .length = len * 8, .tx_buffer = buf };
    spi_device_polling_transmit(s_dev, &t);
}

static void cmd(const uint8_t *c, size_t n) { send(false, c, n); }

esp_err_t oled_init(void)
{
    gpio_config_t io = {
        .pin_bit_mask = (1ULL << BOARD_OLED_DC) | (1ULL << BOARD_OLED_RES),
        .mode = GPIO_MODE_OUTPUT,
    };
    gpio_config(&io);

    spi_bus_config_t bus = {
        .mosi_io_num = BOARD_OLED_MOSI,
        .miso_io_num = -1,
        .sclk_io_num = BOARD_OLED_SCK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = FB_W * FB_H / 8,
    };
    ESP_RETURN_ON_ERROR(spi_bus_initialize(BOARD_OLED_SPI_HOST, &bus, SPI_DMA_CH_AUTO), TAG, "bus");
    spi_device_interface_config_t dev = {
        .clock_speed_hz = 8 * 1000 * 1000,
        .mode = 0,
        .spics_io_num = BOARD_OLED_CS,
        .queue_size = 1,
    };
    ESP_RETURN_ON_ERROR(spi_bus_add_device(BOARD_OLED_SPI_HOST, &dev, &s_dev), TAG, "device");

    gpio_set_level(BOARD_OLED_RES, 0);
    vTaskDelay(pdMS_TO_TICKS(10));
    gpio_set_level(BOARD_OLED_RES, 1);
    vTaskDelay(pdMS_TO_TICKS(10));

    static const uint8_t init[] = {
        0xAE,             /* display off */
        0xD5, 0x80,       /* clock divide */
        0xA8, 0x3F,       /* multiplex 64 */
        0xD3, 0x00,       /* display offset */
        0x40,             /* start line 0 */
        0x8D, 0x14,       /* charge pump on */
        0x20, 0x00,       /* horizontal addressing */
        0xA1,             /* segment remap */
        0xC8,             /* COM scan descending */
        0xDA, 0x12,       /* COM pins */
        0x81, 0x7F,       /* contrast ~50% (burn-in, write-up §8) */
        0xD9, 0xF1,       /* precharge */
        0xDB, 0x40,       /* VCOMH */
        0xA4,             /* follow RAM */
        0xA6,             /* normal, not inverted */
        0xAF,             /* display on */
    };
    cmd(init, sizeof init);
    return ESP_OK;
}

void oled_set_contrast(uint8_t level)
{
    uint8_t c[] = { 0x81, level };
    cmd(c, sizeof c);
}

void oled_show(const fb_t *fb)
{
    static const uint8_t window[] = { 0x21, 0, 127, 0x22, 0, 7 };
    cmd(window, sizeof window);
    send(true, fb->buf, sizeof fb->buf);
}
