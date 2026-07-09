// This file is part of the CircuitPython project: https://circuitpython.org
//
// SPDX-FileCopyrightText: Copyright (c) 2026 Adafruit Industries
//
// SPDX-License-Identifier: MIT

#pragma once

#include "py/obj.h"

#include "shared-module/audiowriter/AudioWriter.h"

extern const mp_obj_type_t audiowriter_audiowriter_type;

void common_hal_audiowriter_audiowriter_construct(audiowriter_audiowriter_obj_t *self,
    mp_obj_t file, uint32_t buffer_size);

void common_hal_audiowriter_audiowriter_deinit(audiowriter_audiowriter_obj_t *self);
bool common_hal_audiowriter_audiowriter_deinited(audiowriter_audiowriter_obj_t *self);

void common_hal_audiowriter_audiowriter_play(audiowriter_audiowriter_obj_t *self, mp_obj_t sample);
void common_hal_audiowriter_audiowriter_stop(audiowriter_audiowriter_obj_t *self);
bool common_hal_audiowriter_audiowriter_get_playing(audiowriter_audiowriter_obj_t *self);
