#include "display.h"

#include <string.h>

#include "board_cardputer_adv.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_check.h"
#include "esp_heap_caps.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_st7789.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

static const char *TAG = "display";

#define OUT_H   (FB_H * 2)
#define OUT_Y   ((BOARD_LCD_H - OUT_H) / 2)
#define PIX_ON  0xFFFF
#define PIX_OFF 0x0000

static esp_lcd_panel_handle_t s_panel;
static uint16_t *s_px;              /* DMA buffer, BOARD_LCD_W x OUT_H */
static SemaphoreHandle_t s_idle;    /* given when the last transfer finished */
static uint8_t s_src_x[BOARD_LCD_W];
static fb_t s_last;
static bool s_have_last;
static bool s_on = true;

static bool on_trans_done(esp_lcd_panel_io_handle_t io, esp_lcd_panel_io_event_data_t *e, void *ctx)
{
    BaseType_t woken = pdFALSE;
    xSemaphoreGiveFromISR(s_idle, &woken);
    return woken == pdTRUE;
}

static void draw(int y0, int y1)
{
    xSemaphoreTake(s_idle, portMAX_DELAY);
    esp_lcd_panel_draw_bitmap(s_panel, 0, y0, BOARD_LCD_W, y1, s_px);
}

esp_err_t display_init(void)
{
    gpio_config_t bl = { .pin_bit_mask = 1ULL << BOARD_LCD_BL, .mode = GPIO_MODE_OUTPUT };
    gpio_config(&bl);
    gpio_set_level(BOARD_LCD_BL, 1);

    spi_bus_config_t bus = {
        .mosi_io_num = BOARD_LCD_MOSI,
        .miso_io_num = -1,
        .sclk_io_num = BOARD_LCD_SCLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = BOARD_LCD_W * OUT_H * sizeof(uint16_t),
    };
    ESP_RETURN_ON_ERROR(spi_bus_initialize(BOARD_LCD_SPI_HOST, &bus, SPI_DMA_CH_AUTO), TAG, "spi bus");

    esp_lcd_panel_io_handle_t io;
    esp_lcd_panel_io_spi_config_t io_cfg = {
        .cs_gpio_num = BOARD_LCD_CS,
        .dc_gpio_num = BOARD_LCD_DC,
        .spi_mode = 0,
        .pclk_hz = 40 * 1000 * 1000,
        .trans_queue_depth = 4,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
        .on_color_trans_done = on_trans_done,
    };
    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)BOARD_LCD_SPI_HOST,
                                                 &io_cfg, &io), TAG, "panel io");

    esp_lcd_panel_dev_config_t panel_cfg = {
        .reset_gpio_num = BOARD_LCD_RST,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = 16,
    };
    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_st7789(io, &panel_cfg, &s_panel), TAG, "st7789");
    esp_lcd_panel_reset(s_panel);
    esp_lcd_panel_init(s_panel);
    esp_lcd_panel_invert_color(s_panel, true);
    esp_lcd_panel_swap_xy(s_panel, BOARD_LCD_SWAP_XY);
    esp_lcd_panel_mirror(s_panel, BOARD_LCD_MIRROR_X, BOARD_LCD_MIRROR_Y);
    esp_lcd_panel_set_gap(s_panel, BOARD_LCD_GAP_X, BOARD_LCD_GAP_Y);

    s_px = heap_caps_calloc(BOARD_LCD_W * OUT_H, sizeof(uint16_t), MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
    s_idle = xSemaphoreCreateBinary();
    if (!s_px || !s_idle) {
        return ESP_ERR_NO_MEM;
    }
    xSemaphoreGive(s_idle);

    for (int x = 0; x < BOARD_LCD_W; x++) {
        s_src_x[x] = (uint8_t)(x * FB_W / BOARD_LCD_W);
    }

    /* Clear the whole panel, including the bands above and below the image. */
    draw(0, OUT_H);
    draw(OUT_H, BOARD_LCD_H);
    esp_lcd_panel_disp_on_off(s_panel, true);
    return ESP_OK;
}

void display_set_on(bool on)
{
    if (on == s_on) {
        return;
    }
    s_on = on;
    gpio_set_level(BOARD_LCD_BL, on);
    esp_lcd_panel_disp_on_off(s_panel, on);
    s_have_last = false;  /* redraw on the next frame after turning on */
}

bool display_is_on(void)
{
    return s_on;
}

void display_show(const fb_t *fb)
{
    if (!s_on) {
        return;
    }
    if (s_have_last && memcmp(fb, &s_last, sizeof s_last) == 0) {
        return;
    }
    s_last = *fb;
    s_have_last = true;

    /* Wait for the previous frame's DMA before reusing the buffer. */
    xSemaphoreTake(s_idle, portMAX_DELAY);
    uint16_t *p = s_px;
    for (int y = 0; y < OUT_H; y++) {
        for (int x = 0; x < BOARD_LCD_W; x++) {
            *p++ = fb_get(fb, s_src_x[x], y / 2) ? PIX_ON : PIX_OFF;
        }
    }
    xSemaphoreGive(s_idle);
    draw(OUT_Y, OUT_Y + OUT_H);
}
