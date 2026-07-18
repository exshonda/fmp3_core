/*
 *  kernel.h のターゲット依存部（RP2350 / RaspberryPi Pico 2 / FMP3 用）
 *
 *  このインクルードファイルは，kernel.h でインクルードされる．このファ
 *  イルをインクルードする前に，t_stddef.h がインクルードされる．
 */

#ifndef TOPPERS_TARGET_KERNEL_H
#define TOPPERS_TARGET_KERNEL_H

/*
 *  プロセッサ数
 *
 *  RP2350 は dual Cortex-M33（最大 2 コア）構成．
 *  TNUM_PRCID が未指定なら単一プロセッサ（1）とする．
 */
#ifndef TNUM_PRCID
#define TNUM_PRCID	1
#endif /* TNUM_PRCID */

#if TNUM_PRCID < 1 || TNUM_PRCID > 2
#error TNUM_PRCID is out of range (RP2350 supports 1 or 2 processors).
#endif

/*
 *  プロセッサID
 */
#define PRC1	1
#if TNUM_PRCID >= 2
#define PRC2	2
#endif /* TNUM_PRCID >= 2 */

/*
 *  マスタプロセッサのID
 */
#define TOPPERS_MASTER_PRCID	PRC1
#define TOPPERS_TMASTER_PRCID	PRC1

/*
 *  クラスID
 */
#if TNUM_PRCID == 1
#define CLS_PRC1		1	/* 割付け可能：PRC1，初期割付け：PRC1 */
#define CLS_ALL_PRC1	2	/* 割付け可能：すべて，初期割付け：PRC1 */
#elif TNUM_PRCID == 2
#define CLS_PRC1		1	/* 割付け可能：PRC1，初期割付け：PRC1 */
#define CLS_PRC2		2	/* 割付け可能：PRC2，初期割付け：PRC2 */
#define CLS_ALL_PRC1	3	/* 割付け可能：すべて，初期割付け：PRC1 */
#define CLS_ALL_PRC2	4	/* 割付け可能：すべて，初期割付け：PRC2 */
#endif /* TNUM_PRCID */

/*
 *  通信パスが存在するプロセッサの集合
 */
#ifndef TOPPERS_TEPP_PRC
#if TNUM_PRCID == 1
#define TOPPERS_TEPP_PRC	0x1		/* PRC1 のみ */
#else /* TNUM_PRCID == 1 */
#define TOPPERS_TEPP_PRC	0x3		/* PRC1 と PRC2 */
#endif /* TNUM_PRCID == 1 */
#endif /* TOPPERS_TEPP_PRC */

/*
 *  カーネル管理の割込み優先度の範囲
 */
#define TMIN_INTPRI	(-3)	/* 割込み優先度の最小値（最高値）*/

/*
 *  高分解能タイマの設定
 *
 *  RP2350 の共有 TIMER0（1MHz）をイベント駆動 HRT として用いる．PRC1 は
 *  ALARM0，PRC2 は ALARM1 を使用する（target_timer.c）．カウント値の進み
 *  幅は 1us，タイマ周期は 2^32 [us] のため TCYC_HRTCNT は定義しない．
 */
#ifndef USE_TIM_AS_HRT
#define USE_TIM_AS_HRT
#endif /* USE_TIM_AS_HRT */
#undef TCYC_HRTCNT
#define TSTEP_HRTCNT	1U

#ifndef TOPPERS_MACRO_ONLY
extern void	sta_ker(void);
#endif /* TOPPERS_MACRO_ONLY */

/*
 *  チップ依存で共通な定義
 */
#include "chip_kernel.h"

#endif /* TOPPERS_TARGET_KERNEL_H */
