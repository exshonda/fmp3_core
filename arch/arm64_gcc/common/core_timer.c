/*
 *  TOPPERS/FMP Kernel
 *      Toyohashi Open Platform for Embedded Real-Time Systems/
 *      Flexible MultiProcessor Kernel
 *
 *  Copyright (C) 2006-2021 by Embedded and Real-Time Systems Laboratory
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
 *  @(#) $Id: core_timer.c 446 2025-06-27 07:54:07Z ertl-honda $
 */

/*
 *  タイマドライバ（ARMv8 Generic Timer用）
 */

#include "kernel_impl.h"
#include "target_syssvc.h"
#include "time_event.h"
#include "target_timer.h"

#ifdef  TOPPERS_RUN_TTSP3
    /*  ttsp3実行用     */
#include "ttsp_target_test.h"
extern volatile bool_t tick_int;
#endif

/*
 *  タイマの設定値
 */
CLOCK	timer_cval[TNUM_PRCID];

/*
 *  タイマの起動処理
 */
void
target_hrt_initialize(EXINF exinf)
{
	/*
	 *  タイマの停止
	 */
#ifdef TOPPERS_TZ_S
	CNTPS_CTL_EL1_WRITE((uint32_t)CNTPS_CTL_IMASK_BIT);
#else /* !TOPPERS_TZ_S */
	CNTP_CTL_EL0_WRITE((uint32_t)CNTP_CTL_IMASK_BIT);
#endif /* TOPPERS_TZ_S */

	/*
	 *  現在カウンタ値読み込み
	 */
	timer_cval[get_my_prcidx()] = target_timer_get_count();

#ifdef  TOPPERS_RUN_TTSP3
	/*  ttsp3実行用  */
	tick_int = false;
#endif
}

/*
 *  タイマの停止処理
 */
void
target_hrt_terminate(EXINF exinf)
{
	/*
	 *  タイマの停止
	 */
#if defined(TOPPERS_TZ_S)
	CNTPS_CTL_EL1_WRITE((uint32_t)CNTPS_CTL_IMASK_BIT);
#else /* !TOPPERS_TZ_S */
	CNTP_CTL_EL0_WRITE((uint32_t)CNTP_CTL_IMASK_BIT);
#endif /* TOPPERS_TZ_S */
}

/*
 *  タイマ割込みハンドラ
 */
void
target_hrt_handler(void)
{
	/*
	 *  タイマの停止
	 */
#if defined(TOPPERS_TZ_S)
	CNTPS_CTL_EL1_WRITE((uint32_t)CNTPS_CTL_IMASK_BIT);
#else /* !TOPPERS_TZ_S */
	CNTP_CTL_EL0_WRITE((uint32_t)CNTP_CTL_IMASK_BIT);
#endif /* TOPPERS_TZ_S */

#ifdef  TOPPERS_RUN_TTSP3
	/*  ttsp3実行用     */
	tick_int = true;
#endif

	/*
	 *  現在カウンタ値読み込み
	 */
	timer_cval[get_my_prcidx()] = target_timer_get_count();

	/*
	 *  高分解能タイマ割込みを処理する．
	 */
	signal_time();				/* タイムティックの供給 */
}
