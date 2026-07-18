/*
 *  TOPPERS/FMP Kernel
 *      Toyohashi Open Platform for Embedded Real-Time Systems/
 *      Flexible MultiProcessor Kernel
 *
 *  Copyright (C) 2000-2003 by Embedded and Real-Time Systems Laboratory
 *                              Toyohashi Univ. of Technology, JAPAN
 *  Copyright (C) 2005-2021 by Embedded and Real-Time Systems Laboratory
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
 *  @(#) $Id: core_timer.h 350 2023-04-21 01:59:41Z ertl-honda $
 */

/*
 *  タイマドライバ（ARM64用）
 */

#ifndef TOPPERS_CORE_TIMER_H
#define TOPPERS_CORE_TIMER_H

#include "kernel/kernel_impl.h"
#include <sil.h>

/*
 *  高分解能タイマ割込みハンドラ登録のための定数
 */
#ifdef TOPPERS_TZ_S
#define INHNO_TIMER			GIC_IRQNO_SPHYTIMER
#define INTNO_TIMER			GIC_IRQNO_SPHYTIMER
#else /* TOPPERS_TZ_S */
#define INHNO_TIMER			GIC_IRQNO_NSPHYTIMER
#define INTNO_TIMER			GIC_IRQNO_NSPHYTIMER
#endif /* TOPPERS_TZ_S */

#define INHNO_TIMER_PRC1	(0x00010000 | INHNO_TIMER)	/* ハンドラ番号 */
#define INHNO_TIMER_PRC2	(0x00020000 | INHNO_TIMER)	/* ハンドラ番号 */
#define INHNO_TIMER_PRC3	(0x00030000 | INHNO_TIMER)	/* ハンドラ番号 */
#define INHNO_TIMER_PRC4	(0x00040000 | INHNO_TIMER)	/* ハンドラ番号 */
#define INTNO_TIMER_PRC1	(0x00010000 | INTNO_TIMER)	/* 割込み番号 */
#define INTNO_TIMER_PRC2	(0x00020000 | INTNO_TIMER)	/* 割込み番号 */
#define INTNO_TIMER_PRC3	(0x00030000 | INTNO_TIMER)	/* 割込み番号 */
#define INTNO_TIMER_PRC4	(0x00040000 | INTNO_TIMER)	/* 割込み番号 */

#ifndef INTPRI_TIMER
#define INTPRI_TIMER		(TMAX_INTPRI - 1)				/* 割込み優先度 */
#endif /* INTPRI_TIMER */

#ifndef INTATR_TIMER
#define INTATR_TIMER		TA_NULL							/* 割込み属性 */
#endif /* INTATR_TIMER */

#ifndef TOPPERS_MACRO_ONLY

/*
 *  タイマ値の内部表現の型
 */
typedef uint64_t CLOCK;

/*
 *  タイマの設定値
 */
extern CLOCK timer_cval[];

/*
 *  タイマの起動処理
 *
 *  タイマを初期化し，周期的なタイマ割込み要求を発生させる．
 */
extern void target_hrt_initialize(EXINF exinf);

/*
 *  タイマの停止処理
 *
 *  タイマの動作を停止させる．
 */
extern void target_hrt_terminate(EXINF exinf);

#ifdef TOPPERS_RUN_TTSP3
/*------------------------------------------------------------------*/
/*  TTSP3ビルド時 TOPPERS_RUN_TTSP3 を定義して全体をビルドする必要がある    */
/*  （各ターゲットの ttsp_target.sh の CONFIG_OPT で -DTOPPERS_RUN_TTSP3）   */
/*  ARM64 のコア内蔵タイマはカウンタを停止できないため，TTSP3 では    */
/*  ソフト制御クロック(stop_tick/gain_tick)で時刻を制御する．         */
/*------------------------------------------------------------------*/
extern volatile bool_t stop_tick;
extern volatile HRTCNT temp_gtc;
extern volatile HRTCNT stop_time;
extern volatile bool_t tick_int;
extern void ttsp_int_raise(INTNO intno);
#if TNUM_PRCID >= 2
/*  MP(2コア以上)の TTSP3 ソフト制御クロック用：gain_tick(タイムマスタPE)が
    各PEのコンパレータ値を走査して cross-PE に割込みを発火させるための共有保存．
    単一PE構成では不要なので宣言しない（cval_shared を定義しないターゲットの
    単一PE TTSP3 ビルドを壊さないため）．  */
extern volatile HRTCNT cval_shared[];
#endif /* TNUM_PRCID >= 2 */
#endif  /*  TOPPERS_RUN_TTSP3 */

/*
 *  ジェネリックタイマの現在値の読出し
 */
Inline CLOCK
target_timer_get_count(void)
{
	CLOCK cnt;
#ifdef TOPPERS_RUN_TTSP3
	/*  ttsp3実行用 */
	if(stop_tick == true)
	  /*  ソフトカウンタを返す    */
	  return (CLOCK)stop_time;
#ifdef TOPPERS_TZ_S
	CNTPCT_EL0_READ(cnt);
#else /* !TOPPERS_TZ_S */
	CNTPCT_EL0_READ(cnt);
#endif /* TOPPERS_TZ_S */
	return cnt;
#else /*  TOPPERS_RUN_TTSP3 */
    /*  現在のカウンタ値を返す  */
#ifdef TOPPERS_TZ_S
	CNTPCT_EL0_READ(cnt);
#else /* !TOPPERS_TZ_S */
	CNTPCT_EL0_READ(cnt);
#endif /* TOPPERS_TZ_S */
	return cnt;
#endif /*  TOPPERS_RUN_TTSP3 */
}

/*
 *  タイマのコンパレータ値の設定
 */
Inline void
target_timer_set_cval(uint64_t cvr)
{
#if defined(TOPPERS_RUN_TTSP3) && (TNUM_PRCID >= 2)
	/*  MP TTSP3：自PEのコンパレータ値を共有保存し，gain_tick(タイムマスタPE)が
	    cross-PE に割込みを発火させる際の照合に用いる（単一PEでは不要）．  */
	cval_shared[get_my_prcidx()] = (HRTCNT)cvr;
#endif /* TOPPERS_RUN_TTSP3 && TNUM_PRCID >= 2 */
	/* 割込みをディスエーブル */
#ifdef TOPPERS_TZ_S
	CNTPS_CTL_EL1_WRITE((uint32_t)CNTPS_CTL_IMASK_BIT);
#else /* !TOPPERS_TZ_S */
	CNTP_CTL_EL0_WRITE((uint32_t)CNTP_CTL_IMASK_BIT);
#endif /* TOPPERS_TZ_S */

	/* コンパレータ値を設定 */
#ifdef TOPPERS_TZ_S
	CNTPS_CVAL_EL1_WRITE(cvr);
#else /* !TOPPERS_TZ_S */
	CNTP_CVAL_EL0_WRITE(cvr);
#endif /* TOPPERS_TZ_S */

#ifdef TOPPERS_RUN_TTSP3
	/*  ttsp3実行用     */
	/*  割り込みが完了している場合のみ  */
	if(stop_tick == false)
#endif /*  TOPPERS_RUN_TTSP3 */

	/* 割込みをイネーブル */
#ifdef TOPPERS_TZ_S
	CNTPS_CTL_EL1_WRITE((uint32_t)CNTPS_CTL_ENABLE_BIT);
#else /* !TOPPERS_TZ_S */
	CNTP_CTL_EL0_WRITE((uint32_t)CNTP_CTL_ENABLE_BIT);
#endif /* TOPPERS_TZ_S */
}

/*
 *  タイマ割込み要求のチェック
 */
Inline bool_t
target_timer_probe_int(void)
{
#if 0
	uint32_t ctl;

#if defined(TOPPERS_TZ_S)
	CNTPS_CTL_EL1_READ(ctl);
#else
	CNTP_CTL_EL0_READ(ctl);
#endif

	return ((ctl & CNTP_CTL_ISTATUS_BIT) == CNTP_CTL_ISTATUS_BIT);
#else
	/*
	 * Generic Timerはタイムアウト後はCNTP_CVALを更新するまで，CNTP_TVALの値は0のまま
	 * であるため， target_timer_probe_int()がtrueを返すとget_utm()に不整合が発生するため，
	 * 常にfalseを返すこととする． 
	 */
	return false;
#endif /* 0 */
}

/*
 *  高分解能タイマ
 */
/*
 *  高分解能タイマ割込みハンドラ
 */
extern void	target_hrt_handler(void);


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
Inline HRTCNT
target_hrt_get_current(void)
{
	/*
	 *  タイマのカウント値を読み出し，
	 *  timer_clockで除した値を返す．
	 */
	return(HRT_CNT_TO_HRTCNT(target_timer_get_count()));

}

/*
 *  高分解能タイマへの割込みタイミングの設定
 *
 *  高分解能タイマを，hrtcntで指定した値カウントアップしたら割込みを発
 *  生させるように設定する．
 */
Inline void
target_hrt_set_event(ID prcid, HRTCNT hrtcnt)
{
#ifdef  TOPPERS_RUN_TTSP3
	/*  ttsp3実行用     */
	tick_int = false;
#endif /*  TOPPERS_RUN_TTSP3 */
	/*
	 *  コンパレータ値を，(現在のカウント値＋hrtcnt×timer_clock)
	 *  に設定し，コンパレータと割込みをイネーブルする．
	 */
	target_timer_set_cval(target_timer_get_count()
						  + HRT_HRTCNT_TO_CNT((uint64_t)hrtcnt));
}

/*
 *  高分解能タイマへの割込みタイミングのクリア
 */
Inline void
target_hrt_clear_event(ID prcid)
{
	uint32_t ctl;

#if defined(TOPPERS_RUN_TTSP3) && (TNUM_PRCID >= 2)
	/*  MP TTSP3：イベント無しを共有保存に反映し，gain_tick の誤発火を防ぐ  */
	cval_shared[get_my_prcidx()] = 0U;
#endif /* TOPPERS_RUN_TTSP3 && TNUM_PRCID >= 2 */

#ifdef TOPPERS_TZ_S
	CNTPS_CTL_EL1_READ(ctl);
	CNTPS_CTL_EL1_WRITE(ctl & ~((uint32_t)CNTPS_CTL_ENABLE_BIT));
#else /* !TOPPERS_TZ_S */
	CNTP_CTL_EL0_READ(ctl);
	CNTP_CTL_EL0_WRITE(ctl & ~((uint32_t)CNTP_CTL_ENABLE_BIT));
#endif /* TOPPERS_TZ_S */
}

/*
 *  高分解能タイマ割込みの要求
 */
Inline void
target_hrt_raise_event(ID prcid)
{
#ifdef  TOPPERS_RUN_TTSP3
	/*  ttsp3実行用     */
	/*  [arm64 MP] prcid のタイマ割込みを許可する（旧コードは自PE固定の
	    INTNO_TIMER_PRC1 にハードコードされ，他PEの due イベントを自PEで処理して
	    しまっていた．単一PEでは prcid=自PE なので従来と等価）．
	    サービスコール ena_int() ではなく依存部の低レベル割込み許可関数
	    enable_int() を直接呼ぶ（ena_int は CPU ロックやパラメータチェックを伴う
	    ユーザAPIであり，カーネル依存部の内部から呼ぶのは不適切なため）．  */
	enable_int((((INTNO) prcid) << 16) | INTNO_TIMER);
	/*  [arm64 MP] stop_time(ソフト制御クロック)の temp_gtc 同期は gain_tick が
	    動くタイムマスタPEでのみ行う．他PEで即時 due イベントを raise する際に
	    temp_gtc(タイムマスタPE のローカル変数)で stop_time を上書きすると当該PEの
	    current_evttim が狂い，RELTIM=0 等の cross-PE アラームが due 判定されず
	    発火しなくなる．  */
	if (get_my_prcidx() == 0U) {		/* タイムマスタPE(gain_tick 実行PE)のみ */
		stop_time = temp_gtc;
	}
	ttsp_int_raise(INTNO_TIMER);
#else   /*  TOPPERS_RUN_TTSP3 */
	target_hrt_set_event(prcid, 1U);
#endif  /*  TOPPERS_RUN_TTSP3 */
}

/*
 *  割込みタイミングに指定する最大値
 */
#if !defined(TCYC_HRTCNT) || (TCYC_HRTCNT > 4002000002U)
#define HRTCNT_BOUND		4000000002U
#else
#define HRTCNT_BOUND		(TCYC_HRTCNT - 2000000U)
#endif

#endif /* TOPPERS_MACRO_ONLY */
#endif /* TOPPERS_CORE_TIMER_H */
