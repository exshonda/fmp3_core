/*
 *  TOPPERS/FMP Kernel
 *      Toyohashi Open Platform for Embedded Real-Time Systems/
 *      Flexible MultiProcessor Kernel
 *
 *  Copyright (C) 2026 by Embedded and Real-Time Systems Laboratory
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
 *  $Id$
 */

/*
 *		タイマドライバ（ZynqMP RPU用）
 *
 *  TTC3を用いて高分解能タイマを実現する．
 *
 *  ZynqMPのTTCは32ビットカウンタであるが，QEMU（xlnx-zcu102）のTTCモ
 *  デルはZynq-7000相当の16ビットカウンタである．両者で同一に動作する
 *  ように，インターバルモード（周期 TTC_HRT_CYCLE_US，16ビットに収ま
 *  るカウント数）で自走させ，周期の折返し（インターバル割込み）でソフ
 *  トウェアの基準時刻（hrt_base_us）を進める方式とする．カーネルから
 *  の割込みタイミング要求は，マッチレジスタ1で実現する．
 *
 *  マルチコア構成では，各プロセッサに1つのTTC3カウンタ（プロセッサイ
 *  ンデックス prcidx にカウンタ prcidx）を割り当て，各プロセッサのイベ
 *  ント割込み（マッチ）を自プロセッサのカウンタで設定する．一方，FMP3
 *  のソフトウェア時刻管理（current_evttim）は全プロセッサで一貫したグ
 *  ローバル時刻を要求するため，target_hrt_get_current() は全プロセッサ
 *  がタイムマスタプロセッサのカウンタを共通の時間基準として読み出す．
 *  各カウンタは同一クロックで駆動されるため，イベントの相対間隔は自プ
 *  ロセッサのカウンタで正確に計れる（詳細は ttc_hrt.c の先頭コメント）．
 *
 *  また，QEMUのTTCモデルは133MHz固定（実機のTTC入力クロックは
 *  LPD_LSBUS_CLKで通常100MHz）のため，駆動周波数をTTC_CLK_HZで設定で
 *  きるようにしている（Makefile.targetで指定）．
 */

#ifndef TOPPERS_CHIP_TIMER_H
#define TOPPERS_CHIP_TIMER_H

#include "kernel/kernel_impl.h"
#include <sil.h>
#include "zynqmp_r5.h"

/*
 *  タイマ割込みハンドラ登録のための定数
 *
 *  マルチコア構成では，各プロセッサに1つのTTC3カウンタを割り当てる．
 *  プロセッサ番号 prcid（1オリジン）に対してカウンタ (prcid-1) を使用し，
 *  割込み番号は ZYNQMP_TTC3_0_IRQ から連番（77, 78, ...）とする．
 */
#define INHNO_TIMER_PRC1	(0x10000U | ZYNQMP_TTC3_0_IRQ)	/* ハンドラ番号 */
#define INTNO_TIMER_PRC1	ZYNQMP_TTC3_0_IRQ				/* 割込み番号 */
#define INHNO_TIMER_PRC2	(0x20000U | ZYNQMP_TTC3_1_IRQ)	/* ハンドラ番号 */
#define INTNO_TIMER_PRC2	ZYNQMP_TTC3_1_IRQ				/* 割込み番号 */
#define INTPRI_TIMER		(TMAX_INTPRI - 1)				/* 割込み優先度 */
#define INTATR_TIMER		TA_NULL							/* 割込み属性 */

/*
 *  プロセッサインデックス（0オリジン）からTTC割込み番号を求める
 */
#define INTNO_TIMER(prcidx)	(ZYNQMP_TTC3_0_IRQ + (prcidx))

/*
 *  TTCの駆動周波数（Hz）
 *
 *  1MHzの倍数でなければならない．
 *  実機：100MHz（LPD_LSBUS_CLK），QEMU：133MHz（モデル内固定値）
 */
#ifndef TTC_CLK_HZ
#define TTC_CLK_HZ			100000000U
#endif /* TTC_CLK_HZ */

/*
 *  1μ秒あたりのカウント数
 */
#define TTC_TICKS_PER_US	(TTC_CLK_HZ / 1000000U)

/*
 *  高分解能タイマの基準時刻を進める周期（μ秒）
 *
 *  TTCをインターバルモードで自走させ，周期の折返し（インターバル割込み）
 *  でソフトウェアの基準時刻（hrt_base_us）を進める．グローバルな時間基準
 *  target_hrt_get_current() は，タイムマスタプロセッサのカウンタの基準時
 *  刻（折返しの累積）＋カウンタ値で求めるため，折返しはタイムマスタのイ
 *  ンターバル割込みが処理されて初めて基準時刻に反映される．したがって周
 *  期が短いと，タイムマスタが割込みを禁止している区間（CPUロックやスピン
 *  ロックの保持中など）にカウンタが折返した場合，その割込みが処理される
 *  までグローバル時間基準が一時的に停滞しうる．
 *
 *  ZynqMPのTTCは32ビットカウンタであり，実機では周期を十分に長く取れる
 *  （カウント数 TTC_HRT_CYCLE_US * TTC_TICKS_PER_US が32ビットに収まる範
 *  囲）ため，上記の停滞がタイムマスタの割込み処理遅延に対して実質的に問
 *  題とならないよう長周期で自走させる．一方，QEMU（xlnx-zcu102）のTTCモ
 *  デルはZynq-7000相当の16ビットカウンタであるため，16ビットに収まる周期
 *  とする．
 */
#ifdef TOPPERS_USE_QEMU
#define TTC_HRT_CYCLE_US	400U			/* 16ビット制約（133MHz時 53200） */
#else /* TOPPERS_USE_QEMU */
#define TTC_HRT_CYCLE_US	40000000U		/* 40秒（100MHz時 4e9 < 2^32） */
#endif /* TOPPERS_USE_QEMU */

/*
 *  1周期あたりのカウント数
 */
#define TTC_HRT_INTERVAL	(TTC_HRT_CYCLE_US * TTC_TICKS_PER_US)

/*
 *  設定値の静的チェック
 */
#if (TTC_CLK_HZ % 1000000U) != 0U
#error TTC_CLK_HZ must be a multiple of 1MHz.
#endif
#ifdef TOPPERS_USE_QEMU
#if TTC_HRT_INTERVAL > 0x10000U
#error TTC_HRT_INTERVAL must fit in 16 bits (QEMU TTC model restriction).
#endif
#else /* TOPPERS_USE_QEMU */
#if TTC_HRT_INTERVAL > 0xFFFFFFFFU
#error TTC_HRT_INTERVAL must fit in 32 bits (ZynqMP TTC counter width).
#endif
#endif /* TOPPERS_USE_QEMU */

/*
 *  TTCのレジスタの番地の定義
 *
 *  TTC3は3個のカウンタを持ち，各カウンタのレジスタは同種レジスタが
 *  4バイト間隔で並ぶ（カウンタ0のオフセット + カウンタ番号*4）．
 *  プロセッサインデックス prcidx（0オリジン）にカウンタ prcidx を対応
 *  させ，各レジスタへのポインタを求める．
 */
#define TTC_CNT_STRIDE		0x04U	/* カウンタ間のレジスタ間隔 */

#define TTC_REG(prcidx, off) \
		((uint32_t *)(ZYNQMP_TTC3_BASE + (off) + (prcidx) * TTC_CNT_STRIDE))

#define TTC_CLK_CNTRL(prcidx)	TTC_REG((prcidx), 0x00U)
#define TTC_CNT_CNTRL(prcidx)	TTC_REG((prcidx), 0x0cU)
#define TTC_CNT_VALUE(prcidx)	TTC_REG((prcidx), 0x18U)
#define TTC_INTERVAL(prcidx)	TTC_REG((prcidx), 0x24U)
#define TTC_MATCH1(prcidx)		TTC_REG((prcidx), 0x30U)
#define TTC_INT_REG(prcidx)		TTC_REG((prcidx), 0x54U)
#define TTC_INT_EN(prcidx)		TTC_REG((prcidx), 0x60U)

/*
 *  カウンタ制御レジスタ（TTC_CNT_CNTRL）の設定値
 */
#define TTC_CNT_CNTRL_DIS		UINT_C(0x01)	/* カウンタ停止 */
#define TTC_CNT_CNTRL_INT		UINT_C(0x02)	/* インターバルモード */
#define TTC_CNT_CNTRL_DEC		UINT_C(0x04)	/* ダウンカウント */
#define TTC_CNT_CNTRL_MATCH		UINT_C(0x08)	/* マッチモード */
#define TTC_CNT_CNTRL_RST		UINT_C(0x10)	/* カウンタリセット */
#define TTC_CNT_CNTRL_WAVE_DIS	UINT_C(0x20)	/* 波形出力ディスエーブル */

/*
 *  割込みレジスタ（TTC_INT_REG／TTC_INT_EN）のビット
 */
#define TTC_INT_IV			UINT_C(0x01)	/* インターバル割込み */
#define TTC_INT_M1			UINT_C(0x02)	/* マッチ1割込み */

/*
 *  割込みが発生しないマッチレジスタの値
 *
 *  インターバルモードではカウンタは TTC_HRT_INTERVAL-1 までしか進まな
 *  いため，それより大きい値を設定すればマッチ割込みは発生しない．カウン
 *  タ幅（QEMU:16ビット，実機:32ビット）に応じた最大値とする．
 */
#ifdef TOPPERS_USE_QEMU
#define TTC_MATCH_IDLE		UINT_C(0xffff)
#else /* TOPPERS_USE_QEMU */
#define TTC_MATCH_IDLE		UINT_C(0xffffffff)
#endif /* TOPPERS_USE_QEMU */

/*
 *  割込みタイミングに指定する最大値（μ秒）
 *
 *  時刻比較を32ビットの符号付き差分で行うため，2^31より十分小さい値と
 *  する．
 */
#define HRTCNT_BOUND		2000000000U

#ifndef TOPPERS_MACRO_ONLY

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
 */
extern HRTCNT target_hrt_get_current(void);

/*
 *  高分解能タイマへの割込みタイミングの設定
 *
 *  TOPPERS_SUPPORT_CONTROL_OTHER_HRTを定義していないため，prcidは必ず
 *  自プロセッサとなる．
 */
extern void target_hrt_set_event(ID prcid, HRTCNT hrtcnt);

/*
 *  高分解能タイマへの割込みタイミングのクリア
 */
extern void target_hrt_clear_event(ID prcid);

/*
 *  高分解能タイマ割込みの要求
 */
extern void target_hrt_raise_event(ID prcid);

/*
 *  高分解能タイマ割込みハンドラ
 */
extern void	target_hrt_handler(void);

#endif /* TOPPERS_MACRO_ONLY */
#endif /* TOPPERS_CHIP_TIMER_H */
