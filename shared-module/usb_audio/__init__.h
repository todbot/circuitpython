// This file is part of the CircuitPython project: https://circuitpython.org
//
// SPDX-FileCopyrightText: Copyright (c) 2026 Tim Cocks for Adafruit Industries LLC
//
// SPDX-License-Identifier: MIT

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "py/obj.h"
#include "supervisor/usb.h"

#include "shared-bindings/usb_audio/Direction.h"

// Enable/disable the USB Audio Class (UAC2) interface. These may only be
// called before USB is connected (i.e. from boot.py); they return false
// otherwise.
bool shared_module_usb_audio_enable(mp_int_t sample_rate, mp_int_t channel_count, mp_int_t bits_per_sample, usb_audio_direction_t direction);
bool shared_module_usb_audio_disable(void);

// True once enable() has been called successfully.
bool usb_audio_enabled(void);

// True while the host has opened the AudioStreaming alternate setting, i.e. it is
// actively listening. This is the real "stream the audio now" signal.
bool usb_audio_streaming(void);

// Negotiated audio format, valid when usb_audio_enabled() is true.
extern uint32_t usb_audio_sample_rate;
extern uint8_t usb_audio_channel_count;
extern uint8_t usb_audio_bits_per_sample;

// Stream direction requested in enable(), valid when usb_audio_enabled() is true.
extern usb_audio_direction_t usb_audio_direction;

// Descriptor injection hooks, called from supervisor/shared/usb/usb_desc.c.
size_t usb_audio_descriptor_length(void);
size_t usb_audio_add_descriptor(uint8_t *descriptor_buf, descriptor_counts_t *descriptor_counts, uint8_t *current_interface_string);

// Background task that streams samples to the host, called from
// supervisor/shared/usb/usb.c.
void usb_audio_task(void);
