/*
 * Memory map for BGG XInput/HID bootloader system
 * 
 * This linker script defines the memory layout for the bootloader
 * and reserves space for the main firmware images.
 */

MEMORY
{
    /* Bootloader occupies first 32KB of flash */
    FLASH_BOOTLOADER (rx) : ORIGIN = 0x10000000, LENGTH = 32k
    
    /* XInput firmware at 32KB offset */
    FLASH_XINPUT (rx) : ORIGIN = 0x10008000, LENGTH = 480k
    
    /* HID firmware at 512KB offset */  
    FLASH_HID (rx) : ORIGIN = 0x10080000, LENGTH = 480k
    
    /* Config/storage area at 992KB */
    FLASH_STORAGE (rx) : ORIGIN = 0x100F8000, LENGTH = 32k
    
    /* RAM layout (shared by all firmware) */
    RAM (rwx) : ORIGIN = 0x20000000, LENGTH = 264k
    
    /* Scratch X RAM */
    SCRATCH_X (rwx) : ORIGIN = 0x20040000, LENGTH = 4k
    
    /* Scratch Y RAM */  
    SCRATCH_Y (rwx) : ORIGIN = 0x20041000, LENGTH = 4k
}

/* Entry point for bootloader */
ENTRY(_entry_point)

SECTIONS
{
    /* Bootloader code and data */
    .flash_begin : {
        __flash_binary_start = .;
    } > FLASH_BOOTLOADER

    .boot2 : {
        __boot2_start__ = .;
        KEEP (*(.boot2))
        __boot2_end__ = .;
    } > FLASH_BOOTLOADER

    ASSERT(__boot2_end__ - __boot2_start__ == 256,
        "ERROR: Pico second stage bootloader must be 256 bytes in size")

    .text : {
        __logical_binary_start = .;
        KEEP (*(.vectors))
        KEEP (*(.binary_info_header))
        __binary_info_header_end = .;
        KEEP (*(.reset))
        *(.text*)
        . = ALIGN(4);
        *(.rodata*)
        . = ALIGN(4);
    } > FLASH_BOOTLOADER

    .ARM.extab :
    {
        *(.ARM.extab* .gnu.linkonce.armextab.*)
    } > FLASH_BOOTLOADER

    __exidx_start = .;
    .ARM.exidx :
    {
        *(.ARM.exidx* .gnu.linkonce.armexidx.*)
    } > FLASH_BOOTLOADER
    __exidx_end = .;

    /* Machine readable information */
    .binary_info : {
        KEEP(*(.binary_info.keep.*))
        *(.binary_info.*)
    } > FLASH_BOOTLOADER

    __etext = .;

    /* RAM sections */
    .ram_vector_table (COPY): {
        *(.ram_vector_table)
    } > RAM

    .data : {
        __data_start__ = .;
        *(vtable)
        *(.time_critical*)
        *(.data*)
        . = ALIGN(4);
        __data_end__ = .;
    } > RAM AT> FLASH_BOOTLOADER

    .uninitialized_data (COPY): {
        . = ALIGN(4);
        *(.uninitialized_data*)
    } > RAM

    .bss  : {
        . = ALIGN(4);
        __bss_start__ = .;
        *(COMMON)
        *(.bss*)
        . = ALIGN(4);
        __bss_end__ = .;
    } > RAM

    .heap (COPY):
    {
        __end__ = .;
        end = __end__;
        KEEP(*(.heap*))
        __HeapLimit = .;
    } > RAM

    /* C++ initialization arrays */
    .preinit_array : {
        __preinit_array_start = .;
        KEEP (*(.preinit_array))
        __preinit_array_end = .;
    } > FLASH_BOOTLOADER

    .init_array : {
        __init_array_start = .;
        KEEP (*(SORT(.init_array.*)))
        KEEP (*(.init_array))
        __init_array_end = .;
    } > FLASH_BOOTLOADER

    .fini_array : {
        __fini_array_start = .;
        KEEP (*(.fini_array))
        KEEP (*(SORT(.fini_array.*)))
        __fini_array_end = .;
    } > FLASH_BOOTLOADER

    /* Binary info markers */
    .binary_info_start : {
        __binary_info_start = .;
    } > FLASH_BOOTLOADER

    .binary_info_end : {
        __binary_info_end = .;
    } > FLASH_BOOTLOADER

    /* Stack pointer */
    __StackTop = ORIGIN(RAM) + LENGTH(RAM);

    .stack1_dummy (COPY):
    {
        KEEP(*(.stack1*))
    } > SCRATCH_X

    .stack_dummy (COPY):
    {
        KEEP(*(.stack*))
    } > SCRATCH_Y

    /* Scratch memory symbols */
    __scratch_x_source__ = LOADADDR(.stack1_dummy);
    __scratch_x_start__ = ORIGIN(SCRATCH_X);
    __scratch_x_end__ = ORIGIN(SCRATCH_X) + LENGTH(SCRATCH_X);
    
    __scratch_y_source__ = LOADADDR(.stack_dummy);
    __scratch_y_start__ = ORIGIN(SCRATCH_Y);
    __scratch_y_end__ = ORIGIN(SCRATCH_Y) + LENGTH(SCRATCH_Y);

    __flash_binary_end = __etext + SIZEOF(.data);

    /* Firmware storage areas (not used by bootloader) */
    .xinput_firmware_area : {
        KEEP(*(.xinput_firmware_placeholder))
        . = ORIGIN(FLASH_XINPUT) + LENGTH(FLASH_XINPUT) - 1;
        BYTE(0)
    } > FLASH_XINPUT

    .hid_firmware_area : {
        KEEP(*(.hid_firmware_placeholder))
        . = ORIGIN(FLASH_HID) + LENGTH(FLASH_HID) - 1;
        BYTE(0)
    } > FLASH_HID

    .storage_area : {
        KEEP(*(.storage_placeholder))
        . = ORIGIN(FLASH_STORAGE) + LENGTH(FLASH_STORAGE) - 1;
        BYTE(0)
    } > FLASH_STORAGE
}

ASSERT(__binary_info_header_end - __logical_binary_start <= 256, 
    "Binary info must be in first 256 bytes of the binary")

ASSERT( __flash_binary_end <= 0x10008000, "Bootloader is too big")
