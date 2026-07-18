/*
 *  TOPPERS/FMP Kernel
 *      Toyohashi Open Platform for Embedded Real-Time Systems/
 *      Flexible MultiProcessor Kernel
 *
 *  Copyright (C) 2004-2020 by Embedded and Real-Time Systems Laboratory
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
 *  @(#) $Id: core_kernel.h 282 2021-06-03 06:35:25Z ertl-honda $
 */

/*
 *		kernel.hのコア依存部（ARM64用）
 *
 *  このヘッダファイルは，target_kernel.h（または，そこからインクルード
 *  されるファイル）のみからインクルードされる．他のファイルから直接イ
 *  ンクルードしてはならない．
 */

#ifndef TOPPERS_CORE_KERNEL_H
#define TOPPERS_CORE_KERNEL_H

#include "arm64.h"

/*
 *  ターゲット定義のタスク属性
 */
#define TA_FPU		UINT_C(0x08)	/* FPUレジスタをコンテキストに含める */

/*
 *  スタックの型
 * 
 *  ARM64では，スタックを16byte境界に配置する必要がある
 */
#define TOPPERS_STK_T		__int128

/*
 *  CPU例外ハンドラ番号の定義
 */
#define EXCNO_CUR_SP0_SYNC		UINT_C(0)
#define EXCNO_CUR_SP0_IRQ		UINT_C(1)
#define EXCNO_CUR_SP0_FIQ		UINT_C(2)
#define EXCNO_CUR_SP0_SERR		UINT_C(3)
#define EXCNO_CUR_SPX_SYNC		UINT_C(4)
#define EXCNO_CUR_SPX_IRQ		UINT_C(5)
#define EXCNO_CUR_SPX_FIQ		UINT_C(6)
#define EXCNO_CUR_SPX_SERR		UINT_C(7)
#define EXCNO_L64_SYNC			UINT_C(8)
#define EXCNO_L64_IRQ			UINT_C(9)
#define EXCNO_L64_FIQ			UINT_C(10)
#define EXCNO_L64_SERR			UINT_C(11)
#define EXCNO_L32_SYNC			UINT_C(12)
#define EXCNO_L32_IRQ			UINT_C(13)
#define EXCNO_L32_FIQ			UINT_C(14)
#define EXCNO_L32_SERR			UINT_C(15)

/*
 *  割込み優先度に関する設定
 */
#if defined(TOPPERS_TZ_S)
#define TMIN_INTPRI (-((GIC_PRI_LEVEL >> 1) - 1))	/* 割込み優先度の最小値（最高値）*/
#elif defined(TOPPERS_TZ_NS)
#define TMIN_INTPRI (-((GIC_PRI_LEVEL >> 1) - 1))	/* 割込み優先度の最小値（最高値）*/
#else /* defined(TOPPERS_TZ_S) */
#define TMIN_INTPRI (-(GIC_PRI_LEVEL >> 1))			/* 割込み優先度の最小値（最高値）*/
#endif /* defined(TOPPERS_TZ_S) */

#define TMAX_INTPRI (-1)					/* 割込み優先度の最大値（最低値）*/

#ifndef TOPPERS_MACRO_ONLY

/*
 *  CPU例外の情報を記憶しているメモリ領域の構造
 */
typedef struct {
	int64_t		intpri;
	uint64_t	nest_count;
#ifdef USE_ARM64_FPU
	uint64_t	fpsr;
	uint64_t	dmy1;
	uint64_t	fpcr;
	uint64_t	dmy2;
	uint64_t	q0_0;
	uint64_t	q0_1;
	uint64_t	q1_0;
	uint64_t	q1_1;
	uint64_t	q2_0;
	uint64_t	q2_1;
	uint64_t	q3_0;
	uint64_t	q3_1;
	uint64_t	q4_0;
	uint64_t	q4_1;
	uint64_t	q5_0;
	uint64_t	q5_1;
	uint64_t	q6_0;
	uint64_t	q6_1;
	uint64_t	q7_0;
	uint64_t	q7_1;
	uint64_t	q8_0;
	uint64_t	q8_1;
	uint64_t	q9_0;
	uint64_t	q9_1;
	uint64_t	q10_0;
	uint64_t	q10_1;
	uint64_t	q11_0;
	uint64_t	q11_1;
	uint64_t	q12_0;
	uint64_t	q12_1;
	uint64_t	q13_0;
	uint64_t	q13_1;
	uint64_t	q14_0;
	uint64_t	q14_1;
	uint64_t	q15_0;
	uint64_t	q15_1;
	uint64_t	q16_0;
	uint64_t	q16_1;
	uint64_t	q17_0;
	uint64_t	q17_1;
	uint64_t	q18_0;
	uint64_t	q18_1;
	uint64_t	q19_0;
	uint64_t	q19_1;
	uint64_t	q20_0;
	uint64_t	q20_1;
	uint64_t	q21_0;
	uint64_t	q21_1;
	uint64_t	q22_0;
	uint64_t	q22_1;
	uint64_t	q23_0;
	uint64_t	q23_1;
	uint64_t	q24_0;
	uint64_t	q24_1;
	uint64_t	q25_0;
	uint64_t	q25_1;
	uint64_t	q26_0;
	uint64_t	q26_1;
	uint64_t	q27_0;
	uint64_t	q27_1;
	uint64_t	q28_0;
	uint64_t	q28_1;
	uint64_t	q29_0;
	uint64_t	q29_1;
	uint64_t	q30_0;
	uint64_t	q30_1;
	uint64_t	q31_0;
	uint64_t	q31_1;
#endif /* USE_ARM64_FPU */
	uint64_t	pc;
	uint64_t	pstate;
	uint64_t	x0;
	uint64_t	x1;
	uint64_t	x2;
	uint64_t	x3;
	uint64_t	x4;
	uint64_t	x5;
	uint64_t	x6;
	uint64_t	x7;
	uint64_t	x8;
	uint64_t	x9;
	uint64_t	x10;
	uint64_t	x11;
	uint64_t	x12;
	uint64_t	x13;
	uint64_t	x14;
	uint64_t	x15;
	uint64_t	x16;
	uint64_t	x17;
	uint64_t	x18;
	uint64_t	x19;
	uint64_t	x20;
	uint64_t	x21;
	uint64_t	x22;
	uint64_t	x23;
	uint64_t	x24;
	uint64_t	x25;
	uint64_t	x26;
	uint64_t	x27;
	uint64_t	x28;
	uint64_t	x29;
	uint64_t	x30;
	uint64_t	sp;
} T_EXCINF;
#endif /* TOPPERS_MACRO_ONLY */
#endif /* TOPPERS_CORE_KERNEL_H */
