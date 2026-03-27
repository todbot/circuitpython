// This file is part of the CircuitPython project: https://circuitpython.org
//
// SPDX-FileCopyrightText: Copyright (c) 2026 Tod Kurt
//
// SPDX-License-Identifier: MIT

#include <stdint.h>

#include "py/obj.h"
#include "py/runtime.h"

#include "shared-bindings/audiotools/SpeedChanger.h"

//| """Audio processing tools"""

static const mp_rom_map_elem_t audiotools_module_globals_table[] = {
    { MP_ROM_QSTR(MP_QSTR___name__), MP_ROM_QSTR(MP_QSTR_audiotools) },
    { MP_ROM_QSTR(MP_QSTR_SpeedChanger), MP_ROM_PTR(&audiotools_speedchanger_type) },
};

static MP_DEFINE_CONST_DICT(audiotools_module_globals, audiotools_module_globals_table);

const mp_obj_module_t audiotools_module = {
    .base = { &mp_type_module },
    .globals = (mp_obj_dict_t *)&audiotools_module_globals,
};

MP_REGISTER_MODULE(MP_QSTR_audiotools, audiotools_module);
