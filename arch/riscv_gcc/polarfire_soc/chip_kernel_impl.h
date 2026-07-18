/*
 *  TOPPERS/FMP Kernel
 *      Toyohashi Open Platform for Embedded Real-Time Systems/
 *      Flexible MultiProcessor Kernel
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
 *  $Id: chip_kernel_impl.h 335 2023-04-18 10:50:40Z ertl-honda $
 */

/*
 *    kernel_impl.hのチップ依存部（PolarFire SoC 用）
 *
 *  このヘッダファイルは，target_kernel_impl.h（または，そこからインク
 *  ルードされるファイル）のみからインクルードされる．他のファイルから
 *  直接インクルードしてはならない．
 */

#ifndef TOPPERS_CHIP_KERNEL_IMPL_H
#define TOPPERS_CHIP_KERNEL_IMPL_H

/*
 *  PolarFire SoCのハードウェア資源の定義
 */
#include "polarfire_soc.h"

/*
 *  RISC-Vの定義
 */
#include "riscv.h"

/*
 *  デフォルトの非タスクコンテキスト用のスタック領域の定義
 */
#ifndef DEFAULT_ISTKSZ
#define DEFAULT_ISTKSZ  0x2000U    /* 8KB */
#endif /* DEFAULT_ISTKSZ */

/*
 *  デフォルトのアイドル処理用のスタック領域の定義
 */
#define DEFAULT_IDSTKSZ  0x0400U    /* 1KB */

#ifndef TOPPERS_MACRO_ONLY

/*
 *  各種IDの変換
 * 
 *  chip_asm.inc のアセンブラ用の各種IDの取得関数と内容を統一すること．
 *  plic_kernel.trbの生成スクリプトにも存在する 
 */

/*
 *  自プロセッサのインデックス（0オリジン）の取得
 */
Inline uint_t
get_my_prcidx(void)
{
    return(riscv_read_mhartid() - 1);
}

/*
 *  自プロセッサのPLICのコンテキストINDEXの取得（U54のみ）
 *   コンテキストINDEX = (hartid - 1) * 2 + 1
 */
Inline uint_t
get_my_plic_cidx(void)
{
    return(((riscv_read_mhartid() - 1) * 2) + 1);
}

#endif /* TOPPERS_MACRO_ONLY */

/*
 *  割込み番号の数，最小値と最大値
 */
#define TNUM_INTNO  PLIC_TNUM_INTNO
#define TMIN_INTNO  UINT_C(1)
#define TMAX_INTNO  PLIC_TNUM_INTNO

/*
 *  割込みハンドラ番号の数
 */
#define TNUM_INHNO  PLIC_TNUM_INTNO

/* 外部表現への変換 */
#define EXT_IPM(pri)  (-(PRI)(pri))

/* 内部表現への変換 */
#define INT_IPM(ipm)  ((uint_t)(-(ipm)))

/*
 *  割込み番号の範囲の判定
 */
#define VALID_INTNO(prcid, intno) \
  ((TMIN_INTNO <= INTNO_MASK(intno)) \
    && (INTNO_MASK(intno) <= TMAX_INTNO))

/*
 *  割込み要求ラインのための標準的な初期化情報を生成する
 */
#define USE_INTINIB_TABLE

/*
 *  割込み要求ライン設定テーブルを生成する
 */
#define USE_INTCFG_TABLE

/*
 *  PLIC依存部
 */
#include "plic_kernel_impl.h"

#ifndef TOPPERS_MACRO_ONLY

/*
 *  割込み属性の設定のチェック
 */
Inline bool_t
check_intno_cfg(INTNO intno)
{
    uint_t prcidx = get_my_prcidx();

    return((p_intcfg_table[prcidx])[INTNO_MASK(intno)] != 0U);
}

/*
 *  割込み優先度マスクの設定
 */
Inline void
t_set_ipm(PRI intpri)
{
    uint_t cidx = get_my_plic_cidx();
    
    plic_set_context_priority(cidx, INT_IPM(intpri));

    /*
     *  MSI/MTIの設定
     */
    if (intpri == TIPM_ENAALL) {
        riscv_set_mie(MIE_MSIE | MIE_MTIE);
    }
    else {
        riscv_clear_mie(MIE_MSIE | MIE_MTIE);
    }        
}

/*
 *  割込み優先度マスクの参照
 */
Inline PRI
t_get_ipm(void)
{
    uint_t cidx = get_my_plic_cidx();
    
    return(EXT_IPM(plic_get_context_priority(cidx)));
}

/*
 *  割込み要求禁止フラグが操作できる割込み番号の範囲の判定
 */
#define VALID_INTNO_DISINT(prcid, intno)  VALID_INTNO(prcid, intno)

/*
 *  割込み要求禁止フラグのセット
 */
Inline void
disable_int(INTNO intno)
{
    plic_disable_int(INTNO_MASK(intno));
}

/* 
 *  割込み要求禁止フラグのクリア
 */
Inline void
enable_int(INTNO intno)
{
    plic_enable_int(INTNO_MASK(intno));
}

/*
 *  割込み要求のチェック
 */
Inline bool_t
probe_int(INTNO intno)
{
    return(plic_probe_pending(INTNO_MASK(intno)));
}

/*
 *  チップ依存の初期化（マスタプロセッサ用）
 */
extern void chip_mprc_initialize(void);
     
/*
 *  チップ依存の初期化
 */
extern void chip_initialize(PCB *p_my_pcb);

/*
 *  チップ依存の終了処理
 */
extern void chip_terminate(void);

#endif /* TOPPERS_MACRO_ONLY */

/*
 *  追加コンテキスト保存/解放フック（コプロセッサ等を持たないため空）
 *    save_context: mig_tsk(移行時)，release_context: task_terminate(終了時)に呼ばれる．
 */
#define save_context(p_tcb)		((void)(p_tcb))
#define release_context(p_tcb)		((void)(p_tcb))

#endif /* TOPPERS_CHIP_KERNEL_IMPL_H */
