/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2019 Ha Thach (tinyusb.org)
 * Copyright (c) 2025 BGG Unified Firmware Configuration
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

#ifndef _TUSB_CONFIG_UNIFIED_H_
#define _TUSB_CONFIG_UNIFIED_H_

#ifdef __cplusplus
 extern "C" {
#endif

//--------------------------------------------------------------------
// BOARD CONFIGURATION
//--------------------------------------------------------------------

// Board config
#ifndef CFG_TUSB_MCU
#define CFG_TUSB_MCU    OPT_MCU_RP2040
#endif

#ifndef CFG_TUSB_OS
#define CFG_TUSB_OS     OPT_OS_PICO
#endif

#ifndef CFG_TUSB_DEBUG
#define CFG_TUSB_DEBUG  0
#endif

// Enable Device stack
#define CFG_TUD_ENABLED 1

// Default is max speed that hardware controller could support with on-board PHY
#define CFG_TUD_MAX_SPEED   OPT_MODE_DEFAULT_SPEED

//--------------------------------------------------------------------
// DEVICE CONFIGURATION
//--------------------------------------------------------------------

#ifndef CFG_TUD_ENDPOINT0_SIZE
#define CFG_TUD_ENDPOINT0_SIZE    64
#endif

//------------- DYNAMIC CLASS CONFIGURATION -------------//

// Forward declare the runtime USB mode variable
extern int g_runtime_usb_mode;

// Dynamic class configuration based on runtime detection
// 0 = XInput mode, 1 = HID mode
#define CFG_TUD_HID               (g_runtime_usb_mode == 1 ? 1 : 0)
#define CFG_TUD_VENDOR            (g_runtime_usb_mode == 0 ? 1 : 0)
#define CFG_TUD_CDC               0    // Always disabled
#define CFG_TUD_MSC               0    // Always disabled
#define CFG_TUD_MIDI              0    // Always disabled
#define CFG_TUD_AUDIO             0    // Always disabled
#define CFG_TUD_VIDEO             0    // Always disabled
#define CFG_TUD_DFU_RUNTIME       0    // Always disabled
#define CFG_TUD_NET               0    // Always disabled
#define CFG_TUD_BTH               0    // Always disabled

// HID buffer size Should be sufficient to hold ID (if any) + Data
#define CFG_TUD_HID_EP_BUFSIZE    16

// Vendor FIFO size of TX and RX
// If not configured vendor endpoints will not be buffered
#define CFG_TUD_VENDOR_RX_BUFSIZE 64
#define CFG_TUD_VENDOR_TX_BUFSIZE 64

//--------------------------------------------------------------------
// USB PORT CONFIGURATION
//--------------------------------------------------------------------

#define CFG_TUSB_RHPORT0_MODE   (OPT_MODE_DEVICE)

#ifdef __cplusplus
 }
#endif

#endif /* _TUSB_CONFIG_UNIFIED_H_ */
