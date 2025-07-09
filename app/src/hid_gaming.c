/*
 * Copyright (c) 2024 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#include <zephyr/device.h>
#include <zephyr/init.h>
#include <zephyr/usb/usb_device.h>
#include <zephyr/usb/class/usb_hid.h>
#include <zephyr/logging/log.h>
#include <zephyr/kernel.h>
#include <string.h>

#include <zmk/hid_gaming.h>
#include <zmk/keymap.h>
#include <zmk/event_manager.h>
#include <zmk/events/position_state_changed.h>
#include <zmk/usb_hid.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

// Gaming mode state - always active for global position-based split
static bool gaming_mode_active = true;

// Gaming keyboard reports for each device
static struct zmk_gaming_keyboard_report gaming_reports[ZMK_GAMING_DEVICE_COUNT];

// We'll use ZMK's existing USB HID infrastructure

// Position to device mapping for Corne 42-key layout
// Based on the key groupings specified by the user
uint8_t zmk_hid_gaming_get_device_for_position(uint32_t position) {
    // Left half: positions 0-17 (first 18 keys)
    if (position < 18) {
        return ZMK_GAMING_DEVICE_LEFT_HALF;
    }
    
    // Right half groupings based on Corne layout
    switch (position) {
        // Y, U keys (top row right side, positions 18-19)
        case 18: // Y
        case 19: // U
            return ZMK_GAMING_DEVICE_GROUP_YU;
            
        // H, J keys (home row right side, positions 24-25)
        case 24: // H 
        case 25: // J
            return ZMK_GAMING_DEVICE_GROUP_HJ;
            
        // N, M keys (bottom row right side, positions 30-31)
        case 30: // N
        case 31: // M
            return ZMK_GAMING_DEVICE_GROUP_NM;
            
        // Rest: i,o,p,k,l,',,,.,/,enter,delbkspc,del
        default:
            return ZMK_GAMING_DEVICE_GROUP_REST;
    }
}

// We'll send reports through ZMK's existing USB HID system

// Initialize gaming keyboard reports
static void zmk_hid_gaming_init_reports(void) {
    for (int i = 0; i < ZMK_GAMING_DEVICE_COUNT; i++) {
        memset(&gaming_reports[i], 0, sizeof(struct zmk_gaming_keyboard_report));
        gaming_reports[i].report_id = ZMK_HID_GAMING_REPORT_ID_LEFT_HALF + i;
    }
}

// Send gaming HID report using ZMK's USB HID infrastructure
static int zmk_hid_gaming_send_report(uint8_t device_id) {
    if (device_id >= ZMK_GAMING_DEVICE_COUNT) {
        return -EINVAL;
    }

#if IS_ENABLED(CONFIG_ZMK_USB)
    // Send the gaming report with the specific report ID
    struct zmk_gaming_keyboard_report *report = &gaming_reports[device_id];
    
    // Make sure report ID is set correctly
    report->report_id = ZMK_HID_GAMING_REPORT_ID_LEFT_HALF + device_id;
    
    // Send the report through the USB HID endpoint
    size_t report_size = sizeof(struct zmk_gaming_keyboard_report);
    
    // Use ZMK's USB HID send function - this will send our custom report
    LOG_DBG("Sending gaming report: device_id=%d, report_id=0x%02x, keys=[%02x,%02x,%02x,%02x,%02x,%02x]", 
            device_id, report->report_id, 
            report->body.keys[0], report->body.keys[1], report->body.keys[2], 
            report->body.keys[3], report->body.keys[4], report->body.keys[5]);
    return zmk_usb_hid_send_report((uint8_t *)report, report_size);
#else
    return 0;
#endif
}

// Gaming mode control
bool zmk_hid_gaming_is_active(void) {
    return gaming_mode_active;
}

void zmk_hid_gaming_set_active(bool active) {
    if (gaming_mode_active != active) {
        gaming_mode_active = active;
        if (active) {
            // Clear all gaming reports
            zmk_hid_gaming_keyboard_clear_all();
            
            // Send an empty report for each gaming device to make Linux aware of them
            for (int i = 0; i < ZMK_GAMING_DEVICE_COUNT; i++) {
                zmk_hid_gaming_send_report(i);
            }
        } else {
            // Send empty reports to clear all gaming devices
            zmk_hid_gaming_keyboard_clear_all();
        }
    }
}

// Gaming keyboard functions
int zmk_hid_gaming_keyboard_press(uint8_t device_id, zmk_key_t key) {
    if (device_id >= ZMK_GAMING_DEVICE_COUNT) {
        return -EINVAL;
    }

    struct zmk_gaming_keyboard_report *report = &gaming_reports[device_id];
    
    // Find empty slot or check if already pressed
    for (int i = 0; i < ZMK_GAMING_MAX_KEYS_PER_DEVICE; i++) {
        if (report->body.keys[i] == key) {
            return 0; // Already pressed
        }
        if (report->body.keys[i] == 0) {
            report->body.keys[i] = key;
            return zmk_hid_gaming_send_report(device_id);
        }
    }
    
    return -ENOMEM; // No more key slots
}

int zmk_hid_gaming_keyboard_release(uint8_t device_id, zmk_key_t key) {
    if (device_id >= ZMK_GAMING_DEVICE_COUNT) {
        return -EINVAL;
    }

    struct zmk_gaming_keyboard_report *report = &gaming_reports[device_id];
    
    // Find and remove key
    for (int i = 0; i < ZMK_GAMING_MAX_KEYS_PER_DEVICE; i++) {
        if (report->body.keys[i] == key) {
            // Shift remaining keys down
            for (int j = i; j < ZMK_GAMING_MAX_KEYS_PER_DEVICE - 1; j++) {
                report->body.keys[j] = report->body.keys[j + 1];
            }
            report->body.keys[ZMK_GAMING_MAX_KEYS_PER_DEVICE - 1] = 0;
            return zmk_hid_gaming_send_report(device_id);
        }
    }
    
    return 0; // Key not found, no error
}

void zmk_hid_gaming_keyboard_clear(uint8_t device_id) {
    if (device_id >= ZMK_GAMING_DEVICE_COUNT) {
        return;
    }

    struct zmk_gaming_keyboard_report *report = &gaming_reports[device_id];
    memset(report->body.keys, 0, ZMK_GAMING_MAX_KEYS_PER_DEVICE);
    
    zmk_hid_gaming_send_report(device_id);
}

void zmk_hid_gaming_keyboard_clear_all(void) {
    for (int i = 0; i < ZMK_GAMING_DEVICE_COUNT; i++) {
        zmk_hid_gaming_keyboard_clear(i);
    }
}

int zmk_hid_gaming_register_mod(uint8_t device_id, zmk_mod_t modifier) {
    if (device_id >= ZMK_GAMING_DEVICE_COUNT) {
        return -EINVAL;
    }

    struct zmk_gaming_keyboard_report *report = &gaming_reports[device_id];
    report->body.modifiers |= modifier;
    return zmk_hid_gaming_send_report(device_id);
}

int zmk_hid_gaming_unregister_mod(uint8_t device_id, zmk_mod_t modifier) {
    if (device_id >= ZMK_GAMING_DEVICE_COUNT) {
        return -EINVAL;
    }

    struct zmk_gaming_keyboard_report *report = &gaming_reports[device_id];
    report->body.modifiers &= ~modifier;
    return zmk_hid_gaming_send_report(device_id);
}

struct zmk_gaming_keyboard_report *zmk_hid_gaming_get_keyboard_report(uint8_t device_id) {
    if (device_id >= ZMK_GAMING_DEVICE_COUNT) {
        return NULL;
    }
    return &gaming_reports[device_id];
}

// Layer state change listener removed - gaming HID is always active now

// Periodic keep-alive to ensure all gaming devices stay visible to Linux
static void gaming_hid_keepalive_work_handler(struct k_work *work) {
    // Send empty reports to all gaming devices to keep them active
    for (int i = 0; i < ZMK_GAMING_DEVICE_COUNT; i++) {
        zmk_hid_gaming_send_report(i);
    }
}

K_WORK_DEFINE(gaming_hid_keepalive_work, gaming_hid_keepalive_work_handler);

static void gaming_hid_keepalive_timer_handler(struct k_timer *timer) {
    k_work_submit(&gaming_hid_keepalive_work);
}

K_TIMER_DEFINE(gaming_hid_keepalive_timer, gaming_hid_keepalive_timer_handler, NULL);

// Initialize gaming HID system  
static int zmk_hid_gaming_init(void) {
    // Initialize gaming reports
    zmk_hid_gaming_init_reports();

    // Send initial empty reports multiple times to make Linux aware of all gaming devices
    for (int j = 0; j < 3; j++) {
        for (int i = 0; i < ZMK_GAMING_DEVICE_COUNT; i++) {
            zmk_hid_gaming_send_report(i);
        }
        k_sleep(K_MSEC(10)); // Small delay between batches
    }

    // Start periodic keep-alive timer (every 5 seconds)
    k_timer_start(&gaming_hid_keepalive_timer, K_SECONDS(5), K_SECONDS(5));

    LOG_INF("Gaming HID initialized with %d virtual devices - always active for global position-based split", ZMK_GAMING_DEVICE_COUNT);
    return 0;
}

SYS_INIT(zmk_hid_gaming_init, APPLICATION, 96);