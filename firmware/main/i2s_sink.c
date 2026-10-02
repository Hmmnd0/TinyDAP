#include "i2s_sink.h"

#include <stdlib.h>

#include "driver/i2s_std.h"
#include "esp_log.h"

static const char *TAG = "i2s_sink";

typedef struct {
    audio_sink_t base;
    int port, bclk, ws, dout, mclk;
    i2s_chan_handle_t tx;
    audio_format_t fmt;
} i2s_sink_t;

static i2s_data_bit_width_t bit_width(uint8_t bits)
{
    switch (bits) {
    case 24: return I2S_DATA_BIT_WIDTH_24BIT;
    case 32: return I2S_DATA_BIT_WIDTH_32BIT;
    default: return I2S_DATA_BIT_WIDTH_16BIT;
    }
}

static int i2s_open(audio_sink_t *self, const audio_format_t *fmt)
{
    i2s_sink_t *s = self->ctx;

    if (s->tx) {
        if (fmt->bits_per_sample == s->fmt.bits_per_sample && fmt->channels == s->fmt.channels) {
            /* Sample-rate change only: retune the clock in place. */
            i2s_std_clk_config_t clk = I2S_STD_CLK_DEFAULT_CONFIG(fmt->sample_rate);
            i2s_channel_disable(s->tx);
            esp_err_t err = i2s_channel_reconfig_std_clock(s->tx, &clk);
            i2s_channel_enable(s->tx);
            s->fmt = *fmt;
            return err == ESP_OK ? 0 : -1;
        }
        self->close(self);
    }

    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(s->port, I2S_ROLE_MASTER);
    chan_cfg.auto_clear_after_cb = true;  /* send silence, not stale audio, on underrun */
    esp_err_t err = i2s_new_channel(&chan_cfg, &s->tx, NULL);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "new channel: %s", esp_err_to_name(err));
        return -1;
    }

    i2s_std_config_t std_cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(fmt->sample_rate),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(bit_width(fmt->bits_per_sample),
                                                        fmt->channels == 1 ? I2S_SLOT_MODE_MONO
                                                                           : I2S_SLOT_MODE_STEREO),
        .gpio_cfg = {
            .mclk = s->mclk < 0 ? I2S_GPIO_UNUSED : s->mclk,
            .bclk = s->bclk,
            .ws = s->ws,
            .dout = s->dout,
            .din = I2S_GPIO_UNUSED,
        },
    };
    err = i2s_channel_init_std_mode(s->tx, &std_cfg);
    if (err == ESP_OK) {
        err = i2s_channel_enable(s->tx);
    }
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "init: %s", esp_err_to_name(err));
        i2s_del_channel(s->tx);
        s->tx = NULL;
        return -1;
    }

    s->fmt = *fmt;
    ESP_LOGI(TAG, "I2S%d: %u Hz, %u-bit, %u ch", s->port, (unsigned)fmt->sample_rate,
             fmt->bits_per_sample, fmt->channels);
    return 0;
}

static int i2s_write(audio_sink_t *self, const void *data, size_t len, uint32_t timeout_ms)
{
    i2s_sink_t *s = self->ctx;
    size_t written = 0;
    esp_err_t err = i2s_channel_write(s->tx, data, len, &written, timeout_ms);
    if (err != ESP_OK && err != ESP_ERR_TIMEOUT) {
        return -1;
    }
    return (int)written;
}

static void i2s_close(audio_sink_t *self)
{
    i2s_sink_t *s = self->ctx;
    if (s->tx) {
        i2s_channel_disable(s->tx);
        i2s_del_channel(s->tx);
        s->tx = NULL;
    }
}

audio_sink_t *i2s_sink_create(int port, int bclk, int ws, int dout, int mclk)
{
    i2s_sink_t *s = calloc(1, sizeof *s);
    if (!s) {
        return NULL;
    }
    s->port = port;
    s->bclk = bclk;
    s->ws = ws;
    s->dout = dout;
    s->mclk = mclk;
    s->base.open = i2s_open;
    s->base.write = i2s_write;
    s->base.close = i2s_close;
    s->base.ctx = s;
    return &s->base;
}
