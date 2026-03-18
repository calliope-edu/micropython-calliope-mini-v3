/*
 * MicroPython motor driver for Calliope mini V3.
 *
 * Exposes the on-board DRV8835 dual H-bridge motor driver.
 * Motor A and Motor B can be independently controlled with
 * speed from -100 (full reverse) to +100 (full forward).
 *
 * Python API:
 *   from calliopemini import *
 *   motor.on(motor.A, speed=50)    # Motor A forward at 50%
 *   motor.on(motor.B, speed=-100)  # Motor B full reverse
 *   motor.off(motor.A)             # Motor A coast (power off)
 *   motor.on(motor.A, speed=0)     # Motor A brake (active stop)
 *
 * Copyright (c) 2025 Calliope gGmbH
 * SPDX-License-Identifier: MIT
 */

#include "py/runtime.h"
#include "py/mphal.h"

// HAL function declarations (implemented in microbithal.cpp)
extern void microbit_hal_motor_on(int motor, int speed);
extern void microbit_hal_motor_off(int motor);

typedef struct _calliope_motor_obj_t {
    mp_obj_base_t base;
} calliope_motor_obj_t;

// --- motor.on(motor, speed) ---

static mp_obj_t calliope_motor_on(mp_obj_t self_in, mp_obj_t motor_in, mp_obj_t speed_in) {
    int motor = mp_obj_get_int(motor_in);
    int speed = mp_obj_get_int(speed_in);
    if (motor < 0 || motor > 1) {
        mp_raise_ValueError(MP_ERROR_TEXT("motor must be motor.A (0) or motor.B (1)"));
    }
    if (speed < -100 || speed > 100) {
        mp_raise_ValueError(MP_ERROR_TEXT("speed must be between -100 and 100"));
    }
    microbit_hal_motor_on(motor, speed);
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_3(calliope_motor_on_obj, calliope_motor_on);

// --- motor.off(motor) ---

static mp_obj_t calliope_motor_off(mp_obj_t self_in, mp_obj_t motor_in) {
    int motor = mp_obj_get_int(motor_in);
    if (motor < 0 || motor > 1) {
        mp_raise_ValueError(MP_ERROR_TEXT("motor must be motor.A (0) or motor.B (1)"));
    }
    microbit_hal_motor_off(motor);
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_2(calliope_motor_off_obj, calliope_motor_off);

// --- Type definition ---

static const mp_rom_map_elem_t calliope_motor_locals_dict_table[] = {
    { MP_ROM_QSTR(MP_QSTR_on), MP_ROM_PTR(&calliope_motor_on_obj) },
    { MP_ROM_QSTR(MP_QSTR_off), MP_ROM_PTR(&calliope_motor_off_obj) },
    // Motor index constants
    { MP_ROM_QSTR(MP_QSTR_A), MP_ROM_INT(0) },
    { MP_ROM_QSTR(MP_QSTR_B), MP_ROM_INT(1) },
};
static MP_DEFINE_CONST_DICT(calliope_motor_locals_dict, calliope_motor_locals_dict_table);

MP_DEFINE_CONST_OBJ_TYPE(
    calliope_motor_type,
    MP_QSTR_CalliopeMotor,
    MP_TYPE_FLAG_NONE,
    locals_dict, &calliope_motor_locals_dict
);

const calliope_motor_obj_t calliope_motor_obj = {
    {&calliope_motor_type},
};
