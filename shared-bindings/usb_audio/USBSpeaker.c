// This file is part of the CircuitPython project: https://circuitpython.org
//
// SPDX-FileCopyrightText: Copyright (c) 2026 Tim Cocks Adafruit Industries LLC
//
// SPDX-License-Identifier: MIT

#include <stdint.h>

#include "shared/runtime/context_manager_helpers.h"
#include "py/objproperty.h"
#include "py/runtime.h"
#include "shared-bindings/usb_audio/USBSpeaker.h"
#include "shared-bindings/audiocore/__init__.h"
#include "shared-bindings/util.h"
#include "shared-module/usb_audio/__init__.h"

//| class USBSpeaker:
//|     """Plays audio streamed from the host computer as a USB Audio Class speaker.
//|
//|     A ``USBSpeaker`` is a *source* of audio samples, exactly like
//|     `audiocore.RawSample` or `audiocore.WaveFile`. The host PC streams audio to
//|     the board over USB, and the ``USBSpeaker`` hands that audio to a consumer
//|     such as `audiobusio.I2SOut`, `audiopwmio.PWMAudioOut` or `audioio.AudioOut`
//|     (optionally through the effect modules), so the board appears as a speaker.
//|
//|     ``usb_audio.enable(direction=usb_audio.Direction.OUTPUT)`` must have been
//|     called in ``boot.py`` before this object can be constructed.
//|
//|     .. code-block:: py
//|
//|         # boot.py
//|         import usb_audio
//|         usb_audio.enable(sample_rate=16000, channel_count=1, bits_per_sample=16,
//|                          direction=usb_audio.Direction.OUTPUT)
//|
//|     .. code-block:: py
//|
//|         # code.py
//|         import board
//|         import usb_audio
//|         import audiobusio
//|
//|         spk = usb_audio.USBSpeaker()
//|         out = audiobusio.I2SOut(board.I2S_BIT_CLOCK, board.I2S_WORD_SELECT, board.I2S_DATA)
//|         out.play(spk, loop=True)
//|
//|     """
//|
//|     def __init__(self) -> None:
//|         """Create a USBSpeaker using the audio format configured in ``boot.py``."""
//|         ...
//|
static mp_obj_t usb_audio_usbspeaker_make_new(const mp_obj_type_t *type, size_t n_args, size_t n_kw, const mp_obj_t *all_args) {
    static const mp_arg_t allowed_args[] = {};
    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all_kw_array(n_args, n_kw, all_args, MP_ARRAY_SIZE(allowed_args), allowed_args, args);

    // The audio format and USB interface are claimed by usb_audio.enable() in
    // boot.py. Without it there is no speaker for the host to play to.
    if (!usb_audio_enabled()) {
        mp_raise_RuntimeError(MP_ERROR_TEXT("USB audio not enabled in boot.py"));
    }
    // A USBSpeaker only makes sense when the OUT endpoint is enumerated.
    if (usb_audio_direction != USB_AUDIO_DIRECTION_OUTPUT &&
        usb_audio_direction != USB_AUDIO_DIRECTION_INPUT_OUTPUT) {
        mp_raise_RuntimeError(MP_ERROR_TEXT("USB audio not enabled for output in boot.py"));
    }

    usb_audio_usbspeaker_obj_t *self = mp_obj_malloc_with_finaliser(usb_audio_usbspeaker_obj_t, &usb_audio_USBSpeaker_type);
    common_hal_usb_audio_usbspeaker_construct(self);

    return MP_OBJ_FROM_PTR(self);
}

//|     def deinit(self) -> None:
//|         """Deinitialises the USBSpeaker and releases any resources for reuse."""
//|         ...
//|
static mp_obj_t usb_audio_usbspeaker_deinit(mp_obj_t self_in) {
    usb_audio_usbspeaker_obj_t *self = MP_OBJ_TO_PTR(self_in);
    common_hal_usb_audio_usbspeaker_deinit(self);
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_1(usb_audio_usbspeaker_deinit_obj, usb_audio_usbspeaker_deinit);

static void check_for_deinit(usb_audio_usbspeaker_obj_t *self) {
    audiosample_check_for_deinit(&self->base);
}

//|     def read(self, destination: circuitpython_typing.WriteableBuffer, destination_length: int) -> int:
//|         """Copies up to ``destination_length`` of the most recent samples streamed
//|         from the host into ``destination``, for analysis such as an audio-reactive
//|         effect or VU meter. This does not block: it returns whatever the host has
//|         delivered so far, which may be fewer than ``destination_length`` samples
//|         (or zero when the host is not streaming).
//|
//|         ``destination`` must be an ``array.array`` of 16-bit signed samples
//|         (typecode ``"h"``), matching the negotiated speaker format.
//|
//|         Reading consumes the samples, so a ``USBSpeaker`` being read this way
//|         should not also be passed to an output backend's ``play()`` at the same
//|         time.
//|
//|         :return: The number of samples copied into ``destination``."""
//|         ...
//|
static mp_obj_t usb_audio_usbspeaker_obj_read(mp_obj_t self_in, mp_obj_t destination, mp_obj_t destination_length) {
    usb_audio_usbspeaker_obj_t *self = MP_OBJ_TO_PTR(self_in);
    check_for_deinit(self);
    uint32_t length = mp_arg_validate_type_int(destination_length, MP_QSTR_length);

    mp_buffer_info_t bufinfo;
    mp_get_buffer_raise(destination, &bufinfo, MP_BUFFER_WRITE);
    // The negotiated speaker format is 16-bit signed PCM, so require a matching
    // 'h' array. This keeps the copy a straight memcpy with no conversion.
    if (bufinfo.typecode != 'h') {
        mp_raise_TypeError(MP_ERROR_TEXT("destination must be an array of type 'h'"));
    }
    size_t capacity = bufinfo.len / sizeof(int16_t);
    if (capacity < length) {
        length = capacity;
    }

    uint32_t length_read = common_hal_usb_audio_usbspeaker_read(self, bufinfo.buf, length);
    return MP_OBJ_NEW_SMALL_INT(length_read);
}
static MP_DEFINE_CONST_FUN_OBJ_3(usb_audio_usbspeaker_read_obj, usb_audio_usbspeaker_obj_read);

//|     def __enter__(self) -> USBSpeaker:
//|         """No-op used by Context Managers."""
//|         ...
//|
//  Provided by context manager helper.

//|     def __exit__(self) -> None:
//|         """Automatically deinitializes the hardware when exiting a context. See
//|         :ref:`lifetime-and-contextmanagers` for more info."""
//|         ...
//|
//  Provided by context manager helper.

//|     connected: bool
//|     """True while the host is streaming audio to this speaker. (read-only)"""
//|
//|
static mp_obj_t usb_audio_usbspeaker_obj_get_connected(mp_obj_t self_in) {
    usb_audio_usbspeaker_obj_t *self = MP_OBJ_TO_PTR(self_in);
    check_for_deinit(self);
    return mp_obj_new_bool(common_hal_usb_audio_usbspeaker_get_connected(self));
}
MP_DEFINE_CONST_FUN_OBJ_1(usb_audio_usbspeaker_get_connected_obj, usb_audio_usbspeaker_obj_get_connected);

MP_PROPERTY_GETTER(usb_audio_usbspeaker_connected_obj,
    (mp_obj_t)&usb_audio_usbspeaker_get_connected_obj);

//|     sample_rate: int
//|     """The sample rate negotiated with the host in ``boot.py``. (read-only)"""
//|
//|     bits_per_sample: int
//|     """The bit depth negotiated with the host in ``boot.py``. (read-only)"""
//|
//|     channel_count: int
//|     """The number of channels negotiated with the host in ``boot.py``. (read-only)"""
//|
//|
static const mp_rom_map_elem_t usb_audio_usbspeaker_locals_dict_table[] = {
    // Methods
    { MP_ROM_QSTR(MP_QSTR___del__), MP_ROM_PTR(&usb_audio_usbspeaker_deinit_obj) },
    { MP_ROM_QSTR(MP_QSTR_deinit), MP_ROM_PTR(&usb_audio_usbspeaker_deinit_obj) },
    { MP_ROM_QSTR(MP_QSTR_read), MP_ROM_PTR(&usb_audio_usbspeaker_read_obj) },
    { MP_ROM_QSTR(MP_QSTR___enter__), MP_ROM_PTR(&default___enter___obj) },
    { MP_ROM_QSTR(MP_QSTR___exit__), MP_ROM_PTR(&default___exit___obj) },

    // Properties
    { MP_ROM_QSTR(MP_QSTR_connected), MP_ROM_PTR(&usb_audio_usbspeaker_connected_obj) },
    AUDIOSAMPLE_FIELDS,
};
static MP_DEFINE_CONST_DICT(usb_audio_usbspeaker_locals_dict, usb_audio_usbspeaker_locals_dict_table);

static const audiosample_p_t usb_audio_usbspeaker_proto = {
    MP_PROTO_IMPLEMENT(MP_QSTR_protocol_audiosample)
    .reset_buffer = (audiosample_reset_buffer_fun)usb_audio_usbspeaker_reset_buffer,
    .get_buffer = (audiosample_get_buffer_fun)usb_audio_usbspeaker_get_buffer,
};

MP_DEFINE_CONST_OBJ_TYPE(
    usb_audio_USBSpeaker_type,
    MP_QSTR_USBSpeaker,
    MP_TYPE_FLAG_HAS_SPECIAL_ACCESSORS,
    make_new, usb_audio_usbspeaker_make_new,
    locals_dict, &usb_audio_usbspeaker_locals_dict,
    protocol, &usb_audio_usbspeaker_proto
    );
