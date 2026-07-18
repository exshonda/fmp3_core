/*
 *  TOPPERS/ASP Kernel
 *      Toyohashi Open Platform for Embedded Real-Time Systems/
 *      Advanced Standard Profile Kernel
 *
 *  Copyright (C) 2026 by Embedded and Real-Time Systems Laboratory
 *              Graduate School of Information Science, Nagoya Univ., JAPAN
 *
 *  上記著作権者は，本ソフトウェアを TOPPERS ライセンス（条件は他のソー
 *  スファイルの先頭コメントを参照）の下で利用することを許諾する．本ソフ
 *  トウェアは無保証で提供される．
 *
 */

/*
 *  テストプログラムのターゲット依存定義（ARM Musca-B1 用）
 */

#ifndef TOPPERS_TARGET_TEST_H
#define TOPPERS_TARGET_TEST_H

#define STACK_SIZE (1024)

/*
 *  int1（割込み管理機能）テスト用の割込み定義
 *
 *  QEMU musca-b1（SSE-200・EXP_NUMIRQ=96）で，デバイスに接続されていない
 *  予備の外部 IRQ 60（→ NVIC例外番号 60+16=76）をソフト割込み源として使う
 *  （デバイス IRQ は uart0=7〜12, uart1=13〜18, rtc=39 に限られる）．
 *  arm_m は ras_int / prb_int をサポートし，NVIC のソフト pend（ISPR）で
 *  発生・ハンドラ入口で自動クリアされるため intno1_clear() は空でよい．
 */
/*
 *  FMP3 では割込み番号をプロセッサ単位（(prcid << 16) | intno）で指定する．
 *  本ターゲットは単一プロセッサ（PRC1）．
 */
#define INTNO1			((1U << 16) | (60U + 16U))	/* PRC1, 予備NVIC IRQ60 → INTNO 76 */
#define INTNO1_INTATR	TA_ENAINT
#define INTNO1_INTPRI	(-2)
#define intno1_clear()

/*
 *  コア依存モジュール（ARM-M 用）
 */
#include "core_test.h"

#endif /* TOPPERS_TARGET_TEST_H */
