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

// Endpoint number for the single isochronous audio data endpoint. Most device
// controllers accept an ISO endpoint on any number, so the descriptor builder
// just takes the next sequential one (signalled by 0 here). The nRF52 USBD,
// however, implements isochronous transfers only on a fixed, dedicated endpoint
// number (8): its dcd_edpt_open() rejects any other number for an ISO endpoint,
// so the stream silently never opens and the host sees a mic/speaker that
// transfers no data. Such ports override this (e.g. -DUSB_AUDIO_ISO_EP_NUM=8 in
// the port's mpconfigport.mk) to force the audio endpoint onto that number.
#ifndef USB_AUDIO_ISO_EP_NUM
#define USB_AUDIO_ISO_EP_NUM (0)
#endif

// Fixed UAC2 entity IDs baked into TUD_AUDIO_MIC_ONE_CH_DESCRIPTOR and the
// hand-rolled speaker descriptor (USB_AUDIO_SPEAKER_DESCRIPTOR in __init__.c).
// The speaker reuses the same IDs as the mic; only the terminal roles reverse
// (input terminal = USB streaming, output terminal = desktop speaker).
#define USB_AUDIO_ENTITY_INPUT_TERMINAL (0x01)
#define USB_AUDIO_ENTITY_FEATURE_UNIT (0x02)
#define USB_AUDIO_ENTITY_OUTPUT_TERMINAL (0x03)
#define USB_AUDIO_ENTITY_CLOCK_SOURCE (0x04)

// Length of the no-feedback mono speaker descriptor. It uses the same set of
// sub-descriptors as TUD_AUDIO_MIC_ONE_CH_DESCRIPTOR (one isochronous data
// endpoint, no feedback endpoint), so this is identical to
// TUD_AUDIO_MIC_ONE_CH_DESC_LEN -- but spell it out independently so the two
// can diverge later (e.g. stereo) without silently mis-sizing the descriptor.
// These TUD_AUDIO_DESC_*_LEN macros come from TinyUSB's usbd.h; this expression
// is only expanded where that header is already included (never at the point
// tusb_config.h includes us), so the header stays dependency-free.
#define USB_AUDIO_SPEAKER_DESC_LEN (TUD_AUDIO_DESC_IAD_LEN \
    + TUD_AUDIO_DESC_STD_AC_LEN \
    + TUD_AUDIO_DESC_CS_AC_LEN \
    + TUD_AUDIO_DESC_CLK_SRC_LEN \
    + TUD_AUDIO_DESC_INPUT_TERM_LEN \
    + TUD_AUDIO_DESC_OUTPUT_TERM_LEN \
    + TUD_AUDIO_DESC_FEATURE_UNIT_ONE_CHANNEL_LEN \
    + TUD_AUDIO_DESC_STD_AS_INT_LEN \
    + TUD_AUDIO_DESC_STD_AS_INT_LEN \
    + TUD_AUDIO_DESC_CS_AS_INT_LEN \
    + TUD_AUDIO_DESC_TYPE_I_FORMAT_LEN \
    + TUD_AUDIO_DESC_STD_AS_ISO_EP_LEN \
    + TUD_AUDIO_DESC_CS_AS_ISO_EP_LEN)
