/*
 *		タイマドライバ
 * 
 *  $Id: target_timer.h 350 2023-04-21 01:59:41Z ertl-honda $
 */

#ifndef TOPPERS_TARGET_TIMER_H
#define TOPPERS_TARGET_TIMER_H


#define INTPRI_TIMER		(TMAX_INTPRI - 1)		/* 割込み優先度 */

/*
 *  タイマのクロックを保持する変数
 *  単位はMHz
 *  target_initialize() で初期化される
 */
extern uint32_t timer_clock_mhz;

/*
 *  高分解能タイマのカウント値からHRTCNTへの変換
 */
#define HRT_CNT_TO_HRTCNT(cnt)		((HRTCNT)(cnt/timer_clock_mhz))

/*
 *  HRTCNTから高分解能タイマのカウント値への変換
 */
#define HRT_HRTCNT_TO_CNT(hrtcnt)	((uint64_t)hrtcnt * timer_clock_mhz)

/*
 *  チップで共通な定義
 */
#include "chip_timer.h"

#endif /* TOPPERS_TARGET_TIMER_H */
