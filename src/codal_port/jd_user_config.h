// Jacdac user configuration for MicroPython on Calliope mini V3 (nRF52833)
// Licensed under the MIT license.
// Copyright (c) 2025 Calliope gGmbH
//
// Memory budget: nRF52833 has 128KB RAM shared with CODAL + MicroPython heap.
// Every byte counts — keep buffers small and disable unused features.

#ifndef JD_USER_CONFIG_H
#define JD_USER_CONFIG_H

// We are running on physical hardware with a single-wire bus
#define JD_PHYSICAL 1

// Enable client mode so we can both expose and consume services
#define JD_CLIENT 1

// Enable raw frame access for Python-level packet inspection
#define JD_RAW_FRAME 1

// nRF52833 flash page size
#define JD_FLASH_PAGE_SIZE 4096

// TX queue buffer — 256 bytes fits 1 max-size frame, sufficient for
// a device that mostly responds rather than initiates bulk transfers.
#define JD_SEND_FRAME_SIZE 256

// RX queue — 256 bytes; incoming announce packets are ~60 bytes each.
#define JD_RX_QUEUE_SIZE 256

// Disable features we don't need to save memory
#define JD_CONFIG_WATCHDOG 0
#define JD_USB_BRIDGE 0
#define JD_NET_BRIDGE 0
#define JD_DEVICESCRIPT 0
#define JD_LORA 0
#define JD_WIFI 0
#define JD_NETWORK 0
#define JD_HID 0
#define JD_SPI 0
#define JD_DCFG 0
#define JD_LSTORE 0
#define JD_SD_PANIC 0

// Disable settings/storage (not needed for basic operation)
#define JD_SETTINGS_LARGE 0

// Use system malloc/free for allocation
#define JD_SIMPLE_ALLOC 0
#define JD_HW_ALLOC 0
#define JD_GC_ALLOC 0
#define JD_FREE_SUPPORTED 1

// 6 services used: control (auto) + temperature + accelerometer + buttonA + buttonB + rolemgr.
// Add a small margin for future additions.
#define JD_MAX_SERVICES 8

// No status LED pins defined (Calliope uses RGB NeoPixel, not separate pins)
#define JD_CONFIG_STATUS 0

// --- RAM savings: disable features auto-enabled by JD_CLIENT ---

// Verbose assertions include format strings in flash — not needed in production
#define JD_VERBOSE_ASSERT 0

// Advanced string formatting (160-byte line buffer) — not needed
#define JD_ADVANCED_STRING 0

// DMESG ring buffer — 0 disables entirely (saves buffer + code).
// DMESG is already defined as no-op below.
#define JD_DMESG_BUFFER_SIZE 0

// Instance names add string handling overhead; not needed for basic operation
#define JD_INSTANCE_NAME 0

// Timer overhead calibrated for nRF52833 at 64MHz
#define JD_TIM_OVERHEAD 12
#define JD_WR_OVERHEAD 8

// Wake main thread when Jacdac needs processing
void jd_wake_main(void);
#define JD_WAKE_MAIN() jd_wake_main()

// Forward declare DMESG as no-op (CODAL DMESG is not available in C context)
#ifndef DMESG
#define DMESG(...) ((void)0)
#endif

#endif // JD_USER_CONFIG_H
