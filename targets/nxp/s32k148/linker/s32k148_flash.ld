ENTRY(Reset_Handler)

MEMORY
{
    FLASH (rx)  : ORIGIN = 0x00000000, LENGTH = 0x00180000
    RAM   (rwx) : ORIGIN = 0x1FFE0000, LENGTH = 0x0003F000
}

__ram_start = ORIGIN(RAM);
__ram_end = ORIGIN(RAM) + LENGTH(RAM);
__StackTop = __ram_end;
__StackLimit = __StackTop - 0x1000;

SECTIONS
{
    .isr_vector 0x00000000 :
    {
        KEEP(*(.isr_vector))
    } > FLASH

    .flash_config 0x00000400 :
    {
        KEEP(*(.flash_config))
    } > FLASH

    .text 0x00000410 :
    {
        *(.text*)
        *(.rodata*)
        KEEP(*(.init))
        KEEP(*(.fini))
        . = ALIGN(4);
        __text_end = .;
    } > FLASH

    .data : AT(__text_end)
    {
        . = ALIGN(4);
        __data_start = .;
        *(.data*)
        . = ALIGN(4);
        __data_end = .;
    } > RAM
    __data_load = LOADADDR(.data);

    .bss (NOLOAD) :
    {
        . = ALIGN(16);
        __bss_start = .;
        *(.bss*)
        *(COMMON)
        . = ALIGN(16);
        __bss_end = .;
    } > RAM

    __image_ram_end = .;
    ASSERT(__image_ram_end <= __StackLimit,
           "Static RAM sections overlap the reserved MSP stack")
    ASSERT((__StackTop & 7) == 0, "MSP top must be 8-byte aligned")
    ASSERT(SIZEOF(.isr_vector) == 0x400,
           "S32K148 vector table must contain exactly 256 entries")
    ASSERT(ADDR(.flash_config) == 0x400,
           "S32K148 flash configuration field must be at 0x400")

    /DISCARD/ :
    {
        *(.comment)
        *(.ARM.exidx*)
        *(.ARM.extab*)
    }
}
