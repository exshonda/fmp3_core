/*
 *  kernel.h のターゲット依存部（ARM Musca-B1 / FMP3 用）
 *
 *  このインクルードファイルは，kernel.h でインクルードされる．このファ
 *  イルをインクルードする前に，t_stddef.h がインクルードされる．
 */

#ifndef TOPPERS_TARGET_KERNEL_H
#define TOPPERS_TARGET_KERNEL_H

/*
 *  プロセッサ数
 *
 *  Musca-B1 は SSE-200 を中核とする dual Cortex-M33（最大 2 コア）構成．
 *  TNUM_PRCID が未指定なら単一プロセッサ（1）とする．
 */
#ifndef TNUM_PRCID
#define TNUM_PRCID	1
#endif /* TNUM_PRCID */

#if TNUM_PRCID < 1 || TNUM_PRCID > 2
#error TNUM_PRCID is out of range (Musca-B1 supports 1 or 2 processors).
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
 *  SysTick によるイベント駆動 HRT（カウント値の進み幅は 1us）．
 *  タイマ周期は 2^32 [us] のため TCYC_HRTCNT は定義しない．
 */
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
