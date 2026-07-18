/*
 *  TOPPERS/FMP Kernel
 *      Toyohashi Open Platform for Embedded Real-Time Systems/
 *      Flexible MultiProcessor Kernel
 * 
 *  Copyright (C) 2006-2023 by Embedded and Real-Time Systems Laboratory
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
 *  $Id: mpcore_timer.c 474 2026-06-06 15:03:35Z ertl-honda $
 */

/*
 *		タイマドライバ（MPCore内蔵タイマ用）
 *
 *  MPCoreがプロセッサ毎に持っているプライベートタイマとウォッチドッグ，
 *  各プロセッサからアクセスできるグローバルタイマを用いて，カーネルが
 *  必要とする各種のタイマを実現する．
 */

#include "kernel_impl.h"
#include "time_event.h"
#include "target_timer.h"
#include <sil.h>
#include "mpcore.h"

/*
 *		グローバルタイマを用いて高分解能タイマを実現
 */

/*
 *  タイマの起動処理（マスタプロセッサで実行）
 *
 *  この関数は，本来はタイムマスタプロセッサで実行すべきであるが，グロー
 *  バル初期化処理ルーチンとして登録するために，マスタプロセッサで実行
 *  される．
 */
void
target_hrt_initialize_global(EXINF exinf)
{
	/*
	 *  タイマをディスエーブルする．
	 */
	sil_wrw_mem(MPCORE_GTC_CTRL, MPCORE_GTC_CTRL_DISABLE);

	/*
	 *  カウンタを0に初期化する（セキュアモードでないと効果がない）．
	 */
	sil_wrw_mem(MPCORE_GTC_COUNT_L, 0U);
	sil_wrw_mem(MPCORE_GTC_COUNT_U, 0U);

	/*
	 *  タイマの動作を開始する（コンパレータと割込みはディスエーブル）．
	 */
	sil_wrw_mem(MPCORE_GTC_CTRL,
				MPCORE_GTC_CTRL_ENABLE
					| (MPCORE_GTC_PS_VALUE << MPCORE_GTC_CTRL_PS_SHIFT));
}

/*
 *  タイマの停止処理（マスタプロセッサで実行）
 *
 *  この関数は，本来はタイムマスタプロセッサで実行すべきであるが，グロー
 *  バル初期化処理ルーチンとして登録するために，マスタプロセッサで実行
 *  される．
 */
void
target_hrt_terminate_global(EXINF exinf)
{
	/*
	 *  タイマを停止する．
	 */
	sil_wrw_mem(MPCORE_GTC_CTRL, MPCORE_GTC_CTRL_DISABLE);
}

/*
 *  タイマの起動処理
 */
void
target_hrt_initialize(EXINF exinf)
{
	/*
	 *  コンペアの停止
	 */
	sil_wrw_mem(MPCORE_GTC_CTRL, sil_rew_mem(MPCORE_GTC_CTRL)
										& ~(MPCORE_GTC_CTRL_ENACOMP));

	/*
	 *  タイマ割込み要求をクリアする．
	 */
	sil_wrw_mem(MPCORE_GTC_ISR, MPCORE_GTC_ISR_EVENTFLAG);
}

/*
 *  タイマの停止処理
 */
void
target_hrt_terminate(EXINF exinf)
{
	/*
	 *  コンペアを停止する．
	 */
	sil_wrw_mem(MPCORE_GTC_CTRL, sil_rew_mem(MPCORE_GTC_CTRL)
										& ~(MPCORE_GTC_CTRL_ENACOMP));
	/*
	 *  タイマ割込み要求をクリアする．
	 */
	sil_wrw_mem(MPCORE_GTC_ISR, MPCORE_GTC_ISR_EVENTFLAG);
}

/*
 *  タイマ割込みハンドラ
 */
void
target_hrt_handler(void)
{
	/*
	 *  タイマ割込み要求をクリアする．
	 */
	sil_wrw_mem(MPCORE_GTC_ISR, MPCORE_GTC_ISR_EVENTFLAG);

	/*
	 *  高分解能タイマ割込みを処理する．
	 */
	signal_time();
}



#ifdef USE_MPCORE_WDG_OVRTIMER
/*
 *		ウォッチドッグタイマを用いてオーバランタイマを実現
 */
#ifdef TOPPERS_SUPPORT_OVRHDR

#include "overrun.h"

/*
 *  オーバランタイマの初期化処理
 */
void
target_ovrtimer_initialize(EXINF exinf)
{
	/*
	 *  ウォッチドッグを停止する．
	 */
	sil_wrw_mem(MPCORE_WDG_CTRL, MPCORE_WDG_CTRL_DISABLE);

	/*
	 *  ウォッチドッグをタイマモードに設定する．
	 */
	sil_wrw_mem(MPCORE_WDG_DIS, 0x12345678);
	sil_wrw_mem(MPCORE_WDG_DIS, 0x87654321);

	/*
	 *  ウォッチドッグタイマを停止した状態で設定する．
	 */
	sil_wrw_mem(MPCORE_WDG_CTRL,
				MPCORE_WDG_CTRL_ENAINT
					| (MPCORE_WDG_PS_VALUE << MPCORE_WDG_CTRL_PS_SHIFT));

	/*
	 *  ウォッチドッグ割込み要求をクリアする．
	 */
	sil_wrw_mem(MPCORE_WDG_ISR, MPCORE_WDG_ISR_EVENTFLAG);
}

/*
 *  オーバランタイマの終了処理
 */
void
target_ovrtimer_terminate(EXINF exinf)
{
	/*
	 *  ウォッチドッグを停止する．
	 */
	sil_wrw_mem(MPCORE_WDG_CTRL, MPCORE_WDG_CTRL_DISABLE);

	/*
	 *  ウォッチドッグ割込み要求をクリアする．
	 */
	sil_wrw_mem(MPCORE_WDG_ISR, MPCORE_WDG_ISR_EVENTFLAG);
}

/*
 *  オーバランタイマ割込みハンドラ
 */
void
target_ovrtimer_handler(void)
{
	/*
	 *  ウォッチドッグ割込み要求をクリアする．
	 */
	sil_wrw_mem(MPCORE_WDG_ISR, MPCORE_WDG_ISR_EVENTFLAG);

	/*
	 *  オーバランハンドラの起動処理をする．
	 */
	call_ovrhdr();
}

#endif /* TOPPERS_SUPPORT_OVRHDR */
#endif /* USE_MPCORE_WDG_OVRTIMER */
