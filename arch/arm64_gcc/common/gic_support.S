/*
*  TOPPERS Software
 *      Toyohashi Open Platform for Embedded Real-Time Systems
 *
 *  Copyright (C) 2000-2003 by Embedded and Real-Time Systems Laboratory
 *                              Toyohashi Univ. of Technology, JAPAN
 *  Copyright (C) 2006-2025 by Embedded and Real-Time Systems Laboratory
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
 *  $Id: gic_support.S 446 2025-06-27 07:54:07Z ertl-honda $
 */

/*
 *		カーネルの割込みコントローラ依存部のアセンブリ言語部（GIC用）
 */

#define TOPPERS_MACRO_ONLY
#define UINT_C(val)			(val)		/* uint_t型の定数を作るマクロ */
#define ULONG_C(val)		(val)		/* ulong_t型の定数を作るマクロ */
#define CAST(type, val)		(val)		/* 型キャストを行うマクロ */
#define GIC_REG(base, offset)	((base) + (offset))
#include "kernel_impl.h"
#include "target_asm.inc"
#include "target_ipi.h"
#include "offset.h"

/*
 *  割込みハンドラ呼出し前の割込みコントローラ操作
 *
 *  x0に割込み番号を返す．
 */
	ATEXT
	AALIGN(2)
	AGLOBAL(irc_begin_int)
ALABEL(irc_begin_int)
	/*
	 *  割込み要因を取得する．
	 */
#if TOPPERS_GIC_VER == 2
	ldr		x1, =GICC_IAR
	ldr		w2, [x1]
#elif (TOPPERS_GIC_VER == 3) || (TOPPERS_GIC_VER == 4)
	mrs		x2, ICC_IAR1_EL1
#endif /* TOPPERS_GIC_VER == 2 */
	ldr		w1, =GICC_IAR_INTERRUPTID_MASK
	and		w0, w2, w1

	/*
	 *  割込み発生前の割込み優先度を保存する．
	 */
#if TOPPERS_GIC_VER == 2
	ldr		x3, =GICC_PMR			/* 割込み発生前の割込み優先度を取得 */
	ldr		w4, [x3]
#elif (TOPPERS_GIC_VER == 3) || (TOPPERS_GIC_VER == 4)
	mrs		x4,ICC_PMR_EL1			/* 割込み発生前の割込み優先度を取得 */
#endif /* TOPPERS_GIC_VER == 2 */

	stp		x2, x4, [sp, #-16]!	/* irc_end_intで用いる情報を保存 */

	/*
	 *  x0に割込み番号を入れた状態でリターンする
	 */
	ret

/*
 *  割込みハンドラ呼出し後の割込みコントローラ操作
 */
	ATEXT
	AALIGN(2)
	AGLOBAL(irc_end_int)
ALABEL(irc_end_int)

	ldp		x2, x4, [sp], #16		/* irc_begin_intで保存した情報を復帰 */

#if (TOPPERS_GIC_VER == 3) || (TOPPERS_GIC_VER == 4)
	/*
	 *  Spurious割込み(1022/1023)の場合は優先度の設定やEOIへの書き込みは行わない
	 */
	ldr		w1, =GICC_IAR_INTERRUPTID_MASK
	and		w3, w2, w1
	sub		w0, w3, #1022
	cmp		w0, 1
	bls		irc_end_int_1
#endif /* (TOPPERS_GIC_VER == 3) || (TOPPERS_GIC_VER == 4) */

	/*
	 *  EOIを発行する．
	 */
#if TOPPERS_GIC_VER == 2
	ldr		x3, =GICC_EOIR			/* EOIレジスタへの書込み */
	str		w2, [x3]
#elif (TOPPERS_GIC_VER == 3) || (TOPPERS_GIC_VER == 4)
	msr		ICC_EOIR1_EL1, x2		/* EOIレジスタへの書込み */
#endif /* TOPPERS_GIC_VER == 2 */

	/*
	 *  割込み優先度マスクを元に戻してリターンする．
	 */
#if TOPPERS_GIC_VER == 2
	ldr		x1, =GICC_PMR		/* 割込み優先度マスクを元に戻す */
	str		w4, [x1]
#elif (TOPPERS_GIC_VER == 3) || (TOPPERS_GIC_VER == 4)
	msr		ICC_PMR_EL1, x4		/* 割込み優先度マスクを元に戻す */
#endif /* TOPPERS_GIC_VER == 2 */
	dsb		sy						/* 割込み優先度マスクがセットされるのを待つ */

ALABEL(irc_end_int_1)
	ret

/*
 *  CPU例外発生前の割込み優先度の取得
 *
 *  CPU例外の発生で割込み優先度が変わることはないため，現在の割込み優先
 *  度を返す．
 */
	ATEXT
	AALIGN(2)
	AGLOBAL(irc_get_intpri)
ALABEL(irc_get_intpri)
	/*
	 *  割込み優先度マスクを外部表現に変換して返す．
	 */
#if TOPPERS_GIC_VER == 2
	ldr		x1, =GICC_PMR			/* 割込み発生前の割込み優先度を取得 */
	ldr		w0, [x1]
#elif (TOPPERS_GIC_VER == 3) || (TOPPERS_GIC_VER == 4)
	mrs		x0, ICC_PMR_EL1			/* 割込み発生前の割込み優先度を取得 */
#endif /* TOPPERS_GIC_VER == 2 */
	asr		w0, w0, #GIC_PRI_SHIFT		/* 外部表現に変更 */
	sub		w0, w0, #((GIC_PRI_LEVEL >> 1) - 1)
	ret

/*
 *  CPU例外ハンドラ呼出し前の割込みコントローラ操作
 *
 *  irc_end_excで用いる情報（CPU例外発生前の割込み優先度マスク）を，スタッ
 *	クの先頭に保存する．
 */
	ATEXT
	AALIGN(2)
	AGLOBAL(irc_begin_exc)
ALABEL(irc_begin_exc)
	/*
	 *  割込み優先度マスクを保存する．
	 */
#if TOPPERS_GIC_VER == 2
	ldr		x3, =GICC_PMR		/* 現在の割込み優先度を取得 */
	ldr		w0, [x3]
#elif (TOPPERS_GIC_VER == 3) || (TOPPERS_GIC_VER == 4)
	mrs		x0, ICC_PMR_EL1		/* 割込み発生前の割込み優先度を取得 */
#endif /* TOPPERS_GIC_VER == 2 */
	str		x0, [sp]			/* irc_end_excで用いる情報を保存 */
	ret

/*
 *  CPU例外ハンドラ呼出し後の割込みコントローラ操作
 *  x0に割込み発生前の割込み優先度マスクを入れた状態で呼び出される．
 */
	ATEXT
	AALIGN(2)
	AGLOBAL(irc_end_exc)
ALABEL(irc_end_exc)
	/*
	 *  割込み優先度マスクを元に戻す．
	 */
	ldr		x2, [sp]			/* irc_begin_excで保存した情報を復帰 */
#if TOPPERS_GIC_VER == 2
	ldr		x1, =GICC_PMR		/* 割込み発生前の割込み優先度を取得 */
	ldr		w2, [x1]
#elif (TOPPERS_GIC_VER == 3) || (TOPPERS_GIC_VER == 4)
	msr		ICC_PMR_EL1, x2			/* 新しい割込み優先度マスクをセットする */
#endif /* TOPPERS_GIC_VER == 2 */
	ret

