// This file is part of the CircuitPython project: https://circuitpython.org
//
// SPDX-FileCopyrightText: Copyright (c) 2026 Tim Cocks for Adafruit Industries LLC
//
// SPDX-License-Identifier: MIT

#pragma once

// Fixed audio format for the UAC2 microphone profile. These must have no other
// dependencies because this header is included from the TinyUSB tusb_config.h
// (to size the IN endpoint) as well as from the descriptor/binding code.

// The isochronous IN endpoint's wMaxPacketSize in the USB descriptor is computed
// for this rate, so it is the highest rate usb_audio.enable() will accept.
#define USB_AUDIO_MAX_SAMPLE_RATE (48000)

// 16-bit signed LE PCM, mono.
#define USB_AUDIO_N_BYTES_PER_SAMPLE (2)
#define USB_AUDIO_N_CHANNELS (1)
#define USB_AUDIO_BITS_PER_SAMPLE (USB_AUDIO_N_BYTES_PER_SAMPLE * 8)

// Fixed UAC2 entity IDs baked into TUD_AUDIO_MIC_ONE_CH_DESCRIPTOR.
#define USB_AUDIO_ENTITY_INPUT_TERMINAL (0x01)
#define USB_AUDIO_ENTITY_FEATURE_UNIT (0x02)
#define USB_AUDIO_ENTITY_OUTPUT_TERMINAL (0x03)
#define USB_AUDIO_ENTITY_CLOCK_SOURCE (0x04)
