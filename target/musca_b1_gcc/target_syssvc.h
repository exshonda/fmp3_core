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
 *  システムサービスのターゲット依存部（ARM Musca-B1 用）
 */
#ifndef TOPPERS_TARGET_SYSSVC_H
#define TOPPERS_TARGET_SYSSVC_H

#ifdef TOPPERS_OMIT_TECS

#include "musca_b1.h"

/*
 *  起動メッセージのターゲットシステム名
 */
#define TARGET_NAME "ARM Musca-B1"

/*
 *  シリアルインタフェースドライバを割り付けるクラス
 */
#define CLS_SERIAL		CLS_PRC1

/*
 *  システムログの低レベル出力のための文字出力
 */
extern void target_fput_log(char c);

/*
 *  シリアルポートの数
 */
#define TNUM_PORT		1		/* サポートするシリアルポートの数 */

/*
 *  SIOドライバで使用するUART（PL011）に関する設定
 *
 *  ボーレート分周 IBRD = UARTCLK / (16 × baud)．QEMU の PL011 はボーレート
 *  を実際の伝送速度に反映しないため，値は形式的なものである．
 */
#define SIO_UART_BASE		MUSCA_B1_UART0_BASE		/* UARTのベース番地 */
#define SIO_UART_BAUDRATE	115200U					/* ボーレート */
#define SIO_UART_IBRD		(CPU_CLOCK_HZ / (16U * SIO_UART_BAUDRATE))
												/* 整数ボーレート分周比 */
#define SIO_UART_FBRD		0U						/* 小数ボーレート分周比 */

/*
 *  SIO割込みを登録するための定義（combined割込みを使用）
 */
#define INTNO_SIO		(MUSCA_B1_UART0_COMBINED_IRQn + 16)
												/* UART割込み番号（生）*/
#define INTNO_SIO_PRC1	((1 << 16) | INTNO_SIO)	/* PRC1 割込み番号 */
#define INHNO_SIO_PRC1	INTNO_SIO_PRC1			/* PRC1 割込みハンドラ番号 */
#define ISRPRI_SIO		1						/* UART ISR優先度 */
#define INTPRI_SIO		(-2)					/* UART割込み優先度 */
#define INTATR_SIO		TA_NULL					/* UART割込み属性 */

/*
 *  低レベル出力で使用する SIO ポート
 */
#define SIOPID_FPUT 1

#endif /* TOPPERS_OMIT_TECS */

#endif /* TOPPERS_TARGET_SYSSVC_H */
