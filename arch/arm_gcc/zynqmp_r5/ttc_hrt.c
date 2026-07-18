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
 *		高分解能タイマドライバ（ZynqMP RPU，TTC3用）
 *
 *  実現方式はchip_timer.hの先頭コメントを参照．
 *
 *  マルチコア構成では，各プロセッサに1つのTTC3カウンタを割り当てる
 *  （プロセッサインデックス prcidx にカウンタ prcidx を対応）．カウン
 *  タとその割込み（ZYNQMP_TTC3_0_IRQ からの連番）は各プロセッサが独立
 *  に操作し，管理データもプロセッサ毎の配列で保持する．
 *
 *  TTCの割込みレジスタ（TTC_INT_REG）はリードクリアであるため，読み出
 *  した未処理の割込み要因はttc_pendingに蓄積して管理する．
 *
 *  管理データ（hrt_base_us等）は，タスクコンテキスト（CPUロック状態）
 *  と多重割込みが許可された割込みハンドラの両方から操作される（高優先
 *  度の割込みハンドラ内のsyslogからtarget_hrt_get_currentが呼ばれる経
 *  路がある）ため，各関数の内部で全割込みロック状態にして操作する．自
 *  プロセッサ分の要素のみを操作するので，要素間の排他は不要である．
 */

#include "kernel_impl.h"
#include "time_event.h"
#include "target_timer.h"
#include <sil.h>
#include "arm.h"

/*
 *  高分解能タイマの管理データ（プロセッサ毎）
 *
 *  自プロセッサの要素を全割込みロック状態で操作する．
 */
static uint32_t		hrt_base_us[TNUM_PRCID];	/* 現在周期の先頭の時刻（μ秒）*/
static uint32_t		hrt_event_us[TNUM_PRCID];	/* 割込みを発生させる時刻（μ秒）*/
static bool_t		hrt_event_enable[TNUM_PRCID];	/* 割込みタイミング設定の有無 */
static uint32_t		ttc_pending[TNUM_PRCID];	/* 未処理のTTC割込み要因 */

/*
 *  グローバルな時間基準の管理データ
 *
 *  FMP3 のソフトウェア時刻管理（current_evttim）は，すべてのプロセッサ
 *  が target_hrt_get_current() で同一の時刻を得られること（全プロセッサ
 *  一貫のグローバル時刻）を前提としている．これは kernel/time_event.c の
 *  update_current_evttim() が，どのプロセッサから呼ばれても矛盾しない
 *  差分（new_hrtcnt - current_hrtcnt）でグローバルな current_evttim を進
 *  めるためである（TCYC_HRTCNT を定義していないので時刻の逆行も不可）．
 *
 *  各プロセッサに割り当てた TTC3 カウンタは独立かつ未同期（実機では起
 *  動時刻差により絶対値が大きくずれる）であるため，時間基準としては用
 *  いず，タイムマスタプロセッサのカウンタ（カウンタ TMASTER_PRCIDX）を
 *  全プロセッサ共通の時間基準として読み出す．カウンタ TMASTER_PRCIDX の
 *  基準時刻 hrt_base_us[TMASTER_PRCIDX] はタイムマスタのインターバル割込
 *  みで進められる．各プロセッサのイベント割込み（マッチ）は，従来どお
 *  り自プロセッサのカウンタで相対的に設定する（カウンタは全て同一クロッ
 *  クで駆動されるため相対間隔は正確である）．
 *
 *  時間基準はプロセッサ間で同時に読み出されるため，スピンロックで排他
 *  し，折返し直後の一時的な逆行を直前値でクランプして単調性を保証する．
 */
#define TMASTER_PRCIDX		INDEX_PRC(TOPPERS_TMASTER_PRCID)

static LOCK			hrt_glock;			/* 時間基準読出しの排他用ロック */
static uint32_t		hrt_global_last;	/* 直前に返した時刻（μ秒，単調保証用）*/

/*
 *  TTCの割込み要因の取込みと基準時刻の更新
 *
 *  インターバル割込み（周期の折返し）が発生していれば，基準時刻を1周
 *  期分進める．マッチ割込みの要因はttc_pendingに残す．
 *  全割込みロック状態で呼び出すこと．
 */
Inline void
ttc_sync(uint_t prcidx)
{
	ttc_pending[prcidx] |= sil_rew_mem(TTC_INT_REG(prcidx));
	if ((ttc_pending[prcidx] & TTC_INT_IV) != 0U) {
		ttc_pending[prcidx] &= ~((uint32_t) TTC_INT_IV);
		hrt_base_us[prcidx] += TTC_HRT_CYCLE_US;
	}
}

/*
 *  現在の時刻（μ秒）の算出
 *
 *  カウンタ値の読出し中に周期の折返しが起こった場合は読み直す．
 *  全割込みロック状態で呼び出すこと．
 */
static uint32_t
ttc_get_current_us(uint_t prcidx)
{
	uint32_t	base, count;

	ttc_sync(prcidx);
	do {
		base = hrt_base_us[prcidx];
		count = sil_rew_mem(TTC_CNT_VALUE(prcidx));
		ttc_sync(prcidx);
	} while (base != hrt_base_us[prcidx]);
	return(base + count / TTC_TICKS_PER_US);
}

/*
 *  設定されている割込みタイミングに向けてマッチレジスタを設定
 *
 *  割込みを発生させる時刻が現在の周期内であればマッチレジスタを設定す
 *  る．周期より先であれば，マッチを無効値にして折返し割込みの度に再評
 *  価する．マッチ値をカウンタが既に通過していた場合は，GICにペンディ
 *  ングをセットして割込みハンドラを起動する．
 */
static void
ttc_set_match(uint_t prcidx)
{
	uint32_t	rel_us, match;

	rel_us = hrt_event_us[prcidx] - hrt_base_us[prcidx];
	if (rel_us < TTC_HRT_CYCLE_US) {
		match = rel_us * TTC_TICKS_PER_US;
		sil_wrw_mem(TTC_MATCH1(prcidx), match);
		if (sil_rew_mem(TTC_CNT_VALUE(prcidx)) >= match) {
			raise_int(INTNO_TIMER(prcidx));
		}
	}
	else {
		sil_wrw_mem(TTC_MATCH1(prcidx), TTC_MATCH_IDLE);
	}
}

/*
 *  高分解能タイマの起動処理
 *
 *  各プロセッサで，自プロセッサに割り当てられたTTC3カウンタを初期化す
 *  る．
 */
void
target_hrt_initialize(EXINF exinf)
{
	uint_t	prcidx = get_my_prcidx();

	/*
	 *  TTC3のリセットを解除する（実機ではFSBLが解除していない場合があ
	 *  る．QEMUでは初めから解除されている）．モジュール全体のリセット
	 *  解除であり，各プロセッサから多重に実行しても問題ない．
	 */
	sil_wrw_mem((uint32_t *) CRL_APB_RST_LPD_IOU2,
				sil_rew_mem((uint32_t *) CRL_APB_RST_LPD_IOU2)
							& ~CRL_APB_RST_LPD_IOU2_TTC3_RESET_MASK);

	/*
	 *  カウンタを停止し，割込みをディスエーブルする．
	 */
	sil_wrw_mem(TTC_CNT_CNTRL(prcidx),
				TTC_CNT_CNTRL_DIS | TTC_CNT_CNTRL_WAVE_DIS);
	sil_wrw_mem(TTC_INT_EN(prcidx), 0U);

	/*
	 *  プリスケーラを無効にする（入力クロックをそのまま使用）．
	 */
	sil_wrw_mem(TTC_CLK_CNTRL(prcidx), 0U);

	/*
	 *  周期とマッチレジスタを設定する．
	 */
	sil_wrw_mem(TTC_INTERVAL(prcidx), TTC_HRT_INTERVAL - 1U);
	sil_wrw_mem(TTC_MATCH1(prcidx), TTC_MATCH_IDLE);

	/*
	 *  残留している割込み要因をクリアする（リードクリア）．
	 */
	(void) sil_rew_mem(TTC_INT_REG(prcidx));

	/*
	 *  管理データを初期化する．
	 */
	hrt_base_us[prcidx] = 0U;
	hrt_event_enable[prcidx] = false;
	ttc_pending[prcidx] = 0U;

	/*
	 *  タイムマスタプロセッサで，グローバルな時間基準の排他ロックと単調
	 *  保証用の直前値を初期化する（カウンタ TMASTER_PRCIDX を全プロセッ
	 *  サ共通の時間基準として用いる）．本処理は startup.c で current_hrtcnt
	 *  を読み出す barrier_sync(5) より前に，全プロセッサの本ルーチンが完
	 *  了するため，初回の target_hrt_get_current より前に行われる．
	 */
	if (prcidx == TMASTER_PRCIDX) {
		initialize_lock(&hrt_glock);
		hrt_global_last = 0U;
	}

	/*
	 *  インターバル割込みとマッチ1割込みをイネーブルし，カウンタをリ
	 *  セットして動作を開始する．
	 */
	sil_wrw_mem(TTC_INT_EN(prcidx), TTC_INT_IV | TTC_INT_M1);
	sil_wrw_mem(TTC_CNT_CNTRL(prcidx), TTC_CNT_CNTRL_INT | TTC_CNT_CNTRL_MATCH
					| TTC_CNT_CNTRL_RST | TTC_CNT_CNTRL_WAVE_DIS);
}

/*
 *  高分解能タイマの停止処理
 */
void
target_hrt_terminate(EXINF exinf)
{
	uint_t	prcidx = get_my_prcidx();

	/*
	 *  カウンタを停止し，割込みをディスエーブルする．
	 */
	sil_wrw_mem(TTC_CNT_CNTRL(prcidx),
				TTC_CNT_CNTRL_DIS | TTC_CNT_CNTRL_WAVE_DIS);
	sil_wrw_mem(TTC_INT_EN(prcidx), 0U);
	(void) sil_rew_mem(TTC_INT_REG(prcidx));
}

/*
 *  高分解能タイマの現在のカウント値の読出し
 *
 *  全プロセッサ共通の時間基準として，タイムマスタプロセッサのカウンタ
 *  （カウンタ TMASTER_PRCIDX）を読み出す（理由は先頭付近のコメント参照）．
 *  カウンタ値そのもの（TTC_CNT_VALUE）はどのプロセッサからも読み出せ，
 *  基準時刻 hrt_base_us[TMASTER_PRCIDX] はタイムマスタのインターバル割込
 *  みで進められる．割込み要因レジスタ（TTC_INT_REG，リードクリア）はタ
 *  イムマスタのみが操作するため，ここでは ttc_sync を呼ばない．
 */
HRTCNT
target_hrt_get_current(void)
{
	uint32_t	base, count, now;
	SIL_PRE_LOC;

	SIL_LOC_INT();
	/*
	 *  プロセッサ間でカウンタ TMASTER_PRCIDX の読出しと単調保証を排他す
	 *  る．リーフロックであり，全割込みロック状態で短時間だけ保持する．
	 */
	while (try_lock(&hrt_glock)) {
		/* 他プロセッサが操作中：スピン */
	}

	/*
	 *  基準時刻とカウンタ値を読む．読出し中にタイムマスタが基準時刻を更
	 *  新した場合は読み直す（基準時刻とカウンタ値の整合をとる）．
	 */
	do {
		base = hrt_base_us[TMASTER_PRCIDX];
		count = sil_rew_mem(TTC_CNT_VALUE(TMASTER_PRCIDX));
	} while (base != hrt_base_us[TMASTER_PRCIDX]);
	now = base + count / TTC_TICKS_PER_US;

	/*
	 *  カウンタが折り返した直後で，タイムマスタがまだ基準時刻を進めてい
	 *  ない場合，now が直前値より小さくなることがある．time_event.c は時
	 *  刻の逆行を許容しない（TCYC_HRTCNT 未定義）ため，直前値でクランプ
	 *  する（タイムマスタが基準時刻を進めれば追いつく）．
	 */
	if (((int32_t)(now - hrt_global_last)) < 0) {
		now = hrt_global_last;
	}
	else {
		hrt_global_last = now;
	}

	release_lock(&hrt_glock);
	SIL_UNL_INT();
	return((HRTCNT) now);
}

/*
 *  高分解能タイマへの割込みタイミングの設定
 */
void
target_hrt_set_event(ID prcid, HRTCNT hrtcnt)
{
	uint_t	prcidx = INDEX_PRC(prcid);
	SIL_PRE_LOC;

	SIL_LOC_INT();
	hrt_event_us[prcidx] = ttc_get_current_us(prcidx) + (uint32_t) hrtcnt;
	hrt_event_enable[prcidx] = true;
	ttc_set_match(prcidx);
	SIL_UNL_INT();
}

/*
 *  高分解能タイマへの割込みタイミングのクリア
 */
void
target_hrt_clear_event(ID prcid)
{
	uint_t	prcidx = INDEX_PRC(prcid);
	SIL_PRE_LOC;

	SIL_LOC_INT();
	hrt_event_enable[prcidx] = false;
	sil_wrw_mem(TTC_MATCH1(prcidx), TTC_MATCH_IDLE);
	SIL_UNL_INT();
}

/*
 *  高分解能タイマ割込みの要求
 */
void
target_hrt_raise_event(ID prcid)
{
	uint_t	prcidx = INDEX_PRC(prcid);
	SIL_PRE_LOC;

	SIL_LOC_INT();
	hrt_event_us[prcidx] = ttc_get_current_us(prcidx);
	hrt_event_enable[prcidx] = true;
	raise_int(INTNO_TIMER(prcidx));
	SIL_UNL_INT();
}

/*
 *  高分解能タイマ割込みハンドラ
 *
 *  自プロセッサに割り当てられたTTC3カウンタを操作する．周期の折返し
 *  （基準時刻の更新）はttc_sync内で処理する．設定されている割込みタイ
 *  ミングに達していればsignal_timeを呼び出し，達していなければマッチレ
 *  ジスタを再設定する．signal_timeは全割込みロックを解除した状態で呼び
 *  出す．
 */
void
target_hrt_handler(void)
{
	uint_t		prcidx = get_my_prcidx();
	uint32_t	now;
	bool_t		signal = false;
	SIL_PRE_LOC;

	SIL_LOC_INT();
	ttc_sync(prcidx);
	ttc_pending[prcidx] &= ~((uint32_t) TTC_INT_M1);

	if (hrt_event_enable[prcidx]) {
		now = ttc_get_current_us(prcidx);
		if (((int32_t)(now - hrt_event_us[prcidx])) >= 0) {
			hrt_event_enable[prcidx] = false;
			sil_wrw_mem(TTC_MATCH1(prcidx), TTC_MATCH_IDLE);
			signal = true;
		}
		else {
			ttc_set_match(prcidx);
		}
	}
	SIL_UNL_INT();

	if (signal) {
		/*
		 *  高分解能タイマ割込みを処理する．
		 */
		signal_time();
	}
}
