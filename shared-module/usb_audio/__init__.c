// This file is part of the CircuitPython project: https://circuitpython.org
//
// SPDX-FileCopyrightText: Copyright (c) 2026 Tim Cocks for Adafruit Industries LLC
//
// SPDX-License-Identifier: MIT

#include "shared-module/usb_audio/__init__.h"
#include "shared-module/usb_audio/USBMicrophone.h"
#include "shared-module/usb_audio/USBSpeaker.h"
#include "shared-module/usb_audio/usb_audio_descriptors.h"

#include <string.h>

#include "py/misc.h"
#include "tusb.h"

static bool usb_audio_is_enabled = false;
static bool usb_audio_is_streaming = false;

uint32_t usb_audio_sample_rate;
uint8_t usb_audio_channel_count;
uint8_t usb_audio_bits_per_sample;
usb_audio_direction_t usb_audio_direction;

// Audio control state surfaced to the host. One extra entry for the master channel 0.
static int8_t usb_audio_mute[USB_AUDIO_N_CHANNELS + 1];
static int16_t usb_audio_volume[USB_AUDIO_N_CHANNELS + 1];

bool shared_module_usb_audio_enable(mp_int_t sample_rate, mp_int_t channel_count, mp_int_t bits_per_sample, usb_audio_direction_t direction) {
    if (tud_connected()) {
        return false;
    }

    usb_audio_sample_rate = sample_rate;
    usb_audio_channel_count = channel_count;
    usb_audio_bits_per_sample = bits_per_sample;
    usb_audio_direction = direction;
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

// Hand-rolled UAC2 mono speaker (host -> board) descriptor WITHOUT an async
// feedback endpoint. This mirrors TinyUSB's TUD_AUDIO_SPEAKER_MONO_FB_DESCRIPTOR
// (lib/tinyusb/src/device/usbd.h) but drops the trailing feedback endpoint, so
// the streaming alt-setting declares a single OUT endpoint (_nEPs = 0x01). The
// entity IDs match the mic descriptor (see usb_audio_descriptors.h); only the
// terminal roles reverse: the input terminal is the USB-streaming side and the
// output terminal is the desktop speaker, and the AS interface links the input
// terminal (0x01). Async feedback for true clock matching is a later step.
#define USB_AUDIO_SPEAKER_DESCRIPTOR(_itfnum, _stridx, _nBytesPerSample, _nBitsUsedPerSample, _epout, _epsize) \
    /* Standard Interface Association Descriptor (IAD) */ \
    TUD_AUDIO_DESC_IAD(/*_firstitf*/ _itfnum, /*_nitfs*/ 0x02, /*_stridx*/ 0x00), \
    /* Standard AC Interface Descriptor(4.7.1) */ \
    TUD_AUDIO_DESC_STD_AC(/*_itfnum*/ _itfnum, /*_nEPs*/ 0x00, /*_stridx*/ _stridx), \
    /* Class-Specific AC Interface Header Descriptor(4.7.2) */ \
    TUD_AUDIO_DESC_CS_AC(/*_bcdADC*/ 0x0200, /*_category*/ AUDIO_FUNC_DESKTOP_SPEAKER, /*_totallen*/ TUD_AUDIO_DESC_CLK_SRC_LEN + TUD_AUDIO_DESC_INPUT_TERM_LEN + TUD_AUDIO_DESC_OUTPUT_TERM_LEN + TUD_AUDIO_DESC_FEATURE_UNIT_ONE_CHANNEL_LEN, /*_ctrl*/ AUDIO_CS_AS_INTERFACE_CTRL_LATENCY_POS), \
    /* Clock Source Descriptor(4.7.2.1) */ \
    TUD_AUDIO_DESC_CLK_SRC(/*_clkid*/ USB_AUDIO_ENTITY_CLOCK_SOURCE, /*_attr*/ AUDIO_CLOCK_SOURCE_ATT_INT_FIX_CLK, /*_ctrl*/ (AUDIO_CTRL_R << AUDIO_CLOCK_SOURCE_CTRL_CLK_FRQ_POS), /*_assocTerm*/ USB_AUDIO_ENTITY_INPUT_TERMINAL, /*_stridx*/ 0x00), \
    /* Input Terminal Descriptor(4.7.2.4) -- USB streaming in from the host */ \
    TUD_AUDIO_DESC_INPUT_TERM(/*_termid*/ USB_AUDIO_ENTITY_INPUT_TERMINAL, /*_termtype*/ AUDIO_TERM_TYPE_USB_STREAMING, /*_assocTerm*/ 0x00, /*_clkid*/ USB_AUDIO_ENTITY_CLOCK_SOURCE, /*_nchannelslogical*/ USB_AUDIO_N_CHANNELS, /*_channelcfg*/ AUDIO_CHANNEL_CONFIG_NON_PREDEFINED, /*_idxchannelnames*/ 0x00, /*_ctrl*/ 0 * (AUDIO_CTRL_R << AUDIO_IN_TERM_CTRL_CONNECTOR_POS), /*_stridx*/ 0x00), \
    /* Output Terminal Descriptor(4.7.2.5) -- desktop speaker */ \
    TUD_AUDIO_DESC_OUTPUT_TERM(/*_termid*/ USB_AUDIO_ENTITY_OUTPUT_TERMINAL, /*_termtype*/ AUDIO_TERM_TYPE_OUT_DESKTOP_SPEAKER, /*_assocTerm*/ USB_AUDIO_ENTITY_INPUT_TERMINAL, /*_srcid*/ USB_AUDIO_ENTITY_FEATURE_UNIT, /*_clkid*/ USB_AUDIO_ENTITY_CLOCK_SOURCE, /*_ctrl*/ 0x0000, /*_stridx*/ 0x00), \
    /* Feature Unit Descriptor(4.7.2.8) */ \
    TUD_AUDIO_DESC_FEATURE_UNIT_ONE_CHANNEL(/*_unitid*/ USB_AUDIO_ENTITY_FEATURE_UNIT, /*_srcid*/ USB_AUDIO_ENTITY_INPUT_TERMINAL, /*_ctrlch0master*/ AUDIO_CTRL_RW << AUDIO_FEATURE_UNIT_CTRL_MUTE_POS | AUDIO_CTRL_RW << AUDIO_FEATURE_UNIT_CTRL_VOLUME_POS, /*_ctrlch1*/ AUDIO_CTRL_RW << AUDIO_FEATURE_UNIT_CTRL_MUTE_POS | AUDIO_CTRL_RW << AUDIO_FEATURE_UNIT_CTRL_VOLUME_POS, /*_stridx*/ 0x00), \
    /* Standard AS Interface Descriptor(4.9.1) -- alt 0, zero bandwidth */ \
    TUD_AUDIO_DESC_STD_AS_INT(/*_itfnum*/ (uint8_t)((_itfnum) + 1), /*_altset*/ 0x00, /*_nEPs*/ 0x00, /*_stridx*/ 0x00), \
    /* Standard AS Interface Descriptor(4.9.1) -- alt 1, one OUT endpoint */ \
    TUD_AUDIO_DESC_STD_AS_INT(/*_itfnum*/ (uint8_t)((_itfnum) + 1), /*_altset*/ 0x01, /*_nEPs*/ 0x01, /*_stridx*/ 0x00), \
    /* Class-Specific AS Interface Descriptor(4.9.2) -- linked to the input terminal */ \
    TUD_AUDIO_DESC_CS_AS_INT(/*_termid*/ USB_AUDIO_ENTITY_INPUT_TERMINAL, /*_ctrl*/ AUDIO_CTRL_NONE, /*_formattype*/ AUDIO_FORMAT_TYPE_I, /*_formats*/ AUDIO_DATA_FORMAT_TYPE_I_PCM, /*_nchannelsphysical*/ USB_AUDIO_N_CHANNELS, /*_channelcfg*/ AUDIO_CHANNEL_CONFIG_NON_PREDEFINED, /*_stridx*/ 0x00), \
    /* Type I Format Type Descriptor(2.3.1.6 - Audio Formats) */ \
    TUD_AUDIO_DESC_TYPE_I_FORMAT(_nBytesPerSample, _nBitsUsedPerSample), \
    /* Standard AS Isochronous Audio Data Endpoint Descriptor(4.10.1.1) */ \
    TUD_AUDIO_DESC_STD_AS_ISO_EP(/*_ep*/ _epout, /*_attr*/ (uint8_t)((uint8_t)TUSB_XFER_ISOCHRONOUS | (uint8_t)TUSB_ISO_EP_ATT_ASYNCHRONOUS | (uint8_t)TUSB_ISO_EP_ATT_DATA), /*_maxEPsize*/ _epsize, /*_interval*/ 0x01), \
    /* Class-Specific AS Isochronous Audio Data Endpoint Descriptor(4.10.1.2) */ \
    TUD_AUDIO_DESC_CS_AS_ISO_EP(/*_attr*/ AUDIO_CS_AS_ISO_DATA_EP_ATT_NON_MAX_PACKETS_OK, /*_ctrl*/ AUDIO_CTRL_NONE, /*_lockdelayunit*/ AUDIO_CS_AS_ISO_DATA_EP_LOCK_DELAY_UNIT_UNDEFINED, /*_lockdelay*/ 0x0000)

static bool usb_audio_direction_is_output(void) {
    return usb_audio_direction == USB_AUDIO_DIRECTION_OUTPUT;
}

size_t usb_audio_descriptor_length(void) {
    if (usb_audio_direction_is_output()) {
        return USB_AUDIO_SPEAKER_DESC_LEN;
    }
    return TUD_AUDIO_MIC_ONE_CH_DESC_LEN;
}

size_t usb_audio_add_descriptor(uint8_t *descriptor_buf, descriptor_counts_t *descriptor_counts, uint8_t *current_interface_string) {
    // Pick the isochronous endpoint number. By default it follows the same
    // sequential allocation as every other interface. On ports that pin ISO to a
    // fixed, dedicated endpoint (USB_AUDIO_ISO_EP_NUM != 0; see the header for the
    // nRF52 case), we use that number instead and leave the sequential counters
    // untouched: the dedicated ISO endpoint is a separate hardware resource, so
    // it must not consume one of the regular endpoint numbers the other
    // interfaces draw from.
    const bool forced_iso_ep = (USB_AUDIO_ISO_EP_NUM != 0);
    const uint8_t iso_ep_num = forced_iso_ep ? USB_AUDIO_ISO_EP_NUM : descriptor_counts->current_endpoint;

    if (usb_audio_direction_is_output()) {
        usb_add_interface_string(*current_interface_string, "CircuitPython Speaker");
        const uint8_t usb_audio_descriptor[] = {
            USB_AUDIO_SPEAKER_DESCRIPTOR(
                /*_itfnum*/ descriptor_counts->current_interface,
                /*_stridx*/ *current_interface_string,
                /*_nBytesPerSample*/ USB_AUDIO_N_BYTES_PER_SAMPLE,
                /*_nBitsUsedPerSample*/ USB_AUDIO_BITS_PER_SAMPLE,
                /*_epout*/ iso_ep_num,
                /*_epsize*/ CFG_TUD_AUDIO_FUNC_1_EP_OUT_SZ_MAX)
        };

        (*current_interface_string)++;
        // One IAD wrapping an AudioControl + an AudioStreaming interface, plus one OUT endpoint.
        descriptor_counts->current_interface += 2;
        if (!forced_iso_ep) {
            descriptor_counts->num_out_endpoints++;
            descriptor_counts->current_endpoint++;
        }

        memcpy(descriptor_buf, usb_audio_descriptor, sizeof(usb_audio_descriptor));

        return sizeof(usb_audio_descriptor);
    }

    usb_add_interface_string(*current_interface_string, "CircuitPython Microphone");
    const uint8_t usb_audio_descriptor[] = {
        TUD_AUDIO_MIC_ONE_CH_DESCRIPTOR(
            /*_itfnum*/ descriptor_counts->current_interface,
            /*_stridx*/ *current_interface_string,
            /*_nBytesPerSample*/ USB_AUDIO_N_BYTES_PER_SAMPLE,
            /*_nBitsUsedPerSample*/ USB_AUDIO_BITS_PER_SAMPLE,
            /*_epin*/ iso_ep_num | 0x80,
            /*_epsize*/ CFG_TUD_AUDIO_FUNC_1_EP_IN_SZ_MAX)
    };

    (*current_interface_string)++;
    // One IAD wrapping an AudioControl + an AudioStreaming interface, plus one IN endpoint.
    descriptor_counts->current_interface += 2;
    if (!forced_iso_ep) {
        descriptor_counts->num_in_endpoints++;
        descriptor_counts->current_endpoint++;
    }

    memcpy(descriptor_buf, usb_audio_descriptor, sizeof(usb_audio_descriptor));

    return sizeof(usb_audio_descriptor);
}

// --------------------------------------------------------------------+
// Speaker (host -> board) receive task
// --------------------------------------------------------------------+

// Drain everything the host has delivered to the OUT endpoint since the last
// pass into the active USBSpeaker's ring. TinyUSB's weak rx-done handler has
// already moved the isochronous data into ep_out_ff; we copy it out here, in
// task (non-ISR) context, matching the project's "defer ISR work" rule. The ring
// itself, and the overrun/underrun handling, live in USBSpeaker.c so the data
// sits in the audiosample source the output backend pulls from.
static void usb_audio_speaker_task(void) {
    // One USB packet of scratch; we loop until ep_out_ff is empty.
    static uint8_t chunk[CFG_TUD_AUDIO_FUNC_1_EP_OUT_SZ_MAX];

    while (tud_audio_available() > 0) {
        uint16_t got = tud_audio_read(chunk, sizeof(chunk));
        if (got == 0) {
            break;
        }
        // tud_audio_read() never returns more than the buffer it was given, but
        // clamp so the compiler can prove the drain copies stay within chunk[]
        // once this loop is inlined into it.
        if (got > sizeof(chunk)) {
            got = sizeof(chunk);
        }
        usb_audio_usbspeaker_background_drain(chunk, got);
    }
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
    if (usb_audio_direction_is_output()) {
        // Start each speaker streaming session from live audio: drop anything
        // left in the OUT FIFO/ring from a previous session.
        tud_audio_clear_ep_out_ff();
        usb_audio_usbspeaker_streaming_reset();
    }
    return true;
}

bool tud_audio_set_itf_close_ep_cb(uint8_t rhport, tusb_control_request_t const *p_request) {
    (void)rhport;
    (void)p_request;
    usb_audio_is_streaming = false;
    if (usb_audio_direction_is_output()) {
        tud_audio_clear_ep_out_ff();
        usb_audio_usbspeaker_streaming_reset();
    }
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
