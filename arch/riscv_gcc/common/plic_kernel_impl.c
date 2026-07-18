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
 *  $Id: gic_kernel_impl.c 376 2023-09-02 04:34:49Z ertl-honda $
 */

/*
 *    カーネルの割込みコントローラ依存部（PLIC用）
 */

#include "kernel_impl.h"
#include "interrupt.h"
#include <sil.h>
#include "riscv.h"

/*
 *  PLICのcontext単位の初期化
 */
void
plic_context_initialize(PCB *p_my_pcb)
{
    uint_t loop;    
    INTNO intno;
    uint_t cidx = get_my_plic_cidx();
    
    /*
     *  割込み優先度マスクを最低優先度に設定
     */
    plic_set_context_priority(cidx, 0U);
    
   /*
    *  自コア向けの割込みを全て禁止
    */
    for (loop = 0; loop < (PLIC_TNUM_INTNO / 32); loop++) {
        sil_swrw_mem(PLIC_IEM(cidx, loop), 0U);
    }
           
    /*
     *  アクティブな割込みがあればクリア
     */    
    intno = sil_rew_mem(PLIC_CC(cidx));
    
    if (intno != 0) {
        sil_swrw_mem(PLIC_CC(cidx), intno);
    }
}

/*
 *  PLICのグローバルな初期化
 * 
 *  この関数は，他のプロセッサが実行を開始する前に，マスタプロセッサの
 *  みから呼び出されるため，プロセッサ間排他制御は必要ない．
 */
void
plic_global_initialize(void)
{
    uint_t loop;
    uint_t cidx = get_my_plic_cidx();

    /*
     *  自プロセサ向けの全ての割込みを禁止
     */
    for (loop = 0; loop <= (PLIC_TNUM_INTNO/32); loop++) {
        sil_swrw_mem(PLIC_IEM(cidx, loop), 0x00000000U);
    }    

    /*
     *  すべての割込みを最低優先度に設定
     */
    for (loop = 1; loop < PLIC_TNUM_INTNO; loop++) {
        sil_swrw_mem(PLIC_IPRI(loop), 0U);
    }
}

#ifndef OMIT_PLIC_INITIALIZE_INTERRUPT

/*
 *  割込み要求ラインの属性の設定
 *
 *  FMP3カーネルでの利用を想定して，パラメータエラーはアサーションで
 *  チェックしている．
 */
Inline void
plic_config_int(PCB *p_my_pcb, INTNO intno, ATR intatr, PRI intpri, uint_t affinity)
{
    SIL_PRE_LOC;

    assert(VALID_INTNO(p_my_pcb->prcid, intno));
    assert(TMIN_INTPRI <= intpri && intpri <= TMAX_INTPRI);
    
    /*
     *  SILスピンロックを取得
     */
    SIL_LOC_SPN();
    
    /*
     *  割込み優先度を設定
     */
    plic_set_priority(INTNO_MASK(intno), INT_IPM(intpri));

    /*
     * 割込みを許可
     */
    if ((intatr & TA_ENAINT) != 0U) {
        plic_enable_int(intno);

    }

    /*
     *  SILスピンロックを解放
     */
    SIL_UNL_SPN();
}

/*
 *  割込み管理機能の初期化
 */
void
plic_initialize_interrupt(PCB *p_my_pcb)
{
    uint_t loop;
    const INTINIB  *p_intinib;

    for (loop = 0; loop < tnum_cfg_intno; loop++) {
        p_intinib = &(intinib_table[loop]);
        if (p_intinib->iprcid == p_my_pcb->prcid) {
            plic_config_int(p_my_pcb, p_intinib->intno, p_intinib->intatr,
                            p_intinib->intpri, p_intinib->affinity);
        }
    }
}

#endif /* OMIT_PLIC_INITIALIZE_INTERRUPT */
