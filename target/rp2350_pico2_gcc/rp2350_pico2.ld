/*
 *  リンカスクリプト（RP2350 / RaspberryPi Pico 2 / FMP3 用）
 *
 *  RP2350 のメモリマップ：
 *    0x10000000.. : QSPI フラッシュ（XIP，4MB）．bootROM はここの先頭 4KB
 *                   から IMAGE_DEF（.picobin_block）を探索し，イメージ先頭を
 *                   ベクタ表とみなして SP=vector[0] / PC=vector[1] で起動する．
 *    0x20000000.. : SRAM（512KB）．
 *
 *  配置順が重要：先頭に .vector（コンフィギュレータ生成のベクタ表），その
 *  直後に .picobin_block（IMAGE_DEF）を置く（pico-sdk memmap_default.ld と同順）．
 */

OUTPUT_FORMAT("elf32-littlearm", "elf32-bigarm", "elf32-littlearm")
OUTPUT_ARCH(arm)

MEMORY
{
    FLASH(rx)  : ORIGIN = 0x10000000, LENGTH = 4096k
    RAM(rwx)   : ORIGIN = 0x20000000, LENGTH = 512k
}

ENTRY(_kernel_start)

SECTIONS
{
    /* TEXT セクション（ベクタ表→IMAGE_DEF を先頭に配置） */
    .text : ALIGN(4)
    {
        KEEP(*(.vector))
        KEEP(*(.picobin_block))
        *(.text*)
        *(.rodata .rodata.* .constdata .constdata.*)
        . = ALIGN(4);
    } > FLASH

    __idata_start = .;
    /* DATA セクション（RAM に配置，初期値は FLASH に置く） */
    .data : ALIGN(4)
    {
        __data_start = .;
        *(.data*)
        . = ALIGN(4);
    } > RAM AT> FLASH
    __idata_end = __idata_start + SIZEOF(.data);

    /* BSS セクション（割込みスタック・タスクスタック等を含む） */
    .bss : ALIGN(4)
    {
        __bss_start = .;
        *(.bss*)
        *(COMMON)
        . = ALIGN(4);
        __bss_end = .;
    } > RAM

    /* NOINIT セクション（初期化対象外） */
    .noinit (NOLOAD) : ALIGN(4)
    {
        __noinit_start = .;
        *(.noinit*)
        . = ALIGN(4);
        __noinit_end = .;
    } > RAM
}
