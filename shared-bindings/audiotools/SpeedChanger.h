// This file is part of the CircuitPython project: https://circuitpython.org
//
// SPDX-FileCopyrightText: Copyright (c) 2026 Tod Kurt
//
// SPDX-License-Identifier: MIT

#pragma once

#include "shared-module/audiotools/SpeedChanger.h"

extern const mp_obj_type_t audiotools_speedchanger_type;

void common_hal_audiotools_speedchanger_construct(audiotools_speedchanger_obj_t *self,
    mp_obj_t source, uint32_t rate_fp);
void common_hal_audiotools_speedchanger_deinit(audiotools_speedchanger_obj_t *self);
void common_hal_audiotools_speedchanger_set_rate(audiotools_speedchanger_obj_t *self, uint32_t rate_fp);
uint32_t common_hal_audiotools_speedchanger_get_rate(audiotools_speedchanger_obj_t *self);
