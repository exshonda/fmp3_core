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

/*
 * ターゲット依存部モジュール（RP2350用）
 */

#include "kernel_impl.h"

#if TNUM_PRCID >= 2

/*
 * ジャイアントロック用のロック変数（SIO_SPINLOCKn のアドレスを保持）
 */
LOCK *giant_lock;

/*
 * SIL スピンロック変数（取得しているプロセッサID，0=未取得）
 */
uint32_t TOPPERS_sil_spn_var;

/*
 * SIL スピンロックの取得
 *
 * 返り値は，呼出し前の PRIMASK 状態（bit0）に，再帰取得を示す bit1 を加えた
 * もの．自プロセッサが既に取得していた場合は bit1 をセットして返す．
 */
uint32_t
TOPPERS_sil_loc_spn(void)
{
	uint32_t	primask = get_primask();
	ID			prcid;

	set_primask();
	sil_get_pid(&prcid);

	if (TOPPERS_sil_spn_var == (uint32_t) prcid) {
		/* すでに取得済み（再帰取得） */
		return (1U << 1) | primask;
	}
	while (1) {
		if (*TOPPERS_sil_spn_hw_reg) {
			/* SIO スピンロック取得成功 */
			TOPPERS_sil_spn_var = (uint32_t) prcid;
			ARM_MEMORY_CHANGED;
			return primask;
		}
		assign_primask(primask);
		set_primask();
	}
}

#else /* TNUM_PRCID >= 2 */

/*
 * ジャイアントロック用のロック変数（ユニプロセッサ用ダミー）
 */
LOCK giant_lock;

#endif /* TNUM_PRCID >= 2 */
