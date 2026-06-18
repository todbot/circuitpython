// This file is part of the CircuitPython project: https://circuitpython.org
//
// SPDX-FileCopyrightText: Copyright (c) 2026 Tim Cocks for Adafruit Industries LLC
//
// SPDX-License-Identifier: MIT

#include "shared-module/usb_audio/__init__.h"
#include "shared-module/usb_audio/USBMicrophone.h"
#include "shared-module/usb_audio/usb_audio_descriptors.h"

#include <string.h>

#include "tusb.h"

static bool usb_audio_is_enabled = false;
static bool usb_audio_is_streaming = false;

uint32_t usb_audio_sample_rate;
uint8_t usb_audio_channel_count;
uint8_t usb_audio_bits_per_sample;

// Audio control state surfaced to the host. One extra entry for the master channel 0.
static int8_t usb_audio_mute[USB_AUDIO_N_CHANNELS + 1];
static int16_t usb_audio_volume[USB_AUDIO_N_CHANNELS + 1];

bool shared_module_usb_audio_enable(mp_int_t sample_rate, mp_int_t channel_count, mp_int_t bits_per_sample) {
    if (tud_connected()) {
        return false;
    }

    usb_audio_sample_rate = sample_rate;
    usb_audio_channel_count = channel_count;
    usb_audio_bits_per_sample = bits_per_sample;
    usb_audio_is_enabled = true;

    return true;
}

bool shared_module_usb_audio_disable(void) {
    if (tud_connected()) {
        return false;
    }
    usb_audio_is_enabled = false;
    return true;
}

bool usb_audio_enabled(void) {
    return usb_audio_is_enabled;
}

bool usb_audio_streaming(void) {
    return usb_audio_is_streaming;
}

size_t usb_audio_descriptor_length(void) {
    return TUD_AUDIO_MIC_ONE_CH_DESC_LEN;
}

size_t usb_audio_add_descriptor(uint8_t *descriptor_buf, descriptor_counts_t *descriptor_counts, uint8_t *current_interface_string) {
    usb_add_interface_string(*current_interface_string, "CircuitPython Microphone");
    const uint8_t usb_audio_descriptor[] = {
        TUD_AUDIO_MIC_ONE_CH_DESCRIPTOR(
            /*_itfnum*/ descriptor_counts->current_interface,
            /*_stridx*/ *current_interface_string,
            /*_nBytesPerSample*/ USB_AUDIO_N_BYTES_PER_SAMPLE,
            /*_nBitsUsedPerSample*/ USB_AUDIO_BITS_PER_SAMPLE,
            /*_epin*/ descriptor_counts->current_endpoint | 0x80,
            /*_epsize*/ CFG_TUD_AUDIO_FUNC_1_EP_IN_SZ_MAX)
    };

    (*current_interface_string)++;
    // One IAD wrapping an AudioControl + an AudioStreaming interface, plus one IN endpoint.
    descriptor_counts->current_interface += 2;
    descriptor_counts->num_in_endpoints++;
    descriptor_counts->current_endpoint++;

    memcpy(descriptor_buf, usb_audio_descriptor, sizeof(usb_audio_descriptor));

    return sizeof(usb_audio_descriptor);
}

void usb_audio_task(void) {
    if (!usb_audio_is_streaming) {
        return;
    }

    // Pace production by the IN FIFO level. Each pass we top the software FIFO
    // back up to its half-full setpoint, generating only the samples the host has
    // actually drained since the last pass. This limits our production rate to the
    // host's true consumption rate (its USB SOF / audio clock), keeps the FIFO
    // around the level TinyUSB's flow control targets so it can send steady
    // nominal-size packets, and automatically catches up after any scheduling gap
    // (GCpause, other background work) in a single pass instead of underrunning.
    // Underruns here showed up to the host as discrete sample-drop/insert splices,
    // i.e. the erratic ticking on a held tone.
    tu_fifo_t *ep_in_ff = tud_audio_get_ep_in_ff();
    uint16_t const target = tu_fifo_depth(ep_in_ff) / 2;

    // One scratch chunk (1 ms at the max rate); we loop until the FIFO reaches
    // the setpoint. Sized for the highest rate enable() accepts.
    static int16_t samples[USB_AUDIO_MAX_SAMPLE_RATE / 1000 * USB_AUDIO_N_CHANNELS];

    uint16_t count;
    while ((count = tu_fifo_count(ep_in_ff)) < target) {
        size_t want = target - count;
        if (want > sizeof(samples)) {
            want = sizeof(samples);
        }

        // Pull the next chunk from whichever USBMicrophone is playing.
        bool underran = false;
        size_t filled = usb_audio_usbmicrophone_background_fill((uint8_t *)samples, want);
        if (filled == 0) {
            // No source attached, paused, or fully drained: keep the endpoint
            // alive with silence so the host never sees a starved stream.
            memset((uint8_t *)samples, 0, want);
        } else if (filled < want) {
            // The source momentarily underran. Send just what it produced and
            // let the FIFO cushion ride until it catches up next pass, rather
            // than flooding the stream with a burst of silence.
            want = filled;
            underran = true;
        }

        if (tud_audio_write((uint8_t *)samples, (uint16_t)want) == 0) {
            break;  // FIFO unexpectedly full / host not ready
        }
        if (underran) {
            break;
        }
    }
}

// --------------------------------------------------------------------+
// TinyUSB audio class callbacks (weak symbols overridden here)
// --------------------------------------------------------------------+

// Host opened/closed the AudioStreaming alternate setting.
bool tud_audio_set_itf_cb(uint8_t rhport, tusb_control_request_t const *p_request) {
    (void)rhport;
    uint8_t const alt = (uint8_t)tu_u16_low(p_request->wValue);
    usb_audio_is_streaming = (alt != 0);
    return true;
}

bool tud_audio_set_itf_close_ep_cb(uint8_t rhport, tusb_control_request_t const *p_request) {
    (void)rhport;
    (void)p_request;
    usb_audio_is_streaming = false;
    return true;
}

// Class-specific SET requests for an entity (we accept mute/volume on the feature unit).
bool tud_audio_set_req_entity_cb(uint8_t rhport, tusb_control_request_t const *p_request, uint8_t *pBuff) {
    (void)rhport;

    uint8_t const channelNum = (uint8_t)tu_u16_low(p_request->wValue);
    uint8_t const ctrlSel = (uint8_t)tu_u16_high(p_request->wValue);
    uint8_t const entityID = (uint8_t)tu_u16_high(p_request->wIndex);

    // Only current-value requests are supported.
    TU_VERIFY(p_request->bRequest == AUDIO_CS_REQ_CUR);

    if (entityID == USB_AUDIO_ENTITY_FEATURE_UNIT) {
        if (channelNum > USB_AUDIO_N_CHANNELS) {
            return false;
        }
        switch (ctrlSel) {
            case AUDIO_FU_CTRL_MUTE:
                TU_VERIFY(p_request->wLength == sizeof(audio_control_cur_1_t));
                usb_audio_mute[channelNum] = ((audio_control_cur_1_t *)pBuff)->bCur;
                return true;

            case AUDIO_FU_CTRL_VOLUME:
                TU_VERIFY(p_request->wLength == sizeof(audio_control_cur_2_t));
                usb_audio_volume[channelNum] = ((audio_control_cur_2_t *)pBuff)->bCur;
                return true;

            default:
                return false;
        }
    }
    return false;
}

// Class-specific GET requests for an entity (clock source, feature unit, input terminal).
bool tud_audio_get_req_entity_cb(uint8_t rhport, tusb_control_request_t const *p_request) {
    (void)rhport;

    uint8_t const channelNum = (uint8_t)tu_u16_low(p_request->wValue);
    uint8_t const ctrlSel = (uint8_t)tu_u16_high(p_request->wValue);
    uint8_t const entityID = (uint8_t)tu_u16_high(p_request->wIndex);

    // Input terminal (microphone).
    if (entityID == USB_AUDIO_ENTITY_INPUT_TERMINAL) {
        switch (ctrlSel) {
            case AUDIO_TE_CTRL_CONNECTOR: {
                audio_desc_channel_cluster_t ret;
                ret.bNrChannels = USB_AUDIO_N_CHANNELS;
                ret.bmChannelConfig = (audio_channel_config_t)0;
                ret.iChannelNames = 0;
                return tud_audio_buffer_and_schedule_control_xfer(rhport, p_request, &ret, sizeof(ret));
            }
            default:
                return false;
        }
    }

    // Feature unit (mute/volume).
    if (entityID == USB_AUDIO_ENTITY_FEATURE_UNIT) {
        if (channelNum > USB_AUDIO_N_CHANNELS) {
            return false;
        }
        switch (ctrlSel) {
            case AUDIO_FU_CTRL_MUTE:
                return tud_audio_buffer_and_schedule_control_xfer(rhport, p_request, &usb_audio_mute[channelNum], sizeof(usb_audio_mute[channelNum]));

            case AUDIO_FU_CTRL_VOLUME:
                switch (p_request->bRequest) {
                    case AUDIO_CS_REQ_CUR:
                        return tud_audio_buffer_and_schedule_control_xfer(rhport, p_request, &usb_audio_volume[channelNum], sizeof(usb_audio_volume[channelNum]));

                    case AUDIO_CS_REQ_RANGE: {
                        audio_control_range_2_n_t(1) ret;
                        ret.wNumSubRanges = 1;
                        ret.subrange[0].bMin = -90 * 256;  // -90 dB
                        ret.subrange[0].bMax = 90 * 256;   // +90 dB
                        ret.subrange[0].bRes = 256;        // 1 dB steps
                        return tud_audio_buffer_and_schedule_control_xfer(rhport, p_request, &ret, sizeof(ret));
                    }

                    default:
                        return false;
                }

            default:
                return false;
        }
    }

    // Clock source (sample rate set in usb_audio.enable()).
    if (entityID == USB_AUDIO_ENTITY_CLOCK_SOURCE) {
        switch (ctrlSel) {
            case AUDIO_CS_CTRL_SAM_FREQ:
                switch (p_request->bRequest) {
                    case AUDIO_CS_REQ_CUR: {
                        audio_control_cur_4_t cur = { .bCur = (int32_t)usb_audio_sample_rate };
                        return tud_audio_buffer_and_schedule_control_xfer(rhport, p_request, &cur, sizeof(cur));
                    }

                    case AUDIO_CS_REQ_RANGE: {
                        audio_control_range_4_n_t(1) ret;
                        ret.wNumSubRanges = 1;
                        ret.subrange[0].bMin = (int32_t)usb_audio_sample_rate;
                        ret.subrange[0].bMax = (int32_t)usb_audio_sample_rate;
                        ret.subrange[0].bRes = 0;
                        return tud_audio_buffer_and_schedule_control_xfer(rhport, p_request, &ret, sizeof(ret));
                    }

                    default:
                        return false;
                }

            case AUDIO_CS_CTRL_CLK_VALID: {
                audio_control_cur_1_t cur = { .bCur = 1 };
                return tud_audio_buffer_and_schedule_control_xfer(rhport, p_request, &cur, sizeof(cur));
            }

            default:
                return false;
        }
    }

    return false;
}
