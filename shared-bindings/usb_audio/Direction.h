// This file is part of the CircuitPython project: https://circuitpython.org
//
// SPDX-FileCopyrightText: Copyright (c) 2026 Tim Cocks Adafruit Industries LLC
//
// SPDX-License-Identifier: MIT

#pragma once

#include "py/enum.h"
#include "py/obj.h"

typedef enum _usb_audio_direction_t {
    USB_AUDIO_DIRECTION_INPUT,
    USB_AUDIO_DIRECTION_OUTPUT,
    USB_AUDIO_DIRECTION_INPUT_OUTPUT,
} usb_audio_direction_t;

extern const mp_obj_type_t usb_audio_direction_type;

usb_audio_direction_t validate_direction(mp_obj_t obj, qstr arg_name);
