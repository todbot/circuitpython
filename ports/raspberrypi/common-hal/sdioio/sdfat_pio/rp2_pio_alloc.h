// This file is part of the CircuitPython project: https://circuitpython.org
//
// SPDX-FileCopyrightText: Copyright (c) 2026 Tim Cocks
//
// SPDX-License-Identifier: MIT

// Bridge declarations that let the vendored (C++) PioSdioCard driver cooperate
// with CircuitPython's rp2pio PIO allocator instead of seizing whole PIO blocks
// through the raw SDK. The definitions live in
// common-hal/rp2pio/StateMachine.c (compiled as C), so they are declared with C
// linkage here. This header deliberately does NOT include the full rp2pio
// StateMachine.h, which pulls in MicroPython object headers that are awkward in
// this C++ translation unit.

#pragma once

#include "hardware/pio.h"

#ifdef __cplusplus
extern "C" {
#endif

// See common-hal/rp2pio/StateMachine.c. Returns a PIO index (0..NUM_PIOS-1) with
// room for program_size instructions and sm_count free state machines, or
// NUM_PIOS if none qualifies.
uint8_t rp2pio_statemachine_find_pio(int program_size, int sm_count);

// Mark / unmark a state machine as surviving (or not) a soft reset, so
// rp2pio's reset path keeps its bookkeeping coherent with the SMs this driver
// claims directly.
void rp2pio_statemachine_never_reset(PIO pio, int sm);
void rp2pio_statemachine_reset_ok(PIO pio, int sm);

#ifdef __cplusplus
}
#endif
