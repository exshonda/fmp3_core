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
 */
/*
 *		STM32 USART用 簡易SIOドライバ（非TECS版専用）
 *
 *  USART は TF-A(FSBL) により 115200 8N1 に初期化済みであることを前提と
 *  し，ボーレート(BRR)・オーバーサンプリング等は変更しない．本ドライバは
 *  TE/RE/UE が有効であることを保証するのみとする．
 */

#include <kernel.h>
#include <t_syslog.h>
#include "target_syssvc.h"
#include "stm32mp2.h"
#include "stm32usart.h"

/*
 *  SIOポート初期化ブロックの定義
 */
typedef struct sio_port_initialization_block {
	uintptr_t	base;		/* USARTレジスタのベースアドレス */
} SIOPINIB;

/*
 *  SIOポート管理ブロックの定義
 */
typedef struct sio_port_control_block {
	const SIOPINIB *p_siopinib;		/* SIOポート初期化ブロック */
	EXINF	exinf;					/* 拡張情報 */
	bool_t		opened;				/* オープン済み */
} SIOPCB;

/*
 *  SIOポート初期化ブロック
 */
const SIOPINIB siopinib_table[TNUM_SIOP] = {
	{ SIO_STM32USART_BASE }
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
stm32usart_initialize(void)
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
stm32usart_terminate(void)
{
	uint_t	i;
	SIOPCB	*p_siopcb;

	for (i = 0; i < TNUM_SIOP; i++) {
		p_siopcb = &(siopcb_table[i]);
		if (p_siopcb->opened) {
			/*
			 *  送信完了まで待つ
			 */
			while ((sil_rew_mem(USART_ISR(p_siopcb->p_siopinib->base))
					& USART_ISR_TC) == 0U) {
				sil_dly_nse(100);
			}
			stm32usart_cls_por(&(siopcb_table[i]));
		}
	}
}

/*
 *  SIOポートのオープン
 *
 *  TF-A が設定した BRR / オーバーサンプリング設定は保持し，TE/RE/UE のみ
 *  を有効化する（割込みは無効状態で開始）．
 */
SIOPCB *
stm32usart_opn_por(ID siopid, EXINF exinf)
{
	SIOPCB		*p_siopcb;
	uintptr_t	base;
	uint32_t	cr1;

	p_siopcb = get_siopcb(siopid);

	if (!(p_siopcb->opened)) {
		base = p_siopcb->p_siopinib->base;

		/*
		 *  送受信を有効化（BRR等は触らない）．
		 *  送信/受信割込みは無効のままにする．
		 */
		cr1 = sil_rew_mem(USART_CR1(base));
		cr1 &= ~(USART_CR1_TXEIE | USART_CR1_RXNEIE);
		cr1 |= (USART_CR1_UE | USART_CR1_TE | USART_CR1_RE);
		sil_wrw_mem(USART_CR1(base), cr1);

		/* 滞留したオーバーランをクリア */
		sil_wrw_mem(USART_ICR(base), USART_ICR_ORECF);

		p_siopcb->opened = true;
	}
	p_siopcb->exinf = exinf;
	return(p_siopcb);
}

/*
 *  SIOポートのクローズ
 */
void
stm32usart_cls_por(SIOPCB *p_siopcb)
{
	if (p_siopcb->opened) {
		/*
		 *  送受信割込みのみ禁止する（コンソールとしての送受信は維持）．
		 */
		sil_wrw_mem(USART_CR1(p_siopcb->p_siopinib->base),
					sil_rew_mem(USART_CR1(p_siopcb->p_siopinib->base))
					& ~(USART_CR1_TXEIE | USART_CR1_RXNEIE));
		p_siopcb->opened = false;
	}
}

/*
 *  SIOポートへの文字送信
 */
bool_t
stm32usart_snd_chr(SIOPCB *p_siopcb, char c)
{
	if (stm32usart_putready(p_siopcb->p_siopinib->base)) {
		stm32usart_putchar(p_siopcb->p_siopinib->base, c);
		return(true);
	}
	return(false);
}

/*
 *  SIOポートからの文字受信
 */
int_t
stm32usart_rcv_chr(SIOPCB *p_siopcb)
{
	if (stm32usart_getready(p_siopcb->p_siopinib->base)) {
		return((int_t) stm32usart_getchar(p_siopcb->p_siopinib->base));
	}
	return(-1);
}

/*
 *  SIOポートからのコールバックの許可
 */
void
stm32usart_ena_cbr(SIOPCB *p_siopcb, uint_t cbrtn)
{
	switch (cbrtn) {
	case SIO_RDY_SND:
		stm32usart_enable_send(p_siopcb->p_siopinib->base);
		break;
	case SIO_RDY_RCV:
		stm32usart_enable_receive(p_siopcb->p_siopinib->base);
		break;
	}
}

/*
 *  SIOポートからのコールバックの禁止
 */
void
stm32usart_dis_cbr(SIOPCB *p_siopcb, uint_t cbrtn)
{
	switch (cbrtn) {
	case SIO_RDY_SND:
		stm32usart_disable_send(p_siopcb->p_siopinib->base);
		break;
	case SIO_RDY_RCV:
		stm32usart_disable_receive(p_siopcb->p_siopinib->base);
		break;
	}
}

/*
 *  SIOポートに対する割込み処理
 */
static void
stm32usart_isr_siop(SIOPCB *p_siopcb)
{
	uintptr_t	base = p_siopcb->p_siopinib->base;

	/* オーバーランがあればクリア */
	if (sil_rew_mem(USART_ISR(base)) & USART_ISR_ORE) {
		sil_wrw_mem(USART_ICR(base), USART_ICR_ORECF);
	}

	if (stm32usart_getready(base)) {
		/*
		 *  受信通知コールバックルーチンを呼び出す．
		 *  （RXNE は RDR 読み出しでクリアされる）
		 */
		stm32usart_irdy_rcv(p_siopcb->exinf);
	}
	if (stm32usart_putready(base)) {
		/*
		 *  送信可能コールバックルーチンを呼び出す．
		 */
		stm32usart_irdy_snd(p_siopcb->exinf);
	}
}

/*
 *  SIOの割込みサービスルーチン
 */
void
stm32usart_isr(ID siopid)
{
	stm32usart_isr_siop(get_siopcb(siopid));
}
