/*
 *  TOPPERS Software
 *      Toyohashi Open Platform for Embedded Real-Time Systems
 * 
 *  Copyright (C) 2018,2019 by Embedded and Real-Time Systems Laboratory
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
 *  $Id: test_mtrans2.c 583 2026-07-20 07:27:08Z ertl-honda $
 */

/* 
 *		過渡的な状態のテスト(2)
 *
 * 【テストの目的】
 *
 *	自タスクが強制待ち状態に見える場合があることをテストする．
 *
 * 【テスト項目】
 *
 *	(A) get_tstで，対象タスクが自タスクの場合にも，TTS_SUSが返る場合
 *		があること［NGKI5209］
 *	(B) ref_tskで，対象タスクが自タスクの場合にも，tskstatがTTS_SUSと
 *		なる場合があること［NGKI1226］
 *	(C) sus_tskで，対象タスクが自タスクの場合にも，E_QOVRエラーとなる
 *		場合があること［NGKI1309］
 *	(D) loc_mtxにASP3カーネルと同様のassertを入れると，assert failが起
 *		こる場合があること（assert failになる条件を作り出す）
 *
 * 【使用リソース】
 *
 *	CLS_PRC1: プロセッサ1のみで実行
 *	CLS_PRC2: プロセッサ2のみで実行
 *
 *	TASK1: CLS_PRC1，中優先度タスク，メインタスク，最初から起動
 *	TASK2: CLS_PRC2，中優先度タスク
 *	TASK3: CLS_PRC2，中優先度タスク
 *	MTX1: CLS_PRC1，ミューテックス（TA_CEILING属性，上限は中優先度）
 */

#include <kernel.h>
#include <sil.h>
#include <t_syslog.h>
#include "syssvc/test_svc.h"
#include "kernel_cfg.h"
#include "test_common.h"
#include <stdlib.h>

#ifndef NO_LOOP
#define NO_LOOP			1000U		/* -DNO_LOOP=… でビルド時に短縮/延長可（回帰高速化） */
#endif /* NO_LOOP */

#ifndef TEST_DELAY_TIME_NSE
#define TEST_DELAY_TIME_NSE		10U
#endif /* TEST_DELAY_TIME_NSE */

/*
 *  UART のみでの進捗観測（JTAG 不要・非摂動）用パラメータ．
 *  加害タスク(TASK2/TASK3)は livelock 中も回り続けるので，一定ループ毎に
 *  被害タスク(TASK1)の進捗を syslog(LOG_EMERG=即時出力)でポーリング出力する．
 *  MT2_POLL_MASK: (loops & mask)==0 で出力（既定 約100万回毎＝実測レートで数秒間隔）．
 *  MT2_STORM_CAP: 加害ループがこの回数に達したら storm を打ち切りテストを進める
 *                 （0 なら uint32 wrap まで）．無限ハング回避＋livelock 定量化．
 */
#ifndef MT2_POLL_MASK
#define MT2_POLL_MASK		0x000FFFFFU
#endif /* MT2_POLL_MASK */
#ifndef MT2_STORM_CAP
#define MT2_STORM_CAP		0x10000000U
#endif /* MT2_STORM_CAP */

/*
 *  カウンタは volatile 化し，フェーズ途中でも JTAG/gdb から読めるようにする．
 */
volatile uint_t	count_run;
volatile uint_t	count_sus;

volatile uint_t	count_ok;
volatile uint_t	count_qovr;

volatile bool_t	task2_flag;
volatile bool_t	task3_flag;

/*
 *  テスト進捗の実機観測用（デバッグ支援）．
 *  volatile: レジスタ常駐による最適化を抑止し，livelock 中でも各テストの
 *  ループ変数（進捗）を JTAG/gdb から読めるようにする．
 *  TOPPERS_OMIT_BSS_INIT のターゲットでも初期値が定まるよう明示初期化する
 *  （さらに task1 内でも実行時に設定する）．
 */
volatile uint_t		g_mt2_phase       = 0U;	/* 実行中フェーズ: 1=A 2=B 3=C 4=D, 5=finished */
volatile uint_t		g_mt2_loop_A      = 0U;	/* テスト(A) のループ変数（被害 TASK1） */
volatile uint_t		g_mt2_loop_B      = 0U;	/* テスト(B) のループ変数 */
volatile uint_t		g_mt2_loop_C      = 0U;	/* テスト(C) のループ変数 */
volatile uint_t		g_mt2_loop_D      = 0U;	/* テスト(D) のループ変数 */
volatile uint32_t	g_mt2_task2_loops = 0U;	/* 加害 TASK2 のループ回数（フェーズ毎にリセット） */
volatile uint32_t	g_mt2_task3_loops = 0U;	/* 加害 TASK3 のループ回数（フェーズ毎にリセット） */
volatile HRTCNT		g_mt2_phase_start = 0U;	/* 各フェーズ開始時刻 fch_hrt()（滞在時間・STALL 検出用） */
/* 各テスト実施中フラグ（加害タスクが「今どのテストの進捗を出力すべきか」を判定する） */
volatile bool_t		g_mt2_test_A      = false;
volatile bool_t		g_mt2_test_B      = false;
volatile bool_t		g_mt2_test_C      = false;
volatile bool_t		g_mt2_test_D      = false;
volatile bool_t		g_mt2_abort       = false;	/* storm cap 到達時に被害ループを抜けさせる */

static uint_t
get_rand(uint_t scale)
{
	uint_t	x;

	loc_cpu();
	x = rand();
	unl_cpu();
	return(x % scale);
}

Inline void
delay_count(uint_t count)
{
	sil_dly_nse(count + 1);
}

/*
 *  進捗のポーリング出力（加害タスクから呼ぶ）．
 *  実施中のテストに対応する被害ループ数と加害ループ数を LOG_EMERG で即時 UART 出力．
 *  被害(TASK1)が livelock で止まっていても，加害(TASK2/TASK3)は回り続けるので出力できる．
 */
static void
mt2_poll_report(void)
{
	SYSTIM		systim = 0U;
	uint32_t	hrt_us;

	/* fch_hrt: ハード高分解能カウンタ（mtime 直読, µs）＝割込み starve でも進む．
	 * get_tim: ソフトシステム時刻（tick 更新, ms）＝starve すると凍結．
	 * 両者を並記すると，hrt は進むのに systim が凍結＝タイマ tick 未配送（時刻マスタ飽和）
	 * を UART だけで診断できる． */
	hrt_us = (uint32_t)(fch_hrt() - g_mt2_phase_start);
	(void) get_tim(&systim);

	if (g_mt2_test_A) {
		syslog_4(LOG_EMERG, "[poll] test(A) loopA=%u task2=%u hrt=+%uus systim=%ums",
					g_mt2_loop_A, g_mt2_task2_loops, hrt_us, (uint32_t)systim);
	}
	else if (g_mt2_test_B) {
		syslog_4(LOG_EMERG, "[poll] test(B) loopB=%u task2=%u hrt=+%uus systim=%ums",
					g_mt2_loop_B, g_mt2_task2_loops, hrt_us, (uint32_t)systim);
	}
	else if (g_mt2_test_C) {
		syslog_4(LOG_EMERG, "[poll] test(C) loopC=%u task3=%u hrt=+%uus systim=%ums",
					g_mt2_loop_C, g_mt2_task3_loops, hrt_us, (uint32_t)systim);
	}
	else if (g_mt2_test_D) {
		syslog_4(LOG_EMERG, "[poll] test(D) loopD=%u task2=%u hrt=+%uus systim=%ums",
					g_mt2_loop_D, g_mt2_task2_loops, hrt_us, (uint32_t)systim);
	}
}

/*
 *  storm 打ち切り判定：cap 到達 or uint32 wrap(=0)．無限ハング回避＋livelock 定量化．
 */
Inline bool_t
mt2_storm_capped(uint32_t loops)
{
	return (((MT2_STORM_CAP != 0U) && (loops >= MT2_STORM_CAP)) || (loops == 0U));
}

void
task1(EXINF exinf)
{
	ER_UINT	ercd;
	STAT	tskstat;
	T_RTSK	rtsk;

	/* 観測用グローバルの実行時ゼロ初期化（TOPPERS_OMIT_BSS_INIT 対策） */
	g_mt2_phase = 0U;
	g_mt2_loop_A = 0U;
	g_mt2_loop_B = 0U;
	g_mt2_loop_C = 0U;
	g_mt2_loop_D = 0U;
	g_mt2_task2_loops = 0U;
	g_mt2_task3_loops = 0U;
	g_mt2_phase_start = 0U;
	g_mt2_test_A = false;
	g_mt2_test_B = false;
	g_mt2_test_C = false;
	g_mt2_test_D = false;
	g_mt2_abort = false;

	test_start(__FILE__);

	/*
	 *  テスト(A)
	 *  （OMIT_TEST_A 定義時はスキップ。mtrans2 livelock 切り分け用。既定は有効）
	 */
#ifndef OMIT_TEST_A
	count_run = 0U;
	count_sus = 0U;
	g_mt2_phase = 1U;
	g_mt2_task2_loops = 0U;
	g_mt2_phase_start = fch_hrt();
	g_mt2_test_A = true;

	task2_flag = true;
	ercd = act_tsk(TASK2);
	check_ercd(ercd, E_OK);

	for (g_mt2_loop_A = 0; g_mt2_loop_A < NO_LOOP; g_mt2_loop_A++) {
		ercd = get_tst(TSK_SELF, &tskstat);
		check_ercd(ercd, E_OK);

		switch (tskstat) {
		case TTS_RUN:
			count_run++;
			break;
		case TTS_SUS:
			count_sus++;							/* テスト(A) */
			break;
		default:
			check_assert(false);
		}

		delay_count(get_rand(TEST_DELAY_TIME_NSE * 10));
	}
	task2_flag = false;

	g_mt2_test_A = false;
	syslog_1(LOG_NOTICE, "TTS_RUN is referenced: %d", count_run);
	syslog_1(LOG_NOTICE, "TTS_SUS is referenced: %d", count_sus);
#else /* OMIT_TEST_A */
	syslog_0(LOG_NOTICE, "test(A) skipped (OMIT_TEST_A)");
#endif /* OMIT_TEST_A */

	/*
	 *  テスト(B)
	 *  （OMIT_TEST_B 定義時はスキップ。mtrans2 livelock 切り分け用。既定は有効）
	 */
#ifndef OMIT_TEST_B
	count_run = 0U;
	count_sus = 0U;
	g_mt2_phase = 2U;
	g_mt2_task2_loops = 0U;
	g_mt2_phase_start = fch_hrt();
	g_mt2_test_B = true;

	task2_flag = true;
	ercd = act_tsk(TASK2);
	check_ercd(ercd, E_OK);

	for (g_mt2_loop_B = 0; g_mt2_loop_B < NO_LOOP; g_mt2_loop_B++) {
		ercd = ref_tsk(TSK_SELF, &rtsk);
		check_ercd(ercd, E_OK);

		switch (rtsk.tskstat) {
		case TTS_RUN:
			count_run++;
			break;
		case TTS_SUS:
			count_sus++;							/* テスト(B) */
			break;
		default:
			check_assert(false);
		}

		delay_count(get_rand(TEST_DELAY_TIME_NSE * 10));
	}
	task2_flag = false;

	g_mt2_test_B = false;
	syslog_1(LOG_NOTICE, "TTS_RUN is referenced: %d", count_run);
	syslog_1(LOG_NOTICE, "TTS_SUS is referenced: %d", count_sus);
#else /* OMIT_TEST_B */
	syslog_0(LOG_NOTICE, "test(B) skipped (OMIT_TEST_B)");
#endif /* OMIT_TEST_B */

	/*
	 *  テスト(C)
	 *  （OMIT_TEST_C 定義時はスキップ。mtrans2 livelock 切り分け用。既定は有効）
	 */
#ifndef OMIT_TEST_C
	count_ok = 0U;
	count_qovr = 0U;
	g_mt2_phase = 3U;
	g_mt2_task3_loops = 0U;
	g_mt2_phase_start = fch_hrt();
	g_mt2_abort = false;
	g_mt2_test_C = true;

	task3_flag = true;
	ercd = act_tsk(TASK3);
	check_ercd(ercd, E_OK);

	for (g_mt2_loop_C = 0; (g_mt2_loop_C < NO_LOOP) && !g_mt2_abort; g_mt2_loop_C++) {
		ercd = sus_tsk(TSK_SELF);

		if (MERCD(ercd) == E_QOVR) {
			count_qovr++;							/* テスト(C) */
		}
		else {
			check_ercd(ercd, E_OK);
			count_ok++;
		}

		delay_count(get_rand(TEST_DELAY_TIME_NSE));
	}
	task3_flag = false;

	g_mt2_test_C = false;
	syslog_1(LOG_NOTICE, "E_OK is returned: %d", count_ok);
	syslog_1(LOG_NOTICE, "E_QOVR is returned: %d", count_qovr);
#else /* OMIT_TEST_C */
	syslog_0(LOG_NOTICE, "test(C) skipped (OMIT_TEST_C)");
#endif /* OMIT_TEST_C */

	/*
	 *  テスト(D)
	 */
	g_mt2_phase = 4U;
	g_mt2_task2_loops = 0U;
	g_mt2_phase_start = fch_hrt();
	g_mt2_test_D = true;
	task2_flag = true;
	ercd = act_tsk(TASK2);
	check_ercd(ercd, E_OK);

	for (g_mt2_loop_D = 0; g_mt2_loop_D < NO_LOOP; g_mt2_loop_D++) {
		ercd = loc_mtx(MTX1);
		check_ercd(ercd, E_OK);

		ercd = unl_mtx(MTX1);
		check_ercd(ercd, E_OK);

		delay_count(get_rand(TEST_DELAY_TIME_NSE * 10));
	}

	task2_flag = false;
	g_mt2_test_D = false;
	g_mt2_phase = 5U;			/* 全フェーズ完了 */

	syslog_0(LOG_NOTICE, "Test finished.");
	check_finish_PRC1(0);
	check_assert(false);
}

void
task2(EXINF exinf)
{
	ER_UINT	ercd;

	while (task2_flag) {
		g_mt2_task2_loops++;		/* 加害ループ回数（storm 観測用） */

		ercd = sus_tsk(TASK1);
		check_ercd(ercd, E_OK);

		/*
		 *  ポーリング出力は sus_tsk と rsm_tsk の間で行う．
		 *  この間 TASK1 はサスペンド中で動けないので，syslog の所要時間が
		 *  被害の「前進窓」にならず，livelock を摂動しない（rsm 後に出力すると
		 *  TASK1 が起床済みで前進でき，livelock が解けてしまう）．
		 */
		if ((g_mt2_task2_loops & MT2_POLL_MASK) == 0U) {
			mt2_poll_report();
		}

		ercd = rsm_tsk(TASK1);
		check_ercd(ercd, E_OK);

		/* storm 打ち切り：cap 到達で storm を止める（被害 A/B/D は残りを一気に完走） */
		if (mt2_storm_capped(g_mt2_task2_loops)) {
			syslog_1(LOG_EMERG, "[poll] storm cap reached (task2=%u): stop storm",
										g_mt2_task2_loops);
			task2_flag = false;
			break;
		}
		delay_count(get_rand(TEST_DELAY_TIME_NSE));
	}
}

void
task3(EXINF exinf)
{
	ER_UINT	ercd;

	while (task3_flag) {
		g_mt2_task3_loops++;		/* 加害ループ回数（storm 観測用） */

		ercd = sus_tsk(TASK1);
		if (MERCD(ercd) != E_QOVR) {
			check_ercd(ercd, E_OK);
		}

		/* ポーリング出力は sus_tsk と rsm_tsk の間（TASK1 サスペンド中）で行い非摂動にする． */
		if ((g_mt2_task3_loops & MT2_POLL_MASK) == 0U) {
			mt2_poll_report();
		}

		ercd = rsm_tsk(TASK1);
		check_ercd(ercd, E_OK);

		/* storm 打ち切り：テスト(C) は TASK1 が自己サスペンド中なので，abort を立てて
		 * 最終 rsm_tsk で起こし，被害ループ条件(!g_mt2_abort)で確実に抜けさせる． */
		if (mt2_storm_capped(g_mt2_task3_loops)) {
			syslog_1(LOG_EMERG, "[poll] storm cap reached (task3=%u): abort test(C)",
										g_mt2_task3_loops);
			g_mt2_abort = true;
			(void) rsm_tsk(TASK1);
			task3_flag = false;
			break;
		}
		delay_count(get_rand(TEST_DELAY_TIME_NSE));
	}
}
