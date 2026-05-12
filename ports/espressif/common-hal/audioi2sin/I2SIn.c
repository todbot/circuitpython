// This file is part of the CircuitPython project: https://circuitpython.org
//
// SPDX-FileCopyrightText: Copyright (c) 2026 Tim Cocks for Adafruit Industries
//
// SPDX-License-Identifier: MIT

#include <string.h>

#include "bindings/espidf/__init__.h"

#include "common-hal/audioi2sin/I2SIn.h"
#include "py/runtime.h"
#include "shared-bindings/audioi2sin/I2SIn.h"
#include "shared-bindings/microcontroller/Pin.h"

#include "driver/i2s_std.h"

#if CIRCUITPY_AUDIOI2SIN

void common_hal_audioi2sin_i2sin_construct(audioi2sin_i2sin_obj_t *self,
    const mcu_pin_obj_t *bit_clock, const mcu_pin_obj_t *word_select,
    const mcu_pin_obj_t *data, const mcu_pin_obj_t *main_clock,
    uint32_t sample_rate, uint8_t bit_depth, bool mono, bool left_justified) {

    if (bit_depth != 8 && bit_depth != 16 && bit_depth != 24 && bit_depth != 32) {
        mp_raise_ValueError(MP_ERROR_TEXT("bit_depth must be 8, 16, 24, or 32."));
    }

    i2s_data_bit_width_t bit_width = (i2s_data_bit_width_t)bit_depth;

    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_AUTO, I2S_ROLE_MASTER);
    esp_err_t err = i2s_new_channel(&chan_cfg, NULL, &self->rx_chan);
    if (err == ESP_ERR_NOT_FOUND) {
        mp_raise_RuntimeError(MP_ERROR_TEXT("Peripheral in use"));
    }
    CHECK_ESP_RESULT(err);

    // Always configure the bus as stereo. The newer-family I2S peripherals
    // (S2/S3/C-series) ignore I2S_SLOT_MODE_MONO on RX and write both slots
    // into the DMA buffer regardless, which yields buffers that fill at 2x
    // the WS rate and produces half-speed audio. By configuring stereo and
    // dropping one slot ourselves in record_to_buffer, behavior is uniform
    // across chips.
    i2s_std_slot_config_t slot_cfg = left_justified
        ? (i2s_std_slot_config_t)I2S_STD_MSB_SLOT_DEFAULT_CONFIG(bit_width, I2S_SLOT_MODE_STEREO)
        : (i2s_std_slot_config_t)I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(bit_width, I2S_SLOT_MODE_STEREO);

    i2s_std_config_t std_cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(sample_rate),
        .slot_cfg = slot_cfg,
        .gpio_cfg = {
            .mclk = main_clock != NULL ? main_clock->number : I2S_GPIO_UNUSED,
            .bclk = bit_clock->number,
            .ws = word_select->number,
            .dout = I2S_GPIO_UNUSED,
            .din = data->number,
        },
    };
    CHECK_ESP_RESULT(i2s_channel_init_std_mode(self->rx_chan, &std_cfg));
    CHECK_ESP_RESULT(i2s_channel_enable(self->rx_chan));

    self->bit_clock = bit_clock;
    self->word_select = word_select;
    self->data = data;
    self->mclk = main_clock;
    self->sample_rate = sample_rate;
    self->bit_depth = bit_depth;
    self->mono = mono;

    claim_pin(bit_clock);
    claim_pin(word_select);
    claim_pin(data);
    if (main_clock) {
        claim_pin(main_clock);
    }
}

bool common_hal_audioi2sin_i2sin_deinited(audioi2sin_i2sin_obj_t *self) {
    return self->data == NULL;
}

void common_hal_audioi2sin_i2sin_deinit(audioi2sin_i2sin_obj_t *self) {
    if (common_hal_audioi2sin_i2sin_deinited(self)) {
        return;
    }

    if (self->rx_chan) {
        i2s_channel_disable(self->rx_chan);
        i2s_del_channel(self->rx_chan);
        self->rx_chan = NULL;
    }

    if (self->bit_clock) {
        reset_pin_number(self->bit_clock->number);
    }
    self->bit_clock = NULL;

    if (self->word_select) {
        reset_pin_number(self->word_select->number);
    }
    self->word_select = NULL;

    if (self->data) {
        reset_pin_number(self->data->number);
    }
    self->data = NULL;

    if (self->mclk) {
        reset_pin_number(self->mclk->number);
    }
    self->mclk = NULL;
}

uint32_t common_hal_audioi2sin_i2sin_record_to_buffer(audioi2sin_i2sin_obj_t *self,
    void *buffer, uint32_t length) {
    size_t element_size = self->bit_depth / 8;
    // 24-bit samples occupy a 32-bit slot on the I2S bus.
    if (self->bit_depth == 24) {
        element_size = 4;
    }

    if (!self->mono) {
        size_t result = 0;
        esp_err_t err = i2s_channel_read(self->rx_chan, buffer, length * element_size,
            &result, portMAX_DELAY);
        CHECK_ESP_RESULT(err);
        return result / element_size;
    }

    // Mono: bus is configured stereo, so each WS frame yields two slots in
    // the DMA buffer. Read in chunks into a scratch and keep only the left
    // slot of each frame.
    uint8_t scratch[256];
    const size_t frame_bytes = 2 * element_size;
    const size_t scratch_frames = sizeof(scratch) / frame_bytes;
    uint8_t *out = (uint8_t *)buffer;
    uint32_t produced = 0;
    while (produced < length) {
        size_t want_frames = length - produced;
        if (want_frames > scratch_frames) {
            want_frames = scratch_frames;
        }
        size_t got_bytes = 0;
        esp_err_t err = i2s_channel_read(self->rx_chan, scratch,
            want_frames * frame_bytes, &got_bytes, portMAX_DELAY);
        CHECK_ESP_RESULT(err);
        size_t got_frames = got_bytes / frame_bytes;
        for (size_t i = 0; i < got_frames; i++) {
            memcpy(out + produced * element_size,
                scratch + i * frame_bytes,
                element_size);
            produced++;
        }
        if (got_frames < want_frames) {
            break;
        }
    }
    return produced;
}

uint8_t common_hal_audioi2sin_i2sin_get_bit_depth(audioi2sin_i2sin_obj_t *self) {
    return self->bit_depth;
}

uint32_t common_hal_audioi2sin_i2sin_get_sample_rate(audioi2sin_i2sin_obj_t *self) {
    return self->sample_rate;
}

#endif // CIRCUITPY_AUDIOI2SIN
