// This file is part of the CircuitPython project: https://circuitpython.org
//
// SPDX-FileCopyrightText: Copyright (c) 2026 Tod Kurt
//
// SPDX-License-Identifier: MIT

#pragma once

#include "shared-module/audiocore/SpeedChanger.h"

extern const mp_obj_type_t audiocore_speedchanger_type;

void common_hal_audiocore_speedchanger_construct(audiocore_speedchanger_obj_t *self,
    mp_obj_t source, uint32_t rate_fp);
void common_hal_audiocore_speedchanger_deinit(audiocore_speedchanger_obj_t *self);
void common_hal_audiocore_speedchanger_set_rate(audiocore_speedchanger_obj_t *self, uint32_t rate_fp);
uint32_t common_hal_audiocore_speedchanger_get_rate(audiocore_speedchanger_obj_t *self);
