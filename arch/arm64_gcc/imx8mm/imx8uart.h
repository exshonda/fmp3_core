/*
 *  TOPPERS/FMP Kernel
 *      Toyohashi Open Platform for Embedded Real-Time Systems/
 *      Flexible MultiProcessor Kernel
 *
 *  Copyright (C) 2020-2021 by Embedded and Real-Time Systems Laboratory
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
 *  @(#) $Id: imx8uart.h 282 2021-06-03 06:35:25Z ertl-honda $
 */

/*
 * シリアルI/Oデバイス（SIO）ドライバ（IMX8MM用）
 */

#ifndef TOPPERS_IMX8_UART_H
#define TOPPERS_IMX8_UART_H

/*
 *		i.mx8mm UARTに関する定義
 */

/*
 *  UARTレジスタの番地の定義
 */
#define UART_URXD(base)		((uint32_t *)((base) + 0x00U))
#define UART_UTXD(base)		((uint32_t *)((base) + 0x40U))
#define UART_UCR1(base)		((uint32_t *)((base) + 0x80U))
#define UART_UCR2(base)		((uint32_t *)((base) + 0x84U))
#define UART_UCR3(base)		((uint32_t *)((base) + 0x88U))
#define UART_UCR4(base)		((uint32_t *)((base) + 0x8CU))
#define UART_UFCR(base)		((uint32_t *)((base) + 0x90U))
#define UART_USR1(base)		((uint32_t *)((base) + 0x94U))
#define UART_USR2(base)		((uint32_t *)((base) + 0x98U))
#define UART_UESC(base)		((uint32_t *)((base) + 0x9CU))
#define UART_UTIM(base)		((uint32_t *)((base) + 0xA0U))
#define UART_UBIR(base)		((uint32_t *)((base) + 0xA4U))
#define UART_UBMR(base)		((uint32_t *)((base) + 0xA8U))
#define UART_UBRC(base)		((uint32_t *)((base) + 0xACU))
#define UART_ONEMS(base)	((uint32_t *)((base) + 0xB0U))
#define UART_UTS(base)		((uint32_t *)((base) + 0xB4U))
#define UART_UMCR(base)		((uint32_t *)((base) + 0xB8U))

#define UART_UTS_TXEMPTY	(1<<6U)
#define UART_UTS_TXFULL		(1<<4U)
#define UART_UTS_RXEMPTY	(1<<5U)

#define UART_UCR1_TXMPTYEN	(1<<6U)
#define UART_UCR1_RRDYEN	(1<<9U)
#define UART_UCR1_UARTEN	(1<<0U)

#define UART_UCR2_IRTS		(1<<14U)
#define UART_UCR2_PREN		(1<<8U)
#define UART_UCR2_STPB		(1<<6U)
#define UART_UCR2_WS		(1<<5U)
#define UART_UCR2_TXEN		(1<<2U)
#define UART_UCR2_RXEN		(1<<1U)
#define UART_UCR2_SRST		(1<<0U)

#define UART_USR2_TXFE		(1<<14U)

#define UART_UTS_SOFTRST	(1<<0U)

#ifdef TOPPERS_OMIT_TECS
/*
 *		i.mx8mm UART用 簡易SIOドライバ
 */
#include <sil.h>

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
 *  受信バッファに文字があるか？
 */
Inline bool_t
imx8uart_getready(uintptr_t base)
{
	return(!(sil_rew_mem(UART_UTS(base)) & UART_UTS_RXEMPTY));
}

/*
 *  送信バッファに空きがあるか？
 */
Inline bool_t
imx8uart_putready(uintptr_t base)
{
	return(!(sil_rew_mem(UART_UTS(base)) & UART_UTS_TXFULL));
}

/*
 *  受信した文字の取出し
 */
Inline char
imx8uart_getchar(uintptr_t base)
{
	return((char) sil_rew_mem(UART_URXD(base)));
}

/*
 *  送信する文字の書込み
 */
Inline void
imx8uart_putchar(uintptr_t base, char c)
{
	sil_wrw_mem(UART_UTXD(base), (uint32_t) c);
}

/*
 *  送信割込みイネーブル
 */
Inline void
imx8uart_enable_send(uintptr_t base)
{
	sil_wrw_mem(UART_UCR1(base),
				sil_rew_mem(UART_UCR1(base))|UART_UCR1_TXMPTYEN);
}

/*
 *  送信割込みディスエーブル
 */
Inline void
imx8uart_disable_send(uintptr_t base)
{
	sil_wrw_mem(UART_UCR1(base),
				sil_rew_mem(UART_UCR1(base))&~UART_UCR1_TXMPTYEN);
}

/*
 *  受信割込みイネーブル
 */
Inline void
imx8uart_enable_receive(uintptr_t base)
{
	sil_wrw_mem(UART_UCR1(base),
				sil_rew_mem(UART_UCR1(base))|UART_UCR1_RRDYEN);
}

/*
 *  受信割込みディスエーブル
 */
Inline void
imx8uart_disable_receive(uintptr_t base)
{
	sil_wrw_mem(UART_UCR1(base),
				sil_rew_mem(UART_UCR1(base))&~UART_UCR1_RRDYEN);
}

/*
 *  シリアルインタフェースドライバに提供する機能
 */

/*
 *  SIOドライバの初期化
 */
extern void		imx8uart_initialize(void);

/*
 *  SIOドライバの終了処理
 */
extern void		imx8uart_terminate(void);

/*
 *  SIOの割込みサービスルーチン
 */
extern void		imx8uart_isr(ID siopid);

/*
 *  SIOポートのオープン
 */
extern SIOPCB	*imx8uart_opn_por(ID siopid, EXINF exinf);

/*
 *  SIOポートのクローズ
 */
extern void		imx8uart_cls_por(SIOPCB *siopcb);

/*
 *  SIOポートへの文字送信
 */
extern bool_t	imx8uart_snd_chr(SIOPCB *siopcb, char c);

/*
 *  SIOポートからの文字受信
 */
extern int_t	imx8uart_rcv_chr(SIOPCB *siopcb);

/*
 *  SIOポートからのコールバックの許可
 */
extern void		imx8uart_ena_cbr(SIOPCB *siopcb, uint_t cbrtn);

/*
 *  SIOポートからのコールバックの禁止
 */
extern void		imx8uart_dis_cbr(SIOPCB *siopcb, uint_t cbrtn);

/*
 *  SIOポートからの送信可能コールバック
 */
extern void		imx8uart_irdy_snd(EXINF exinf);

/*
 *  SIOポートからの受信通知コールバック
 */
extern void		imx8uart_irdy_rcv(EXINF exinf);

#endif /* TOPPERS_MACRO_ONLY */
#endif /* TOPPERS_OMIT_TECS */
#endif /* TOPPERS_IMX8_UART_H */
