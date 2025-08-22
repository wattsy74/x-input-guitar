// MINIMAL WORKING USB CDC DEVICE - Based on Pico SDK example
#include "pico/stdlib.h"
#include "bsp/board.h"
#include "tusb.h"

int main() {
    board_init();
    tusb_init();

    while (1) {
        tud_task(); // TinyUSB device task
        
        // Simple blink to show we're alive
        static uint32_t last_blink = 0;
        if (board_millis() - last_blink > 500) {
            board_led_write(board_led_read() ^ 1);
            last_blink = board_millis();
        }
    }
    
    return 0;
}

// TinyUSB callbacks - minimal implementation
void tud_mount_cb(void) {
    // Device mounted (enumerated)
}

void tud_umount_cb(void) {
    // Device unmounted
}

void tud_suspend_cb(bool remote_wakeup_en) {
    (void) remote_wakeup_en;
    // Device suspended
}

void tud_resume_cb(void) {
    // Device resumed
}
