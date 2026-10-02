#include "sdcard.h"

#include "board_cardputer_adv.h"
#include "driver/sdspi_host.h"
#include "driver/spi_master.h"
#include "esp_log.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"

static const char *TAG = "sdcard";

bool sdcard_mount(void)
{
    spi_bus_config_t bus = {
        .mosi_io_num = BOARD_SD_MOSI,
        .miso_io_num = BOARD_SD_MISO,
        .sclk_io_num = BOARD_SD_SCK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 4096,
    };
    esp_err_t err = spi_bus_initialize(BOARD_SD_SPI_HOST, &bus, SPI_DMA_CH_AUTO);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "spi bus: %s", esp_err_to_name(err));
        return false;
    }

    sdspi_device_config_t slot = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot.gpio_cs = BOARD_SD_CS;
    slot.host_id = BOARD_SD_SPI_HOST;
    esp_vfs_fat_sdmmc_mount_config_t mount = {
        .format_if_mount_failed = false,
        .max_files = 4,
        .allocation_unit_size = 16 * 1024,
    };

    /* 20 MHz (SD default speed). Stage 0 tested 40 MHz: the card rejects
     * high-speed mode over SPI (ESP_ERR_INVALID_RESPONSE), and the next
     * clock step, 26.7 MHz, is also above the 25 MHz default-speed limit.
     * Faster storage needs 4-bit SDMMC (Stage 1 / Rev A). */
    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    host.slot = BOARD_SD_SPI_HOST;
    host.max_freq_khz = SDMMC_FREQ_DEFAULT;

    sdmmc_card_t *card;
    err = esp_vfs_fat_sdspi_mount(SDCARD_MOUNT, &host, &slot, &mount, &card);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "mount failed: %s (card must be FAT32)", esp_err_to_name(err));
        return false;
    }
    ESP_LOGI(TAG, "mounted %s, %llu MB, SPI clock %d kHz", card->cid.name,
             (unsigned long long)card->csd.capacity * card->csd.sector_size / (1024 * 1024),
             card->real_freq_khz);
    return true;
}
