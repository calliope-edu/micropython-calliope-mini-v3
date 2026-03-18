/*
 * Jacdac service implementations for Calliope mini V3 sensors.
 *
 * Exposes the on-board accelerometer, temperature sensor, and buttons A/B
 * as Jacdac services that are automatically advertised on the bus.
 *
 * The MIT License (MIT)
 * Copyright (c) 2025 Calliope gGmbH
 */

#include "jd_services.h"
#include "jd_sensor.h"
#include "microbithal.h"

// Generated Jacdac service spec headers
#include "jacdac/dist/c/accelerometer.h"
#include "jacdac/dist/c/temperature.h"
#include "jacdac/dist/c/button.h"

// Shared no-op sensor API — all services update their readings directly
// in process(), so get_reading is never called via the framework.
// A valid api pointer is still required to avoid NULL dereferences in
// sensor_handle_packet() when optional registers (range, sleep) are queried.
static void *noop_get_reading(void) { return NULL; }
static const sensor_api_t calliope_noop_sensor_api = {
    .get_reading = noop_get_reading,
};

// ============================================================
// Temperature Service (service class 0x1421bac7)
// ============================================================

typedef struct {
    SENSOR_COMMON;
    int32_t reading; // i22.10 fixed point (degrees C * 1024)
} calliope_temp_state_t;

static void calliope_temperature_process(srv_t *_state) {
    calliope_temp_state_t *state = (calliope_temp_state_t *)_state;

    // CODAL caches the reading internally; this is just a field read.
    state->reading = microbit_hal_temperature() << 10; // i22.10

    sensor_process_simple(_state, &state->reading, sizeof(state->reading));
}

static void calliope_temperature_handle_packet(srv_t *_state, jd_packet_t *pkt) {
    calliope_temp_state_t *state = (calliope_temp_state_t *)_state;

    int r = sensor_handle_packet_simple(_state, pkt, &state->reading, sizeof(state->reading));

    // Constant registers — only sent on request, no RAM cost
    if (r == 0) {
        switch (pkt->service_command) {
        case JD_GET(JD_TEMPERATURE_REG_MIN_TEMPERATURE): {
            int32_t v = -40 << 10;
            jd_send(pkt->service_index, pkt->service_command, &v, 4);
            break;
        }
        case JD_GET(JD_TEMPERATURE_REG_MAX_TEMPERATURE): {
            int32_t v = 105 << 10;
            jd_send(pkt->service_index, pkt->service_command, &v, 4);
            break;
        }
        case JD_GET(JD_TEMPERATURE_REG_TEMPERATURE_ERROR): {
            uint32_t v = 2 << 10;
            jd_send(pkt->service_index, pkt->service_command, &v, 4);
            break;
        }
        case JD_GET(JD_TEMPERATURE_REG_VARIANT):
            service_handle_variant(pkt, JD_TEMPERATURE_VARIANT_INDOOR);
            return;
        }
    }
}

SRV_DEF_SZ(calliope_temperature, JD_SERVICE_CLASS_TEMPERATURE, sizeof(calliope_temp_state_t));

void calliope_temperature_init(void) {
    SRV_ALLOC(calliope_temperature);
    calliope_temp_state_t *ts = (calliope_temp_state_t *)state;
    ts->api = &calliope_noop_sensor_api;
    ts->streaming_interval = 1000;
}

// ============================================================
// Accelerometer Service (service class 0x1f140409)
// ============================================================

typedef struct {
    SENSOR_COMMON;
    jd_accelerometer_forces_t forces; // x, y, z in i12.20
} calliope_accel_state_t;

// Convert milli-g to Jacdac i12.20 using only 32-bit arithmetic.
// Exact factor is 1048576/1000 = 1048.576.
// Using 1049 gives < 0.05% error — well within MEMS noise.
// Max input ±8000 mg → ±8,392,000 which fits int32_t.
static inline int32_t mg_to_i12_20(int mg) {
    return mg * 1049;
}

static void calliope_accelerometer_process(srv_t *_state) {
    calliope_accel_state_t *state = (calliope_accel_state_t *)_state;

    int axis[3];
    microbit_hal_accelerometer_get_sample(axis);

    state->forces.x = mg_to_i12_20(axis[0]);
    state->forces.y = mg_to_i12_20(axis[1]);
    state->forces.z = mg_to_i12_20(axis[2]);

    sensor_process_simple(_state, &state->forces, sizeof(state->forces));
}

static void calliope_accelerometer_handle_packet(srv_t *_state, jd_packet_t *pkt) {
    calliope_accel_state_t *state = (calliope_accel_state_t *)_state;
    sensor_handle_packet_simple(_state, pkt, &state->forces, sizeof(state->forces));
}

SRV_DEF_SZ(calliope_accelerometer, JD_SERVICE_CLASS_ACCELEROMETER, sizeof(calliope_accel_state_t));

void calliope_accelerometer_init(void) {
    SRV_ALLOC(calliope_accelerometer);
    calliope_accel_state_t *as = (calliope_accel_state_t *)state;
    as->api = &calliope_noop_sensor_api;
    as->streaming_interval = 50; // 20 Hz
}

// ============================================================
// Button Service (service class 0x1473a263)
// ============================================================

typedef struct {
    SENSOR_COMMON;
    uint16_t pressure;      // u0.16: 0 = released, 0xFFFF = pressed
    uint8_t button_index;   // 0 = Button A, 1 = Button B
    uint8_t prev_pressed;   // previous state for edge detection
    uint32_t press_start;   // timestamp when button was pressed (us)
    uint32_t next_hold;     // next hold event timestamp (us)
} calliope_button_state_t;

#define BUTTON_HOLD_MS 500

static void calliope_button_process(srv_t *_state) {
    calliope_button_state_t *state = (calliope_button_state_t *)_state;

    int pressed = microbit_hal_button_state(state->button_index, NULL, NULL);
    state->pressure = pressed ? 0xFFFF : 0;

    if (pressed && !state->prev_pressed) {
        // Button down
        state->press_start = now;
        state->next_hold = now + (BUTTON_HOLD_MS * 1000);
        jd_send_event(_state, JD_BUTTON_EV_DOWN);
    } else if (!pressed && state->prev_pressed) {
        // Button up — include hold duration
        uint32_t hold_ms = (now - state->press_start) / 1000;
        jd_send_event_ext(_state, JD_BUTTON_EV_UP, &hold_ms, sizeof(hold_ms));
    } else if (pressed && state->prev_pressed && in_past(state->next_hold)) {
        // Hold repeat
        uint32_t hold_ms = (now - state->press_start) / 1000;
        jd_send_event_ext(_state, JD_BUTTON_EV_HOLD, &hold_ms, sizeof(hold_ms));
        state->next_hold = now + (BUTTON_HOLD_MS * 1000);
    }

    state->prev_pressed = pressed;
    sensor_process_simple(_state, &state->pressure, sizeof(state->pressure));
}

static void calliope_button_handle_packet(srv_t *_state, jd_packet_t *pkt) {
    calliope_button_state_t *state = (calliope_button_state_t *)_state;
    sensor_handle_packet_simple(_state, pkt, &state->pressure, sizeof(state->pressure));
}

SRV_DEF_SZ(calliope_button, JD_SERVICE_CLASS_BUTTON, sizeof(calliope_button_state_t));

void calliope_button_init(int button_index) {
    SRV_ALLOC(calliope_button);
    calliope_button_state_t *bs = (calliope_button_state_t *)state;
    bs->api = &calliope_noop_sensor_api;
    bs->button_index = button_index;
    bs->streaming_interval = 100;
}
