/*
 * MicroPython module for Jacdac on Calliope mini V3.
 *
 * Provides:
 *   import jacdac
 *   jacdac.start()         - Start Jacdac bus
 *   jacdac.stop()          - Stop Jacdac bus
 *   jacdac.is_running()    - Check if bus is active
 *   jacdac.send(data)      - Send raw packet bytes
 *   jacdac.receive()       - Receive next raw packet (or None)
 *   jacdac.process()       - Process pending Jacdac events
 *   jacdac.device_id()     - Get this device's 64-bit Jacdac ID
 *   jacdac.diagnostics()   - Get bus diagnostics dict
 *   jacdac.scan()          - Discover connected Jacdac devices
 *   jacdac.num_devices()   - Get number of connected devices
 *
 * The MIT License (MIT)
 * Copyright (c) 2025 Calliope gGmbH
 */

#include <string.h>

#include "py/runtime.h"
#include "py/mphal.h"
#include "py/obj.h"
#include "py/objstr.h"
#include "py/objlist.h"

#include "microbithal.h"

// Include jacdac-c headers for proper type definitions and config
#include "jd_protocol.h"

#if JD_CLIENT
#include "jd_client.h"
#endif

// State tracking
static bool jacdac_initialized = false;

// --- Module functions ---

static mp_obj_t mod_jacdac_start(void) {
    if (!jacdac_initialized) {
        microbit_hal_jacdac_start();
        jd_init();
        jacdac_initialized = true;
    } else {
        microbit_hal_jacdac_start();
    }
    return mp_const_none;
}
MP_DEFINE_CONST_FUN_OBJ_0(mod_jacdac_start_obj, mod_jacdac_start);

static mp_obj_t mod_jacdac_stop(void) {
    microbit_hal_jacdac_stop();
    return mp_const_none;
}
MP_DEFINE_CONST_FUN_OBJ_0(mod_jacdac_stop_obj, mod_jacdac_stop);

static mp_obj_t mod_jacdac_is_running(void) {
    return mp_obj_new_bool(microbit_hal_jacdac_is_running());
}
MP_DEFINE_CONST_FUN_OBJ_0(mod_jacdac_is_running_obj, mod_jacdac_is_running);

static mp_obj_t mod_jacdac_process(void) {
    if (!jacdac_initialized) {
        mp_raise_msg(&mp_type_RuntimeError, MP_ERROR_TEXT("jacdac not started"));
    }
    jd_process_everything();
    return mp_const_none;
}
MP_DEFINE_CONST_FUN_OBJ_0(mod_jacdac_process_obj, mod_jacdac_process);

static mp_obj_t mod_jacdac_device_id(void) {
    uint64_t id = hw_device_id();
    // Return as a 8-byte bytes object
    uint8_t buf[8];
    memcpy(buf, &id, 8);
    return mp_obj_new_bytes(buf, 8);
}
MP_DEFINE_CONST_FUN_OBJ_0(mod_jacdac_device_id_obj, mod_jacdac_device_id);

#if JD_RAW_FRAME
static mp_obj_t mod_jacdac_send(mp_obj_t data_in) {
    if (!jacdac_initialized) {
        mp_raise_msg(&mp_type_RuntimeError, MP_ERROR_TEXT("jacdac not started"));
    }

    mp_buffer_info_t bufinfo;
    mp_get_buffer_raise(data_in, &bufinfo, MP_BUFFER_READ);

    if (bufinfo.len < 12 || bufinfo.len > 252) {
        mp_raise_ValueError(MP_ERROR_TEXT("frame must be 12-252 bytes"));
    }

    // Copy to aligned buffer on stack — only the actual payload size,
    // not a full 252-byte jd_frame_t. jd_send_frame_raw reads size+12 bytes.
    uint32_t aligned_buf[(bufinfo.len + 3) / 4]; // 4-byte aligned
    memcpy(aligned_buf, bufinfo.buf, bufinfo.len);

    int ret = jd_send_frame_raw((jd_frame_t *)aligned_buf);
    return mp_obj_new_int(ret);
}
MP_DEFINE_CONST_FUN_OBJ_1(mod_jacdac_send_obj, mod_jacdac_send);

static mp_obj_t mod_jacdac_receive(void) {
    if (!jacdac_initialized) {
        mp_raise_msg(&mp_type_RuntimeError, MP_ERROR_TEXT("jacdac not started"));
    }

    // Process any pending events first
    jd_process_everything();

    jd_frame_t *frame = jd_rx_get_frame();
    if (frame == NULL) {
        return mp_const_none;
    }

    // Return frame as bytes (header + data)
    size_t frame_size = frame->size + 12; // 12 byte header
    mp_obj_t result = mp_obj_new_bytes((uint8_t *)frame, frame_size);

    jd_rx_release_frame(frame);
    return result;
}
MP_DEFINE_CONST_FUN_OBJ_0(mod_jacdac_receive_obj, mod_jacdac_receive);
#endif // JD_RAW_FRAME

static mp_obj_t mod_jacdac_diagnostics(void) {
    if (!jacdac_initialized) {
        mp_raise_msg(&mp_type_RuntimeError, MP_ERROR_TEXT("jacdac not started"));
    }

    jd_diagnostics_t *diag = jd_get_diagnostics();

    mp_obj_dict_t *dict = mp_obj_new_dict(7);
    mp_obj_dict_store(dict, MP_OBJ_NEW_QSTR(MP_QSTR_bus_state), mp_obj_new_int(diag->bus_state));
    mp_obj_dict_store(dict, MP_ROM_QSTR(MP_QSTR_bus_lo_error), mp_obj_new_int(diag->bus_lo_error));
    mp_obj_dict_store(dict, MP_ROM_QSTR(MP_QSTR_bus_uart_error), mp_obj_new_int(diag->bus_uart_error));
    mp_obj_dict_store(dict, MP_ROM_QSTR(MP_QSTR_bus_timeout_error), mp_obj_new_int(diag->bus_timeout_error));
    mp_obj_dict_store(dict, MP_ROM_QSTR(MP_QSTR_packets_sent), mp_obj_new_int(diag->packets_sent));
    mp_obj_dict_store(dict, MP_ROM_QSTR(MP_QSTR_packets_received), mp_obj_new_int(diag->packets_received));
    mp_obj_dict_store(dict, MP_ROM_QSTR(MP_QSTR_packets_dropped), mp_obj_new_int(diag->packets_dropped));

    return MP_OBJ_FROM_PTR(dict);
}
MP_DEFINE_CONST_FUN_OBJ_0(mod_jacdac_diagnostics_obj, mod_jacdac_diagnostics);

// --- Client discovery functions (Step 3) ---

#if JD_CLIENT

// scan() - Returns list of discovered Jacdac devices as tuples.
// Each entry is (device_id: bytes, short_id: str, services: tuple of ints).
// Uses tuples instead of dicts to reduce heap allocations per device.
static mp_obj_t mod_jacdac_scan(void) {
    if (!jacdac_initialized) {
        mp_raise_msg(&mp_type_RuntimeError, MP_ERROR_TEXT("jacdac not started"));
    }

    // Count devices first to pre-allocate the result list
    int n_devs = 0;
    for (jd_device_t *dev = jd_devices; dev != NULL; dev = dev->next) {
        n_devs++;
    }

    mp_obj_t *result_items = m_new(mp_obj_t, n_devs);
    int idx = 0;

    for (jd_device_t *dev = jd_devices; dev != NULL; dev = dev->next) {
        // Service class tuple (skip service 0 = control)
        int n_svc = dev->num_services > 1 ? dev->num_services - 1 : 0;
        mp_obj_t *svc_items = m_new(mp_obj_t, n_svc);
        for (int i = 0; i < n_svc; i++) {
            svc_items[i] = mp_obj_new_int_from_uint(dev->services[i + 1].service_class);
        }
        mp_obj_t svc_tuple = mp_obj_new_tuple(n_svc, svc_items);
        m_del(mp_obj_t, svc_items, n_svc);

        // Build (device_id, short_id, services) tuple
        mp_obj_t items[3] = {
            mp_obj_new_bytes((uint8_t *)&dev->device_identifier, 8),
            mp_obj_new_str(dev->short_id, strlen(dev->short_id)),
            svc_tuple,
        };
        result_items[idx++] = mp_obj_new_tuple(3, items);
    }

    mp_obj_t result = mp_obj_new_list(n_devs, result_items);
    m_del(mp_obj_t, result_items, n_devs);
    return result;
}
MP_DEFINE_CONST_FUN_OBJ_0(mod_jacdac_scan_obj, mod_jacdac_scan);

// num_devices() - Returns number of currently discovered devices
static mp_obj_t mod_jacdac_num_devices(void) {
    if (!jacdac_initialized) {
        return mp_obj_new_int(0);
    }
    int count = 0;
    for (jd_device_t *dev = jd_devices; dev != NULL; dev = dev->next) {
        count++;
    }
    return mp_obj_new_int(count);
}
MP_DEFINE_CONST_FUN_OBJ_0(mod_jacdac_num_devices_obj, mod_jacdac_num_devices);

#endif // JD_CLIENT

// --- Module globals ---

static const mp_rom_map_elem_t jacdac_module_globals_table[] = {
    { MP_ROM_QSTR(MP_QSTR___name__), MP_ROM_QSTR(MP_QSTR_jacdac) },
    { MP_ROM_QSTR(MP_QSTR_start), MP_ROM_PTR(&mod_jacdac_start_obj) },
    { MP_ROM_QSTR(MP_QSTR_stop), MP_ROM_PTR(&mod_jacdac_stop_obj) },
    { MP_ROM_QSTR(MP_QSTR_is_running), MP_ROM_PTR(&mod_jacdac_is_running_obj) },
    { MP_ROM_QSTR(MP_QSTR_process), MP_ROM_PTR(&mod_jacdac_process_obj) },
    { MP_ROM_QSTR(MP_QSTR_device_id), MP_ROM_PTR(&mod_jacdac_device_id_obj) },
#if JD_RAW_FRAME
    { MP_ROM_QSTR(MP_QSTR_send), MP_ROM_PTR(&mod_jacdac_send_obj) },
    { MP_ROM_QSTR(MP_QSTR_receive), MP_ROM_PTR(&mod_jacdac_receive_obj) },
#endif
    { MP_ROM_QSTR(MP_QSTR_diagnostics), MP_ROM_PTR(&mod_jacdac_diagnostics_obj) },

#if JD_CLIENT
    { MP_ROM_QSTR(MP_QSTR_scan), MP_ROM_PTR(&mod_jacdac_scan_obj) },
    { MP_ROM_QSTR(MP_QSTR_num_devices), MP_ROM_PTR(&mod_jacdac_num_devices_obj) },
#endif

    // Jacdac protocol constants
    { MP_ROM_QSTR(MP_QSTR_FLAG_COMMAND), MP_OBJ_NEW_SMALL_INT(0x01) },
    { MP_ROM_QSTR(MP_QSTR_FLAG_ACK_REQUESTED), MP_OBJ_NEW_SMALL_INT(0x02) },
    { MP_ROM_QSTR(MP_QSTR_FLAG_BROADCAST), MP_OBJ_NEW_SMALL_INT(0x04) },

    // Service class constants for common services
    { MP_ROM_QSTR(MP_QSTR_SERVICE_TEMPERATURE), MP_OBJ_NEW_SMALL_INT(0x1421bac7) },
    { MP_ROM_QSTR(MP_QSTR_SERVICE_ACCELEROMETER), MP_OBJ_NEW_SMALL_INT(0x1f140409) },
    { MP_ROM_QSTR(MP_QSTR_SERVICE_BUTTON), MP_OBJ_NEW_SMALL_INT(0x1473a263) },
    { MP_ROM_QSTR(MP_QSTR_SERVICE_LIGHT_LEVEL), MP_OBJ_NEW_SMALL_INT(0x17dc9a1c) },
    { MP_ROM_QSTR(MP_QSTR_SERVICE_HUMIDITY), MP_OBJ_NEW_SMALL_INT(0x16c810b8) },
    { MP_ROM_QSTR(MP_QSTR_SERVICE_SERVO), MP_OBJ_NEW_SMALL_INT(0x12fc8571) },
    { MP_ROM_QSTR(MP_QSTR_SERVICE_LED), MP_OBJ_NEW_SMALL_INT(0x1e3048f8) },
    { MP_ROM_QSTR(MP_QSTR_SERVICE_BUZZER), MP_OBJ_NEW_SMALL_INT(0x1b57b1d7) },
};
static MP_DEFINE_CONST_DICT(jacdac_module_globals, jacdac_module_globals_table);

const mp_obj_module_t jacdac_module = {
    .base = { &mp_type_module },
    .globals = (mp_obj_dict_t *)&jacdac_module_globals,
};

MP_REGISTER_MODULE(MP_QSTR_jacdac, jacdac_module);
