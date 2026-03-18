/*
 * Jacdac hardware abstraction layer for Calliope mini V3.
 *
 * Implements the jacdac-c platform interface (jd_hw.h, jd_alloc.h) using
 * CODAL ZSingleWireSerial on P0.12 (P12 connector pin, Jacdac data line).
 *
 * All uart_*, tim_*, hw_* functions are called from C code in jacdac-c but
 * implemented here in C++ so we can use the CODAL API.
 *
 * The MIT License (MIT)
 * Copyright (c) 2025 Calliope gGmbH
 */

#include "main.h"
#include "microbithal.h"
#include "ZSingleWireSerial.h"
#include "Timer.h"

#include <stdlib.h>
#include <string.h>

// ============================================================
// jacdac-c platform headers (C interface)
// ============================================================
extern "C" {
#include "jd_protocol.h"
#include "interfaces/jd_hw.h"
#include "interfaces/jd_alloc.h"
#include "interfaces/jd_app.h"
}

// ============================================================
// Globals required by jacdac-c (defined here, declared extern in jd_hw.h)
// ============================================================
extern "C" uint32_t now = 0;
extern "C" uint8_t  cpu_mhz = 64;      // nRF52833 at 64 MHz
extern "C" uint8_t  jd_connected_blink = 0;
extern "C" uint32_t jd_max_sleep = 10000; // 10 ms

// ============================================================
// Static module state
// ============================================================

// Jacdac UART on P12 (single-wire half-duplex at 1Mbit/s)
static codal::ZSingleWireSerial *jd_sws = nullptr;
static bool jd_running = false;

// A flag set by the SWS interrupt so microbit_hal_jacdac_needs_processing()
// returns true and the idle loop calls jd_process_everything.
static volatile bool jd_pending = false;

// ============================================================
// Timer callback tracking
// ============================================================
#define JD_TIMER_EVENT_ID   (0x2001)
#define JD_TIMER_EVENT_VAL  (1)
static cb_t jd_timer_callback = nullptr;

static void on_jd_timer(Event) {
    if (jd_timer_callback) {
        cb_t fn = jd_timer_callback;
        jd_timer_callback = nullptr;
        fn();
    }
    jd_pending = true;
}

// ============================================================
// SWS interrupt callback (called at IRQ level)
// ============================================================
static void sws_cb(uint16_t evt) {
    if (evt == SWS_EVT_DATA_RECEIVED) {
        // jd_physical.c called uart_start_rx(); completion notifies us here.
        int bytes_left = (int)sizeof(jd_frame_t) - jd_sws->getBytesReceived();
        jd_rx_completed(bytes_left);
    } else if (evt == SWS_EVT_DATA_SENT) {
        jd_tx_completed(0);
    } else {
        // Error
        jd_rx_completed(-1);
    }
    jd_pending = true;
}

// ============================================================
// Line-falling detection via GPIO FALL event on P12
// ============================================================
static void on_p12_fall(Event) {
    jd_line_falling();
    jd_pending = true;
}

// ============================================================
// microbit HAL: called from codal_app/main.cpp
// ============================================================
extern "C" void microbit_hal_jacdac_init(void) {
    cpu_mhz = 64; // nRF52833 runs at 64 MHz

    // Register the timer event listener on the global bus
    uBit.messageBus.listen(JD_TIMER_EVENT_ID, JD_TIMER_EVENT_VAL,
                           on_jd_timer, MESSAGE_BUS_LISTENER_IMMEDIATE);

    // Create ZSingleWireSerial on P12 (Jacdac data pin)
    jd_sws = new codal::ZSingleWireSerial(uBit.io.P12);
    jd_sws->setIRQ(sws_cb);
    jd_sws->setBaud(1000000);

    // Configure P12 for falling-edge events so we detect incoming frames
    uBit.io.P12.eventOn(DEVICE_PIN_EVENT_ON_EDGE);
    uBit.messageBus.listen(ID_PIN_P12, DEVICE_PIN_EVT_FALL,
                           on_p12_fall, MESSAGE_BUS_LISTENER_IMMEDIATE);
}

extern "C" void microbit_hal_jacdac_start(void) {
    if (!jd_sws) {
        microbit_hal_jacdac_init();
    }
    jd_running = true;
}

extern "C" void microbit_hal_jacdac_stop(void) {
    jd_running = false;
    if (jd_sws) {
        jd_sws->setMode(codal::SingleWireDisconnected);
    }
}

extern "C" bool microbit_hal_jacdac_is_running(void) {
    return jd_running;
}

extern "C" bool microbit_hal_jacdac_needs_processing(void) {
    return jd_running && jd_pending;
}

extern "C" void microbit_hal_jacdac_process(void) {
    jd_pending = false;
    jd_process_everything();
}

// ============================================================
// jd_hw.h — hardware functions called from jacdac-c (C linkage)
// ============================================================

extern "C" uint64_t hw_device_id(void) {
    // nRF52833 unique 64-bit device identifier from FICR
    return ((uint64_t)NRF_FICR->DEVICEID[1] << 32) | NRF_FICR->DEVICEID[0];
}

extern "C" __attribute__((noreturn)) void hw_panic(void) {
    microbit_panic(999);
    for (;;) {}
}

extern "C" int target_in_irq(void) {
    return (SCB->ICSR & SCB_ICSR_VECTACTIVE_Msk) != 0;
}

// tim_get_micros: 64-bit microsecond timestamp from CODAL system timer
extern "C" uint64_t tim_get_micros(void) {
    return (uint64_t)system_timer_current_time_us();
}

// tim_set_timer: schedule 'cb' to be called after 'delta' microseconds
extern "C" void tim_set_timer(int delta, cb_t cb) {
    jd_timer_callback = cb;
    if (delta <= 0) delta = 1;
    system_timer_event_after_us((CODAL_TIMESTAMP)delta,
                                JD_TIMER_EVENT_ID, JD_TIMER_EVENT_VAL);
}

// ============================================================
// jd_hw.h — UART functions (half-duplex single-wire, 1Mbit/s)
// ============================================================

extern "C" int uart_start_tx(const void *data, uint32_t numbytes) {
    if (!jd_sws) return -1;
    jd_sws->setMode(codal::SingleWireTx);
    return jd_sws->sendDMA((uint8_t *)data, (int)numbytes);
}

extern "C" void uart_start_rx(void *data, uint32_t maxbytes) {
    if (!jd_sws) return;
    jd_sws->setMode(codal::SingleWireRx);
    jd_sws->receiveDMA((uint8_t *)data, (int)maxbytes);
}

extern "C" void uart_disable(void) {
    if (!jd_sws) return;
    jd_sws->abortDMA();
    jd_sws->setMode(codal::SingleWireDisconnected);
}

extern "C" int uart_wait_high(void) {
    // Poll line for up to ~1ms (1000 iterations × ~1us each at 64 MHz)
    for (int i = 0; i < 1000; i++) {
        if (uBit.io.P12.getDigitalValue()) {
            return 0;
        }
        target_wait_us(1);
    }
    return -1; // timed out
}

extern "C" void uart_flush_rx(void) {
    // No-op on nRF52 UARTE — DMA writes directly to the buffer
}

// ============================================================
// jd_hw.h — power management
// ============================================================

extern "C" void pwr_enter_no_sleep(void) {
    // Called when jacdac wants to prevent the CPU from sleeping.
    // CODAL's WFI in microbit_hal_idle handles this; nothing extra needed.
}

// ============================================================
// jd_io.h — status indicator (LED blink/glow)
// Calliope has an RGB NeoPixel. Map blink codes to brief RGB flashes.
// We keep these lightweight — just clear after a brief indication.
// ============================================================

extern "C" void jd_blink(uint8_t encoded) {
    (void)encoded;
    // Brief blue flash to indicate jacdac activity
    microbit_hal_rgb_set_colors(0, 0, 0, 10);
}

extern "C" void jd_glow(uint32_t glow) {
    (void)glow;
    // Ignore glow patterns to reduce overhead
}

// jd_connected_blink is an extern uint8_t declared in jd_io.h — already defined
// by jacdac-c in jd_io.c. No definition needed here.

// ============================================================
// jd_wake_main: called from jacdac-c to wake the main thread.
// In our cooperative model, jd_process_everything() is called
// from microbit_hal_background_processing(), so mark pending.
// ============================================================

extern "C" void jd_wake_main(void) {
    jd_pending = true;
}
