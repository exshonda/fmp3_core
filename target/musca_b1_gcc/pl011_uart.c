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
 *  QEMU の musca-b1 が備える PL011 UART（hw/char/pl011.c）を駆動する．
 *  FIFO は使わず（LCR_H の FEN を立てない），送信は holding レジスタが空
 *  になると送信割込み，受信は 1 文字受信で受信割込みが発火する．割込み要因
 *  は送受信の状態（UARTFR）から判定し，UARTICR でクリアする．SIO の送信可能
 *  ／受信通知コールバックに対応付ける．
 *
 *  実装は asp3 の uart_pl011 ドライバを雛形に，FMP3 の SIOPCB/SIOPINIB 構造
 *  へ移植したものである．
 */

#include <sil.h>
#include "target_syssvc.h"
#include "pl011_uart.h"

/*
 *  SIOポート初期化ブロックの定義
 */
typedef struct sio_port_initialization_block {
	uintptr_t	base;			/* UARTレジスタのベースアドレス */
	uint32_t	ibrd;			/* 整数ボーレート分周比 */
	uint32_t	fbrd;			/* 小数ボーレート分周比 */
	uint32_t	lcr_h;			/* ライン制御設定値 */
} SIOPINIB;

/*
 *  SIOポート管理ブロックの定義
 */
struct sio_port_control_block {
	const SIOPINIB *siopinib;	/* SIOポート初期化ブロック */
	EXINF		exinf;			/* 拡張情報 */
	bool_t		opened;			/* オープン済み */
};

/*
 *  SIOポート初期化ブロック
 *
 *  ボーレート分周は IBRD = UARTCLK / (16 × baud)，FBRD は小数部だが QEMU の
 *  PL011 はボーレートを実際には反映しないため，IBRD のみ設定すれば十分である．
 */
const SIOPINIB siopinib_table[TNUM_SIOP] = {
	{ SIO_UART_BASE, SIO_UART_IBRD, SIO_UART_FBRD, PL011_LCRH_WLEN_8 }
};

/*
 *  SIOポート管理ブロックのエリア
 */
SIOPCB	siopcb_table[TNUM_SIOP];

/*
 *  SIOポートIDから管理ブロックを取り出すためのマクロ
 */
#define INDEX_SIOP(siopid)	((uint_t)((siopid) - 1))
#define get_siopcb(siopid)	(&(siopcb_table[INDEX_SIOP(siopid)]))

/*
 *  SIOドライバの初期化
 */
void
pl011_uart_initialize(void)
{
	SIOPCB	*p_siopcb;
	uint_t	i;

	for (p_siopcb = siopcb_table, i = 0; i < TNUM_SIOP; p_siopcb++, i++) {
		p_siopcb->siopinib = &(siopinib_table[i]);
		p_siopcb->opened = false;
	}
}

/*
 *  SIOドライバの終了処理
 */
void
pl011_uart_terminate(void)
{
	uint_t	i;

	for (i = 0; i < TNUM_SIOP; i++) {
		pl011_uart_cls_por(&(siopcb_table[i]));
	}
}

/*
 *  SIOポートのオープン
 */
SIOPCB *
pl011_uart_opn_por(ID siopid, EXINF exinf)
{
	SIOPCB		*p_siopcb;
	uintptr_t	base;

	p_siopcb = get_siopcb(siopid);

	if (!(p_siopcb->opened)) {
		base = p_siopcb->siopinib->base;

		/*
		 *  いったん UART を停止し，割込みをすべてマスク・クリアする．
		 */
		sil_wrw_mem((void *) PL011_UARTCR(base), 0U);
		sil_wrw_mem((void *) PL011_UARTIMSC(base), 0U);
		sil_wrw_mem((void *) PL011_UARTICR(base),
					PL011_INT_OE | PL011_INT_BE | PL011_INT_PE | PL011_INT_FE
					| PL011_INT_RT | PL011_INT_TX | PL011_INT_RX);

		/*
		 *  受信FIFOに残った文字を読み捨てる．
		 */
		while (pl011_uart_getready(base)) {
			(void) pl011_uart_getchar(base);
		}

		/*
		 *  ボーレートと通信規格（8N1，FIFO 不使用）を設定する．
		 */
		sil_wrw_mem((void *) PL011_UARTIBRD(base), p_siopcb->siopinib->ibrd);
		sil_wrw_mem((void *) PL011_UARTFBRD(base), p_siopcb->siopinib->fbrd);
		sil_wrw_mem((void *) PL011_UARTLCR_H(base), p_siopcb->siopinib->lcr_h);

		/*
		 *  送受信を許可して UART を起動する（割込みはまだ許可しない）．
		 */
		sil_wrw_mem((void *) PL011_UARTCR(base),
					PL011_CR_UARTEN | PL011_CR_TXE | PL011_CR_RXE);

		p_siopcb->opened = true;
	}
	p_siopcb->exinf = exinf;
	return(p_siopcb);
}

/*
 *  SIOポートのクローズ
 */
void
pl011_uart_cls_por(SIOPCB *p_siopcb)
{
	if (p_siopcb->opened) {
		sil_wrw_mem((void *) PL011_UARTIMSC(p_siopcb->siopinib->base), 0U);
		sil_wrw_mem((void *) PL011_UARTCR(p_siopcb->siopinib->base), 0U);
		p_siopcb->opened = false;
	}
}

/*
 *  SIOポートへの文字送信
 */
bool_t
pl011_uart_snd_chr(SIOPCB *p_siopcb, char c)
{
	if (pl011_uart_putready(p_siopcb->siopinib->base)) {
		pl011_uart_putchar(p_siopcb->siopinib->base, c);
		return(true);
	}
	return(false);
}

/*
 *  SIOポートからの文字受信
 */
int_t
pl011_uart_rcv_chr(SIOPCB *p_siopcb)
{
	if (pl011_uart_getready(p_siopcb->siopinib->base)) {
		return((int_t)(uint8_t) pl011_uart_getchar(p_siopcb->siopinib->base));
	}
	return(-1);
}

/*
 *  SIOポートからのコールバックの許可
 */
void
pl011_uart_ena_cbr(SIOPCB *p_siopcb, uint_t cbrtn)
{
	uintptr_t	base = p_siopcb->siopinib->base;
	uint32_t	imsc;

	imsc = sil_rew_mem((void *) PL011_UARTIMSC(base));
	switch (cbrtn) {
	case SIO_RDY_SND:
		imsc |= PL011_INT_TX;
		break;
	case SIO_RDY_RCV:
		imsc |= PL011_INT_RX;
		break;
	default:
		break;
	}
	sil_wrw_mem((void *) PL011_UARTIMSC(base), imsc);
}

/*
 *  SIOポートからのコールバックの禁止
 */
void
pl011_uart_dis_cbr(SIOPCB *p_siopcb, uint_t cbrtn)
{
	uintptr_t	base = p_siopcb->siopinib->base;
	uint32_t	imsc;

	imsc = sil_rew_mem((void *) PL011_UARTIMSC(base));
	switch (cbrtn) {
	case SIO_RDY_SND:
		imsc &= ~PL011_INT_TX;
		break;
	case SIO_RDY_RCV:
		imsc &= ~PL011_INT_RX;
		break;
	default:
		break;
	}
	sil_wrw_mem((void *) PL011_UARTIMSC(base), imsc);
}

/*
 *  SIOポートに対する割込み処理
 *
 *  FIFO 不使用のため，送受信状態（UARTFR）から要因を判定する．送信割込みは
 *  UARTICR への書込みでクリアする（受信割込みはデータレジスタの読出しで自然
 *  にクリアされるが，念のため UARTICR でもクリアする）．
 */
static void
pl011_uart_isr_siop(SIOPCB *p_siopcb)
{
	uintptr_t	base = p_siopcb->siopinib->base;

	if (pl011_uart_getready(base)) {
		sil_wrw_mem((void *) PL011_UARTICR(base), PL011_INT_RX);
		pl011_uart_irdy_rcv(p_siopcb->exinf);
	}
	if (pl011_uart_putready(base)) {
		sil_wrw_mem((void *) PL011_UARTICR(base), PL011_INT_TX);
		pl011_uart_irdy_snd(p_siopcb->exinf);
	}
}

/*
 *  SIOの割込みサービスルーチン
 */
void
pl011_uart_isr(ID siopid)
{
	pl011_uart_isr_siop(get_siopcb(siopid));
}
