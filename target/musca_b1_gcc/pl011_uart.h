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
 *		ARM PrimeCell PL011 UART用 簡易SIOドライバ（非TECS版専用）
 *
 *  レジスタの定義は musca_b1.h に置かれている．
 */

#ifndef TOPPERS_PL011_UART_H
#define TOPPERS_PL011_UART_H

#ifdef TOPPERS_OMIT_TECS

#include <sil.h>
#include "musca_b1.h"

/*
 *  SIOポート数の定義
 */
#define TNUM_SIOP		1		/* サポートするSIOポートの数 */

/*
 *  コールバックルーチンの識別番号
 */
#define SIO_RDY_SND		1U		/* 送信可能コールバック */
#define SIO_RDY_RCV		2U		/* 受信通知コールバック */

#ifndef TOPPERS_MACRO_ONLY

/*
 *  SIOポート管理ブロックの定義
 */
typedef struct sio_port_control_block	SIOPCB;

/*
 *  プリミティブな送信／受信関数
 */

/*
 *  受信バッファに文字があるか？（受信FIFOが空でない）
 */
Inline bool_t
pl011_uart_getready(uintptr_t base)
{
	return((sil_rew_mem((void *) PL011_UARTFR(base)) & PL011_FR_RXFE) == 0U);
}

/*
 *  送信バッファに空きがあるか？（送信FIFOがフルでない）
 */
Inline bool_t
pl011_uart_putready(uintptr_t base)
{
	return((sil_rew_mem((void *) PL011_UARTFR(base)) & PL011_FR_TXFF) == 0U);
}

/*
 *  受信した文字の取出し
 */
Inline char
pl011_uart_getchar(uintptr_t base)
{
	return((char)(sil_rew_mem((void *) PL011_UARTDR(base)) & 0xffU));
}

/*
 *  送信する文字の書込み
 */
Inline void
pl011_uart_putchar(uintptr_t base, char c)
{
	sil_wrw_mem((void *) PL011_UARTDR(base), (uint32_t)(uint8_t) c);
}

/*
 *  シリアルインタフェースドライバに提供する機能
 */

/*
 *  SIOドライバの初期化
 */
extern void		pl011_uart_initialize(void);

/*
 *  SIOドライバの終了処理
 */
extern void		pl011_uart_terminate(void);

/*
 *  SIOの割込みサービスルーチン
 */
extern void		pl011_uart_isr(ID siopid);

/*
 *  SIOポートのオープン
 */
extern SIOPCB	*pl011_uart_opn_por(ID siopid, EXINF exinf);

/*
 *  SIOポートのクローズ
 */
extern void		pl011_uart_cls_por(SIOPCB *siopcb);

/*
 *  SIOポートへの文字送信
 */
extern bool_t	pl011_uart_snd_chr(SIOPCB *siopcb, char c);

/*
 *  SIOポートからの文字受信
 */
extern int_t	pl011_uart_rcv_chr(SIOPCB *siopcb);

/*
 *  SIOポートからのコールバックの許可
 */
extern void		pl011_uart_ena_cbr(SIOPCB *siopcb, uint_t cbrtn);

/*
 *  SIOポートからのコールバックの禁止
 */
extern void		pl011_uart_dis_cbr(SIOPCB *siopcb, uint_t cbrtn);

/*
 *  SIOポートからの送信可能コールバック
 */
extern void		pl011_uart_irdy_snd(EXINF exinf);

/*
 *  SIOポートからの受信通知コールバック
 */
extern void		pl011_uart_irdy_rcv(EXINF exinf);

#endif /* TOPPERS_MACRO_ONLY */
#endif /* TOPPERS_OMIT_TECS */
#endif /* TOPPERS_PL011_UART_H */
