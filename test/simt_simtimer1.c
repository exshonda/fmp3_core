/*
 *  TOPPERS Software
 *      Toyohashi Open Platform for Embedded Real-Time Systems
 * 
 *  Copyright (C) 2024-2026 by Embedded and Real-Time Systems Laboratory
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
 *  $Id: simt_simtimer1.c 1230 2026-05-29 03:10:23Z ertl-hiro $
 */

/* 
 *		タイマドライバシミュレータのテスト(1)
 *
 * 【テストの目的】
 *
 *  タイマドライバシミュレータが，simtim_advanceのネスト（または，プリ
 *  エンプション）を正しくシミュレートできているかをテストする。
 *
 * 【テスト項目】
 *
 * 【使用リソース】
 *
 *	高分解能タイマモジュールの性質：HRT_CONFIG1
 *		TCYC_HRTCNT		未定義（2^32の意味）
 *		TSTEP_HRTCNT	1U
 *		HRTCNT_BOUND	4000000002U
 *
 *	タイマドライバシミュレータのパラメータ
 *		SIMTIM_INIT_CURRENT		10
 *		SIMTIM_OVERHEAD_HRTINT	10
 *
 *	TASK1: 中優先度タスク，メインタスク，最初から起動
 *	TASK2: 高優先度タスク
 *	ALM1:  アラームハンドラ
 *
 * 【テストシーケンス】
 *
 *	== START
 *	// カーネル起動．高分解能タイマのカウント値とイベント時刻は10ずれる
 *	1:	[hook_hrt_set_event <- HRTCNT_EMPTY]
 *	== TASK1（優先度：中）==
 *	2:	assert(fch_hrt() == 10U)							// 時刻：10
 *	// タイムイベントを登録
 *	3:	sta_alm(ALM1, 100U)									// 発生：111
 *	4:	[hook_hrt_set_event <- 101U]
 *	5:	DO(simtim_advance(200U))		// 時間を200（時刻210まで）進める
 *	// simtim_advanceが時間を101進めたところで，高分解能タイマ割込みが発生
 *	// 進めるべき時間が99残る
 *	== HRT_HANDLER											// 時刻：111
 *	== ALM1 ==
 *	6:	assert(fch_hrt() == 121U)							// 時刻：121
 *	7:	act_tsk(TASK2)
 *	8:	DO(simtim_advance(50U))			// 時間を50（時刻171まで）進める
 *	9:	RETURN												// 時刻：171
 *	// タイムイベントがなくなる
 *	10:	[hook_hrt_set_event <- HRTCNT_EMPTY]
 *	== TASK2（優先度：高）==
 *	11:	assert(fch_hrt() == 171U)							// 時刻：171
 *	12:	DO(simtim_advance(100U))		// 時間を100（時刻271まで）進める
 *	13:	assert(fch_hrt() == 271U)							// 時刻：271
 *	14:	ext_tsk()
 *	== TASK1（続き）==
 *	// TASK1に残った時間（99）がここで進む
 *	15:	assert(fch_hrt() == 370U)							// 時刻：370
 *	16:	END
 */

#include <kernel.h>
#include <t_syslog.h>
#include "syssvc/test_svc.h"
#include "arch/simtimer/sim_timer_cntl.h"
#include "kernel_cfg.h"
#include "test_common.h"

#ifndef HRT_CONFIG1
#error Compiler option "-DHRT_CONFIG1" is missing.
#endif /* HRT_CONFIG1 */

#ifndef SIMTIM_TEST
#error Compiler option "-DSIMTIM_TEST" is missing.
#endif /* SIMTIM_TEST */

/*
 *  HRTCNT_EMPTYの定義
 */
#ifdef TOPPERS_SUPPORT_DRIFT
#define HRTCNT_EMPTY	TMAX_RELTIM
#else /* TOPPERS_SUPPORT_DRIFT */
#define HRTCNT_EMPTY	HRTCNT_BOUND
#endif /* TOPPERS_SUPPORT_DRIFT */

void
hook_hrt_raise_event(ID prcid)
{
}

void
hook_hrt_clear_event(ID prcid)
{
}

/* DO NOT DELETE THIS LINE -- gentest depends on it. */

void
alarm1_handler(EXINF exinf)
{
	ER_UINT	ercd;

	check_point(6);
	check_assert(fch_hrt() == 121U);

	check_point(7);
	ercd = act_tsk(TASK2);
	check_ercd(ercd, E_OK);

	check_point(8);
	simtim_advance(50U);

	check_point(9);
	return;

	check_assert(false);
}

void
task1(EXINF exinf)
{
	ER_UINT	ercd;

	check_point(2);
	check_assert(fch_hrt() == 10U);

	check_point(3);
	ercd = sta_alm(ALM1, 100U);
	check_ercd(ercd, E_OK);

	check_point(5);
	simtim_advance(200U);

	check_point(15);
	check_assert(fch_hrt() == 370U);

	check_finish(16);
	check_assert(false);
}

void
task2(EXINF exinf)
{
	ER_UINT	ercd;

	check_point(11);
	check_assert(fch_hrt() == 171U);

	check_point(12);
	simtim_advance(100U);

	check_point(13);
	check_assert(fch_hrt() == 271U);

	check_point(14);
	ercd = ext_tsk();
	check_ercd(ercd, E_OK);

	check_assert(false);
}

static uint_t	hook_hrt_set_event_count = 0;

void
hook_hrt_set_event(ID prcid, HRTCNT hrtcnt)
{

	switch (++hook_hrt_set_event_count) {
	case 1:
		test_start(__FILE__);

		check_point(1);
		check_assert(hrtcnt == HRTCNT_EMPTY);

		return;

		check_assert(false);

	case 2:
		check_point(4);
		check_assert(hrtcnt == 101U);

		return;

		check_assert(false);

	case 3:
		check_point(10);
		check_assert(hrtcnt == HRTCNT_EMPTY);

		return;

		check_assert(false);

	default:
		check_assert(false);
	}
	check_assert(false);
}
