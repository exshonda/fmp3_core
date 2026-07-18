/*
 *  TOPPERS Software
 *      Toyohashi Open Platform for Embedded Real-Time Systems
 *
 *  Copyright (C) 2000-2003 by Embedded and Real-Time Systems Laboratory
 *                              Toyohashi Univ. of Technology, JAPAN
 *  Copyright (C) 2006-2023 by Embedded and Real-Time Systems Laboratory
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
 *  $Id: xuartps.c 335 2023-04-18 10:50:40Z ertl-honda $
 */

/*
 *   MMUART用 簡易SIOドライバ（非TECS版専用）
 */

#include <kernel.h>
#include <t_syslog.h>
#include "target_syssvc.h"
#include "polarfire_soc.h"
#include "mmuart.h"

/*
 *  SIOポート初期化ブロックの定義
 */
typedef struct sio_port_initialization_block {
    uintptr_t base;      /* UARTレジスタのベースアドレス */
    uint16_t  lconfig;   /* ラインコンフィグレジスタの設定値 */
    uint16_t  baud;      /* ボーレート生成レジスタの設定値 */
    uint8_t   fbauddiv;  /* ボーレート分割レジスタの設定値 */
} SIOPINIB;

/*
 *  SIOポート管理ブロックの定義
 */
typedef struct sio_port_control_block {
    const SIOPINIB *p_siopinib; /* SIOポート初期化ブロック */
    EXINF exinf;                /* 拡張情報 */
    bool_t opened;              /* オープン済み */
} SIOPCB;

/*
 *  SIOポート初期化ブロック
 */
const SIOPINIB siopinib_table[TNUM_SIOP] = {
    { SIO0_BASE, SIO0_LCONFIG, SIO0_BAUD, SIO0_FBAUDDIV }
};

/*
 *  SIOポート管理ブロックのエリア
 */
SIOPCB	siopcb_table[TNUM_SIOP];

/*
 *  SIOポートIDから管理ブロックを取り出すためのマクロ
 */
#define INDEX_SIOP(siopid) ((uint_t)((siopid) - 1))
#define get_siopcb(siopid) (&(siopcb_table[INDEX_SIOP(siopid)]))

/*
 *  SIOドライバの初期化
 */
void
mmuart_initialize(void)
{
    SIOPCB *p_siopcb;
    uint_t i;

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
mmuart_terminate(void)
{
    uint_t i;
    SIOPCB *p_siopcb;

    for (i = 0; i < TNUM_SIOP; i++) {
        p_siopcb = &(siopcb_table[i]);
        if (p_siopcb->opened) {
            /*
             *  送信FIFOが空になるまで待つ
             */
            while ((sil_rew_mem(MMUART_LSR(p_siopcb->p_siopinib->base))
                    & MMUART_LSR_TEMT) == 0U) {
                sil_dly_nse(100);
            }
            
            /*
             *  オープンされているSIOポートのクローズ
             */
            mmuart_cls_por(&(siopcb_table[i]));
        }
    }
}

/*
 *  SIOポートのオープン
 */
SIOPCB *
mmuart_opn_por(ID siopid, EXINF exinf)
{
    SIOPCB    *p_siopcb;
    uintptr_t base;

    p_siopcb = get_siopcb(siopid);

    if (!(p_siopcb->opened)) {
        /*
         *  既にオープンしている場合は、二重にオープンしない．
         */
        base = p_siopcb->p_siopinib->base;
        
        /*
         *  全割込みをディスエーブル
         */
        sil_wrw_mem(MMUART_IER(base), 0x00U);

        /*
         *  FIFO停止
         */
        sil_wrw_mem(MMUART_FCR(base), 0x00);

        /*
         *  送受信FIFOのクリア
         */
        sil_wrw_mem(MMUART_FCR(base),
                    (MMUART_FCR_CLEAR_RX_FIFO|MMUART_FCR_CLEAR_TX_FIFO));
        
        /*
         *  受信トリガを1バイトに設定
         */
        sil_wrw_mem(MMUART_FCR(base),
                    sil_rew_mem(MMUART_FCR(base))
                    & ~MMUART_FCR_RX_TRIG_MASK);

        /*
         *  FIFOを有効に
         */
        sil_wrw_mem(MMUART_FCR(base),
                    sil_rew_mem(MMUART_FCR(base))
                    | MMUART_FCR_ENABLE_RXRDY_TXRDYN );

        
        /*
         *  Divisorのラッチを設定
         */
        sil_wrw_mem(MMUART_LCR(base),
                    sil_rew_mem(MMUART_LCR(base))
                    | MMUART_LCR_DLAB);
        sil_wrw_mem(MMUART_DMR(base), (uint8_t)(p_siopcb->p_siopinib->baud >> 8));
        sil_wrw_mem(MMUART_DLR(base), (uint8_t)(p_siopcb->p_siopinib->baud));
        sil_wrw_mem(MMUART_LCR(base),
                    sil_rew_mem(MMUART_LCR(base))
                    & ~MMUART_LCR_DLAB);

        /*
         *  Fration baud rate を有効に
         */
        sil_wrw_mem(MMUART_MM0(base),
                    sil_rew_mem(MMUART_MM0(base))
                    | MMUART_MM0_EFBR);

        /*
         *  Fraction baud rate registerの設定
         */
        sil_wrw_mem(MMUART_DFR(base), p_siopcb->p_siopinib->fbauddiv);

        /*
         *  Line config
         */
        sil_wrw_mem(MMUART_LCR(base), p_siopcb->p_siopinib->lconfig);
        
        p_siopcb->opened = true;
    }
    p_siopcb->exinf = exinf;
    return(p_siopcb);   
}

/*
 *  SIOポートのクローズ
 */
void
mmuart_cls_por(SIOPCB *p_siopcb)
{
    if (p_siopcb->opened) {
        /*
         *  送受信のディスエーブル
         */
//        sil_wrw_mem(XUARTPS_CR(p_siopcb->p_siopinib->base),
//                    XUARTPS_CR_TX_DIS | XUARTPS_CR_RX_DIS | XUARTPS_CR_STOPBRK);

        /*
         *  全割込みをディスエーブル
         */
        sil_wrw_mem(MMUART_IER(p_siopcb->p_siopinib->base), 0x00U);

        p_siopcb->opened = false;
    }
}

/*
 *  SIOポートへの文字送信
 */
bool_t
mmuart_snd_chr(SIOPCB *p_siopcb, char c)
{
    if (mmuart_putready(p_siopcb->p_siopinib->base)){
        mmuart_putchar(p_siopcb->p_siopinib->base, c);
        return(true);
    }
    return(false);
}

/*
 *  SIOポートからの文字受信
 */
int_t
mmuart_rcv_chr(SIOPCB *p_siopcb)
{
    if (mmuart_getready(p_siopcb->p_siopinib->base)) {
        return((int_t) mmuart_getchar(p_siopcb->p_siopinib->base));
    }
    return(-1);
}

/*
 *  SIOポートからのコールバックの許可
 */
void
mmuart_ena_cbr(SIOPCB *p_siopcb, uint_t cbrtn)
{
    switch (cbrtn) {
      case SIO_RDY_SND:
        mmuart_enable_send(p_siopcb->p_siopinib->base);
        break;
      case SIO_RDY_RCV:
        mmuart_enable_receive(p_siopcb->p_siopinib->base);
        break;
    }
}

/*
 *  SIOポートからのコールバックの禁止
 */
void
mmuart_dis_cbr(SIOPCB *p_siopcb, uint_t cbrtn)
{
    switch (cbrtn) {
      case SIO_RDY_SND:
        mmuart_disable_send(p_siopcb->p_siopinib->base);
        break;
      case SIO_RDY_RCV:
        mmuart_disable_receive(p_siopcb->p_siopinib->base);
        break;
    }
}

/*
 *  SIOポートに対する割込み処理
 */
static void
mmuart_isr_siop(SIOPCB *p_siopcb)
{
//    uint32_t mmuart_isr;

    /*
     *  この時点の割込み要求の状態を保存
     *  この関数の最後に実施すると，関数内で処理していない要求をクリアする
     *  可能性があるため．
     */
//    mmuart_isr = sil_rew_mem(XUARTPS_ISR(p_siopcb->p_siopinib->base));

    (void)sil_rew_mem(MMUART_IIR(p_siopcb->p_siopinib->base));
                              
    if (mmuart_getready(p_siopcb->p_siopinib->base)) {
        /*
         *  受信通知コールバックルーチンを呼び出す．
         */
        mmuart_irdy_rcv(p_siopcb->exinf);
    }

    /*
     *  送信可能かつSIOドライバで送信したいデータがあるかぎり
     *  送信可能コールバックルーチンを呼び出す．
     */ 
//    while(mmuart_putready(p_siopcb->p_siopinib->base) &&
//          (sil_rew_mem(XUARTPS_IMR(p_siopcb->p_siopinib->base)) & XUARTPS_IXR_TXEMPTY)) {
    if (mmuart_putready(p_siopcb->p_siopinib->base)) {
        /*
         *  送信可能コールバックルーチンを呼び出す．
         */
        mmuart_irdy_snd(p_siopcb->exinf);
    }

    /*
     *  ペンディングしている割込みをクリア
     */
//    sil_wrw_mem(XUARTPS_ISR(p_siopcb->p_siopinib->base), mmuart_isr);
}

/*
 *  SIOの割込みサービスルーチン
 */
void
mmuart_isr(ID siopid)
{
    mmuart_isr_siop(get_siopcb(siopid));
}
