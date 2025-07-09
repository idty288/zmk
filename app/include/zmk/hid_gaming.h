/*
 * Copyright (c) 2024 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <zephyr/usb/usb_ch9.h>
#include <zephyr/usb/class/usb_hid.h>
#include <zmk/keys.h>
#include <dt-bindings/zmk/hid_usage.h>
#include <dt-bindings/zmk/hid_usage_pages.h>

// Gaming HID Report IDs (non-conflicting with standard HID)
#define ZMK_HID_GAMING_REPORT_ID_LEFT_HALF    0x10
#define ZMK_HID_GAMING_REPORT_ID_GROUP_YU     0x11
#define ZMK_HID_GAMING_REPORT_ID_GROUP_HJ     0x12
#define ZMK_HID_GAMING_REPORT_ID_GROUP_NM     0x13
#define ZMK_HID_GAMING_REPORT_ID_GROUP_REST   0x14
#define ZMK_HID_GAMING_REPORT_ID_THUMBS       0x15
#define ZMK_HID_GAMING_REPORT_ID_GROUP_ED     0x16
#define ZMK_HID_GAMING_REPORT_ID_GROUP_RFV    0x17

// Gaming device indices - refined grouping as requested:
// Device 0: Left hand [w,s,x] 
// Device 1: Right index [y,u]
// Device 2: Right index [h,j]
// Device 3: Right index [n,m]
// Device 4: Rest group [q,a,z,t,g,b,c] + right side [i,o,p,k,l,;,',,,.,/]
// Device 5: Both thumb clusters
// Device 6: Left hand [e,d] 
// Device 7: Left hand [r,f,v]
#define ZMK_GAMING_DEVICE_LEFT_HALF    0  // Left hand [w,s,x]
#define ZMK_GAMING_DEVICE_GROUP_YU     1  // Right index [y,u]
#define ZMK_GAMING_DEVICE_GROUP_HJ     2  // Right index [h,j]
#define ZMK_GAMING_DEVICE_GROUP_NM     3  // Right index [n,m]
#define ZMK_GAMING_DEVICE_GROUP_REST   4  // Rest group + right side
#define ZMK_GAMING_DEVICE_THUMBS       5  // Both thumb clusters
#define ZMK_GAMING_DEVICE_GROUP_ED     6  // Left hand [e,d]
#define ZMK_GAMING_DEVICE_GROUP_RFV    7  // Left hand [r,f,v]
#define ZMK_GAMING_DEVICE_COUNT        8

// Maximum keys per gaming device
#define ZMK_GAMING_MAX_KEYS_PER_DEVICE 18

// Gaming HID reports will be sent using existing USB HID with different report IDs
// This approach integrates with ZMK's existing HID infrastructure

// Gaming keyboard report structures
struct zmk_gaming_keyboard_report_body {
    uint8_t modifiers;
    uint8_t _reserved;
    uint8_t keys[ZMK_GAMING_MAX_KEYS_PER_DEVICE];
} __packed;

struct zmk_gaming_keyboard_report {
    uint8_t report_id;
    struct zmk_gaming_keyboard_report_body body;
} __packed;

// Function declarations
int zmk_hid_gaming_keyboard_press(uint8_t device_id, zmk_key_t key);
int zmk_hid_gaming_keyboard_release(uint8_t device_id, zmk_key_t key);
void zmk_hid_gaming_keyboard_clear(uint8_t device_id);
void zmk_hid_gaming_keyboard_clear_all(void);

// Position-based gaming HID functions with tracking
int zmk_hid_gaming_position_press(uint32_t position, zmk_key_t key);
int zmk_hid_gaming_position_release(uint32_t position);

int zmk_hid_gaming_register_mod(uint8_t device_id, zmk_mod_t modifier);
int zmk_hid_gaming_unregister_mod(uint8_t device_id, zmk_mod_t modifier);

struct zmk_gaming_keyboard_report *zmk_hid_gaming_get_keyboard_report(uint8_t device_id);

// Gaming layer detection
bool zmk_hid_gaming_is_active(void);
void zmk_hid_gaming_set_active(bool active);

// Position to device mapping for Corne 42-key layout
uint8_t zmk_hid_gaming_get_device_for_position(uint32_t position);

// Gaming position state handling
int zmk_keymap_gaming_position_state_changed(uint8_t source, uint32_t position, bool pressed,
                                           int64_t timestamp);