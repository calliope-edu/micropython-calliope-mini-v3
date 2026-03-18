/*
 * Jacdac-c platform implementation for Calliope mini V3 (nRF52833).
 *
 * Provides:
 *   - jd_alloc / jd_free / jd_alloc_init / jd_alloc_stack_check
 *   - app_get_device_class / app_fw_version / app_dev_class_name
 *   - app_init_services
 *
 * The MIT License (MIT)
 * Copyright (c) 2025 Calliope gGmbH
 */

#include <string.h>
#include <stdlib.h>
#include <stdint.h>

/*
 * Pull in jacdac-c headers.  jd_user_config.h is picked up automatically
 * because it lives in this directory and is included by jd_config.h via
 * the -I flag for the current directory.
 */
#include "jd_protocol.h"
#include "interfaces/jd_alloc.h"
#include "interfaces/jd_app.h"
#include "jd_service_framework.h"
#if JD_CLIENT
#include "jd_client.h"
#endif
#include "jd_calliope_services.h"

// ============================================================
// Memory allocation
//
// We delegate to the system heap (malloc/free) since the MicroPython
// GC heap is managed separately and we don't want jacdac-c touching it.
// JD_FREE_SUPPORTED=1 is set in jd_user_config.h so jd_free is called.
// ============================================================

void jd_alloc_init(void) {
    // Nothing to set up — using system malloc.
}

void *jd_alloc(uint32_t size) {
    void *p = calloc(1, size);
    if (!p) {
        hw_panic();
    }
    return p;
}

void jd_free(void *ptr) {
    free(ptr);
}

void jd_alloc_stack_check(void) {
    // No stack sentinel to check when using system heap.
}

void *jd_alloc_emergency_area(uint32_t size) {
    return jd_alloc(size);
}

uint32_t jd_available_memory(void) {
    // Not meaningful with system heap; return a safe large value.
    return 32 * 1024;
}

// ============================================================
// Application descriptor constants
// ============================================================

// Calliope mini V3 device class (custom value in Jacdac vendor range)
// Low 4 bits indicate product variant; upper bits are vendor-assigned.
const char app_dev_class_name[] = "Calliope mini V3";
const char app_fw_version[]     = "MicroPython-1.0";

uint32_t app_get_device_class(void) {
    // Use 0x3400_0001 as a placeholder Calliope vendor device class.
    // Replace with an officially registered class if one is assigned.
    return 0x34000001u;
}

// ============================================================
// Service initialisation
// Called once by jd_services_init() (inside jd_init()) to register
// all Jacdac services that this device exposes on the bus.
// ============================================================
void app_init_services(void) {
    calliope_temperature_init();
    calliope_accelerometer_init();
    calliope_button_init(0); // Button A
    calliope_button_init(1); // Button B

#if JD_CLIENT
    jd_role_manager_init();
#endif
}
