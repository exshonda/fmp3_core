/*
 *  TOPPERS/FMP Kernel
 *      Toyohashi Open Platform for Embedded Real-Time Systems/
 *      Flexible MultiProcessor Kernel
 *
 *  Copyright (C) 2007-2020 by Embedded and Real-Time Systems Laboratory
 *              Graduate School of Information Science, Nagoya Univ., JAPAN
 *
 *  上記著作権者は，以下の(1)〜(4)の条件を満たす場合に限り，本ソフトウェ
 *  ア（本ソフトウェアを改変したものを含む．以下同じ）を使用・複製・改
 *  変・再配布（以下，利用と呼ぶ）することを無償で許諾する．（条件略）
 *
 *  本ソフトウェアは，無保証で提供されているものである．
 */

/*
 * テストプログラムのターゲット依存定義（RP2350用）
 */

#ifndef TOPPERS_TARGET_TEST_H
#define TOPPERS_TARGET_TEST_H

#include <sil.h>
#include "rpi_pico2.h"
#include "RP2350.h"

#ifndef STACK_SIZE
#define STACK_SIZE	(1024)
#endif /* STACK_SIZE */

/*
 *  ソフトウェア割込みの代用として，カーネルが使用しない TIMER0 の
 *  ALARM2（IRQ2）／ALARM3（IRQ3）を用いる（ALARM0/1 はカーネル HRT が
 *  使用）．INTNO1 は PRC1，INTNO2 は PRC2 のクラスに割り付ける．
 */
#define INTNO1		((0x00010000U | RP2350_TIMER0_2_IRQn) + 16)
#define INTNO2		((0x00020000U | RP2350_TIMER0_3_IRQn) + 16)

#define INTNO1_INTATR	TA_ENAINT
#define INTNO2_INTATR	TA_ENAINT

#define INTNO1_INTPRI	(TMAX_INTPRI - 1)
#define INTNO2_INTPRI	(TMAX_INTPRI - 1)

#ifndef TOPPERS_MACRO_ONLY

Inline void
intno1_clear(void)
{
	sil_wrw_mem(RP2350_TIMER0_INTR, RP2350_TIMER0_INT_ALARM_2);
}

Inline void
intno2_clear(void)
{
	sil_wrw_mem(RP2350_TIMER0_INTR, RP2350_TIMER0_INT_ALARM_3);
}

#endif /* TOPPERS_MACRO_ONLY */

/*
 *  コア依存モジュール（ARM-M 用）
 */
#include "core_test.h"

#endif /* TOPPERS_TARGET_TEST_H */
