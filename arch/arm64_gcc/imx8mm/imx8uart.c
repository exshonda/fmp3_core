/*
 *  TOPPERS/FMP Kernel
 *      Toyohashi Open Platform for Embedded Real-Time Systems/
 *      Flexible MultiProcessor Kernel
 * 
 *  Copyright (C) 2018 by Embedded and Real-Time Systems Laboratory
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
 *  @(#) $Id: imx8uart.c 282 2021-06-03 06:35:25Z ertl-honda $
 */
/*
 *		IMX8MM UART用 簡易SIOドライバ（非TECS版専用）
 */

#include <kernel.h>
#include <t_syslog.h>
#include "target_syssvc.h"
#include "imx8mm.h"
#include "imx8uart.h"

/*
 *  SIOポート初期化ブロックの定義
 */
typedef struct sio_port_initialization_block {
	uintptr_t	base;		/* UARTレジスタのベースアドレス */
	uint16_t	rfdif;		/* UFCRレジスタのRFDIV設定値 */
	uint16_t	ubir;		/* ボーレート生成レジスタの設定値 */
	uint8_t		ubmr;		/* ボーレート分割レジスタの設定値 */
} SIOPINIB;

/*
 *  SIOポート管理ブロックの定義
 */
typedef struct sio_port_control_block {
	const SIOPINIB *p_siopinib;		/* SIOポート初期化ブロック */
	EXINF	exinf;				/* 拡張情報 */
	bool_t		opened;				/* オープン済み */
} SIOPCB;

/*
 *  SIOポート初期化ブロック
 */
const SIOPINIB siopinib_table[TNUM_SIOP] = {
	{ SIO_IMX8UART_BASE, SIO_IMX8UART_RFDIF,
								SIO_IMX8UART_UBIR, SIO_IMX8UART_UBMR }
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
imx8uart_initialize(void)
{
	SIOPCB	*p_siopcb;
	uint_t	i;

	/*
	 *  SIOポート管理ブロックの初期化
	 */
	for (p_siopcb = siopcb_table, i = 0; i < TNUM_SIOP; p_siopcb++, i++) {
		p_siopcb->p_siopinib = &(siopinib_table[i]);
		p_siopcb->opened = false;
	}
}

/*
 *  SIOドライバの終了処理
 */
void
imx8uart_terminate(void)
{
	uint_t	i;
	SIOPCB	*p_siopcb;

	for (i = 0; i < TNUM_SIOP; i++) {
		p_siopcb = &(siopcb_table[i]);
		if (p_siopcb->opened) {
			/*
			 *  送信FIFOが空になるまで待つ
			 */
			while ((sil_rew_mem(UART_UTS(p_siopcb->p_siopinib->base))
					& UART_UTS_TXEMPTY) == 0U) {
				sil_dly_nse(100);
			}

			/*
			 *  オープンされているSIOポートのクローズ
			 */
			imx8uart_cls_por(&(siopcb_table[i]));
		}
	}
}

/*
 *  SIOポートのオープン
 */
SIOPCB *
imx8uart_opn_por(ID siopid, EXINF exinf)
{
	SIOPCB		*p_siopcb;
	uintptr_t	base;

	p_siopcb = get_siopcb(siopid);

	if (!(p_siopcb->opened)) {
		/*
		 *  既にオープンしている場合は、二重にオープンしない．
		 */
		base = p_siopcb->p_siopinib->base;

		/* Wait until Tx FIFO empty */
		if (sil_rew_mem(UART_UCR1(base)) & UART_UCR1_UARTEN) {
			while(!(sil_rew_mem(UART_USR2(base)) & UART_USR2_TXFE));
		}

		/* Disable */
		sil_wrw_mem(UART_UCR1(base), 0x00);

		/* Disable tranmitter/receiver, Software Reset */
		sil_wrw_mem(UART_UCR2(base), 0x00);
		while((sil_rew_mem(UART_UTS(base)) & UART_UTS_SOFTRST) != 0U);

		/* Set RXD Muxed Input Selected */
		sil_wrw_mem(UART_UCR3(base), 0x0784);

		/* CTS Trigger Level */
		sil_wrw_mem(UART_UCR4(base), 0x8000);

		/* Set Escape Character */
		sil_wrw_mem(UART_UESC(base), 0x002b);
		/* Set Escape Timer */
		sil_wrw_mem(UART_UTIM(base), 0x0000);

		/* Receive FIFO interrupt trigger level */
		sil_wrw_mem(UART_UFCR(base),
					(sil_rew_mem(UART_UFCR(base)) & ~0x3f) | 0x01);

		/* Transmit FIFO interrupt trigger level */
		sil_wrw_mem(UART_UFCR(base),
					((sil_rew_mem(UART_UFCR(base)) & ~0xfc00) | (0x02<<10)));

		/*
		 * Baud rate関連(u-bootでのPLLの設定が変化すると追従する必要あり)
		 */
		/* Rererence Frequency Divider by 1*/
		sil_wrw_mem(UART_UFCR(base),
					((sil_rew_mem(UART_UFCR(base)) & ~0x0380) | (p_siopcb->p_siopinib->rfdif<<7)));

		/* Set Baud rate */
		sil_wrw_mem(UART_UBIR(base), p_siopcb->p_siopinib->ubir);
		sil_wrw_mem(UART_UBMR(base), p_siopcb->p_siopinib->ubmr);

		/* 8bit, 1stop, Noprity, Ignore RTS, Enable tranmitter/receiver */
		sil_wrw_mem(UART_UCR2(base), UART_UCR2_IRTS|UART_UCR2_WS|UART_UCR2_TXEN|UART_UCR2_RXEN|UART_UCR2_SRST);

		/*
		 * Enable UART with Disable All Interrupt
		 */
		sil_wrw_mem(UART_UCR1(base), UART_UCR1_UARTEN);

		p_siopcb->opened = true;
	}
	p_siopcb->exinf = exinf;
	return(p_siopcb);   
}

/*
 *  SIOポートのクローズ
 */
void
imx8uart_cls_por(SIOPCB *p_siopcb)
{
	if (p_siopcb->opened) {
		/*
		 *  送受信のディスエーブル & 全割込みをディスエーブル
		 */
		sil_wrw_mem(UART_UCR1(p_siopcb->p_siopinib->base), 0x00U);

		p_siopcb->opened = false;
	}
}

/*
 *  SIOポートへの文字送信
 */
bool_t
imx8uart_snd_chr(SIOPCB *p_siopcb, char c)
{
	if (imx8uart_putready(p_siopcb->p_siopinib->base)){
		imx8uart_putchar(p_siopcb->p_siopinib->base, c);
		return(true);
	}
	return(false);
}

/*
 *  SIOポートからの文字受信
 */
int_t
imx8uart_rcv_chr(SIOPCB *p_siopcb)
{
	if (imx8uart_getready(p_siopcb->p_siopinib->base)) {
		return((int_t) imx8uart_getchar(p_siopcb->p_siopinib->base));
	}
	return(-1);
}

/*
 *  SIOポートからのコールバックの許可
 */
void
imx8uart_ena_cbr(SIOPCB *p_siopcb, uint_t cbrtn)
{
	switch (cbrtn) {
	case SIO_RDY_SND:
		imx8uart_enable_send(p_siopcb->p_siopinib->base);
		break;
	case SIO_RDY_RCV:
		imx8uart_enable_receive(p_siopcb->p_siopinib->base);
		break;
	}
}

/*
 *  SIOポートからのコールバックの禁止
 */
void
imx8uart_dis_cbr(SIOPCB *p_siopcb, uint_t cbrtn)
{
	switch (cbrtn) {
	case SIO_RDY_SND:
		imx8uart_disable_send(p_siopcb->p_siopinib->base);
		break;
	case SIO_RDY_RCV:
		imx8uart_disable_receive(p_siopcb->p_siopinib->base);
		break;
	}
}

/*
 *  SIOポートに対する割込み処理
 */
static void
imx8uart_isr_siop(SIOPCB *p_siopcb)
{
	uintptr_t	base = p_siopcb->p_siopinib->base;

	/* 割込みのクリア */
	sil_wrw_mem(UART_USR1(base), sil_rew_mem(UART_USR1(base)));
	sil_wrw_mem(UART_USR2(base), sil_rew_mem(UART_USR2(base)));
	
	if (imx8uart_getready(p_siopcb->p_siopinib->base)) {
		/*
		 *  受信通知コールバックルーチンを呼び出す．
		 */
		imx8uart_irdy_rcv(p_siopcb->exinf);
	}
	if (imx8uart_putready(p_siopcb->p_siopinib->base)) {
		/*
		 *  送信可能コールバックルーチンを呼び出す．
		 */
		imx8uart_irdy_snd(p_siopcb->exinf);
	}
}

/*
 *  SIOの割込みサービスルーチン
 */
void
imx8uart_isr(ID siopid)
{
	imx8uart_isr_siop(get_siopcb(siopid));
}
