#include "sdcard.h"

#include "board_devkitc.h"
#include "driver/sdmmc_host.h"
#include "esp_log.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"

static const char *TAG = "sdcard";

static esp_err_t try_mount(int width, int freq_khz, sdmmc_card_t **card)
{
    sdmmc_host_t host = SDMMC_HOST_DEFAULT();
    host.max_freq_khz = freq_khz;

    sdmmc_slot_config_t slot = SDMMC_SLOT_CONFIG_DEFAULT();
    slot.width = width;
    slot.clk = BOARD_SD_CLK;
    slot.cmd = BOARD_SD_CMD;
    slot.d0 = BOARD_SD_D0;
    slot.d1 = BOARD_SD_D1;
    slot.d2 = BOARD_SD_D2;
    slot.d3 = BOARD_SD_D3;
    slot.cd = SDMMC_SLOT_NO_CD;
    slot.wp = SDMMC_SLOT_NO_WP;
    /* The Adafruit breakout has its own pull-ups; these are a backup. */
    slot.flags |= SDMMC_SLOT_FLAG_INTERNAL_PULLUP;

    esp_vfs_fat_sdmmc_mount_config_t mount = {
        .format_if_mount_failed = false,
        .max_files = 4,
        .allocation_unit_size = 16 * 1024,
    };
    return esp_vfs_fat_sdmmc_mount(SDCARD_MOUNT, &host, &slot, &mount, card);
}

bool sdcard_mount(void)
{
    sdmmc_card_t *card;
    /* 1-bit mode only needs CLK, CMD and D0: if it works where 4-bit
     * doesn't, check the D1/DAT2/D3 wires. */
    static const struct { int width, khz; } tries[] = {
        { 4, SDMMC_FREQ_HIGHSPEED }, { 4, SDMMC_FREQ_DEFAULT }, { 1, SDMMC_FREQ_DEFAULT },
    };
    esp_err_t err = ESP_FAIL;
    int width = 0;
    for (size_t i = 0; i < sizeof tries / sizeof tries[0] && err != ESP_OK; i++) {
        width = tries[i].width;
        err = try_mount(width, tries[i].khz, &card);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "%d-bit at %d kHz failed: %s", width, tries[i].khz, esp_err_to_name(err));
        }
    }
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "mount failed: %s (card must be FAT32)", esp_err_to_name(err));
        return false;
    }
    ESP_LOGI(TAG, "mounted %s, %llu MB, %d-bit SDMMC at %d kHz", card->cid.name,
             (unsigned long long)card->csd.capacity * card->csd.sector_size / (1024 * 1024),
             width, card->real_freq_khz);
    return true;
}
