/*
 *  TOPPERS Software
 *      Toyohashi Open Platform for Embedded Real-Time Systems
 * 
 *  Copyright (C) 2024 by Embedded and Real-Time Systems Laboratory
 *              Graduate School of Information Science, Nagoya Univ., JAPAN
 * 
 *  上記著作権者は，以下の(1)〜(4)の条件を満たす場合に限り，本ソフトウェ
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
 *  $Id: xuartps.h 334 2023-04-14 07:39:43Z ertl-honda $
 */

/*
 *    MMUARTに関する定義と簡易SIOドライバ
 */

#ifndef TOPPERS_MMUART_H
#define TOPPERS_MMUART_H

/*
 *    MMUARTに関する定義
 */

/*
 *  MMUARTレジスタの番地の定義
 */
#define MMUART_RBR(base)  ((uint32_t *)((base) + 0x000U))
#define MMUART_IER(base)  ((uint32_t *)((base) + 0x004U))
#define MMUART_IIR(base)  ((uint32_t *)((base) + 0x008U))
#define MMUART_LCR(base)  ((uint32_t *)((base) + 0x00CU))
#define MMUART_MCR(base)  ((uint32_t *)((base) + 0x010U))
#define MMUART_LSR(base)  ((uint32_t *)((base) + 0x014U))
#define MMUART_MSR(base)  ((uint32_t *)((base) + 0x018U))
#define MMUART_SR(base)   ((uint32_t *)((base) + 0x01CU))
#define MMUART_IEM(base)  ((uint32_t *)((base) + 0x024U))
#define MMUART_IIM(base)  ((uint32_t *)((base) + 0x028U))
#define MMUART_MM0(base)  ((uint32_t *)((base) + 0x030U))
#define MMUART_MM1(base)  ((uint32_t *)((base) + 0x034U))
#define MMUART_MM2(base)  ((uint32_t *)((base) + 0x038U))
#define MMUART_DFR(base)  ((uint32_t *)((base) + 0x03CU))
#define MMUART_GFR(base)  ((uint32_t *)((base) + 0x044U))
#define MMUART_TTG(base)  ((uint32_t *)((base) + 0x048U))
#define MMUART_RTO(base)  ((uint32_t *)((base) + 0x04CU))
#define MMUART_ADR(base)  ((uint32_t *)((base) + 0x050U))
#define MMUART_DLR(base)  ((uint32_t *)((base) + 0x080U))
#define MMUART_DMR(base)  ((uint32_t *)((base) + 0x084U))
#define MMUART_THR(base)  ((uint32_t *)((base) + 0x000U))  /* if DLAB in LCR is 0 */
#define MMUART_FCR(base)  ((uint32_t *)((base) + 0x108U))

/*
 *  MMUART_IERの設定値
 */
#define MMUART_IER_ERBFI  UINT_C(0x0001)  /* 受信割込み許可 */
#define MMUART_IER_ETBEI  UINT_C(0x0002)  /* 送信割込み許可 */

/*
 *  MMUART_LSRの設定値
 */
#define MMUART_LSR_DR    UINT_C(0x0001)  /* データ受信フラグ */
#define MMUART_LSR_THRE  UINT_C(0x0020)  /* データ送信エンプティフラグ */
#define MMUART_LSR_TEMT  UINT_C(0x0020)  /* データ送信オールエンプティフラグ */

/*
 *  MMUART_FCRの設定値
 */
#define MMUART_FCR_RX_TRIG_MASK         UINT_C(0x00C0)
#define MMUART_FCR_CLEAR_TX_FIFO        UINT_C(0x0002)
#define MMUART_FCR_CLEAR_RX_FIFO        UINT_C(0x0004)
#define MMUART_FCR_ENABLE_RXRDY_TXRDYN  UINT_C(0x01)

/*
 *  MMUART_LCRの設定値
 */
#define MMUART_LCR_WLS_8BIT  UINT_C(0x03)
#define MMUART_LCR_PEN_NOP   UINT_C(0x00)
#define MMUART_LCR_STB_ONE   UINT_C(0x00)
#define MMUART_LCR_DLAB      UINT_C(0x80)

/*
 *  MMUART_MM0の設定値
 */
#define MMUART_MM0_EFBR      UINT_C(0x80)

#ifdef TOPPERS_OMIT_TECS
/*
 *    MMUART用 簡易SIOドライバ
 */
#include <sil.h>

/*
 *  SIOポート数の定義
 */
#define TNUM_SIOP  1  /* サポートするSIOポートの数 */

/*
 *  コールバックルーチンの識別番号
 */
#define SIO_RDY_SND  1U  /* 送信可能コールバック */
#define SIO_RDY_RCV  2U  /* 受信通知コールバック */

#ifndef TOPPERS_MACRO_ONLY

/*
 *  SIOポート管理ブロックの定義
 */
typedef struct sio_port_control_block SIOPCB;

/*
 *  プリミティブな送信／受信関数
 */

/*
 *  受信バッファに文字があるか？
 */
Inline bool_t
mmuart_getready(uintptr_t base)
{
    return((sil_rew_mem(MMUART_LSR(base)) & MMUART_LSR_DR) != 0U);
}

/*
 *  送信バッファに空きがあるか？
 */
Inline bool_t
mmuart_putready(uintptr_t base)
{
    return((sil_rew_mem(MMUART_LSR(base)) & MMUART_LSR_THRE) != 0U);
}

/*
 *  受信した文字の取出し
 */
Inline char
mmuart_getchar(uintptr_t base)
{
    return((char) sil_rew_mem(MMUART_RBR(base)));
}

/*
 *  送信する文字の書込み
 */
Inline void
mmuart_putchar(uintptr_t base, char c)
{
    sil_wrw_mem(MMUART_THR(base), (uint32_t) c);
}

/*
 *  送信割込みイネーブル
 */
Inline void
mmuart_enable_send(uintptr_t base)
{
    sil_wrw_mem(MMUART_IER(base),
                sil_rew_mem(MMUART_IER(base)) | MMUART_IER_ETBEI);
}

/*
 *  送信割込みディスエーブル
 */
Inline void
mmuart_disable_send(uintptr_t base)
{
    sil_wrw_mem(MMUART_IER(base),
                sil_rew_mem(MMUART_IER(base)) & ~MMUART_IER_ETBEI);
}

/*
 *  受信割込みイネーブル
 */
Inline void
mmuart_enable_receive(uintptr_t base)
{
    sil_wrw_mem(MMUART_IER(base),
                sil_rew_mem(MMUART_IER(base)) | MMUART_IER_ERBFI);
}

/*
 *  受信割込みディスエーブル
 */
Inline void
mmuart_disable_receive(uintptr_t base)
{
    sil_wrw_mem(MMUART_IER(base),
                sil_rew_mem(MMUART_IER(base)) & ~MMUART_IER_ERBFI);
}

/*
 *  シリアルインタフェースドライバに提供する機能
 */

/*
 *  SIOドライバの初期化
 */
extern void mmuart_initialize(void);

/*
 *  SIOドライバの終了処理
 */
extern void mmuart_terminate(void);

/*
 *  SIOの割込みサービスルーチン
 */
extern void mmuart_isr(ID siopid);

/*
 *  SIOポートのオープン
 */
extern SIOPCB *mmuart_opn_por(ID siopid, EXINF exinf);

/*
 *  SIOポートのクローズ
 */
extern void mmuart_cls_por(SIOPCB *siopcb);

/*
 *  SIOポートへの文字送信
 */
extern bool_t mmuart_snd_chr(SIOPCB *siopcb, char c);

/*
 *  SIOポートからの文字受信
 */
extern int_t mmuart_rcv_chr(SIOPCB *siopcb);

/*
 *  SIOポートからのコールバックの許可
 */
extern void mmuart_ena_cbr(SIOPCB *siopcb, uint_t cbrtn);

/*
 *  SIOポートからのコールバックの禁止
 */
extern void mmuart_dis_cbr(SIOPCB *siopcb, uint_t cbrtn);

/*
 *  SIOポートからの送信可能コールバック
 */
extern void mmuart_irdy_snd(EXINF exinf);

/*
 *  SIOポートからの受信通知コールバック
 */
extern void mmuart_irdy_rcv(EXINF exinf);

#endif /* TOPPERS_MACRO_ONLY */
#endif /* TOPPERS_OMIT_TECS */
#endif /* TOPPERS_MMUART_H */
