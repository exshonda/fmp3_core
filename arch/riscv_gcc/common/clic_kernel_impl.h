/*
 *  TOPPERS Software
 *      Toyohashi Open Platform for Embedded Real-Time Systems
 *
 *  Copyright (C) 2024-2026 by Embedded and Real-Time Systems Laboratory
 *              Graduate School of Information Science, Nagoya Univ., JAPAN
 *
 *  利用条件は TOPPERS ライセンス（plic_kernel_impl.h と同一）．無保証．
 */

/*
 *     カーネルの割込みコントローラ依存部（CLIC 用）
 *
 *  CLIC モードで動作するチップに共通．PLIC を持つチップの plic_kernel_impl.h に
 *  対応する．割込み許可/優先度/ペンディングは CLIC_INT_CTRL(line) レジスタで，
 *  割込み優先度マスクは CLIC_INT_THRESH で操作する（これらのレジスタ定義は
 *  チップ依存ヘッダが提供する）．
 *
 *  本ファイルは target_kernel_impl.h（またはそこからインクルードされる
 *  ファイル）のみからインクルードされる．
 *
 *  注: FMP3 の INTNO は CLIC 線番号と同一に扱う（チップ依存ヘッダ参照）．
 *      割込み優先度の内部表現 pri は CTL/THRESH の [31:24] にそのまま入れる
 *      8bit 値とする（レベル/優先度の割付は NLBITS に従う）．
 */

#ifndef TOPPERS_CLIC_KERNEL_IMPL_H
#define TOPPERS_CLIC_KERNEL_IMPL_H

#include <sil.h>
#include "riscv.h"
/*
 *  チップ依存ヘッダ(CLIC_INT_CTRL/THRESH 等のベースマクロ)は，本ヘッダを
 *  include する側(chip_kernel_impl.h)が先に include する(PLIC の
 *  plic_kernel_impl.h と同じ流儀．common 配置のため特定チップに依存しない)．
 */

/*
 *  RISC-Vコアで共通な定義
 */
#include "core_kernel_impl.h"

#ifndef TOPPERS_MACRO_ONLY

/*
 *  割込みターゲットテーブル（kernel_cfg.c, clic_kernel.trb が生成）
 *    割込み要求ラインの割込み先コンテキストINDEX．未割付は0xff．
 */
extern const uint8_t clic_target_cidx_table[CLIC_TNUM_INTNO + 1];

/*
 *  IPM ソフトウェアシャドウ（clic_kernel_impl.c で定義, コア別内部表現8bit）
 *    CLIC_INT_THRESH の read-back が保証されない実装に備え，IPM の真値はこちらで
 *    保持する（HW 非依存の防御的設計．詳細は clic_kernel_impl.c）．
 */
extern volatile uint8_t clic_ipm_shadow[TNUM_PRCID];

/*
 *  割込み優先度マスクの設定（priは内部表現, [31:24]）
 *    CLIC_INT_THRESH は自コアのメモリマップド閾値．cidx は使用しない．
 *    実マスク用に HW 閾値レジスタへ書込み(read-back付き)つつ，論理 IPM を
 *    シャドウへ記録する．
 */
Inline void
clic_set_context_priority(uint_t cidx, uint_t pri)
{
    (void) cidx;
    sil_wrw_mem(CLIC_INT_THRESH, (pri & 0xFFU) << CLIC_INT_THRESH_SHIFT);
    (void) sil_rew_mem(CLIC_INT_THRESH);   /* read-back */
    clic_ipm_shadow[get_my_prcidx()] = (uint8_t)(pri & 0xFFU);
}

/*
 *  割込み優先度マスクの取得（内部表現で返す）
 *    CLIC_INT_THRESH の read-back に依存せず，シャドウを真値として返す．
 */
Inline uint_t
clic_get_context_priority(uint_t cidx)
{
    (void) cidx;
    return((uint_t) clic_ipm_shadow[get_my_prcidx()]);
}

/*
 *  割込み禁止（CLIC_INT_CTRL の IE クリア）
 */
Inline void
clic_disable_int(INTNO intno)
{
    uint32_t val = sil_rew_mem(CLIC_INT_CTRL(intno));
    val &= ~CLIC_INT_IE_BIT;
    sil_swrw_mem(CLIC_INT_CTRL(intno), val);
}

/*
 *  割込み許可（CLIC_INT_CTRL の IE セット）
 */
Inline void
clic_enable_int(INTNO intno)
{
    uint32_t val = sil_rew_mem(CLIC_INT_CTRL(intno));
    val |= CLIC_INT_IE_BIT;
    sil_swrw_mem(CLIC_INT_CTRL(intno), val);
}

/*
 *  割込みペンディングのチェック（CLIC_INT_CTRL の IP）
 */
Inline bool_t
clic_probe_pending(INTNO intno)
{
    return((sil_rew_mem(CLIC_INT_CTRL(intno)) & CLIC_INT_IP_BIT) != 0U);
}

/*
 *  割込み要求ラインに対する割込み優先度の設定（priは内部表現, CTL[31:24]）
 */
Inline void
clic_set_priority(INTNO intno, uint_t pri)
{
    uint32_t val = sil_rew_mem(CLIC_INT_CTRL(intno));
    val &= ~(0xFFUL << CLIC_INT_CTL_SHIFT);
    val |= (pri & 0xFFU) << CLIC_INT_CTL_SHIFT;
    sil_swrw_mem(CLIC_INT_CTRL(intno), val);
}

/*
 *  割込み要求(ペンディング)のセット（CLIC_INT_CTRL の IP）
 *    software による割込み発生(ras_int)．IP を software で立てるには対象線が
 *    エッジtrigである必要があるため，エッジ設定とセットで使う(clic_set_edge)．
 */
Inline void
clic_raise_pending(INTNO intno)
{
    uint32_t val = sil_rew_mem(CLIC_INT_CTRL(intno));
    val |= CLIC_INT_IP_BIT;
    sil_swrw_mem(CLIC_INT_CTRL(intno), val);
}

/*
 *  割込み要求(ペンディング)のクリア（CLIC_INT_CTRL の IP）
 */
Inline void
clic_clear_pending(INTNO intno)
{
    uint32_t val = sil_rew_mem(CLIC_INT_CTRL(intno));
    val &= ~CLIC_INT_IP_BIT;
    sil_swrw_mem(CLIC_INT_CTRL(intno), val);
}

/*
 *  割込み線のトリガをエッジに設定（software で IP を立てて発生させるため）
 */
Inline void
clic_set_edge(INTNO intno)
{
    uint32_t val = sil_rew_mem(CLIC_INT_CTRL(intno));
    val |= CLIC_INT_TRIG_EDGE;
    sil_swrw_mem(CLIC_INT_CTRL(intno), val);
}

/*
 *  IPI 受信入口フック irc_begin_ipi の CLIC 実装（dispatch-IPI storm livelock
 *  breaker）．msi_ipi.c から呼ばれる．実体・根拠は clic_kernel_impl.c を参照．
 *  宣言は msi_ipi.c が持つ（extern void irc_begin_ipi(PCB *)）．
 */

/*
 *  CLIC のコンテキスト単位の初期化
 */
extern void clic_context_initialize(PCB *p_my_pcb);

/*
 *  CLIC のグローバルな初期化
 */
extern void clic_global_initialize(void);

/*
 *  割込み管理機能の初期化
 */
extern void clic_initialize_interrupt(PCB *p_my_pcb);

#endif /* TOPPERS_MACRO_ONLY */
#endif /* TOPPERS_CLIC_KERNEL_IMPL_H */
