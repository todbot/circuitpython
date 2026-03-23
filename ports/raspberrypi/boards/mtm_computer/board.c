// This file is part of the CircuitPython project: https://circuitpython.org
//
// SPDX-FileCopyrightText: Copyright (c) 2021 Scott Shawcroft for Adafruit Industries
//
// SPDX-License-Identifier: MIT

#include "supervisor/board.h"
#include "shared-bindings/mcp4822/MCP4822.h"
#include "shared-bindings/microcontroller/Pin.h"
#include "peripherals/pins.h"
#include "py/runtime.h"

// board.DAC() — factory function that constructs an mcp4822.MCP4822 with
// the MTM Workshop Computer's DAC pins (GP18=SCK, GP19=SDI, GP21=CS).
static mp_obj_t board_dac_factory(void) {
    mcp4822_mcp4822_obj_t *dac = mp_obj_malloc_with_finaliser(
        mcp4822_mcp4822_obj_t, &mcp4822_mcp4822_type);
    common_hal_mcp4822_mcp4822_construct(
        dac,
        &pin_GPIO18,   // clock (SCK)
        &pin_GPIO19,   // mosi (SDI)
        &pin_GPIO21,   // cs
        2);            // gain 2x
    return MP_OBJ_FROM_PTR(dac);
}
MP_DEFINE_CONST_FUN_OBJ_0(board_dac_obj, board_dac_factory);

// Use the MP_WEAK supervisor/shared/board.c versions of routines not defined here.
