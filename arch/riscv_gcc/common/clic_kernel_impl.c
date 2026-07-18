/*
 *  TOPPERS/FMP Kernel
 *      Flexible MultiProcessor Kernel
 *
 *  Copyright (C) 2024-2026 by Embedded and Real-Time Systems Laboratory
 *              Graduate School of Information Science, Nagoya Univ., JAPAN
 *
 *  利用条件は TOPPERS ライセンス（plic_kernel_impl.c と同一）．無保証．
 */

/*
 *    カーネルの割込みコントローラ依存部（CLIC 用）
 *    CLIC モードで動作するチップに共通．PLIC を持つチップの plic_kernel_impl.c に
 *    対応する．チップ依存のレジスタ定義(CLIC_INT_CTRL/THRESH 等)はチップ依存
 *    ヘッダが提供する．
 */

#include "kernel_impl.h"
#include "interrupt.h"
#include <sil.h>
#include "riscv.h"

/*
 *  割込み優先度マスク(IPM)のソフトウェアシャドウ（コア別, 内部表現8bit）
 *
 *  CLIC の閾値レジスタ CLIC_INT_THRESH は read-back が保証されない実装があり
 *  (ソフトが書いた閾値をそのまま読み戻せず，ライブ状態を返す等)，HW 読戻しに
 *  依存すると t_get_ipm が誤った IPM を返してディスパッチ可否判定を誤らせ得る．
 *  そこで IPM の真値はこのソフトウェアシャドウで管理し，t_get_ipm はシャドウを
 *  返す．HW 閾値レジスタへの書込み(=実マスク)は従来通り行うため割込みマスク動作は
 *  不変．シャドウは t_set_ipm 経路でのみ更新する(irc_begin/end の一時的な閾値変更は
 *  タスクの論理 IPM を変えないため対象外)．read-back が安定なチップでも本シャドウは
 *  冗長なだけで無害(HW 非依存の防御的設計)．
 */
volatile uint8_t clic_ipm_shadow[TNUM_PRCID];

/*
 *  注: IPI 受信入口フック irc_begin_ipi（dispatch-IPI storm livelock breaker 等の
 *  IPI 入口処理）は，必要性・チューニングがチップ依存のためチップ依存部で提供する
 *  （ESP32-P4 は chip_kernel_impl.c で実装．発生しないチップは空の irc_begin_ipi）．
 */

/*
 *  CLIC のコンテキスト単位の初期化
 *    割込み優先度マスクを最低（全通過＝0）に設定し，自コア向け割込みを全禁止．
 */
void
clic_context_initialize(PCB *p_my_pcb)
{
    uint_t intno;

    (void) p_my_pcb;

    /*  割込み優先度マスクを最低優先度に設定（0＝全通過）  */
    clic_set_context_priority(0U, 0U);

    /*
     *  全 CLIC 線を禁止し，優先度/ペンディングをクリア．
     *  CLIC_TNUM_INTNO は線「数」(有効線は 0..CLIC_TNUM_INTNO-1)．'<' で回し，
     *  範囲外 CLIC_INT_CTRL(CLIC_TNUM_INTNO) への OOB MMIO 書込みを避ける
     *  (TMAX_INTNO の off-by-one と同種．chip_kernel_impl.h 参照)．
     */
    for (intno = 0; intno < CLIC_TNUM_INTNO; intno++) {
        sil_swrw_mem(CLIC_INT_CTRL(intno), 0U);
    }
}

/*
 *  CLIC のグローバルな初期化
 *    マスタプロセッサのみから，他コア開始前に呼ばれる．
 */
void
clic_global_initialize(void)
{
    /*
     *  CLIC のグローバル設定．NLBITS の設定等はチップ既定値を使用する．
     *  TODO(要HW確認): CLIC のモード/NLBITS レジスタ設定が必要なら追加．
     */
    clic_context_initialize(NULL);
}

#ifndef OMIT_CLIC_INITIALIZE_INTERRUPT

/*
 *  割込み要求ラインの属性の設定
 */
Inline void
clic_config_int(PCB *p_my_pcb, INTNO intno, ATR intatr, PRI intpri, uint_t affinity)
{
    SIL_PRE_LOC;

    (void) affinity;
    assert(VALID_INTNO(p_my_pcb->prcid, intno));
    assert(TMIN_INTPRI <= intpri && intpri <= TMAX_INTPRI);

    SIL_LOC_SPN();

    /*  割込み優先度を設定  */
    clic_set_priority(INTNO_MASK(intno), INT_IPM(intpri));

    /*
     *  TA_EDGE 指定の割込みはエッジトリガに設定する．
     *  software による割込み発生(ras_int=IP セット)はエッジトリガ線でのみ
     *  ラッチされるため，テスト用割込み等で使う．
     */
    if ((intatr & TA_EDGE) != 0U) {
        clic_set_edge(INTNO_MASK(intno));
    }

    /*  割込みを許可  */
    if ((intatr & TA_ENAINT) != 0U) {
        clic_enable_int(INTNO_MASK(intno));
    }

    SIL_UNL_SPN();
}

/*
 *  割込み管理機能の初期化
 */
void
clic_initialize_interrupt(PCB *p_my_pcb)
{
    uint_t loop;
    const INTINIB *p_intinib;

    for (loop = 0; loop < tnum_cfg_intno; loop++) {
        p_intinib = &(intinib_table[loop]);
        if (p_intinib->iprcid == p_my_pcb->prcid) {
            clic_config_int(p_my_pcb, p_intinib->intno, p_intinib->intatr,
                            p_intinib->intpri, p_intinib->affinity);
        }
    }
}

#endif /* OMIT_CLIC_INITIALIZE_INTERRUPT */
