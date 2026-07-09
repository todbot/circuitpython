// This file is part of the CircuitPython project: https://circuitpython.org
//
// SPDX-FileCopyrightText: Copyright (c) 2026 Adafruit Industries
//
// SPDX-License-Identifier: MIT

#include <stdint.h>

#include "py/obj.h"
#include "py/runtime.h"

#include "shared-bindings/audiowriter/__init__.h"
#include "shared-bindings/audiowriter/AudioWriter.h"

//| """Support for streaming audio to a WAV file
//|
//| The `audiowriter` module contains `AudioWriter`, a *sink* that records an
//| audio source (a microphone or an effect chain) to a ``.wav`` file in the
//| background -- the inverse of `audiocore.WaveFile`.
//|
//| """

static const mp_rom_map_elem_t audiowriter_module_globals_table[] = {
    { MP_ROM_QSTR(MP_QSTR___name__), MP_ROM_QSTR(MP_QSTR_audiowriter) },
    { MP_ROM_QSTR(MP_QSTR_AudioWriter), MP_ROM_PTR(&audiowriter_audiowriter_type) },
};

static MP_DEFINE_CONST_DICT(audiowriter_module_globals, audiowriter_module_globals_table);

const mp_obj_module_t audiowriter_module = {
    .base = { &mp_type_module },
    .globals = (mp_obj_dict_t *)&audiowriter_module_globals,
};

MP_REGISTER_MODULE(MP_QSTR_audiowriter, audiowriter_module);
