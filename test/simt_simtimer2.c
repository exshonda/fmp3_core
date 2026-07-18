/*
 *  TOPPERS Software
 *      Toyohashi Open Platform for Embedded Real-Time Systems
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
 *  $Id: simt_simtimer2.c 1244 2026-06-12 16:02:59Z ertl-hiro $
 */

/* 
 *		タイマドライバシミュレータのテスト(2)
 *
 * 【テストの目的】
 *
 *  タイマドライバシミュレータが，タスクのマイグレーション時に正しく動
 *  作するかをテストする．
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
 *	CLS_ALL_PRC1：両プロセッサで実行，初期割付けはプロセッサ1
 *
 *	TASK1: CLS_ALL_PRC1，中優先度タスク，メインタスク，最初から起動
 *	TASK2: CLS_ALL_PRC1，高優先度タスク
 *	ALM1:  CLS_ALL_PRC1，アラームハンドラ
 *
 * 【テストシーケンス】
 *
 *	== START_PRC1						｜	== START_PRC2
 *	// カーネル起動．高分解能タイマのカ	｜
 *	// ウント値とイベント時刻は10ずれる	｜
 *	1:	[hook_set_PRC1 <- HRTCNT_EMPTY]	｜		[hook_clear_PRC2]
 *	== TASK1（優先度：中）==			｜
 *	2:	DO(set_barrier_numprc(2))		｜
 *		assert(fch_hrt() == 10U)		｜	// 時刻：10
 *	// タイムイベントを登録				｜
 *	3:	sta_alm(ALM1, 100U)				｜	// 発生：111
 *	4:	[hook_set_PRC1 <- 101U]			｜
 *	// 時間を200（時刻210まで）進める	｜
 *	5:	DO(simtim_advance(200U))		｜
 *	// simtim_advanceが時間を101進めたと｜
 *	// ころで，高分解能タイマ割込みが発	｜
 *	// 生．進めるべき時間が99残る		｜
 *	== HRT_HANDLER						｜	// 時刻：111
 *	== ALM1 ==							｜
 *	6:	assert(fch_hrt() == 121U)		｜	// 時刻：121
 *	7:	act_tsk(TASK2)					｜
 *	// 時間を50（時刻171まで）進める	｜
 *	8:	DO(simtim_advance(50U))			｜
 *	9:	RETURN							｜	// 時刻：171
 *	// タイムイベントがなくなる			｜
 *	10:	[hook_set_PRC1 <- HRTCNT_EMPTY]	｜
 *	== TASK2（優先度：高）==			｜
 *	11:	assert(fch_hrt() == 171U)		｜	// 時刻：171
 *	12:	mig_tsk(TASK1, PRC2)			｜	== TASK1（続き）==
 *										｜	// 進めるべき時間が99残っている
 *	// 時間を50（時刻221まで）進める	｜
 *	13:	DO(simtim_advance(50U))			｜
 *	14:	assert(fch_hrt() == 221U)		｜	// 時刻：221
 *		ref_tsk(TASK1, &rtsk)			｜	// 進めるべき時間が49残っている
 *		assert(rtsk.tskstat == TTS_RUN)	｜
 *		assert(rtsk.prcid == PRC2)		｜
 *	// 時間を50（時刻271まで）進める	｜
 *	15:	DO(simtim_advance(50U))			｜	// 残った時間（49）がここで進む
 *										｜	1:	assert(fch_hrt() == 270U)
 *										｜	2:	DO(simtim_advance(1U))
 *	16:	assert(fch_hrt() == 271U)		｜	3:	assert(fch_hrt() == 271U)
 *	17:	BARRIER(1)						｜	4:	BARRIER(1)
 *	18:	END								｜	5:	ext_tsk()
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

extern void	hook_set_PRC1(HRTCNT hrtcnt);
extern void	hook_set_PRC2(HRTCNT hrtcnt);

void
hook_hrt_set_event(ID prcid, HRTCNT hrtcnt)
{
	switch (prcid) {
	case PRC1:
		hook_set_PRC1(hrtcnt);
		break;
	case PRC2:
//		hook_set_PRC2(hrtcnt);
		break;
	}
}

void
hook_hrt_raise_event(ID prcid)
{
	check_assert(false);
}

extern void	hook_clear_PRC1(void);
extern void	hook_clear_PRC2(void);

void
hook_hrt_clear_event(ID prcid)
{
	switch (prcid) {
	case PRC1:
//		hook_clear_PRC1();
		break;
	case PRC2:
		hook_clear_PRC2();
		break;
	}
}

/* DO NOT DELETE THIS LINE -- gentest depends on it. */

void
alarm1_handler(EXINF exinf)
{
	ER_UINT	ercd;

	check_point_PRC1(6);
	check_assert(fch_hrt() == 121U);

	check_point_PRC1(7);
	ercd = act_tsk(TASK2);
	check_ercd(ercd, E_OK);

	check_point_PRC1(8);
	simtim_advance(50U);

	check_point_PRC1(9);
	return;

	check_assert(false);
}

void
task1(EXINF exinf)
{
	ER_UINT	ercd;

	check_point_PRC1(2);
	set_barrier_numprc(2);

	check_assert(fch_hrt() == 10U);

	check_point_PRC1(3);
	ercd = sta_alm(ALM1, 100U);
	check_ercd(ercd, E_OK);

	check_point_PRC1(5);
	simtim_advance(200U);

	check_point_PRC2(1);
	check_assert(fch_hrt() == 270U);

	check_point_PRC2(2);
	simtim_advance(1U);

	check_point_PRC2(3);
	check_assert(fch_hrt() == 271U);

	check_point_PRC2(4);
	test_barrier(1);

	check_point_PRC2(5);
	ercd = ext_tsk();
	check_ercd(ercd, E_OK);

	check_assert(false);
}

void
task2(EXINF exinf)
{
	ER_UINT	ercd;
	T_RTSK	rtsk;

	check_point_PRC1(11);
	check_assert(fch_hrt() == 171U);

	check_point_PRC1(12);
	ercd = mig_tsk(TASK1, PRC2);
	check_ercd(ercd, E_OK);

	check_point_PRC1(13);
	simtim_advance(50U);

	check_point_PRC1(14);
	check_assert(fch_hrt() == 221U);

	ercd = ref_tsk(TASK1, &rtsk);
	check_ercd(ercd, E_OK);

	check_assert(rtsk.tskstat == TTS_RUN);

	check_assert(rtsk.prcid == PRC2);

	check_point_PRC1(15);
	simtim_advance(50U);

	check_point_PRC1(16);
	check_assert(fch_hrt() == 271U);

	check_point_PRC1(17);
	test_barrier(1);

	check_finish_PRC1(18);
	check_assert(false);
}

static uint_t	hook_clear_PRC2_count = 0;

void
hook_clear_PRC2(void)
{

	switch (++hook_clear_PRC2_count) {
	case 1:
		return;

		check_assert(false);

	default:
		check_assert(false);
	}
	check_assert(false);
}

static uint_t	hook_set_PRC1_count = 0;

void
hook_set_PRC1(HRTCNT hrtcnt)
{

	switch (++hook_set_PRC1_count) {
	case 1:
		test_start(__FILE__);

		check_point_PRC1(1);
		check_assert(hrtcnt == HRTCNT_EMPTY);

		return;

		check_assert(false);

	case 2:
		check_point_PRC1(4);
		check_assert(hrtcnt == 101U);

		return;

		check_assert(false);

	case 3:
		check_point_PRC1(10);
		check_assert(hrtcnt == HRTCNT_EMPTY);

		return;

		check_assert(false);

	default:
		check_assert(false);
	}
	check_assert(false);
}
