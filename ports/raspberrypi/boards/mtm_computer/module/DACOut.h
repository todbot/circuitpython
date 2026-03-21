// This file is part of the CircuitPython project: https://circuitpython.org
//
// SPDX-FileCopyrightText: Copyright (c) 2026 Tod Kurt
//
// SPDX-License-Identifier: MIT

#pragma once

#include "common-hal/microcontroller/Pin.h"
#include "common-hal/rp2pio/StateMachine.h"

#include "audio_dma.h"
#include "py/obj.h"

typedef struct {
    mp_obj_base_t base;
    rp2pio_statemachine_obj_t state_machine;
    audio_dma_t dma;
    bool playing;
} mtm_hardware_dacout_obj_t;

void common_hal_mtm_hardware_dacout_construct(mtm_hardware_dacout_obj_t *self,
    const mcu_pin_obj_t *clock, const mcu_pin_obj_t *mosi,
    const mcu_pin_obj_t *cs);

void common_hal_mtm_hardware_dacout_deinit(mtm_hardware_dacout_obj_t *self);
bool common_hal_mtm_hardware_dacout_deinited(mtm_hardware_dacout_obj_t *self);

void common_hal_mtm_hardware_dacout_play(mtm_hardware_dacout_obj_t *self,
    mp_obj_t sample, bool loop);
void common_hal_mtm_hardware_dacout_stop(mtm_hardware_dacout_obj_t *self);
bool common_hal_mtm_hardware_dacout_get_playing(mtm_hardware_dacout_obj_t *self);

void common_hal_mtm_hardware_dacout_pause(mtm_hardware_dacout_obj_t *self);
void common_hal_mtm_hardware_dacout_resume(mtm_hardware_dacout_obj_t *self);
bool common_hal_mtm_hardware_dacout_get_paused(mtm_hardware_dacout_obj_t *self);

extern const mp_obj_type_t mtm_hardware_dacout_type;
