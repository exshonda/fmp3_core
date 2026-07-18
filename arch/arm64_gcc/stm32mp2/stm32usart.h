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
 */

/*
 * シリアルI/Oデバイス（SIO）ドライバ（STM32 USART用）
 *
 *  STM32MP2 の USART は FSBL(TF-A) により 115200 8N1 で初期化済みである
 *  ことを前提とし，ボーレート(BRR)等は再設定しない．本ドライバはコンソー
 *  ル(USART2)の TE/RE/UE を保証しつつ送受信を行う．
 */

#ifndef TOPPERS_STM32_USART_H
#define TOPPERS_STM32_USART_H

/*
 *		STM32 USART レジスタの定義
 */

/*
 *  UARTレジスタの番地の定義 (オフセット)
 */
#define USART_CR1(base)		((uint32_t *)((base) + 0x00U))
#define USART_CR2(base)		((uint32_t *)((base) + 0x04U))
#define USART_CR3(base)		((uint32_t *)((base) + 0x08U))
#define USART_BRR(base)		((uint32_t *)((base) + 0x0CU))
#define USART_GTPR(base)	((uint32_t *)((base) + 0x10U))
#define USART_RTOR(base)	((uint32_t *)((base) + 0x14U))
#define USART_RQR(base)		((uint32_t *)((base) + 0x18U))
#define USART_ISR(base)		((uint32_t *)((base) + 0x1CU))
#define USART_ICR(base)		((uint32_t *)((base) + 0x20U))
#define USART_RDR(base)		((uint32_t *)((base) + 0x24U))
#define USART_TDR(base)		((uint32_t *)((base) + 0x28U))

/* CR1 ビット */
#define USART_CR1_UE		(1U << 0)	/* USART Enable */
#define USART_CR1_RE		(1U << 2)	/* Receiver Enable */
#define USART_CR1_TE		(1U << 3)	/* Transmitter Enable */
#define USART_CR1_RXNEIE	(1U << 5)	/* RXNE/RXFNE 割込み許可 */
#define USART_CR1_TXEIE		(1U << 7)	/* TXE/TXFNF 割込み許可 */

/* ISR ビット */
#define USART_ISR_RXNE		(1U << 5)	/* 受信データレディ (RXNE/RXFNE) */
#define USART_ISR_TC		(1U << 6)	/* 送信完了 */
#define USART_ISR_TXE		(1U << 7)	/* 送信データレジスタ空 (TXE/TXFNF) */
#define USART_ISR_ORE		(1U << 3)	/* オーバーラン */

/* ICR ビット */
#define USART_ICR_ORECF		(1U << 3)	/* オーバーランクリア */

#ifdef TOPPERS_OMIT_TECS
/*
 *		STM32 USART用 簡易SIOドライバ
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
stm32usart_getready(uintptr_t base)
{
	return((sil_rew_mem(USART_ISR(base)) & USART_ISR_RXNE) != 0U);
}

/*
 *  送信バッファに空きがあるか？
 */
Inline bool_t
stm32usart_putready(uintptr_t base)
{
	return((sil_rew_mem(USART_ISR(base)) & USART_ISR_TXE) != 0U);
}

/*
 *  受信した文字の取出し
 */
Inline char
stm32usart_getchar(uintptr_t base)
{
	return((char)(sil_rew_mem(USART_RDR(base)) & 0xffU));
}

/*
 *  送信する文字の書込み
 */
Inline void
stm32usart_putchar(uintptr_t base, char c)
{
	sil_wrw_mem(USART_TDR(base), (uint32_t)(uint8_t) c);
}

/*
 *  送信割込みイネーブル
 */
Inline void
stm32usart_enable_send(uintptr_t base)
{
	sil_wrw_mem(USART_CR1(base),
				sil_rew_mem(USART_CR1(base)) | USART_CR1_TXEIE);
}

/*
 *  送信割込みディスエーブル
 */
Inline void
stm32usart_disable_send(uintptr_t base)
{
	sil_wrw_mem(USART_CR1(base),
				sil_rew_mem(USART_CR1(base)) & ~USART_CR1_TXEIE);
}

/*
 *  受信割込みイネーブル
 */
Inline void
stm32usart_enable_receive(uintptr_t base)
{
	sil_wrw_mem(USART_CR1(base),
				sil_rew_mem(USART_CR1(base)) | USART_CR1_RXNEIE);
}

/*
 *  受信割込みディスエーブル
 */
Inline void
stm32usart_disable_receive(uintptr_t base)
{
	sil_wrw_mem(USART_CR1(base),
				sil_rew_mem(USART_CR1(base)) & ~USART_CR1_RXNEIE);
}

/*
 *  シリアルインタフェースドライバに提供する機能
 */
extern void		stm32usart_initialize(void);
extern void		stm32usart_terminate(void);
extern void		stm32usart_isr(ID siopid);
extern SIOPCB	*stm32usart_opn_por(ID siopid, EXINF exinf);
extern void		stm32usart_cls_por(SIOPCB *siopcb);
extern bool_t	stm32usart_snd_chr(SIOPCB *siopcb, char c);
extern int_t	stm32usart_rcv_chr(SIOPCB *siopcb);
extern void		stm32usart_ena_cbr(SIOPCB *siopcb, uint_t cbrtn);
extern void		stm32usart_dis_cbr(SIOPCB *siopcb, uint_t cbrtn);
extern void		stm32usart_irdy_snd(EXINF exinf);
extern void		stm32usart_irdy_rcv(EXINF exinf);

#endif /* TOPPERS_MACRO_ONLY */
#endif /* TOPPERS_OMIT_TECS */
#endif /* TOPPERS_STM32_USART_H */
