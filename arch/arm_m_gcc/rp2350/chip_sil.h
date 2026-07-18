/*
 *  TOPPERS Software
 *      Toyohashi Open Platform for Embedded Real-Time Systems
 *
 *  Copyright (C) 2007,2011,2015 by Embedded and Real-Time Systems Laboratory
 *              Graduate School of Information Science, Nagoya Univ., JAPAN
 *
 *  上記著作権者は，以下の(1)〜(4)の条件を満たす場合に限り，本ソフトウェ
 *  ア（本ソフトウェアを改変したものを含む．以下同じ）を使用・複製・改
 *  変・再配布（以下，利用と呼ぶ）することを無償で許諾する．（条件略）
 *
 *  本ソフトウェアは，無保証で提供されているものである．
 */

/*
 * sil.hのチップ依存部（RP2350用）
 */

#ifndef TOPPERS_CHIP_SIL_H
#define TOPPERS_CHIP_SIL_H

/*
 * プロセッサのインディアン定義（RP2350 はリトルエンディアン）
 */
#define SIL_ENDIAN_LITTLE

/*
 * NVIC の割込み優先度ビット幅
 *
 * RP2350（Cortex-M33）の NVIC は 4 ビットの優先度を実装する．未定義だと
 * core_kernel_impl.h がコンパイルエラーとなる．
 */
#ifndef TBITW_IPRI
#define TBITW_IPRI 4
#endif /* TBITW_IPRI */

/*
 * プロセッサで共通な定義
 */
#include <core_sil.h>
#include <core_insn.h>

/*
 * 一般共通レジスタ操作関数
 *
 * RP2350 はレジスタアドレスのエイリアス（+0x1000:xor / +0x2000:set /
 * +0x3000:clear）を用いて単一の書込みでアトミックなビット操作を行える．
 * ただしこのエイリアスは APB/AHB ペリフェラル用であり，M33 の PPB
 * （NVIC/SCB/CPACR 等，0xE000xxxx）には存在しない（書込みは無視される）．
 * PPB へのビット操作は read-modify-write を直接行うこと．
 */
#define sil_mskb( mem, val, msk ) sil_wrb_mem((uint8_t *)((uintptr_t)(mem) + 0x1000), ((sil_reb_mem(mem) ^ (val)) & (msk)))
#define sil_orb( mem, val )  sil_wrb_mem((uint8_t *)((uintptr_t)(mem) + 0x2000), val)
#define sil_andb( mem, val ) sil_wrb_mem(mem, sil_reb_mem(mem) & val)
#define sil_clrb( mem, val ) sil_wrb_mem((uint8_t *)((uintptr_t)(mem) + 0x3000), val)
#define sil_mskh( mem, val, msk ) sil_wrh_mem((uint16_t *)((uintptr_t)(mem) + 0x1000), ((sil_reh_mem(mem) ^ (val)) & (msk)))
#define sil_orh( mem, val )  sil_wrh_mem((uint16_t *)((uintptr_t)(mem) + 0x2000), val)
#define sil_andh( mem, val ) sil_wrh_mem(mem, sil_reh_mem(mem) & val)
#define sil_clrh( mem, val ) sil_wrh_mem((uint16_t *)((uintptr_t)(mem) + 0x3000), val)
#define sil_mskw( mem, val, msk ) sil_wrw_mem((uint32_t *)((uintptr_t)(mem) + 0x1000), ((sil_rew_mem(mem) ^ (val)) & (msk)))
#define sil_orw( mem, val )  sil_wrw_mem((uint32_t *)((uintptr_t)(mem) + 0x2000), val)
#define sil_andw( mem, val ) sil_wrw_mem(mem, sil_rew_mem(mem) & val)
#define sil_clrw( mem, val ) sil_wrw_mem((uint32_t *)((uintptr_t)(mem) + 0x3000), val)

#include "RP2350.h"

/*
 * SIO ハードウェアスピンロックの割当て
 *
 *   15 : SIL スピンロック（SIL_LOC_SPN）
 *   30 : ジャイアントロック
 *   31 : OS 予約（未使用）
 * ネイティブスピンロックは chip_kernel.trb が 0 から順に割り当てる．
 */
#ifndef SPINLOCK_NO_FOR_SIL_LOCK
#define SPINLOCK_NO_FOR_SIL_LOCK   15
#endif /* SPINLOCK_NO_FOR_SIL_LOCK */
#ifndef SPINLOCK_NO_FOR_GIANT_LOCK
#define SPINLOCK_NO_FOR_GIANT_LOCK 30
#endif /* SPINLOCK_NO_FOR_GIANT_LOCK */

#ifndef TOPPERS_MACRO_ONLY

/*
 * プロセッサIDの取得（1オリジン）
 */
Inline void
sil_get_pid(ID *p_prcid)
{
#if TNUM_PRCID >= 2
	*p_prcid = (ID)((*RP2350_SIO_CPUID) + 1);
#else /* TNUM_PRCID >= 2 */
	*p_prcid = 1;
#endif /* TNUM_PRCID >= 2 */
}

#if TNUM_PRCID >= 2

#define TOPPERS_sil_spn_hw_reg ((volatile uint32_t *)RP2350_SIO_SPINLOCKn(SPINLOCK_NO_FOR_SIL_LOCK))

/*
 * SIL スピンロック変数（chip_kernel_impl.c）
 *
 * 値は取得しているプロセッサID（1オリジン）．再帰取得を識別するために用いる．
 */
extern uint32_t TOPPERS_sil_spn_var;

/*
 * SIL スピンロックの取得
 */
extern uint32_t TOPPERS_sil_loc_spn(void);

/*
 * SIL スピンロックの返却
 */
Inline void
TOPPERS_sil_unl_spn(uint32_t primask)
{
	if (primask & (1U << 1)) {
		/* 再帰取得（既に取得済みだった）の場合は何もしない */
		primask &= ~(1U << 1);
	}
	else {
		TOPPERS_sil_spn_var = 0;
		ARM_MEMORY_CHANGED;
		*TOPPERS_sil_spn_hw_reg = 0; /* SIO スピンロック解放 */
	}
	assign_primask(primask);
}

/*
 *  SILスピンロック
 */
#define SIL_LOC_SPN() ((void)(TOPPERS_locked = TOPPERS_sil_loc_spn()))
#define SIL_UNL_SPN() (TOPPERS_sil_unl_spn(TOPPERS_locked))

/*
 * SIL スピンロックの強制解放（自プロセッサが取得していれば解放する）
 */
Inline void
TOPPERS_sil_force_unl_spn(void)
{
	ID prcid;
	sil_get_pid(&prcid);
	if (TOPPERS_sil_spn_var == (uint32_t) prcid) {
		*TOPPERS_sil_spn_hw_reg = 0;
		TOPPERS_sil_spn_var = 0;
		ARM_MEMORY_CHANGED;
	}
}

#else /* TNUM_PRCID >= 2 */

/*
 * SIL スピンロック（単一プロセッサ版）
 *
 * 単一プロセッサであり，スピンロックは割込みロックと等価である．
 */
#define SIL_LOC_SPN()  SIL_LOC_INT()
#define SIL_UNL_SPN()  SIL_UNL_INT()

/*
 * SIL スピンロックの強制解放（単一プロセッサでは何もしない）
 */
Inline void
TOPPERS_sil_force_unl_spn(void)
{
}

#endif /* TNUM_PRCID >= 2 */

#endif /* TOPPERS_MACRO_ONLY */

#endif /* TOPPERS_CHIP_SIL_H */
