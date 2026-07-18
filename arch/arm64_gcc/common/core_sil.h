/*
 *  TOPPERS Software
 *      Toyohashi Open Platform for Embedded Real-Time Systems
 *
 *  Copyright (C) 2000-2003 by Embedded and Real-Time Systems Laboratory
 *                              Toyohashi Univ. of Technology, JAPAN
 *  Copyright (C) 2004-2025 by Embedded and Real-Time Systems Laboratory
 *              Graduate School of Information Science, Nagoya Univ., JAPAN
 *
 *  上記著作権者は，以下の(1)～(4)の条件を満たす場合に限り，本ソフトウェ
 *  ア（本ソフトウェアを改変したものを含む．以下同じ）を使用・複製・改
 *  変・再配布（以下，利用と呼ぶ）することを無償で許諾する．
 *  (1) 本ソフトウェアをソースコードの形で利用する場合には，上記の著作
 *      権表示，この利用条件および下記の無保証規定が，そのままの形でソー
 *      スコード中に含まれていること．
 *  (2) 本ソフトウェアを，ライブラリ形式など，他のソフトウェア開発に使
 *      用できる形で再配布する場合には，再配布に伴うドキュメント（利用
 *      者マニュアルなど）に，上記の著作権表示，この利用条件および下記
 *      の無保証規定を掲載すること．
 *  (3) 本ソフトウェアを，機器に組み込むなど，他のソフトウェア開発に使
 *      用できない形で再配布する場合には，次のいずれかの条件を満たすこ
 *      と．
 *    (a) 再配布に伴うドキュメント（利用者マニュアルなど）に，上記の著
 *        作権表示，この利用条件および下記の無保証規定を掲載すること．
 *    (b) 再配布の形態を，別に定める方法によって，TOPPERSプロジェクトに
 *        報告すること．
 *  (4) 本ソフトウェアの利用により直接的または間接的に生じるいかなる損
 *      害からも，上記著作権者およびTOPPERSプロジェクトを免責すること．
 *      また，本ソフトウェアのユーザまたはエンドユーザからのいかなる理
 *      由に基づく請求からも，上記著作権者およびTOPPERSプロジェクトを
 *      免責すること．
 *
 *  本ソフトウェアは，無保証で提供されているものである．上記著作権者お
 *  よびTOPPERSプロジェクトは，本ソフトウェアに関して，特定の使用目的
 *  に対する適合性も含めて，いかなる保証も行わない．また，本ソフトウェ
 *  アの利用により直接的または間接的に生じたいかなる損害に関しても，そ
 *  の責任を負わない．
 *
 *  @(#) $Id: core_sil.h 446 2025-06-27 07:54:07Z ertl-honda $
 */

/*
 *		sil.hのコア依存部（ARM64用）
 *
 *  このヘッダファイルは，target_sil.h（または，そこからインクルードさ
 *  れるファイル）のみからインクルードされる．他のファイルから直接イン
 *  クルードしてはならない．
 */

#ifndef TOPPERS_CORE_SIL_H
#define TOPPERS_CORE_SIL_H

#include <t_stddef.h>

#ifndef TOPPERS_MACRO_ONLY

/*
 *  メモリが変更されることをコンパイラに伝えるためのマクロ
 */
#define TOPPERS_MEMORY_CHANGED Asm("":::"memory")

/*
 *  メモリバリア
 */
#define TOPPERS_DMB()	Asm("dmb sy");
#define TOPPERS_DSB()	Asm("dsb sy");

/*
 *  PSTATEのDAIFフィールドの現在値の読出し
 */
Inline uint32_t
TOPPERS_current_daif(void)
{
	uint32_t	daif;

	Asm("mrs  %0, daif" : "=r"(daif));
	return(daif);
}

/*
 *  PSTATEのDAIFフィールドの現在値の変更
 */
Inline void
TOPPERS_set_daif(uint32_t daif)
{
	Asm("msr daif, %0" : : "r"(daif) : "memory","cc");
}


/*
 *  すべての割込み（FIQとIRQ）の禁止
 */
Inline uint32_t
TOPPERS_disint(void)
{
	uint32_t	daif;
	uint32_t	fiq_irq_mask;

	daif = TOPPERS_current_daif();
	fiq_irq_mask = daif & (0x40U|0x80U);
	Asm("msr DAIFset, #3" ::: "memory");
	return(fiq_irq_mask);
}

/*
 *  FIQとIRQの禁止ビットの復帰
 */
Inline void
TOPPERS_set_fiq_irq(uint32_t fiq_irq_mask)
{
	uint32_t	daif;

	daif = TOPPERS_current_daif();
	daif &= ~(0x40U|0x80U);
	daif |= fiq_irq_mask;
	TOPPERS_set_daif(daif);
}

/*
 *  全割込みロック状態の制御
 */
#define SIL_PRE_LOC		uint32_t TOPPERS_fiq_irq_mask
#define SIL_LOC_INT()	((void)(TOPPERS_fiq_irq_mask = TOPPERS_disint()))
#define SIL_UNL_INT()	(TOPPERS_set_fiq_irq(TOPPERS_fiq_irq_mask))

/*
 *  スピンロック変数（core_kernel_impl.c）
 */
extern volatile uint32_t TOPPERS_sil_spn_var[];

/*
 *  Test&Assign操作
 */
Inline bool_t
TOPPERS_test_and_assign(uint32_t *p_var, uint32_t prcid)
{
	uint32_t		locked;

	Asm("ldxr	w1, [%1]			\n"
	"	cmp		w1, #0				\n"
	"	b.ne	1f					\n"
	"	stxr	w1, %w2, [%1]		\n"
	"1:								\n"
	"	mov		%0, x1				\n"
	: "=r"(locked) : "r"(p_var),"r"(prcid) : "x1", "cc");

	return(locked != 0);
}

/*
 *  スピンロックの取得
 */
Inline uint32_t
TOPPERS_sil_loc_spn(void)
{
	uint32_t	daif;
	uint32_t	fiq_irq_mask;
	ID			prcid;

	/* 全割込みロック状態に */
	daif = TOPPERS_current_daif();
	fiq_irq_mask = daif & (0x40U|0x80U);
	Asm("msr DAIFset, #3" ::: "memory");

	/* スピンロックのチェック */
	sil_get_pid(&prcid);
	if (TOPPERS_sil_spn_var[0] == prcid) {
		/* スピンロックを取得している場合 */
		fiq_irq_mask |= 0x01U;
	}
	else {
		/* スピンロックの取得 */
		while (TOPPERS_test_and_assign(&TOPPERS_sil_spn_var[0], prcid)) {
			TOPPERS_set_daif(daif);
			TOPPERS_DSB();
			Asm("wfe");
			Asm("msr DAIFset, #3" ::: "memory");
		}
		/* ロック取得成功 */
		TOPPERS_DMB();
		TOPPERS_MEMORY_CHANGED;
	}
	return(fiq_irq_mask);
}

/*
 *  スピンロックの返却
 */
Inline void
TOPPERS_sil_unl_spn(uint32_t fiq_irq_mask)
{
	if ((fiq_irq_mask & 0x01U) != 0U) {
		/* スピンロックを取得していた場合 */
		fiq_irq_mask &= ~(0x01U);
	}
	else {
		TOPPERS_MEMORY_CHANGED;
		TOPPERS_DMB();
		TOPPERS_sil_spn_var[0] = 0U;
#ifndef TOPPERS_OMIT_USE_WFE
		TOPPERS_DSB();
		Asm("sev");
#endif /* TOPPERS_OMIT_USE_WFE */
	}
	TOPPERS_set_fiq_irq(fiq_irq_mask);
}

/*
 *  SILスピンロック
 */
#define SIL_LOC_SPN() ((void)(TOPPERS_fiq_irq_mask = TOPPERS_sil_loc_spn()))
#define SIL_UNL_SPN() (TOPPERS_sil_unl_spn(TOPPERS_fiq_irq_mask))

/*
 *  スピンロックの強制解放
 *
 *  自プロセッサがスピンロックを取得している場合に解放する．
 */
Inline void
TOPPERS_sil_force_unl_spn(void)
{
	ID	prcid;

	sil_get_pid(&prcid);
	if (TOPPERS_sil_spn_var[0] == prcid) {
		TOPPERS_sil_spn_var[0] = 0U;
		TOPPERS_DSB();
		Asm("sev");
	}
}

/*
 *  メモリ同期バリア
 */
#define TOPPERS_SIL_WRITE_SYNC()	TOPPERS_DSB()

#endif /* TOPPERS_MACRO_ONLY */
#endif /* TOPPERS_CORE_SIL_H */
