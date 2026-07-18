/*
 *  TOPPERS/FMP Kernel
 *      Toyohashi Open Platform for Embedded Real-Time Systems/
 *      Flexible MultiProcessor Kernel
 * 
 *  Copyright (C) 2000-2003 by Embedded and Real-Time Systems Laboratory
 *                              Toyohashi Univ. of Technology, JAPAN
 *  Copyright (C) 2005-2023 by Embedded and Real-Time Systems Laboratory
 *              Graduate School of Information Science, Nagoya Univ., JAPAN
 * 
 *  上記著作権者は，以下の(1)〜(4)の条件を満たす場合に限り，本ソフトウェ
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
 *  $Id: mpcore_timer.h 384 2023-09-14 05:26:40Z ertl-honda $
 */

/*
 *		タイマドライバ（MPCore内蔵タイマ用）
 *
 *  MPCoreは，プロセッサ毎にプライベートタイマとウォッチドッグを持つ．
 *  ウォッチドッグは，タイマとして使用することができる．また，各プロセッ
 *  サからアクセスできるグローバルタイマを持つ．
 *
 *  このモジュールは，グローバルタイマを用いて，高分解能タイマを実現す
 *	る．グローバルタイマは，Cortex-A9 MPCoreのr2p0以降の新しい仕様のも
 *	のを想定している．
 *
 *  また，対応するシンボルをマクロ定義することにより，以下の機能を提供
 *  する．
 *
 *  USE_MPCORE_TMR_TWDTIMER：
 *		プライベートタイマを用いて，タイムウィンドウタイマを実現する．
 *
 *  USE_MPCORE_WDG_OVRTIMER：
 *		ウォッチドッグを用いて，オーバランタイマを実現する．
 */

#ifndef TOPPERS_MPCORE_TIMER_H
#define TOPPERS_MPCORE_TIMER_H

#include "kernel/kernel_impl.h"
#include <sil.h>
#include "mpcore.h"

/*
 *		グローバルタイマを用いて高分解能タイマを実現
 */

/*
 *  高分解能タイマ割込みハンドラ登録のための定数
 */
#define INHNO_TIMER_PRC1	(0x10000U | MPCORE_IRQNO_GTC)	/* ハンドラ番号 */
#define INHNO_TIMER_PRC2	(0x20000U | MPCORE_IRQNO_GTC)	/* ハンドラ番号 */
#define INHNO_TIMER_PRC3	(0x30000U | MPCORE_IRQNO_GTC)	/* ハンドラ番号 */
#define INHNO_TIMER_PRC4	(0x40000U | MPCORE_IRQNO_GTC)	/* ハンドラ番号 */
#define INTNO_TIMER_PRC1	(0x10000U | MPCORE_IRQNO_GTC)	/* 割込み番号 */
#define INTNO_TIMER_PRC2	(0x20000U | MPCORE_IRQNO_GTC)	/* 割込み番号 */
#define INTNO_TIMER_PRC3	(0x30000U | MPCORE_IRQNO_GTC)	/* 割込み番号 */
#define INTNO_TIMER_PRC4	(0x40000U | MPCORE_IRQNO_GTC)	/* 割込み番号 */
#define INTPRI_TIMER		(TMAX_INTPRI - 1)				/* 割込み優先度 */
#define INTATR_TIMER		TA_NULL							/* 割込み属性 */

/*
 *  タイマの設定値のデフォルト値の定義
 */
#ifndef MPCORE_GTC_FREQ
#define MPCORE_GTC_FREQ			1		/* グローバルタイマの駆動周波数 */
#endif /* MPCORE_GTC_FREQ */

#ifndef TOPPERS_MACRO_ONLY

/*
 *  グローバルタイマの現在のカウント値（64bit）の読出し
 *
 *  この関数は，プロセッサ間で共有しているレジスタをアクセスするが，初
 *  期化処理以外でこれらのレジスタに書き込むことはないため，プロセッサ
 *  間排他制御をせずに呼び出して良い．
 */
Inline uint64_t
mpcore_gtc_get_count(void)
{
	uint32_t	count_l, count_u, prev_count_u;

	count_u = sil_rew_mem(MPCORE_GTC_COUNT_U);
	do {
		prev_count_u = count_u;
		count_l = sil_rew_mem(MPCORE_GTC_COUNT_L);
		count_u = sil_rew_mem(MPCORE_GTC_COUNT_U);
	} while (count_u != prev_count_u);
	return((((uint64_t) count_u) << 32) | ((uint64_t) count_l));
}

/*
 *  グローバルタイマのコンパレータ値（64bit）の設定
 *
 *  MPCORE_GTC_CTRLはプロセッサ間で共有しているレジスタであるが，この
 *  関数内で値を変更するビット（MPCORE_GTC_CTRL_ENACOMPと
 *  MPCORE_GTC_CTRL_ENAINT）はプロセッサ毎に持っている．また，この関数
 *  内でアクセスする他のレジスタはプロセッサ毎に持っている．さらに，
 *  Errata対策コード中のclear_intもプロセッサ間排他制御せずに呼び出し
 *  て良い．そのため，この関数はプロセッサ間排他制御をせずに呼び出して
 *  良い．
 */
Inline void
mpcore_gtc_set_cvr(uint64_t cvr)
{

	uint32_t	cvr_l, cvr_u;
	uint32_t	reg;

	cvr_l = ((uint32_t) cvr);
	cvr_u = ((uint32_t)(cvr >> 32));

	/* コンパレータをディスエーブル */
	reg = sil_rew_mem(MPCORE_GTC_CTRL);
	sil_wrw_mem(MPCORE_GTC_CTRL, reg & ~(MPCORE_GTC_CTRL_ENACOMP));

	/* コンパレータ値を設定 */
	sil_wrw_mem(MPCORE_GTC_CVR_L, cvr_l);
	sil_wrw_mem(MPCORE_GTC_CVR_U, cvr_u);

#ifdef ARM_CA9_GTC_ERRATA
	/* ARM Cortex-A9 Errata 740657への対策 */
	sil_wrw_mem(MPCORE_GTC_ISR, MPCORE_GTC_ISR_EVENTFLAG);
	clear_int(MPCORE_IRQNO_GTC);
#endif /* ARM_CA9_GTC_ERRATA */

	/* コンパレータと割込みをイネーブル */
	sil_wrw_mem(MPCORE_GTC_CTRL,
					reg | (MPCORE_GTC_CTRL_ENACOMP|MPCORE_GTC_CTRL_ENAINT));
}

/*
 * 高分解能タイマの起動処理（マスタプロセッサで実行）
 */
extern void target_hrt_initialize_global(EXINF exinf);

/*
 *  高分解能タイマの停止処理（マスタプロセッサで実行）
 */
extern void target_hrt_terminate_global(EXINF exinf);

/*
 *  高分解能タイマの起動処理
 */
extern void	target_hrt_initialize(EXINF exinf);

/*
 *  高分解能タイマの停止処理
 */
extern void	target_hrt_terminate(EXINF exinf);

/*
 *  高分解能タイマの現在のカウント値の読出し
 *
 *  この関数はシステムログへのログ情報の出力時に呼び出されるため，この
 *  関数内でsyslogやassertを使ってはならない（無限の再帰呼出しが起こ
 *  る）．
 */
Inline HRTCNT
target_hrt_get_current(void)
{
	/*
	 *  グローバルタイマのカウント値（64ビット）を読み出し，
	 *  MPCORE_GTC_FREQで除し，32ビットに切り詰めた値を返す．
	 */
	return((HRTCNT)(mpcore_gtc_get_count() / MPCORE_GTC_FREQ));
}

/*
 *  高分解能タイマへの割込みタイミングの設定
 *
 *  高分解能タイマを，hrtcntで指定した値カウントアップしたら割込みを発
 *  生させるように設定する．
 *
 *  TOPPERS_SUPPORT_CONTROL_OTHER_HRTを定義していないため，prcidは必ず
 *  自プロセッサとなる．
 */
Inline void
target_hrt_set_event(ID prcid, HRTCNT hrtcnt)
{
	/*
	 *  コンパレータ値を，(現在のカウント値＋hrtcnt×MPCORE_GTC_FREQ)
	 *  に設定し，コンパレータと割込みをイネーブルする．
	 */
	mpcore_gtc_set_cvr(mpcore_gtc_get_count()
							+ (((uint64_t) hrtcnt) * MPCORE_GTC_FREQ));
}

/*
 *  高分解能タイマへの割込みタイミングのクリア
 *
 *  TOPPERS_SUPPORT_CONTROL_OTHER_HRTを定義していないため，prcidは必ず
 *  自プロセッサとなる．
 */
Inline void
target_hrt_clear_event(ID prcid)
{
	sil_wrw_mem(MPCORE_GTC_CTRL,
					sil_rew_mem(MPCORE_GTC_CTRL) & ~(MPCORE_GTC_CTRL_ENACOMP));
#ifdef ARM_CA9_GTC_ERRATA
	/* ARM Cortex-A9 Errata 740657への対策 */
	sil_wrw_mem(MPCORE_GTC_ISR, MPCORE_GTC_ISR_EVENTFLAG);
	clear_int(MPCORE_IRQNO_GTC);
#endif /* ARM_CA9_GTC_ERRATA */
}

/*
 *  高分解能タイマ割込みの要求
 *
 *  TOPPERS_SUPPORT_CONTROL_OTHER_HRTを定義していないため，prcidは必ず
 *  自プロセッサとなる．
 */
Inline void
target_hrt_raise_event(ID prcid)
{
	raise_int(MPCORE_IRQNO_GTC);
}

/*
 *  割込みタイミングに指定する最大値
 */
#ifndef USE_64BIT_HRTCNT

#if !defined(TCYC_HRTCNT) || (TCYC_HRTCNT > 4002000002U)
#define HRTCNT_BOUND		4000000002U
#else
#define HRTCNT_BOUND		(TCYC_HRTCNT - 2000000U)
#endif
 
#endif /* USE_64BIT_HRTCNT */

/*
 *  高分解能タイマ割込みハンドラ
 */
extern void	target_hrt_handler(void);

#endif /* TOPPERS_MACRO_ONLY */

#ifdef USE_MPCORE_WDG_OVRTIMER
/*
 *		ウォッチドッグを用いてオーバランタイマを実現
 */
#ifdef TOPPERS_SUPPORT_OVRHDR

/*
 *  オーバランタイマ割込みハンドラ登録のための定数
 */
#define INHNO_OVRTIMER_PRC1	(0x10000U | MPCORE_IRQNO_WDG)	/* ハンドラ番号 */
#define INHNO_OVRTIMER_PRC2	(0x20000U | MPCORE_IRQNO_WDG)	/* ハンドラ番号 */
#define INHNO_OVRTIMER_PRC3	(0x30000U | MPCORE_IRQNO_WDG)	/* ハンドラ番号 */
#define INHNO_OVRTIMER_PRC4	(0x40000U | MPCORE_IRQNO_WDG)	/* ハンドラ番号 */
#define INTNO_OVRTIMER_PRC1	(0x10000U | MPCORE_IRQNO_WDG)	/* 割込み番号 */
#define INTNO_OVRTIMER_PRC2	(0x20000U | MPCORE_IRQNO_WDG)	/* 割込み番号 */
#define INTNO_OVRTIMER_PRC3	(0x30000U | MPCORE_IRQNO_WDG)	/* 割込み番号 */
#define INTNO_OVRTIMER_PRC4	(0x40000U | MPCORE_IRQNO_WDG)	/* 割込み番号 */
#define INTPRI_OVRTIMER		TMAX_INTPRI						/* 割込み優先度 */
#define INTATR_OVRTIMER		TA_NULL							/* 割込み属性 */

/*
 *  タイマの設定値のデフォルト値の定義
 */
#ifndef MPCORE_WDG_FREQ
#define MPCORE_WDG_FREQ			1		/* ウォッチドッグの駆動周波数 */
#endif /* MPCORE_WDG_FREQ */

#ifndef TOPPERS_MACRO_ONLY

/*
 *  オーバランタイマの初期化処理
 */
extern void target_ovrtimer_initialize(EXINF exinf);

/*
 *  オーバランタイマの終了処理
 */
extern void target_ovrtimer_terminate(EXINF exinf);

/*
 *  オーバランタイマの動作開始
 */
Inline void
target_ovrtimer_start(PRCTIM ovrtim)
{
	/*
	 *  ウォッチドッグのカウント値を(ovrtim * MPCORE_WDG_FREQ)に設定する．
	 */
	sil_wrw_mem(MPCORE_WDG_CNT, ovrtim * MPCORE_WDG_FREQ);

	/*
	 *  ウォッチドッグの動作を開始する．
	 */
	sil_wrw_mem(MPCORE_WDG_CTRL,
					sil_rew_mem(MPCORE_WDG_CTRL) | MPCORE_WDG_CTRL_ENABLE);
}

/*
 *  オーバランタイマの停止
 */
Inline PRCTIM
target_ovrtimer_stop(void)
{
	PRCTIM	ovrtim;

	/*
	 *  ウォッチドッグの現在値の読出し
	 */
	ovrtim = (PRCTIM)(sil_rew_mem(MPCORE_WDG_CNT) / MPCORE_WDG_FREQ);

	/*
	 *  ウォッチドッグを停止する．
	 */
	sil_wrw_mem(MPCORE_WDG_CTRL,
				sil_rew_mem(MPCORE_WDG_CTRL) & ~(MPCORE_WDG_CTRL_ENABLE));

	return(ovrtim);
}

/*
 *  オーバランタイマの現在値の読出し
 */
Inline PRCTIM
target_ovrtimer_get_current(void)
{
	/*
	 *  ウォッチドッグのカウント値を読み出し，MPCORE_WDG_FREQで除した
	 *  値を返す．
	 */
	return((PRCTIM)(sil_rew_mem(MPCORE_WDG_CNT) / MPCORE_WDG_FREQ));
}

/*
 *  オーバランタイマ割込みハンドラ
 */
extern void target_ovrtimer_handler(void);

#endif /* TOPPERS_MACRO_ONLY */
#endif /* TOPPERS_SUPPORT_OVRHDR */
#endif /* USE_MPCORE_WDG_OVRTIMER */
#endif /* TOPPERS_MPCORE_TIMER_H */
