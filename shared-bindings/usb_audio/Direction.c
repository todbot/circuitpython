// This file is part of the CircuitPython project: https://circuitpython.org
//
// SPDX-FileCopyrightText: Copyright (c) 2026 Tim Cocks Adafruit Industries LLC
//
// SPDX-License-Identifier: MIT

#include "py/obj.h"
#include "py/enum.h"
#include "py/runtime.h"

#include "shared-bindings/usb_audio/Direction.h"

MAKE_ENUM_VALUE(usb_audio_direction_type, direction, INPUT, USB_AUDIO_DIRECTION_INPUT);
MAKE_ENUM_VALUE(usb_audio_direction_type, direction, OUTPUT, USB_AUDIO_DIRECTION_OUTPUT);
MAKE_ENUM_VALUE(usb_audio_direction_type, direction, INPUT_OUTPUT, USB_AUDIO_DIRECTION_INPUT_OUTPUT);

//| class Direction:
//|     """The direction of a USB audio stream, relative to the host computer."""
//|
//|     def __init__(self) -> None:
//|         """Enum-like class to define the USB audio stream direction."""
//|         ...
//|
//|     INPUT: Direction
//|     """Audio flows board -> host: the board appears as a USB microphone
//|     (audio *source*). This is the default."""
//|
//|     OUTPUT: Direction
//|     """Audio flows host -> board: the board appears as a USB speaker
//|     (audio *sink*)."""
//|
//|     INPUT_OUTPUT: Direction
//|     """Audio flows in both directions: the board appears as a USB headset
//|     (microphone + speaker)."""
//|
//|
MAKE_ENUM_MAP(usb_audio_direction) {
    MAKE_ENUM_MAP_ENTRY(direction, INPUT),
    MAKE_ENUM_MAP_ENTRY(direction, OUTPUT),
    MAKE_ENUM_MAP_ENTRY(direction, INPUT_OUTPUT),
};

static MP_DEFINE_CONST_DICT(usb_audio_direction_locals_dict, usb_audio_direction_locals_table);

MAKE_PRINTER(usb_audio, usb_audio_direction);

MAKE_ENUM_TYPE(usb_audio, Direction, usb_audio_direction);

usb_audio_direction_t validate_direction(mp_obj_t obj, qstr arg_name) {
    return cp_enum_value(&usb_audio_direction_type, obj, arg_name);
}
