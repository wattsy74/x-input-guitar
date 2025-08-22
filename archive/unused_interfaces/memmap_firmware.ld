/*
 * Memory map for BGG firmware running under bootloader
 * 
 * This linker script positions the main firmware at the correct
 * flash offset (after the 32KB bootloader).
 */

MEMORY
{
    /* Main firmware starts at 32KB offset (after bootloader) */
    FLASH (rx) : ORIGIN = 0x10008000, LENGTH = 480k
    
    /* RAM layout (shared) */
    RAM (rwx) : ORIGIN = 0x20000000, LENGTH = 264k
    
    /* Scratch X RAM */
    SCRATCH_X (rwx) : ORIGIN = 0x20040000, LENGTH = 4k
    
    /* Scratch Y RAM */  
    SCRATCH_Y (rwx) : ORIGIN = 0x20041000, LENGTH = 4k
}

/* Entry point */
ENTRY(_entry_point)

SECTIONS
{
    .flash_begin : {
        __flash_binary_start = .;
    } > FLASH

    .boot2 : {
        __boot2_start__ = .;
        KEEP (*(.boot2))
        __boot2_end__ = .;
    } > FLASH

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
    } > FLASH

    .ARM.extab :
    {
        *(.ARM.extab* .gnu.linkonce.armextab.*)
    } > FLASH

    __exidx_start = .;
    .ARM.exidx :
    {
        *(.ARM.exidx* .gnu.linkonce.armexidx.*)
    } > FLASH
    __exidx_end = .;

    .binary_info : {
        KEEP(*(.binary_info.keep.*))
        *(.binary_info.*)
    } > FLASH

    __etext = .;

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
    } > RAM AT> FLASH

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
        *(.heap*)
        __HeapLimit = .;
    } > RAM

    .stack1_dummy (COPY):
    {
        *(.stack1*)
    } > SCRATCH_X

    .stack_dummy (COPY):
    {
        *(.stack*)
    } > SCRATCH_Y

    __flash_binary_end = __etext + SIZEOF(.data);
}

ASSERT(__binary_info_header_end - __logical_binary_start <= 256, 
    "Binary info must be in first 256 bytes of the binary")

/* Ensure firmware fits in allocated space */
ASSERT( __flash_binary_end <= 0x10080000, "Firmware is too big for allocated space")
