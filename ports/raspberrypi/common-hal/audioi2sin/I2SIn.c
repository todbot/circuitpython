// This file is part of the CircuitPython project: https://circuitpython.org
//
// SPDX-FileCopyrightText: Copyright (c) 2026 Tim Cocks for Adafruit Industries
//
// SPDX-License-Identifier: MIT

#include <stdint.h>
#include <string.h>

#include "py/mperrno.h"
#include "py/mphal.h"
#include "py/runtime.h"
#include "shared/runtime/interrupt_char.h"
#include "common-hal/audioi2sin/I2SIn.h"
#include "shared-bindings/audioi2sin/I2SIn.h"
#include "shared-bindings/microcontroller/Pin.h"
#include "bindings/rp2pio/StateMachine.h"
#include "supervisor/port.h"

#include "hardware/dma.h"

#if CIRCUITPY_AUDIOI2SIN

// 16-bit programs sample a stereo frame of 16+16 = 32 bits and rely on
// auto-push at 32 to deliver one packed (right<<16 | left) word per frame.
// 32-bit programs sample 32 bits per channel and produce two FIFO words per
// frame (right first, then left). Each bit takes 6 PIO clocks (a [2]-delay
// pair). 24-bit recordings reuse the 32-bit programs because most 24-bit
// MEMS mics (SPH0645LM4H, INMP441, ICS-43434) transmit their 24 valid bits
// left-justified inside a 32-bit slot.
//
// `in pins 1` runs on a cycle where side-set drives BCLK high. The slave
// updates the data line on the BCLK falling edge, so by the rising edge it
// has settled and is safe to sample. Sampling at BCLK low (the previous,
// incorrect arrangement) catches the data mid-transition and the result is
// effectively noise.
#define PIO_CLOCKS_PER_BIT (6)

// Master-mode RX, regular pin order (BCLK = WS - 1), Philips alignment.
static const uint16_t i2sin_program[] = {
    //     .wrap_target
    0xf04e, //  0: set    y, 14           side 2
    0x5a01, //  1: in     pins, 1         side 3 [2]
    0x1281, //  2: jmp    y--, 1          side 2 [2]
    0x4a01, //  3: in     pins, 1         side 1 [2]
    0xe24e, //  4: set    y, 14           side 0 [2]
    0x4a01, //  5: in     pins, 1         side 1 [2]
    0x0285, //  6: jmp    y--, 5          side 0 [2]
    0x5a01, //  7: in     pins, 1         side 3 [2]
            //     .wrap
};

// Master-mode RX, regular pin order, left-justified.
static const uint16_t i2sin_program_left_justified[] = {
    //     .wrap_target
    0xe04e, //  0: set    y, 14           side 0
    0x5a01, //  1: in     pins, 1         side 3 [2]
    0x1281, //  2: jmp    y--, 1          side 2 [2]
    0x5a01, //  3: in     pins, 1         side 3 [2]
    0xf24e, //  4: set    y, 14           side 2 [2]
    0x4a01, //  5: in     pins, 1         side 1 [2]
    0x0285, //  6: jmp    y--, 5          side 0 [2]
    0x4a01, //  7: in     pins, 1         side 1 [2]
            //     .wrap
};

// Master-mode RX, swapped pin order (BCLK = WS + 1), Philips alignment.
static const uint16_t i2sin_program_swap[] = {
    //     .wrap_target
    0xe84e, //  0: set    y, 14           side 1
    0x5a01, //  1: in     pins, 1         side 3 [2]
    0x0a81, //  2: jmp    y--, 1          side 1 [2]
    0x5201, //  3: in     pins, 1         side 2 [2]
    0xe24e, //  4: set    y, 14           side 0 [2]
    0x5201, //  5: in     pins, 1         side 2 [2]
    0x0285, //  6: jmp    y--, 5          side 0 [2]
    0x5a01, //  7: in     pins, 1         side 3 [2]
            //     .wrap
};

// Master-mode RX, swapped pin order, left-justified.
static const uint16_t i2sin_program_left_justified_swap[] = {
    //     .wrap_target
    0xe04e, //  0: set    y, 14           side 0
    0x5a01, //  1: in     pins, 1         side 3 [2]
    0x0a81, //  2: jmp    y--, 1          side 1 [2]
    0x5a01, //  3: in     pins, 1         side 3 [2]
    0xea4e, //  4: set    y, 14           side 1 [2]
    0x5201, //  5: in     pins, 1         side 2 [2]
    0x0285, //  6: jmp    y--, 5          side 0 [2]
    0x5201, //  7: in     pins, 1         side 2 [2]
            //     .wrap
};

// 32-bit-per-channel variants: identical to the 16-bit programs above except
// the loop counter is set to 30 (so each `bitloop` runs 31 in's, plus one
// outside the loop = 32 in's per channel).
static const uint16_t i2sin_program_32[] = {
    //     .wrap_target
    0xf05e, //  0: set    y, 30           side 2
    0x5a01, //  1: in     pins, 1         side 3 [2]
    0x1281, //  2: jmp    y--, 1          side 2 [2]
    0x4a01, //  3: in     pins, 1         side 1 [2]
    0xe25e, //  4: set    y, 30           side 0 [2]
    0x4a01, //  5: in     pins, 1         side 1 [2]
    0x0285, //  6: jmp    y--, 5          side 0 [2]
    0x5a01, //  7: in     pins, 1         side 3 [2]
            //     .wrap
};

static const uint16_t i2sin_program_left_justified_32[] = {
    //     .wrap_target
    0xe05e, //  0: set    y, 30           side 0
    0x5a01, //  1: in     pins, 1         side 3 [2]
    0x1281, //  2: jmp    y--, 1          side 2 [2]
    0x5a01, //  3: in     pins, 1         side 3 [2]
    0xf25e, //  4: set    y, 30           side 2 [2]
    0x4a01, //  5: in     pins, 1         side 1 [2]
    0x0285, //  6: jmp    y--, 5          side 0 [2]
    0x4a01, //  7: in     pins, 1         side 1 [2]
            //     .wrap
};

static const uint16_t i2sin_program_swap_32[] = {
    //     .wrap_target
    0xe85e, //  0: set    y, 30           side 1
    0x5a01, //  1: in     pins, 1         side 3 [2]
    0x0a81, //  2: jmp    y--, 1          side 1 [2]
    0x5201, //  3: in     pins, 1         side 2 [2]
    0xe25e, //  4: set    y, 30           side 0 [2]
    0x5201, //  5: in     pins, 1         side 2 [2]
    0x0285, //  6: jmp    y--, 5          side 0 [2]
    0x5a01, //  7: in     pins, 1         side 3 [2]
            //     .wrap
};

static const uint16_t i2sin_program_left_justified_swap_32[] = {
    //     .wrap_target
    0xe05e, //  0: set    y, 30           side 0
    0x5a01, //  1: in     pins, 1         side 3 [2]
    0x0a81, //  2: jmp    y--, 1          side 1 [2]
    0x5a01, //  3: in     pins, 1         side 3 [2]
    0xea5e, //  4: set    y, 30           side 1 [2]
    0x5201, //  5: in     pins, 1         side 2 [2]
    0x0285, //  6: jmp    y--, 5          side 0 [2]
    0x5201, //  7: in     pins, 1         side 2 [2]
            //     .wrap
};

// Caller validates that pins are free.
void common_hal_audioi2sin_i2sin_construct(audioi2sin_i2sin_obj_t *self,
    const mcu_pin_obj_t *bit_clock, const mcu_pin_obj_t *word_select,
    const mcu_pin_obj_t *data, const mcu_pin_obj_t *main_clock,
    uint32_t sample_rate, uint8_t bit_depth, bool mono, bool left_justified) {

    if (main_clock != NULL) {
        mp_raise_NotImplementedError_varg(MP_ERROR_TEXT("%q"), MP_QSTR_main_clock);
    }
    if (bit_depth != 16 && bit_depth != 24 && bit_depth != 32) {
        mp_raise_NotImplementedError(MP_ERROR_TEXT("Only 16, 24, or 32 bit depth supported."));
    }

    // 24- and 32-bit recordings both clock 32 bits per channel; 24-bit MEMS
    // mics deliver their data left-justified inside that 32-bit slot.
    bool wide = (bit_depth != 16);
    uint32_t bits_per_channel = wide ? 32 : 16;
    uint32_t pio_clocks_per_frame = bits_per_channel * 2 * PIO_CLOCKS_PER_BIT;

    const mcu_pin_obj_t *sideset_pin = NULL;
    const uint16_t *program = NULL;
    size_t program_len = 0;

    if (bit_clock->number == word_select->number - 1) {
        sideset_pin = bit_clock;
        if (left_justified) {
            program = wide ? i2sin_program_left_justified_32 : i2sin_program_left_justified;
            program_len = wide ? MP_ARRAY_SIZE(i2sin_program_left_justified_32)
                               : MP_ARRAY_SIZE(i2sin_program_left_justified);
        } else {
            program = wide ? i2sin_program_32 : i2sin_program;
            program_len = wide ? MP_ARRAY_SIZE(i2sin_program_32)
                               : MP_ARRAY_SIZE(i2sin_program);
        }
    } else if (bit_clock->number == word_select->number + 1) {
        sideset_pin = word_select;
        if (left_justified) {
            program = wide ? i2sin_program_left_justified_swap_32 : i2sin_program_left_justified_swap;
            program_len = wide ? MP_ARRAY_SIZE(i2sin_program_left_justified_swap_32)
                               : MP_ARRAY_SIZE(i2sin_program_left_justified_swap);
        } else {
            program = wide ? i2sin_program_swap_32 : i2sin_program_swap;
            program_len = wide ? MP_ARRAY_SIZE(i2sin_program_swap_32)
                               : MP_ARRAY_SIZE(i2sin_program_swap);
        }
    } else {
        mp_raise_ValueError(MP_ERROR_TEXT("Bit clock and word select must be sequential GPIO pins"));
    }

    common_hal_rp2pio_statemachine_construct(
        &self->state_machine,
        program, program_len,
        sample_rate * pio_clocks_per_frame,
        NULL, 0, // init
        NULL, 0, // may_exec
        NULL, 0, PIO_PINMASK32_NONE, PIO_PINMASK32_NONE, // out pin
        data, 1, // in pins
        PIO_PINMASK32_NONE, PIO_PINMASK32_NONE, // in pulls
        NULL, 0, PIO_PINMASK32_NONE, PIO_PINMASK32_FROM_VALUE(0x1f), // set pins
        sideset_pin, 2, false, PIO_PINMASK32_NONE, PIO_PINMASK32_FROM_VALUE(0x1f), // sideset pins
        false, // No sideset enable
        NULL, PULL_NONE, // jump pin
        PIO_PINMASK_NONE, // wait gpio pins
        true, // exclusive pin use
        false, 32, false, // out settings (unused)
        false, // Wait for txstall
        true, 32, false, // in settings: auto-push at 32 bits, shift left (MSB first)
        false, // Not user-interruptible.
        0, -1, // wrap settings
        PIO_ANY_OFFSET,
        PIO_FIFO_TYPE_DEFAULT,
        PIO_MOV_STATUS_DEFAULT,
        PIO_MOV_N_DEFAULT);

    uint32_t actual_frequency = common_hal_rp2pio_statemachine_get_frequency(&self->state_machine);
    self->sample_rate = actual_frequency / pio_clocks_per_frame;
    self->bit_depth = bit_depth;
    self->mono = mono;
    self->settled = false;
    self->ring = NULL;
    self->ring_size = 0;
    self->half_size = 0;
    self->read_pos = 0;
    self->dma_channel = -1;
    self->overflow = false;

    // Each PIO frame produces 4 bytes in the FIFO regardless of bit depth
    // (16-bit auto-pushes one packed stereo word, 24/32-bit pushes two
    // separate 32-bit words). One stereo frame is therefore either 4 or
    // 8 bytes; size the half-buffer for ~40 ms of audio so a slow consumer
    // (SD card flush etc.) can complete without overrunning.
    size_t bytes_per_frame = (bit_depth == 16) ? 4 : 8;
    size_t target = (size_t)self->sample_rate * bytes_per_frame * 40 / 1000;
    size_t half_size = (target > 4096) ? target : 4096;
    // Round up to a multiple of 8 so 24/32-bit pair reads never straddle
    // the half boundary.
    half_size = (half_size + 7u) & ~(size_t)7u;

    self->ring = (uint8_t *)port_malloc(2 * half_size, true);
    if (self->ring == NULL) {
        common_hal_rp2pio_statemachine_deinit(&self->state_machine);
        m_malloc_fail(2 * half_size);
    }
    self->half_size = half_size;
    self->ring_size = 2 * half_size;

    common_hal_rp2pio_statemachine_set_read_buffers_raw(&self->state_machine,
        NULL, 0,
        self->ring, half_size,
        self->ring + half_size, half_size);
    if (!common_hal_rp2pio_statemachine_background_read(&self->state_machine, 4, false)) {
        port_free(self->ring);
        self->ring = NULL;
        common_hal_rp2pio_statemachine_deinit(&self->state_machine);
        mp_raise_OSError(MP_EIO);
    }
    self->dma_channel = common_hal_rp2pio_statemachine_get_read_dma_channel(&self->state_machine);
}

bool common_hal_audioi2sin_i2sin_deinited(audioi2sin_i2sin_obj_t *self) {
    return common_hal_rp2pio_statemachine_deinited(&self->state_machine);
}

void common_hal_audioi2sin_i2sin_deinit(audioi2sin_i2sin_obj_t *self) {
    if (common_hal_audioi2sin_i2sin_deinited(self)) {
        return;
    }
    common_hal_rp2pio_statemachine_stop_background_read(&self->state_machine);
    common_hal_rp2pio_statemachine_deinit(&self->state_machine);
    if (self->ring != NULL) {
        port_free(self->ring);
        self->ring = NULL;
    }
    self->ring_size = 0;
    self->half_size = 0;
    self->dma_channel = -1;
}

uint8_t common_hal_audioi2sin_i2sin_get_bit_depth(audioi2sin_i2sin_obj_t *self) {
    return self->bit_depth;
}

uint32_t common_hal_audioi2sin_i2sin_get_sample_rate(audioi2sin_i2sin_obj_t *self) {
    return self->sample_rate;
}

// In 16-bit mode, each PIO frame produces a single 32-bit FIFO word with bits
// 31..16 = right channel and bits 15..0 = left channel (both MSB-first signed
// 16-bit). In 24/32-bit mode each frame produces two FIFO words: right first,
// then left, each MSB-first in the full 32 bits. For mono we keep the left
// channel; for stereo we emit (left, right) pairs.
//
// `output_buffer_length` is the requested number of samples to write (sample
// width = 2 bytes for bit_depth=16, 4 bytes for bit_depth=24 or 32). Returns
// the number actually written.
// Compute the byte offset inside `ring` that the DMA is currently writing
// (one past the last word it finished). Always lies in [0, ring_size).
static size_t i2sin_write_pos(audioi2sin_i2sin_obj_t *self) {
    uintptr_t addr = (uintptr_t)dma_channel_hw_addr(self->dma_channel)->write_addr;
    uintptr_t base = (uintptr_t)self->ring;
    if (addr < base || addr >= base + self->ring_size) {
        // The ISR retargets write_addr to the start of the next half right
        // after a transfer completes; it should always be in-range, but if
        // we observe it mid-update just report "no new data".
        return self->read_pos;
    }
    return (size_t)(addr - base);
}

uint32_t common_hal_audioi2sin_i2sin_record_to_buffer(audioi2sin_i2sin_obj_t *self,
    void *buffer, uint32_t output_buffer_length) {
    uint32_t output_count = 0;
    const size_t ring_size = self->ring_size;
    const size_t half_size = self->half_size;

    if (self->bit_depth == 16) {
        // 16-bit mode auto-pushes one stereo frame per FIFO word. The DMA has
        // been streaming since construct time, so the ring already contains
        // settled data; drop the first 4 bytes once to discard a single
        // pre-record frame (matches the prior synchronous behaviour).
        uint16_t *output = (uint16_t *)buffer;
        while (output_count < output_buffer_length) {
            size_t write_pos = i2sin_write_pos(self);
            size_t avail = (write_pos + ring_size - self->read_pos) % ring_size;
            if (avail > half_size) {
                // DMA has filled more than one half ahead of us -- we lost
                // data. Snap to just behind the DMA on a 4-byte boundary.
                self->overflow = true;
                self->read_pos = write_pos & ~(size_t)3u;
                avail = 0;
            }
            if (!self->settled && avail >= 4) {
                self->read_pos = (self->read_pos + 4) % ring_size;
                avail -= 4;
                self->settled = true;
            }
            if (avail < 4) {
                RUN_BACKGROUND_TASKS;
                if (mp_hal_is_interrupted()) {
                    return output_count;
                }
                continue;
            }
            while (avail >= 4 && output_count < output_buffer_length) {
                uint32_t frame = *(volatile uint32_t *)(self->ring + self->read_pos);
                uint16_t left = (uint16_t)(frame & 0xffff);
                uint16_t right = (uint16_t)(frame >> 16);
                if (self->mono) {
                    output[output_count++] = left;
                } else {
                    output[output_count++] = left;
                    if (output_count >= output_buffer_length) {
                        self->read_pos = (self->read_pos + 4) % ring_size;
                        avail -= 4;
                        break;
                    }
                    output[output_count++] = right;
                }
                self->read_pos = (self->read_pos + 4) % ring_size;
                avail -= 4;
            }
        }
    } else {
        // 24-/32-bit mode emits two FIFO pushes per stereo frame (right then
        // left). The state machine was started at instruction 0 by the
        // constructor, so the very first push is the right channel and
        // alternation is preserved as long as we never touch the program
        // counter and always read an even number of words. half_size is a
        // multiple of 8, so reading 8-byte pairs stays aligned across the
        // ring wrap.
        uint32_t *output = (uint32_t *)buffer;
        while (output_count < output_buffer_length) {
            size_t write_pos = i2sin_write_pos(self);
            size_t avail = (write_pos + ring_size - self->read_pos) % ring_size;
            if (avail > half_size) {
                // Overflow: snap to a frame-aligned position one half behind
                // the DMA's current half so we resume on the right channel.
                self->overflow = true;
                size_t cur_half = (write_pos < half_size) ? 0 : half_size;
                self->read_pos = (cur_half + half_size) % ring_size;
                self->settled = false;
                avail = (write_pos + ring_size - self->read_pos) % ring_size;
            }
            if (!self->settled) {
                if (avail < 8) {
                    RUN_BACKGROUND_TASKS;
                    if (mp_hal_is_interrupted()) {
                        return output_count;
                    }
                    continue;
                }
                self->read_pos = (self->read_pos + 8) % ring_size;
                avail -= 8;
                self->settled = true;
            }
            if (avail < 8) {
                RUN_BACKGROUND_TASKS;
                if (mp_hal_is_interrupted()) {
                    return output_count;
                }
                continue;
            }
            while (avail >= 8 && output_count < output_buffer_length) {
                uint32_t right = *(volatile uint32_t *)(self->ring + self->read_pos);
                size_t next_pos = (self->read_pos + 4) % ring_size;
                uint32_t left = *(volatile uint32_t *)(self->ring + next_pos);
                if (self->mono) {
                    output[output_count++] = left;
                } else {
                    output[output_count++] = left;
                    if (output_count >= output_buffer_length) {
                        self->read_pos = (self->read_pos + 8) % ring_size;
                        avail -= 8;
                        break;
                    }
                    output[output_count++] = right;
                }
                self->read_pos = (self->read_pos + 8) % ring_size;
                avail -= 8;
            }
        }
    }

    return output_count;
}

#endif // CIRCUITPY_AUDIOI2SIN
