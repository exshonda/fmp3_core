/*
 *  TOPPERS/FMP Kernel
 *      Toyohashi Open Platform for Embedded Real-Time Systems/
 *      Flexible MultiProcessor Kernel
 *
 *  Copyright (C) 2006-2023 by Embedded and Real-Time Systems Laboratory
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
 */

/*
 *  kernel.hのチップ依存部（KRIA Cortex-A53(AArch64)用）
 *
 *  このインクルードファイルは，kernel.hでインクルードされる．他のファ
 *  イルから直接インクルードすることはない．このファイルをインクルード
 *  する前に，t_stddef.hがインクルードされるので，それらに依存してもよ
 *  い．
 */

#ifndef TOPPERS_TARGET_KERNEL_H
#define TOPPERS_TARGET_KERNEL_H

/*
 *  プロセッサ数
 */
#ifndef TNUM_PRCID
#define TNUM_PRCID		4
#endif /* TNUM_PRCID */

/*
 *  プロセッサIDの定義
 */
#define PRC1		1
#if TNUM_PRCID >= 2
#define PRC2		2
#endif
#if TNUM_PRCID >= 3
#define PRC3		3
#endif
#if TNUM_PRCID >= 4
#define PRC4		4
#endif

/*
 *  マスタプロセッサ
 */
#define TOPPERS_MASTER_PRCID	PRC1

/*
 *  タイムマスタプロセッサ
 */
#define TOPPERS_TMASTER_PRCID	PRC1

/*
 *  タイムイベント処理プロセッサとするプロセッサ
 */
#define TOPPERS_TEPP_PRC	0xf  /* PRC1とPRC2とPRC3とPRC4 */

/*
 *  クラスIDの定義
 */
#if TNUM_PRCID == 1
#define CLS_PRC1		1		/* 割付け可能：PRC1，初期割付け：PRC1 */
#define CLS_ALL_PRC1	2		/* 割付け可能：すべて，初期割付け：PRC1 */
#elif TNUM_PRCID == 2
#define CLS_PRC1		1		/* 割付け可能：PRC1，初期割付け：PRC1 */
#define CLS_PRC2		2		/* 割付け可能：PRC2，初期割付け：PRC2 */
#define CLS_ALL_PRC1	3		/* 割付け可能：すべて，初期割付け：PRC1 */
#define CLS_ALL_PRC2	4		/* 割付け可能：すべて，初期割付け：PRC2 */
#elif TNUM_PRCID == 3
#define CLS_PRC1		1		/* 割付け可能：PRC1，初期割付け：PRC1 */
#define CLS_PRC2		2		/* 割付け可能：PRC2，初期割付け：PRC2 */
#define CLS_PRC3		3		/* 割付け可能：PRC3，初期割付け：PRC3 */
#define CLS_ALL_PRC1	4		/* 割付け可能：すべて，初期割付け：PRC1 */
#define CLS_ALL_PRC2	5		/* 割付け可能：すべて，初期割付け：PRC2 */
#define CLS_ALL_PRC3	6		/* 割付け可能：すべて，初期割付け：PRC3 */
#elif TNUM_PRCID == 4
#define CLS_PRC1		1		/* 割付け可能：PRC1，初期割付け：PRC1 */
#define CLS_PRC2		2		/* 割付け可能：PRC2，初期割付け：PRC2 */
#define CLS_PRC3		3		/* 割付け可能：PRC3，初期割付け：PRC3 */
#define CLS_PRC4		4		/* 割付け可能：PRC4，初期割付け：PRC4 */
#define CLS_ALL_PRC1	5		/* 割付け可能：すべて，初期割付け：PRC1 */
#define CLS_ALL_PRC2	6		/* 割付け可能：すべて，初期割付け：PRC2 */
#define CLS_ALL_PRC3	7		/* 割付け可能：すべて，初期割付け：PRC3 */
#define CLS_ALL_PRC4	8		/* 割付け可能：すべて，初期割付け：PRC4 */
#else
#error TNUM_PRCID is out of range.
#endif

/*
 *  高分解能タイマのタイマ周期
 */
#ifndef USE_64BIT_HRTCNT
/* TCYC_HRTCNTは定義しない．*/
#endif /* USE_64BIT_HRTCNT */

/*
 *  高分解能タイマのカウント値の進み幅
 */
#define TSTEP_HRTCNT	1U

/*
 *  オーバランハンドラの残りプロセッサ時間に指定できる最大値
 */
#define TMAX_OVRTIM		858993459U			/* floor(2^32/5) */

/*
 *  チップ依存で共通な定義
 */
#include "chip_kernel.h"

#endif /* TOPPERS_TARGET_KERNEL_H */
