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
#include "microbithal.h"
#include "MicroBitDevice.h"
#include "neopixel.h"

#define HAL_ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))

// It's not possible to include the CODAL header file that defines the SFX_DEFAULT_xxx
// constants in C code, because that CODAL header file is C++.  Instead we define our
// own MICROBIT_HAL_SFX_DEFAULT_xxx versions of the constants in a C-compatible header
// file, and assert here that they have the same value as the CODAL constants.
static_assert(MICROBIT_HAL_SFX_DEFAULT_VIBRATO_PARAM == SFX_DEFAULT_VIBRATO_PARAM, "");
static_assert(MICROBIT_HAL_SFX_DEFAULT_VIBRATO_STEPS == SFX_DEFAULT_VIBRATO_STEPS, "");
static_assert(MICROBIT_HAL_SFX_DEFAULT_TREMOLO_PARAM == SFX_DEFAULT_TREMOLO_PARAM, "");
static_assert(MICROBIT_HAL_SFX_DEFAULT_TREMOLO_STEPS == SFX_DEFAULT_TREMOLO_STEPS, "");
static_assert(MICROBIT_HAL_SFX_DEFAULT_WARBLE_PARAM == SFX_DEFAULT_WARBLE_PARAM, "");
static_assert(MICROBIT_HAL_SFX_DEFAULT_WARBLE_STEPS == SFX_DEFAULT_WARBLE_STEPS, "");

NRF52Pin *const pin_obj[] = {
    &uBit.io.P0,
    &uBit.io.P1,
    &uBit.io.P2,
    &uBit.io.P3,
    &uBit.io.P4,
    &uBit.io.P5,
    &uBit.io.P6,
    &uBit.io.P7,
    &uBit.io.P8,
    &uBit.io.P9,
    &uBit.io.P10,
    &uBit.io.P11,
    &uBit.io.P12,
    &uBit.io.P13,
    &uBit.io.P14,
    &uBit.io.P15,
    &uBit.io.A1RX, // Calliope renamed
    &uBit.io.A0SCL, // external I2C SCL // Calliope renamed
    &uBit.io.A0SDA, // external I2C SDA // Calliope renamed
    &uBit.io.logo,
    &uBit.io.speaker,
    &uBit.io.runmic,
    &uBit.io.microphone,
    &uBit.io.sda, // internal I2C
    &uBit.io.scl, // internal I2C
    &uBit.io.row1,
    &uBit.io.row2,
    &uBit.io.row3,
    &uBit.io.row4,
    &uBit.io.row5,
    &uBit.io.usbTx,
    &uBit.io.usbRx,
    &uBit.io.irq1,
    &uBit.io.A1TX, // Calliope renamed
    &uBit.io.P18, // Calliope added
    &uBit.io.RGB, // Calliope added
    &uBit.io.M_A_IN1, // Calliope added
    &uBit.io.M_A_IN2, // Calliope added
    &uBit.io.M_B_IN1, // Calliope added
    &uBit.io.M_B_IN2, // Calliope added
    &uBit.io.M_MODE, // Calliope added
};

static Button *const button_obj[] = {
    &uBit.buttonA,
    &uBit.buttonB,
};

static const PullMode pin_pull_mode_mapping[] = {
    PullMode::Up,
    PullMode::Down,
    PullMode::None,
};

static uint8_t pin_pull_state[32 + 6];
static uint16_t touch_state[4];
static uint16_t button_state[2];

extern "C" {

void microbit_hal_background_processing(void) {
    // This call takes about 200us.
    Event(DEVICE_ID_SCHEDULER, DEVICE_SCHEDULER_EVT_IDLE);
}

void microbit_hal_idle(void) {
    microbit_hal_background_processing();
    __WFI();
}

__attribute__((noreturn)) void microbit_hal_reset(void) {
    microbit_reset();
}

void microbit_hal_panic(int code) {
    microbit_panic(code);
}

int microbit_hal_temperature(void) {
    return uBit.thermometer.getTemperature();
}

void microbit_hal_power_clear_wake_sources(void) {
    for (size_t i = 0; i < HAL_ARRAY_SIZE(pin_obj); ++i) {
        microbit_hal_power_wake_on_pin(i, false);
    }
    for (size_t i = 0; i < HAL_ARRAY_SIZE(button_obj); ++i) {
        microbit_hal_power_wake_on_button(i, false);
    }
}

void microbit_hal_power_wake_on_button(int button, bool wake_on_active) {
    button_obj[button]->wakeOnActive(wake_on_active);
}

void microbit_hal_power_wake_on_pin(int pin, bool wake_on_active) {
    pin_obj[pin]->wakeOnActive(wake_on_active);
}

void microbit_hal_power_off(void) {
    uBit.power.off();
}

bool microbit_hal_power_deep_sleep(bool wake_on_ms, uint32_t ms) {
    if (wake_on_ms) {
        return uBit.power.deepSleep(ms, true);
    } else {
        uBit.power.deepSleep();
        return true; // Sleep was interrupted by a wake event.
    }
}

void microbit_hal_pin_set_pull(int pin, int pull) {
    pin_obj[pin]->setPull(pin_pull_mode_mapping[pull]);
    pin_pull_state[pin] = pull;
}

int microbit_hal_pin_get_pull(int pin) {
    return pin_pull_state[pin];
}

int microbit_hal_pin_set_analog_period_us(int pin, int period) {
    // Change the audio virtual-pin period if the pin is the special mixer pin.
    if (pin == MICROBIT_HAL_PIN_MIXER) {
        uBit.audio.virtualOutputPin.setAnalogPeriodUs(period);
        return 0;
    }

    // Calling setAnalogPeriodUs requires the pin to be in analog-out mode.  So
    // test for this mode by first calling getAnalogPeriodUs, and if it fails then
    // attempt to configure the pin in analog-out mode by calling setAnalogValue.
    if ((ErrorCode)pin_obj[pin]->getAnalogPeriodUs() == DEVICE_NOT_SUPPORTED) {
        if (pin_obj[pin]->setAnalogValue(0) != DEVICE_OK) {
            return -1;
        }
    }

    // Set the analog period.
    if (pin_obj[pin]->setAnalogPeriodUs(period) == DEVICE_OK) {
        return 0;
    } else {
        return -1;
    }
}

int microbit_hal_pin_get_analog_period_us(int pin) {
    int period = pin_obj[pin]->getAnalogPeriodUs();
    if (period != DEVICE_NOT_SUPPORTED) {
        return period;
    } else {
        return -1;
    }
}

void microbit_hal_pin_set_touch_mode(int pin, int mode) {
    pin_obj[pin]->isTouched((TouchMode)mode);
}

int microbit_hal_pin_read(int pin) {
    return pin_obj[pin]->getDigitalValue();
}

void microbit_hal_pin_write(int pin, int value) {
    pin_obj[pin]->setDigitalValue(value);
}

int microbit_hal_pin_read_analog_u10(int pin) {
    return pin_obj[pin]->getAnalogValue();
}

void microbit_hal_pin_write_analog_u10(int pin, int value) {
    if (pin == MICROBIT_HAL_PIN_MIXER) {
        uBit.audio.virtualOutputPin.setAnalogValue(value);
        return;
    }
    pin_obj[pin]->setAnalogValue(value);
}

void microbit_hal_pin_touch_calibrate(int pin) {
    pin_obj[pin]->touchCalibrate();
}

int microbit_hal_pin_touch_state(int pin, int *was_touched, int *num_touches) {
    if (was_touched != NULL || num_touches != NULL) {
        int pin_state_index;
        if (pin == MICROBIT_HAL_PIN_LOGO) {
            pin_state_index = 3;
        } else {
            pin_state_index = pin; // pin0/1/2
        }
        int t = pin_obj[pin]->wasTouched();
        uint16_t state = touch_state[pin_state_index];
        if (t) {
            // Update state based on number of touches since last call.
            // Low bit is "was touched at least once", upper bits are "number of touches".
            state = (state + (t << 1)) | 1;
        }
        if (was_touched != NULL) {
            *was_touched = state & 1;
            state &= ~1;
        }
        if (num_touches != NULL) {
            *num_touches = state >> 1;
            state &= 1;
        }
        touch_state[pin_state_index] = state;
    }

    return pin_obj[pin]->isTouched();
}

void microbit_hal_pin_write_ws2812(int pin, const uint8_t *buf, size_t len) {
    neopixel_send_buffer(*pin_obj[pin], buf, len);
}

int microbit_hal_i2c_init(int scl, int sda, int freq) {
    int ret = uBit.i2c.redirect(*pin_obj[sda], *pin_obj[scl]);
    if (ret != DEVICE_OK) {
        return ret;
    }
    ret = uBit.i2c.setFrequency(freq);
    if (ret != DEVICE_OK) {
        return ret;
    }
    return 0;
}

int microbit_hal_i2c_readfrom(uint8_t addr, uint8_t *buf, size_t len, int stop) {
    int ret = uBit.i2c.read(addr << 1, (uint8_t *)buf, len, !stop);
    if (ret != DEVICE_OK) {
        return ret;
    }
    return 0;
}

int microbit_hal_i2c_writeto(uint8_t addr, const uint8_t *buf, size_t len, int stop) {
    int ret = uBit.i2c.write(addr << 1, (uint8_t *)buf, len, !stop);
    if (ret != DEVICE_OK) {
        return ret;
    }
    return 0;
}

int microbit_hal_uart_init(int tx, int rx, int baudrate, int bits, int parity, int stop) {
    // TODO set bits, parity stop
    int ret = uBit.serial.redirect(*pin_obj[tx], *pin_obj[rx]);
    if (ret != DEVICE_OK) {
        return ret;
    }
    ret = uBit.serial.setBaud(baudrate);
    if (ret != DEVICE_OK) {
        return ret;
    }
    return 0;
}

static NRF52SPI *spi = NULL;

int microbit_hal_spi_init(int sclk, int mosi, int miso, int frequency, int bits, int mode) {
    int ret;
    if (spi == NULL) {
        spi = new NRF52SPI(*pin_obj[mosi], *pin_obj[miso], *pin_obj[sclk], NRF_SPIM2);
    } else {
        ret = spi->redirect(*pin_obj[mosi], *pin_obj[miso], *pin_obj[sclk]);
        if (ret != DEVICE_OK) {
            return ret;
        }
    }
    ret = spi->setFrequency(frequency);
    if (ret != DEVICE_OK) {
        return ret;
    }
    ret = spi->setMode(mode, bits);
    if (ret != DEVICE_OK) {
        return ret;
    }
    return 0;
}

int microbit_hal_spi_transfer(size_t len, const uint8_t *src, uint8_t *dest) {
    int ret;
    if (dest == NULL) {
        ret = spi->transfer(src, len, NULL, 0);
    } else {
        ret = spi->transfer(src, len, dest, len);
    }
    return ret;
}

int microbit_hal_button_state(int button, int *was_pressed, int *num_presses) {
    Button *b = button_obj[button];
    if (was_pressed != NULL || num_presses != NULL) {
        uint16_t state = button_state[button];
        int p = b->wasPressed();
        if (p) {
            // Update state based on number of presses since last call.
            // Low bit is "was pressed at least once", upper bits are "number of presses".
            state = (state + (p << 1)) | 1;
        }
        if (was_pressed != NULL) {
            *was_pressed = state & 1;
            state &= ~1;
        }
        if (num_presses != NULL) {
            *num_presses = state >> 1;
            state &= 1;
        }
        button_state[button] = state;
    }
    return b->isPressed();
}

void microbit_hal_display_enable(int value) {
    if (value) {
        uBit.display.enable();
    } else {
        uBit.display.disable();
    }
}

int microbit_hal_display_get_pixel(int x, int y) {
    uint32_t pixel = uBit.display.image.getPixelValue(x, y);
    if (pixel == 255) {
        return 9;
    } else {
        return 32 - __builtin_clz(pixel);
    }
}

void microbit_hal_display_set_pixel(int x, int y, int bright) {
    // This mapping is designed to give a set of 10 visually distinct levels.
    static uint8_t bright_map[10] = { 0, 1, 2, 4, 8, 16, 32, 64, 128, 255 };
    if (bright < 0) {
        bright = 0;
    } else if (bright > 9) {
        bright = 9;
    }
    uBit.display.image.setPixelValue(x, y, bright_map[bright]);
}

int microbit_hal_display_read_light_level(void) {
    return uBit.display.readLightLevel();
}

void microbit_hal_display_rotate(unsigned int rotation) {
    static DisplayRotation angle_map[4] = {
        MATRIX_DISPLAY_ROTATION_0,
        MATRIX_DISPLAY_ROTATION_90,
        MATRIX_DISPLAY_ROTATION_180,
        MATRIX_DISPLAY_ROTATION_270,
    };
    uBit.display.rotateTo(angle_map[rotation & 3]);
}

void microbit_hal_accelerometer_get_sample(int axis[3]) {
    Sample3D sample = uBit.accelerometer.getSample();
    axis[0] = sample.x;
    axis[1] = sample.y;
    axis[2] = sample.z;
}

int microbit_hal_accelerometer_get_gesture(void) {
    return uBit.accelerometer.getGesture();
}

void microbit_hal_accelerometer_set_range(int r) {
    uBit.accelerometer.setRange(r);
}

int microbit_hal_compass_is_calibrated(void) {
    return uBit.compass.isCalibrated();
}

void microbit_hal_compass_clear_calibration(void) {
    uBit.compass.clearCalibration();
}

void microbit_hal_compass_calibrate(void) {
    uBit.compass.calibrate();
}

void microbit_hal_compass_get_sample(int axis[3]) {
    Sample3D sample = uBit.compass.getSample();
    axis[0] = sample.x;
    axis[1] = sample.y;
    axis[2] = sample.z;
}

int microbit_hal_compass_get_field_strength(void) {
    return uBit.compass.getFieldStrength();
}

int microbit_hal_compass_get_heading(void) {
    return uBit.compass.heading();
}

const uint8_t *microbit_hal_get_font_data(char c) {
    return BitmapFont::getSystemFont().get(c);
}

static int microbit_hal_log_convert_return_value(int result) {
    if (result == DEVICE_OK) {
        return MICROBIT_HAL_DEVICE_OK;
    } else if (result == DEVICE_NO_RESOURCES) {
        return MICROBIT_HAL_DEVICE_NO_RESOURCES;
    } else {
        return MICROBIT_HAL_DEVICE_ERROR;
    }
}

void microbit_hal_log_delete(bool full_erase) {
    uBit.log.clear(full_erase);
}

void microbit_hal_log_set_mirroring(bool serial) {
    uBit.log.setSerialMirroring(serial);
}

void microbit_hal_log_set_timestamp(int period) {
    static_assert(MICROBIT_HAL_LOG_TIMESTAMP_NONE == (int)TimeStampFormat::None);
    static_assert(MICROBIT_HAL_LOG_TIMESTAMP_MILLISECONDS == (int)TimeStampFormat::Milliseconds);
    static_assert(MICROBIT_HAL_LOG_TIMESTAMP_SECONDS == (int)TimeStampFormat::Seconds);
    static_assert(MICROBIT_HAL_LOG_TIMESTAMP_MINUTES == (int)TimeStampFormat::Minutes);
    static_assert(MICROBIT_HAL_LOG_TIMESTAMP_HOURS == (int)TimeStampFormat::Hours);
    static_assert(MICROBIT_HAL_LOG_TIMESTAMP_DAYS == (int)TimeStampFormat::Days);
    uBit.log.setTimeStamp((TimeStampFormat)period);
}

int microbit_hal_log_begin_row(void) {
    return microbit_hal_log_convert_return_value(uBit.log.beginRow());
}

int microbit_hal_log_end_row(void) {
    return microbit_hal_log_convert_return_value(uBit.log.endRow());
}

int microbit_hal_log_data(const char *key, const char *value) {
    return microbit_hal_log_convert_return_value(uBit.log.logData(key, value));
}

// This is needed by the microbitfs implementation.
uint32_t rng_generate_random_word(void) {
    return uBit.random(65536) << 16 | uBit.random(65536);
}

}
