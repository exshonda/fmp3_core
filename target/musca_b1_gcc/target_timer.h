/*
 *  タイマドライバ（ARM Musca-B1 / FMP3 用）
 *
 *  高分解能タイマ（HRT）を SysTick で実現する．SysTick は 24bit のダウン
 *  カウンタであり，プロセッサクロック（QEMU musca-b1 では 40MHz）で駆動す
 *  る．次のタイムイベントまでの相対時間を SysTick のリロード値に設定して
 *  ワンショット的に割込みを発生させる（イベント駆動）．プロセッサクロック
 *  は 40MHz である．
 *
 *  SysTick はコアごとに独立したハードウェアであるため，HRT の状態も
 *  プロセッサ毎に保持する（target_timer.c）．FMP3 ではタイマ操作関数は
 *  プロセッサID（prcid）引数を取るが，他プロセッサの HRT 操作は
 *  TOPPERS_SUPPORT_CONTROL_OTHER_HRT が未定義のため request_set_hrt_event に
 *  よる IPI 経由で行われる．すなわち target_hrt_* は常に自プロセッサに対して
 *  呼ばれるので，本体側は get_my_prcidx() で状態を引く（引数の prcid は
 *  自プロセッサIDと一致する）．
 */

#ifndef TOPPERS_TARGET_TIMER_H
#define TOPPERS_TARGET_TIMER_H

#include "kernel/kernel_impl.h"
#include "musca_b1.h"
#include "arm_m.h"

/*
 *  タイマ割込みハンドラ登録のための定数（SysTick 例外）
 *
 *  FMP3 では割込み番号をプロセッサ単位（(prcid << 16) | intno）で指定する．
 *  SysTick は各コアに内蔵されているため，生の例外番号は全コアで同一であり，
 *  上位16bitのプロセッサIDのみが異なる．
 */
#define INTNO_TIMER       IRQNO_SYSTICK             /* 割込み番号（生） */
#define INHNO_TIMER       IRQNO_SYSTICK             /* 割込みハンドラ番号（生） */
#define INTNO_TIMER_PRC1  ((PRC1 << 16) | IRQNO_SYSTICK)
#define INHNO_TIMER_PRC1  ((PRC1 << 16) | IRQNO_SYSTICK)
#if TNUM_PRCID >= 2
#define INTNO_TIMER_PRC2  ((PRC2 << 16) | IRQNO_SYSTICK)
#define INHNO_TIMER_PRC2  ((PRC2 << 16) | IRQNO_SYSTICK)
#endif /* TNUM_PRCID >= 2 */
#define INTPRI_TIMER (TMAX_INTPRI - 1)      /* 割込み優先度 */
#define INTATR_TIMER TA_NULL                /* 割込み属性 */

/*
 *  プロセッサクロックの 1us あたりのカウント数（QEMU musca-b1 は 40MHz）
 */
#define HRT_CLOCKS_PER_US   (CPU_CLOCK_HZ / 1000000U)

/*
 *  SysTick のリロード値の最大（24bit）
 */
#define HRT_MAX_TICKS       0x00FFFFFFU

/*
 *  割込みタイミングに指定する最大値（us）
 */
#define HRTCNT_BOUND        (HRT_MAX_TICKS / HRT_CLOCKS_PER_US)

#ifndef TOPPERS_MACRO_ONLY

/*
 *  高分解能タイマの操作
 *
 *  本体ロジックと積算器の状態は target_timer.c に置き，ここでは薄い転送
 *  関数とする．状態の添字付けは本体側で get_my_prcidx() により行う．
 */
extern void    target_hrt_initialize(EXINF exinf);
extern void    target_hrt_terminate(EXINF exinf);
extern void    target_hrt_handler(void);

extern HRTCNT  hrt_get_current_body(void);
extern void    hrt_set_event_body(HRTCNT hrtcnt);
extern void    hrt_raise_event_body(void);
extern void    hrt_clear_event_body(void);

Inline HRTCNT
target_hrt_get_current(void)
{
	return hrt_get_current_body();
}

Inline void
target_hrt_set_event(ID prcid, HRTCNT hrtcnt)
{
	hrt_set_event_body(hrtcnt);
}

Inline void
target_hrt_clear_event(ID prcid)
{
	hrt_clear_event_body();
}

Inline void
target_hrt_raise_event(ID prcid)
{
	hrt_raise_event_body();
}

#endif /* TOPPERS_MACRO_ONLY */
#endif /* TOPPERS_TARGET_TIMER_H */
