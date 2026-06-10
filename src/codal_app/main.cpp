/*
 * This file is part of the MicroPython project, http://micropython.org/
 *
 * The MIT License (MIT)
 *
 * Copyright (c) 2020 Damien P. George
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "main.h"

#define MICROPY_TIMER_EVENT (0x1001)

extern "C" void mp_main(void);
extern "C" void m_printf(...);
extern "C" void microbit_hal_timer_callback(void);
extern "C" void microbit_hal_gesture_callback(int);
extern "C" void microbit_hal_sound_synth_callback(int);
extern "C" void microbit_radio_irq_handler(void);

MicroBit uBit;

void timer_handler(Event evt) {
    microbit_hal_timer_callback();
}

void gesture_event_handler(Event evt) {
    microbit_hal_gesture_callback(evt.value);
}

void sound_synth_event_handler(Event evt) {
    microbit_hal_sound_synth_callback(evt.value);
}

// ---------------------------------------------------------------------------
// CPU fault handlers (diagnostic for the BLE-enabled build).
//
// The nrfx startup vector table declares these weak (an endless `b .` spin),
// so a true CPU fault today hangs SILENTLY — the LED matrix goes dark with no
// panic face, which is indistinguishable from the render strobe merely
// stopping. These strong overrides route every fault to microbit_panic(),
// which bit-bangs a scrolling error code onto the matrix without needing the
// scheduler, so a fault becomes visible and can be told apart from a
// fault-free blank screen (e.g. a disabled TIMER4 strobe — see
// NVIC_EnableIRQ(TIMER4_IRQn) below).
//
// Diagnostic status codes:
//   150 = HardFault     151 = MemoryManagement
//   152 = BusFault      153 = UsageFault
// ---------------------------------------------------------------------------
extern "C" void HardFault_Handler(void)        { microbit_panic(150); }
extern "C" void MemoryManagement_Handler(void) { microbit_panic(151); }
extern "C" void BusFault_Handler(void)         { microbit_panic(152); }
extern "C" void UsageFault_Handler(void)       { microbit_panic(153); }

int main() {
    uBit.init();

    // Reconfigure the radio IRQ to our custom handler.
    // This must be done after uBit.init() in case BLE pairing mode is activated there.
    NVIC_SetVector(RADIO_IRQn, (uint32_t)microbit_radio_irq_handler);

    // Re-assert the LED-matrix render-strobe interrupt after BLE/SoftDevice
    // bring-up. With MICROBIT_BLE_ENABLED=1 the SoftDevice + Nordic SDK wrap
    // their critical sections in sd_nvic_critical_region_enter(), which runs
    // `NVIC->ICER[0] = <all application IRQs>` and on exit restores only the
    // set that was enabled when the region was entered (nrf_nvic.h). TIMER4_IRQn
    // drives the self-re-arming NRF52LedMatrix strobe and is enabled via bare
    // CMSIS NVIC_EnableIRQ() — i.e. outside the SoftDevice's __irq_masks
    // bookkeeping — so a critical region entered during boot-time BLE init can
    // snapshot it disabled and never re-enable it. The matrix then lights a
    // single display.show() frame and goes dark forever. Re-enabling it here,
    // once init() has run all the boot-time critical regions, restores the
    // strobe. Harmless on the radio build (no SDK critical region runs there).
    NVIC_EnableIRQ(TIMER4_IRQn);

    // Catch MemManage/BusFault/UsageFault as distinct exceptions instead of
    // letting them escalate to HardFault, so the diagnostic panic code above
    // identifies the fault class.
    SCB->SHCSR |= SCB_SHCSR_MEMFAULTENA_Msk | SCB_SHCSR_BUSFAULTENA_Msk | SCB_SHCSR_USGFAULTENA_Msk;

    // As well as configuring a larger RX buffer, this needs to be called so it
    // calls Serial::initialiseRx, to set up interrupts.
    uBit.serial.setRxBufferSize(128);

    uBit.messageBus.listen(MICROPY_TIMER_EVENT, DEVICE_EVT_ANY, timer_handler, MESSAGE_BUS_LISTENER_IMMEDIATE);
    uBit.messageBus.listen(DEVICE_ID_SERIAL, CODAL_SERIAL_EVT_DELIM_MATCH, serial_interrupt_handler, MESSAGE_BUS_LISTENER_IMMEDIATE);
    uBit.messageBus.listen(DEVICE_ID_GESTURE, DEVICE_EVT_ANY, gesture_event_handler);
    uBit.messageBus.listen(DEVICE_ID_SOUND_EMOJI_SYNTHESIZER_0, DEVICE_EVT_ANY, sound_synth_event_handler);

    // 6ms follows the micro:bit v1 value
    system_timer_event_every(6, MICROPY_TIMER_EVENT, 1);

    uBit.display.setBrightness(255);

    // By default the speaker is enabled but no pin is selected.  The audio system will
    // select the correct pin when any audio related code is first executed.
    uBit.audio.setSpeakerEnabled(true);
    uBit.audio.setPinEnabled(false);

    uBit.io.logo.isTouched(); // Calliope Workarround for pin3 is touched bug

    mp_main();
    return 0;
}
