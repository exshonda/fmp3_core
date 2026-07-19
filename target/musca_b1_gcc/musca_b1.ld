/*
 *  リンカスクリプト（ARM Musca-B1 / QEMU / FMP3 用）
 *
 *  SSE-200 のメモリマップ（hw/arm/armsse.c, hw/arm/musca.c）に基づく．
 *    0x10000000.. : QSPI フラッシュのエイリアス（0x00000000 のコード域）
 *                   musca-b1 の init_svtor = 0x10000000．CPU はリセット時に
 *                   ここからベクタテーブルを読む．
 *    0x20000000.. : SSE-200 内蔵 SRAM（4 バンク，SRAM_ADDR_WIDTH=17．
 *                   1 バンク 128KB × 4 = 512KB）．
 *
 *  CPU はリセット時に init_svtor（=0x10000000）からベクタテーブルを読む
 *  ため，ベクタ（.vector）を FLASH 先頭に配置する．
 */

OUTPUT_FORMAT("elf32-littlearm", "elf32-bigarm", "elf32-littlearm")
OUTPUT_ARCH(arm)

MEMORY
{
    FLASH(rx)  : ORIGIN = 0x10000000, LENGTH = 8192k
    RAM(rwx)   : ORIGIN = 0x20000000, LENGTH = 512k
}

ENTRY(_kernel_start)

SECTIONS
{
    /* TEXT セクション（ベクタテーブルを先頭に配置） */
    .text : ALIGN(4)
    {
        KEEP(*(.vector))
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

    /* BSS セクション */
    .bss : ALIGN(4)
    {
        __bss_start = .;
        *(.bss*)
        *(COMMON)
        . = ALIGN(4);
        __bss_end = .;
    } > RAM

    /*
     *  NOINIT セクション（マルチプロセッサ起動ハンドシェイク用）
     *
     *  BSS（ゼロクリア）にも DATA（初期値コピー）にも含めない領域．二次コア
     *  起動の判定フラグ（_kernel_mp_boot_flag）をここに置く．マスタコア
     *  （CPU0）が BSS をゼロクリアした後で値を書き込んでも消されず，また
     *  二次コア（CPU1）が起動した時点でその値を観測できる必要があるため，
     *  初期化対象のセクションから外す．QEMU では SRAM がマシン起動時に 0 で
     *  確保されるため，初期値は 0 とみなせる．
     */
    .noinit (NOLOAD) : ALIGN(4)
    {
        __noinit_start = .;
        *(.noinit*)
        . = ALIGN(4);
        __noinit_end = .;
    } > RAM

    /*
     *  ヒープ領域
     *
     *  newlib のシステムコールスタブ（libnosys の _sbrk）は，ヒープの開始位置
     *  を示すシンボル end をリンカスクリプトから与えられることを前提とする．
     *  これが無いと，stdio の reent 経路を引き込むアプリケーション
     *  （test_mtrans2 / test_mtrans5 等）が `undefined reference to \`end'` で
     *  リンクできない．RAM の最後の割付け（.noinit）の直後をヒープ開始とする．
     *
     *  PROVIDE を用いるのは，アプリケーション側で独自に定義した場合に，
     *  そちらを優先させるためである．
     */
    . = ALIGN(8);
    PROVIDE(end = .);
    PROVIDE(_end = .);
    PROVIDE(__heap_start = .);
    PROVIDE(__heap_limit = ORIGIN(RAM) + LENGTH(RAM));
}
