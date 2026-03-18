/*
 * MicroPython RGB LED object for Calliope mini V3.
 *
 * Exposes the 3 on-board WS2812B NeoPixels as calliopemini.led
 * with an API compatible with the Calliope mini V1/V2 MicroPython.
 *
 * Usage:
 *   led.set_colors(255, 0, 0)       # all 3 LEDs red
 *   led.set_colors(255, 0, 0, 1)    # LED 1 red (0-indexed)
 *   led.get_colors()                 # get LED 0 colors as (r, g, b)
 *   led.get_colors(2)               # get LED 2 colors
 *   led.set_red(128)                # all LEDs red component = 128
 *   led.set_red(128, 0)             # LED 0 red component = 128
 *   led.clear()                     # all LEDs off
 *
 * Copyright (c) 2025 Calliope gGmbH
 * SPDX-License-Identifier: MIT
 */

#include "py/runtime.h"
#include "py/mphal.h"

// HAL function declarations (implemented in microbithal.cpp)
extern void microbit_hal_rgb_set_colors(int led_index, int r, int g, int b);
extern void microbit_hal_rgb_get_colors(int led_index, int *r, int *g, int *b);
extern void microbit_hal_rgb_clear(void);

#define RGB_LED_COUNT 3

typedef struct _calliope_led_obj_t {
    mp_obj_base_t base;
} calliope_led_obj_t;

// Helper: get optional LED index from args, default to -1 (all LEDs)
static int get_led_index_or_all(size_t n_args, const mp_obj_t *args, int arg_pos) {
    if (n_args > (size_t)arg_pos) {
        int idx = mp_obj_get_int(args[arg_pos]);
        if (idx < 0 || idx >= RGB_LED_COUNT) {
            mp_raise_ValueError(MP_ERROR_TEXT("led index must be 0, 1, or 2"));
        }
        return idx;
    }
    return -1; // all LEDs
}

// Helper: get optional LED index from args, default to 0
static int get_led_index_or_zero(size_t n_args, const mp_obj_t *args, int arg_pos) {
    if (n_args > (size_t)arg_pos) {
        int idx = mp_obj_get_int(args[arg_pos]);
        if (idx < 0 || idx >= RGB_LED_COUNT) {
            mp_raise_ValueError(MP_ERROR_TEXT("led index must be 0, 1, or 2"));
        }
        return idx;
    }
    return 0;
}

// --- Individual color setters: set_red(val [, led_index]) ---

static mp_obj_t calliope_led_set_red(size_t n_args, const mp_obj_t *args) {
    // args[0]=self, args[1]=value, args[2]=optional led_index
    int val = mp_obj_get_int(args[1]);
    int idx = get_led_index_or_all(n_args, args, 2);
    if (idx < 0) {
        for (int i = 0; i < RGB_LED_COUNT; i++) {
            int r, g, b;
            microbit_hal_rgb_get_colors(i, &r, &g, &b);
            microbit_hal_rgb_set_colors(i, val, g, b);
        }
    } else {
        int r, g, b;
        microbit_hal_rgb_get_colors(idx, &r, &g, &b);
        microbit_hal_rgb_set_colors(idx, val, g, b);
    }
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(calliope_led_set_red_obj, 2, 3, calliope_led_set_red);

static mp_obj_t calliope_led_set_green(size_t n_args, const mp_obj_t *args) {
    int val = mp_obj_get_int(args[1]);
    int idx = get_led_index_or_all(n_args, args, 2);
    if (idx < 0) {
        for (int i = 0; i < RGB_LED_COUNT; i++) {
            int r, g, b;
            microbit_hal_rgb_get_colors(i, &r, &g, &b);
            microbit_hal_rgb_set_colors(i, r, val, b);
        }
    } else {
        int r, g, b;
        microbit_hal_rgb_get_colors(idx, &r, &g, &b);
        microbit_hal_rgb_set_colors(idx, r, val, b);
    }
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(calliope_led_set_green_obj, 2, 3, calliope_led_set_green);

static mp_obj_t calliope_led_set_blue(size_t n_args, const mp_obj_t *args) {
    int val = mp_obj_get_int(args[1]);
    int idx = get_led_index_or_all(n_args, args, 2);
    if (idx < 0) {
        for (int i = 0; i < RGB_LED_COUNT; i++) {
            int r, g, b;
            microbit_hal_rgb_get_colors(i, &r, &g, &b);
            microbit_hal_rgb_set_colors(i, r, g, val);
        }
    } else {
        int r, g, b;
        microbit_hal_rgb_get_colors(idx, &r, &g, &b);
        microbit_hal_rgb_set_colors(idx, r, g, val);
    }
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(calliope_led_set_blue_obj, 2, 3, calliope_led_set_blue);

// --- Individual color getters: get_red([led_index]) ---

static mp_obj_t calliope_led_get_red(size_t n_args, const mp_obj_t *args) {
    int idx = get_led_index_or_zero(n_args, args, 1);
    int r, g, b;
    microbit_hal_rgb_get_colors(idx, &r, &g, &b);
    return mp_obj_new_int(r);
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(calliope_led_get_red_obj, 1, 2, calliope_led_get_red);

static mp_obj_t calliope_led_get_green(size_t n_args, const mp_obj_t *args) {
    int idx = get_led_index_or_zero(n_args, args, 1);
    int r, g, b;
    microbit_hal_rgb_get_colors(idx, &r, &g, &b);
    return mp_obj_new_int(g);
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(calliope_led_get_green_obj, 1, 2, calliope_led_get_green);

static mp_obj_t calliope_led_get_blue(size_t n_args, const mp_obj_t *args) {
    int idx = get_led_index_or_zero(n_args, args, 1);
    int r, g, b;
    microbit_hal_rgb_get_colors(idx, &r, &g, &b);
    return mp_obj_new_int(b);
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(calliope_led_get_blue_obj, 1, 2, calliope_led_get_blue);

// --- Bulk color set/get ---
// set_colors(r, g, b [, led_index])   — default: all LEDs
// get_colors([led_index])             — default: LED 0

static mp_obj_t calliope_led_set_colors(size_t n_args, const mp_obj_t *args) {
    // args[0]=self, args[1]=r, args[2]=g, args[3]=b, args[4]=optional led_index
    int r = mp_obj_get_int(args[1]);
    int g = mp_obj_get_int(args[2]);
    int b = mp_obj_get_int(args[3]);
    int idx = get_led_index_or_all(n_args, args, 4);
    microbit_hal_rgb_set_colors(idx, r, g, b);
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(calliope_led_set_colors_obj, 4, 5, calliope_led_set_colors);

static mp_obj_t calliope_led_get_colors(size_t n_args, const mp_obj_t *args) {
    int idx = get_led_index_or_zero(n_args, args, 1);
    int r, g, b;
    microbit_hal_rgb_get_colors(idx, &r, &g, &b);
    mp_obj_t items[3] = {
        mp_obj_new_int(r),
        mp_obj_new_int(g),
        mp_obj_new_int(b),
    };
    return mp_obj_new_tuple(3, items);
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(calliope_led_get_colors_obj, 1, 2, calliope_led_get_colors);

// --- Clear ---

static mp_obj_t calliope_led_clear(mp_obj_t self_in) {
    microbit_hal_rgb_clear();
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_1(calliope_led_clear_obj, calliope_led_clear);

// --- Type definition ---

static const mp_rom_map_elem_t calliope_led_locals_dict_table[] = {
    { MP_ROM_QSTR(MP_QSTR_set_red), MP_ROM_PTR(&calliope_led_set_red_obj) },
    { MP_ROM_QSTR(MP_QSTR_set_green), MP_ROM_PTR(&calliope_led_set_green_obj) },
    { MP_ROM_QSTR(MP_QSTR_set_blue), MP_ROM_PTR(&calliope_led_set_blue_obj) },
    { MP_ROM_QSTR(MP_QSTR_get_red), MP_ROM_PTR(&calliope_led_get_red_obj) },
    { MP_ROM_QSTR(MP_QSTR_get_green), MP_ROM_PTR(&calliope_led_get_green_obj) },
    { MP_ROM_QSTR(MP_QSTR_get_blue), MP_ROM_PTR(&calliope_led_get_blue_obj) },
    { MP_ROM_QSTR(MP_QSTR_set_colors), MP_ROM_PTR(&calliope_led_set_colors_obj) },
    { MP_ROM_QSTR(MP_QSTR_get_colors), MP_ROM_PTR(&calliope_led_get_colors_obj) },
    { MP_ROM_QSTR(MP_QSTR_clear), MP_ROM_PTR(&calliope_led_clear_obj) },
    // LED count constant
    { MP_ROM_QSTR(MP_QSTR_COUNT), MP_ROM_INT(RGB_LED_COUNT) },
};
static MP_DEFINE_CONST_DICT(calliope_led_locals_dict, calliope_led_locals_dict_table);

MP_DEFINE_CONST_OBJ_TYPE(
    calliope_led_type,
    MP_QSTR_CalliopeLED,
    MP_TYPE_FLAG_NONE,
    locals_dict, &calliope_led_locals_dict
);

const calliope_led_obj_t calliope_led_obj = {
    {&calliope_led_type},
};
