/*
 *  TOPPERS/FMP Kernel
 *      Toyohashi Open Platform for Embedded Real-Time Systems/
 *      Flexible MultiProcessor Kernel
 *
 *  Copyright (C) 2005-2020 by Embedded and Real-Time Systems Laboratory
 *              Graduate School of Information Science, Nagoya Univ., JAPAN
 *
 *  上記著作権者は，以下の(1)〜(4)の条件を満たす場合に限り，本ソフトウェ
 *  ア（本ソフトウェアを改変したものを含む．以下同じ）を使用・複製・改
 *  変・再配布（以下，利用と呼ぶ）することを無償で許諾する．（条件略）
 *
 *  本ソフトウェアは，無保証で提供されているものである．
 */

#ifndef TOPPERS_CHIP_ASM_INC
#define TOPPERS_CHIP_ASM_INC

/*
 * dst に自プロセッサのインデックス（0オリジン）を代入するマクロ．
 * dst = get_my_prcidx()
 *
 * RP2350 は SIO の CPUID レジスタ（0xd0000000）で自コア番号（0/1）を直接
 * 読めるため，musca のような割込みスタック範囲判定は不要である．スタック
 * 設定前（起動時）にも読めるため，my_prcidx_boot もデフォルト（my_prcidx
 * へ委譲）でよい．
 */
#if TNUM_PRCID >= 2
.macro my_prcidx reg
    ldr  \reg, =0xd0000000      /* RP2350_SIO_BASE + 0x000 = SIO_CPUID */
    ldr  \reg, [\reg]
.endm
#else /* TNUM_PRCID >= 2 */
.macro my_prcidx reg
    movs \reg, #0
.endm
#endif /* TNUM_PRCID >= 2 */

/*
 * コア依存部で共通な定義（デフォルトの my_prcidx_boot は my_prcidx に委譲）
 */
#include "core_asm.inc"

#endif /* TOPPERS_CHIP_ASM_INC */
