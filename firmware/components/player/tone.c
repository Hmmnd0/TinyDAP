#include "tinydap/tone.h"

#include <math.h>

#define TWO_PI 6.28318530717958647692f

void tone_init(tone_t *t, uint32_t sample_rate, float freq_hz, float amplitude)
{
    t->phase = 0.0f;
    t->step = TWO_PI * freq_hz / (float)sample_rate;
    t->amp = amplitude;
}

void tone_fill_s16_stereo(tone_t *t, int16_t *dst, size_t frames)
{
    for (size_t i = 0; i < frames; i++) {
        int16_t s = (int16_t)(sinf(t->phase) * t->amp * 32767.0f);
        dst[2 * i] = s;
        dst[2 * i + 1] = s;
        t->phase += t->step;
        if (t->phase >= TWO_PI) {
            t->phase -= TWO_PI;
        }
    }
}
