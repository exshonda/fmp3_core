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
 *  $Id: chip_kernel_impl.c 335 2023-04-18 10:50:40Z ertl-honda $
 */

/*
 *    カーネルのチップ依存部（PolarFire SoC 用）
 */

#include "kernel_impl.h"
#include "mtimer.h"

/*
 *  チップ依存の初期化（マスタプロセッサ用）
 */
void
chip_mprc_initialize(void)
{
    plic_global_initialize();
}

/*
 *  チップ依存の初期化
 */
void
chip_initialize(PCB *p_my_pcb)
{
    extern void *trap_vector_table;

    /*
     *  Machine Trap-Vector Base の設定
     */
    riscv_write_mtvec((ulong_t)&trap_vector_table | MTVEC_MODE_VECTORD);

    /*
     *  Machine Software Interrupt の設定
     */

    /*
     *  Machine Timer の設定
     */
    target_hrt_initialize(0);

    /*
     *  Machine External Interrupt の設定
     */
    plic_context_initialize(p_my_pcb);

    /*
     *  MSI/MTI/MEIの許可
     */
    riscv_set_mie(MIE_MSIE | MIE_MTIE | MIE_MEIE);

    /*
     *  コア依存の初期化
     */
    core_initialize(p_my_pcb);
}

/*
 *  チップ依存の終了処理
 */
void
chip_terminate(void)
{
    /*
     *  Machine Timer の終了処理
     */
    target_hrt_terminate(0);
    
    /*
     *  コア依存の終了処理
     */
    core_terminate();
}

/*
 *  割込み管理機能の初期化
 */
void
initialize_interrupt(PCB *p_my_pcb)
{
    plic_initialize_interrupt(p_my_pcb);
}

/*
 *  IPI(MSI)受信入口フック（msi_ipi.c から clear_msip の前に呼ばれる）。
 *  PLIC 構成では追加処理は不要のため空とする。
 */
void
irc_begin_ipi(PCB *p_my_pcb)
{
    (void) p_my_pcb;
}
