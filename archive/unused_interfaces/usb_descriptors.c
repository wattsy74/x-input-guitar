/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2019 Ha Thach (tinyusb.org)
 * Copyright (c) 2025 BGG XInput Implementation - CENTRALIZED DESCRIPTORS
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
 *
 */

#include "tusb.h"
#include "usb_interface.h"
#include "hid_interface.h"
#include <string.h>

//--------------------------------------------------------------------+
// SINGLE CENTRALIZED DESCRIPTOR SYSTEM - NO CONFLICTS!
//--------------------------------------------------------------------+

// XInput constants
#define XINPUT_VID        0x045E  // Microsoft
#define XINPUT_PID        0x028E  // Xbox 360 Controller (wired)
#define XINPUT_BCD        0x0572

// HID constants  
#define HID_VID          0x1209  // Generic VID
#define HID_PID          0x0001  // Generic PID

//--------------------------------------------------------------------+
// DEVICE DESCRIPTORS (SINGLE DYNAMIC SYSTEM)
//--------------------------------------------------------------------+

// XInput Device Descriptor
static tusb_desc_device_t const desc_device_xinput = {
    .bLength            = sizeof(tusb_desc_device_t),
    .bDescriptorType    = TUSB_DESC_DEVICE,
    .bcdUSB             = 0x0200,  // USB 2.0
    .bDeviceClass       = 0xFF,    // Vendor specific
    .bDeviceSubClass    = 0x5D,    // CRITICAL: Xbox 360 SubClass for Windows recognition
    .bDeviceProtocol    = 0x01,    // Xbox 360 Protocol
    .bMaxPacketSize0    = CFG_TUD_ENDPOINT0_SIZE,
    .idVendor           = XINPUT_VID,
    .idProduct          = XINPUT_PID,
    .bcdDevice          = XINPUT_BCD,
    .iManufacturer      = 1,
    .iProduct           = 2,
    .iSerialNumber      = 3,
    .bNumConfigurations = 1
};

// HID Device Descriptor  
static tusb_desc_device_t const desc_device_hid = {
    .bLength            = sizeof(tusb_desc_device_t),
    .bDescriptorType    = TUSB_DESC_DEVICE,
    .bcdUSB             = 0x0200,  // USB 2.0
    .bDeviceClass       = 0x00,    // Interface defined
    .bDeviceSubClass    = 0x00,
    .bDeviceProtocol    = 0x00,
    .bMaxPacketSize0    = CFG_TUD_ENDPOINT0_SIZE,
    .idVendor           = HID_VID,
    .idProduct          = HID_PID,
    .bcdDevice          = 0x0100,
    .iManufacturer      = 1,
    .iProduct           = 4,  // Different product string
    .iSerialNumber      = 3,
    .bNumConfigurations = 1
};

// NOTE: USB descriptor callbacks are handled by usb_interface.c
// This file only provides the descriptor data that the interfaces reference

//--------------------------------------------------------------------+
// CONFIGURATION DESCRIPTORS
//--------------------------------------------------------------------+

// XInput Configuration (EXACT FROM WORKING VERSION)
enum {
    ITF_NUM_VENDOR = 0,
    ITF_NUM_TOTAL
};

#define EPNUM_VENDOR_IN   0x81
#define EPNUM_VENDOR_OUT  0x01
#define CONFIG_TOTAL_LEN_XINPUT    (TUD_CONFIG_DESC_LEN + 9 + 17 + 7 + 7)  // Config + Interface + Unknown + 2 Endpoints

static uint8_t const desc_configuration_xinput[] = {
    // Config descriptor: config number, interface count, string index, total length, attribute, power in mA
    TUD_CONFIG_DESCRIPTOR(1, ITF_NUM_TOTAL, 0, CONFIG_TOTAL_LEN_XINPUT, 0x80, 500),

    // XInput Interface Descriptor (Class 0xFF, SubClass 0x5D, Protocol 0x01)
    0x09,        // bLength
    0x04,        // bDescriptorType (Interface)
    ITF_NUM_VENDOR,        // bInterfaceNumber
    0x00,        // bAlternateSetting
    0x02,        // bNumEndpoints
    0xFF,        // bInterfaceClass (Vendor Specific)
    0x5D,        // bInterfaceSubClass (XInput!)
    0x01,        // bInterfaceProtocol (XInput)
    0x00,        // iInterface

    // CRITICAL: XInput Unknown Descriptor (0x21) - This is what Microsoft driver looks for!
    0x11,        // bLength (17 bytes)
    0x21,        // bDescriptorType (Unknown/Vendor specific)
    0x00, 0x01, 0x01, 0x25, 0x81, 0x14, 0x00, 0x00,
    0x00, 0x00, 0x13, 0x01, 0x08, 0x00, 0x00,

    // Endpoint Descriptor (IN)
    0x07,        // bLength
    0x05,        // bDescriptorType (Endpoint)
    EPNUM_VENDOR_IN,        // bEndpointAddress (IN, endpoint 1)
    0x03,        // bmAttributes (Interrupt)
    CFG_TUD_VENDOR_EPSIZE, 0x00,  // wMaxPacketSize (32 bytes)
    0x01,        // bInterval (1ms)

    // Endpoint Descriptor (OUT)
    0x07,        // bLength
    0x05,        // bDescriptorType (Endpoint)
    EPNUM_VENDOR_OUT,        // bEndpointAddress (OUT, endpoint 1)
    0x03,        // bmAttributes (Interrupt)
    CFG_TUD_VENDOR_EPSIZE, 0x00,  // wMaxPacketSize (32 bytes)
    0x08         // bInterval (8ms)
};

// HID Configuration 
enum {
    ITF_NUM_HID = 0,
    ITF_NUM_HID_TOTAL
};

#define EPNUM_HID   0x81
#define CONFIG_TOTAL_LEN_HID    (TUD_CONFIG_DESC_LEN + TUD_HID_DESC_LEN)

static uint8_t const desc_configuration_hid[] = {
    // Config number, interface count, string index, total length, attribute, power in mA
    TUD_CONFIG_DESCRIPTOR(1, ITF_NUM_HID_TOTAL, 0, CONFIG_TOTAL_LEN_HID, 0x80, 100),

    // Interface number, string index, protocol, report descriptor len, EP In address, size & polling interval
    TUD_HID_DESCRIPTOR(ITF_NUM_HID, 0, HID_ITF_PROTOCOL_NONE, sizeof(desc_hid_report), EPNUM_HID, CFG_TUD_HID_EP_BUFSIZE, 5)
};

// HID Report Descriptor - Forward reference to hid_interface.c
// (This is declared in hid_interface.c and exposed via hid_interface.h)

// NOTE: Configuration descriptor callbacks are handled by usb_interface.c
// This file only provides the descriptor data that the interfaces reference

//--------------------------------------------------------------------+
// STRING DESCRIPTORS
//--------------------------------------------------------------------+

// String Descriptors
static char const* string_desc_arr[] = {
    (const char[]){0x09, 0x04}, // 0: Language (English)
    "Microsoft",                // 1: Manufacturer (for XInput)
    "Controller (XBOX 360 For Windows)", // 2: Product (XInput)
    "BGG000001",               // 3: Serial number
    "BGG Guitar Hero Controller", // 4: Product (HID)
};

// NOTE: String descriptor callbacks are handled by usb_interface.c
// This file only provides the descriptor data that the interfaces reference
