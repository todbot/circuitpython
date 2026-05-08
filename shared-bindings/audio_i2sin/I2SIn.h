// This file is part of the CircuitPython project: https://circuitpython.org
//
// SPDX-FileCopyrightText: Copyright (c) 2026 Tim Cocks for Adafruit Industries
//
// SPDX-License-Identifier: MIT

#pragma once

#include "shared-bindings/microcontroller/Pin.h"

#if CIRCUITPY_AUDIO_I2SIN
#include "common-hal/audio_i2sin/I2SIn.h"
#endif

extern const mp_obj_type_t audio_i2sin_i2sin_type;

#if CIRCUITPY_AUDIO_I2SIN
void common_hal_audio_i2sin_i2sin_construct(audio_i2sin_i2sin_obj_t *self,
    const mcu_pin_obj_t *bit_clock, const mcu_pin_obj_t *word_select,
    const mcu_pin_obj_t *data, const mcu_pin_obj_t *main_clock,
    uint32_t sample_rate, uint8_t bit_depth, bool mono, bool left_justified);
void common_hal_audio_i2sin_i2sin_deinit(audio_i2sin_i2sin_obj_t *self);
bool common_hal_audio_i2sin_i2sin_deinited(audio_i2sin_i2sin_obj_t *self);
uint32_t common_hal_audio_i2sin_i2sin_record_to_buffer(audio_i2sin_i2sin_obj_t *self,
    void *buffer, uint32_t length);
uint8_t common_hal_audio_i2sin_i2sin_get_bit_depth(audio_i2sin_i2sin_obj_t *self);
uint32_t common_hal_audio_i2sin_i2sin_get_sample_rate(audio_i2sin_i2sin_obj_t *self);
#endif
