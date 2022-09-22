/***************************************************************************//**
 * GCC Linker script for Silicon Labs devices
 *******************************************************************************
 * # License
 * <b>Copyright 2020 Silicon Laboratories Inc. www.silabs.com</b>
 *******************************************************************************
 *
 * SPDX-License-Identifier: Zlib
 *
 * The licensor of this software is Silicon Laboratories Inc.
 *
 * This software is provided 'as-is', without any express or implied
 * warranty. In no event will the authors be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 *    claim that you wrote the original software. If you use this software
 *    in a product, an acknowledgment in the product documentation would be
 *    appreciated but is not required.
 * 2. Altered source versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
 *
 ******************************************************************************/

/*
 * This is kind of a clusterfuck and could use with a clean up; it's mostly copied out of the
 * Silicon Labs reference code examples, with a few tweaks to fit the embededd-fw base.
 */
OUTPUT_FORMAT("elf32-littlearm", "elf32-littlearm", "elf32-littlearm")
OUTPUT_ARCH(arm)
SEARCH_DIR(.)
ENTRY(Reset_Handler)

MEMORY
{
    FLASH   (rx)  : ORIGIN = 0x08000000, LENGTH = 0x80000
    RAM     (rwx) : ORIGIN = 0x20000000, LENGTH = 0x10000
}

/*
 * Size of the stack
 *
 * This is only used at initialization, before the FreeRTOS scheduler is started; as well as for
 * interrupt handlers.
 */
STACK_SIZE = 0x800;

SECTIONS
{
    .text ALIGN(16) :
    {
        KEEP(*(.isr_vector))
        KEEP(*(.vectors .vectors.*))

        KEEP(*(.startup .startup.*))

        *(.text .text.* .gnu.linkonce.t.*)
        *(.glue_7t) *(.glue_7)

        KEEP(*(.init))
        KEEP(*(.fini))
    } > FLASH

    /* read-only data */
    .rodata ALIGN(16) :
    {
        *(.rodata .rodata* .gnu.linkonce.r.*)
        *(.eh_frame*)

        /* Support C constructors, and C destructors in both user code
           and the C library. This also provides support for C++ code. */
        . = ALIGN(4);
        KEEP(*(.init))
        . = ALIGN(4);
        PROVIDE_HIDDEN(__preinit_array_start = .);
        KEEP (*(.preinit_array))
        PROVIDE_HIDDEN(__preinit_array_end = .);

        . = ALIGN(4);
        PROVIDE_HIDDEN(__init_array_start = .);
        KEEP (*(SORT(.init_array.*)))
        KEEP (*(.init_array))
        PROVIDE_HIDDEN(__init_array_end = .);

        . = ALIGN(4);
        KEEP(*(.fini))

        . = ALIGN(4);
        PROVIDE_HIDDEN(__fini_array_start = .);
        KEEP (*(.fini_array))
        KEEP (*(SORT(.fini_array.*)))
        PROVIDE_HIDDEN(__fini_array_end = .);
        . = ALIGN(4);
    } >FLASH

    __etext = .;

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

    /* Start placing output sections which are loaded into RAM */
    . = ORIGIN(RAM);

    _sidata = LOADADDR(.data);
    .data :
    {
        _sdata = .;
        *(vtable)
        *(.data .data*);
        . = ALIGN(4);
        _edata = .;

        /* RAM functions (?) */
        PROVIDE(__ram_func_section_start = .);
        *(.ram)
        PROVIDE(__ram_func_section_end = .);

        /* All data end */
        __data_end__ = .;
    } >RAM AT> FLASH
    _datalen = _edata - _sdata;

    /* zeroed sections */
    .bss ALIGN(16) (NOLOAD):
    {
        _sbss = .;
        _szero = .;
        *(.bss .bss.*)
        *(COMMON)
        . = ALIGN(4);
        _ebss = .;
        _ezero = .;
    } >RAM
    _bsslen = _ebss - _sbss;

    /* end of regular, loaded regions */
    __end__ = .;
    end = __end__;
    _end = __end__;

    /* boot and interrupt stack */
    . = ALIGN(16);

    PROVIDE_HIDDEN(_sstack = .);
    . = . + STACK_SIZE;
    PROVIDE_HIDDEN(_estack = .);

    /* remainder of RAM is available as a heap */
    PROVIDE_HIDDEN(_sheap = .);
    PROVIDE_HIDDEN(_eheap = ORIGIN(RAM) + LENGTH(RAM));

    /* this is where we might add sections for NVM3 driver */
}
