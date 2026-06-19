// This file is part of the CircuitPython project: https://circuitpython.org
//
// SPDX-FileCopyrightText: Copyright (c) 2026 Tim Cocks for Adafruit Industries LLC
//
// SPDX-License-Identifier: MIT

#include "py/obj.h"
#include "py/runtime.h"

#include "shared-bindings/usb_audio/__init__.h"
#include "shared-bindings/usb_audio/USBMicrophone.h"
#include "shared-module/usb_audio/__init__.h"
#include "shared-module/usb_audio/usb_audio_descriptors.h"

//| """Stream audio to a host computer via USB
//|
//| This makes your CircuitPython device identify to the host computer as a USB
//| Audio Class (UAC2) microphone: the board is the audio *source* and streams
//| samples to the host over a USB isochronous IN endpoint.
//|
//| This mode requires 1 IN endpoint and 2 interfaces. Generally, microcontrollers
//| have a limit on the number of endpoints. If you exceed the number of endpoints,
//| CircuitPython will automatically enter Safe Mode. Even in this case, you may be
//| able to enable USB audio by also disabling other USB functions, such as
//| `usb_hid` or `usb_midi`.
//|
//| To enable this mode, you must configure the audio format in ``boot.py`` and then
//| create a `USBMicrophone` in ``code.py``.
//|
//| .. code-block:: py
//|
//|     # boot.py
//|     import usb_audio
//|     usb_audio.enable(sample_rate=16000, channel_count=1, bits_per_sample=16)
//|
//| .. code-block:: py
//|
//|     # code.py
//|     import usb_audio
//|     import synthio
//|
//|     mic = usb_audio.USBMicrophone()
//|     synth = synthio.Synthesizer(sample_rate=16000, channel_count=1)
//|     mic.play(synth, loop=True)
//|     synth.press(60)
//|
//| """
//|
//|

//| def enable(
//|     sample_rate: int = 16000, channel_count: int = 1, bits_per_sample: int = 16
//| ) -> None:
//|     """Enable the USB audio microphone interface with the given PCM format.
//|
//|     This function may only be used from ``boot.py``.
//|
//|     :param int sample_rate: Samples per second of the streamed audio.
//|     :param int channel_count: Number of channels. Only mono (1) is supported initially.
//|     :param int bits_per_sample: Bits per signed PCM sample. Only 16 is supported initially."""
//|
//|
static mp_obj_t usb_audio_enable(size_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {
    enum { ARG_sample_rate, ARG_channel_count, ARG_bits_per_sample };
    static const mp_arg_t allowed_args[] = {
        { MP_QSTR_sample_rate, MP_ARG_INT, { .u_int = 16000 } },
        { MP_QSTR_channel_count, MP_ARG_INT, { .u_int = 1 } },
        { MP_QSTR_bits_per_sample, MP_ARG_INT, { .u_int = 16 } },
    };
    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all(n_args, pos_args, kw_args, MP_ARRAY_SIZE(allowed_args), allowed_args, args);

    mp_int_t sample_rate = mp_arg_validate_int_range(args[ARG_sample_rate].u_int, 1, USB_AUDIO_MAX_SAMPLE_RATE, MP_QSTR_sample_rate);
    mp_int_t channel_count = mp_arg_validate_int_range(args[ARG_channel_count].u_int, 1, USB_AUDIO_N_CHANNELS, MP_QSTR_channel_count);
    mp_int_t bits_per_sample = mp_arg_validate_int(args[ARG_bits_per_sample].u_int, USB_AUDIO_BITS_PER_SAMPLE, MP_QSTR_bits_per_sample);

    if (!shared_module_usb_audio_enable(sample_rate, channel_count, bits_per_sample)) {
        mp_raise_RuntimeError(MP_ERROR_TEXT("Cannot change USB devices now"));
    }

    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_KW(usb_audio_enable_obj, 0, usb_audio_enable);

static const mp_rom_map_elem_t usb_audio_module_globals_table[] = {
    { MP_ROM_QSTR(MP_QSTR___name__), MP_ROM_QSTR(MP_QSTR_usb_audio) },
    { MP_ROM_QSTR(MP_QSTR_USBMicrophone), MP_ROM_PTR(&usb_audio_USBMicrophone_type) },
    { MP_ROM_QSTR(MP_QSTR_enable), MP_ROM_PTR(&usb_audio_enable_obj) },
};

static MP_DEFINE_CONST_DICT(usb_audio_module_globals, usb_audio_module_globals_table);

const mp_obj_module_t usb_audio_module = {
    .base = { &mp_type_module },
    .globals = (mp_obj_dict_t *)&usb_audio_module_globals,
};

MP_REGISTER_MODULE(MP_QSTR_usb_audio, usb_audio_module);
