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
 *		シリアルインタフェースドライバのターゲット依存部（ARM Musca-B1用）
 *		（非TECS版専用）
 */

#include <kernel.h>
#include <t_syslog.h>
#include "target_syssvc.h"
#include "target_serial.h"

/*
 *  SIOドライバの初期化
 */
void
sio_initialize(EXINF exinf)
{
	pl011_uart_initialize();
}

/*
 *  SIOドライバの終了処理
 */
void
sio_terminate(EXINF exinf)
{
	pl011_uart_terminate();
}

/*
 *  SIOの割込みサービスルーチン
 */
void
sio_isr(EXINF exinf)
{
	pl011_uart_isr((ID) exinf);
}

/*
 *  SIOポートのオープン
 */
SIOPCB *
sio_opn_por(ID siopid, EXINF exinf)
{
	SIOPCB	*p_siopcb;

	/*
	 *  デバイス依存のオープン処理
	 */
	p_siopcb = pl011_uart_opn_por(siopid, exinf);

	/*
	 *  SIOの割込みマスクを解除する．
	 */
	(void) ena_int(INTNO_SIO);
	return(p_siopcb);
}

/*
 *  SIOポートのクローズ
 */
void
sio_cls_por(SIOPCB *p_siopcb)
{
	/*
	 *  デバイス依存のクローズ処理
	 */
	pl011_uart_cls_por(p_siopcb);

	/*
	 *  SIOの割込みをマスクする．
	 */
	(void) dis_int(INTNO_SIO);
}

/*
 *  SIOポートへの文字送信
 */
bool_t
sio_snd_chr(SIOPCB *p_siopcb, char c)
{
	return(pl011_uart_snd_chr(p_siopcb, c));
}

/*
 *  SIOポートからの文字受信
 */
int_t
sio_rcv_chr(SIOPCB *p_siopcb)
{
	return(pl011_uart_rcv_chr(p_siopcb));
}

/*
 *  SIOポートからのコールバックの許可
 */
void
sio_ena_cbr(SIOPCB *p_siopcb, uint_t cbrtn)
{
	pl011_uart_ena_cbr(p_siopcb, cbrtn);
}

/*
 *  SIOポートからのコールバックの禁止
 */
void
sio_dis_cbr(SIOPCB *p_siopcb, uint_t cbrtn)
{
	pl011_uart_dis_cbr(p_siopcb, cbrtn);
}

/*
 *  SIOポートからの送信可能コールバック
 */
void
pl011_uart_irdy_snd(EXINF exinf)
{
	sio_irdy_snd(exinf);
}

/*
 *  SIOポートからの受信通知コールバック
 */
void
pl011_uart_irdy_rcv(EXINF exinf)
{
	sio_irdy_rcv(exinf);
}
