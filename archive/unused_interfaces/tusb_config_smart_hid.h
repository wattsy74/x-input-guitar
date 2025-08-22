#ifndef _TUSB_CONFIG_SMART_HID_H_
#define _TUSB_CONFIG_SMART_HID_H_

#ifdef __cplusplus
extern "C" {
#endif

//--------------------------------------------------------------------
// COMMON CONFIGURATION
//--------------------------------------------------------------------

#ifndef CFG_TUSB_MCU
#define CFG_TUSB_MCU OPT_MCU_RP2040
#endif

#define CFG_TUSB_RHPORT0_MODE OPT_MODE_DEVICE

#ifndef CFG_TUSB_OS
#define CFG_TUSB_OS OPT_OS_PICO
#endif

#ifndef CFG_TUSB_DEBUG
#define CFG_TUSB_DEBUG 0
#endif

#define CFG_TUD_ENABLED 1

#ifndef CFG_TUD_ENDPOINT0_SIZE
#define CFG_TUD_ENDPOINT0_SIZE 64
#endif

//------------- HID MODE CLASS CONFIGURATION -------------//
#define CFG_TUD_CDC              0
#define CFG_TUD_MSC              0
#define CFG_TUD_HID              1    // ENABLED for HID
#define CFG_TUD_MIDI             0
#define CFG_TUD_AUDIO            0
#define CFG_TUD_VIDEO            0
#define CFG_TUD_ECM_RNDIS        0
#define CFG_TUD_NCM              0
#define CFG_TUD_BTH              0
#define CFG_TUD_DFU_RUNTIME      0
#define CFG_TUD_VENDOR           0    // DISABLED for HID

#define CFG_TUD_HID_EP_BUFSIZE   16

#ifndef CFG_TUSB_MEM_SECTION
#define CFG_TUSB_MEM_SECTION
#endif

#ifndef CFG_TUSB_MEM_ALIGN
#define CFG_TUSB_MEM_ALIGN __attribute__((aligned(4)))
#endif

#ifdef __cplusplus
}
#endif

#endif /* _TUSB_CONFIG_SMART_HID_H_ */
