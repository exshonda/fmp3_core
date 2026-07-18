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
 *  $Id: core_kernel_impl.c 335 2023-04-18 10:50:40Z ertl-honda $
 */

/*
 *    カーネルのコア依存部（RISC-V用）
 */

#include "kernel_impl.h"
#include "check.h"
#include "task.h"
#include "riscv.h"

/*
 *  start.Sでの同期用変数
 */
volatile uint_t start_sync[TNUM_PRCID];

/*
 *  SILスピンロック用変数
 *
 *  スタートアップルーチンで，0に初期化されることを期待している．
 */
LOCK TOPPERS_sil_spn_var = { 0U };

/*
 *  ジャイアントロック
 */
LOCK giant_lock;

/*
 *  コア依存の初期化
 */
void
core_initialize(PCB *p_my_pcb)
{
    TPCB    *p_my_tpcb = &(p_my_pcb->target_pcb);

    /*
     *  カーネル起動時は非タスクコンテキストとして動作させるために，例外
     *  のネスト回数を1に初期化する．
     */ 
    p_my_tpcb->exncnt = 1U;

    /*
     *  非タスクコンテキスト用のスタックの初期値
     */
    p_my_tpcb->istkpt = istkpt_table[INDEX_PRC(p_my_pcb->prcid)];

    /*
     *  アイドル処理用のスタックの初期値
     */
    p_my_tpcb->idstkpt = idstkpt_table[INDEX_PRC(p_my_pcb->prcid)];

    /*
     *  CPU例外ハンドラテーブルへのポインタの初期化
     */
    p_my_tpcb->p_exc_tbl = p_exc_table[INDEX_PRC(p_my_pcb->prcid)];

    /*
     *  割込みハンドラテーブルへのポインタの初期化
     */
    p_my_tpcb->p_inh_tbl = p_inh_table[INDEX_PRC(p_my_pcb->prcid)];
}

/*
 *  コア依存の終了処理
 */
void
core_terminate(void)
{
}

/*
 *  CPU例外の発生状況のログ出力
 */
#ifndef OMIT_XLOG_SYS

/*
 *  32bitと64bitでのレジスタの出力フォーマット
 */
#if __riscv_xlen == 64
#define REGFMT  "0x%016lx"
#elif __riscv_xlen == 32
#define REGFMT  "0x%08x"
#endif /* __riscv_flen == 64 */

/*
 *  CPU例外ハンドラの中から，CPU例外情報ポインタ（p_excinf）を引数とし
 *  て呼び出すことで，CPU例外の発生状況をシステムログに出力する．
 */
void
xlog_sys(void *p_excinf)
{
    syslog_4(LOG_EMERG, "t0 = "REGFMT", t1 = "REGFMT", t2 = "REGFMT", t3 = "REGFMT"",
             ((T_EXCINF *)(p_excinf))->t0, ((T_EXCINF *)(p_excinf))->t1,
             ((T_EXCINF *)(p_excinf))->t2, ((T_EXCINF *)(p_excinf))->t3);

    syslog_3(LOG_EMERG, "t4 = "REGFMT", t5 = "REGFMT", t6 = "REGFMT"",
             ((T_EXCINF *)(p_excinf))->t4, ((T_EXCINF *)(p_excinf))->t5,
             ((T_EXCINF *)(p_excinf))->t6);
    
    syslog_4(LOG_EMERG, "a0 = "REGFMT", a1 = "REGFMT", a2 = "REGFMT", a3 = "REGFMT"",
             ((T_EXCINF *)(p_excinf))->a0, ((T_EXCINF *)(p_excinf))->a1,
             ((T_EXCINF *)(p_excinf))->a2, ((T_EXCINF *)(p_excinf))->a3);

    syslog_4(LOG_EMERG, "a4 = "REGFMT", a5 = "REGFMT", a6 = "REGFMT", a7 = "REGFMT"",
             ((T_EXCINF *)(p_excinf))->a4, ((T_EXCINF *)(p_excinf))->a5,
             ((T_EXCINF *)(p_excinf))->a6, ((T_EXCINF *)(p_excinf))->a7);

    syslog_2(LOG_EMERG, "ra = "REGFMT", tp = "REGFMT"",
             ((T_EXCINF *)(p_excinf))->ra, ((T_EXCINF *)(p_excinf))->tp);
    
    syslog_2(LOG_EMERG, "pc = "REGFMT", mstatus = "REGFMT"",
            ((T_EXCINF *)(p_excinf))->pc,
            ((T_EXCINF *)(p_excinf))->mstatus);
    
    syslog_2(LOG_EMERG, "intpri = %d, exncnt = %d",
             ((T_EXCINF *)(p_excinf))->intpri,
             ((T_EXCINF *)(p_excinf))->exncnt);    
}

#endif /* OMIT_XLOG_SYS */

/*
 *  未定義の割込みが入った場合の処理
 */
#ifndef OMIT_DEFAULT_INT_HANDLER

void
default_int_handler(INTNO intno)
{
    ID  prcid = ID_PRC(get_my_prcidx());
    
    syslog_1(LOG_EMERG, "PRC%d : Unregistered interrupt occurs.", prcid);
    syslog_2(LOG_EMERG, "PRC%d : Interrupt Number is 0x%x.", prcid, intno);
    ext_ker();
}

#endif /* OMIT_DEFAULT_INT_HANDLER */

/*
 *  未定義の例外が入った場合の処理
 */
#ifndef OMIT_DEFAULT_EXC_HANDLER

void
default_exc_handler(void *p_excinf, EXCNO excno)
{
    ID  prcid = ID_PRC(get_my_prcidx());

#ifdef OMIT_XLOG_SYS
    syslog_1(LOG_EMERG, "Unregistered exception %d occurs.", excno);
#else /* OMIT_XLOG_SYS */
    switch (excno) {
    case EXCNO_MISALIGNED_FETCH :
        syslog_1(LOG_EMERG, "PRC%d : Instruction address misaligned.", prcid);
        break;
    case EXCNO_FAULT_FETCH:
        syslog_1(LOG_EMERG, "PRC%d : Instruction access fault.", prcid);
        break;
    case EXCNO_IINST:
        syslog_1(LOG_EMERG, "PRC%d : Illegal instruction.", prcid);
        break;
    case EXCNO_BREAKPOINT:
        syslog_1(LOG_EMERG, "PRC%d : Breakpoint.", prcid);
        break;
    case EXCNO_MISALIGNED_LOAD:
        syslog_1(LOG_EMERG, "PRC%d : Load address misaligned.", prcid);
        break;
    case EXCNO_FAULT_LOAD:
        syslog_1(LOG_EMERG, "PRC%d : Load access fault.", prcid);
        break;
    case EXCNO_MISALIGNED_STORE:
        syslog_1(LOG_EMERG, "PRC%d : Store/AMO address misaligned.", prcid);
        break;
    case EXCNO_FAULT_STORE:
        syslog_1(LOG_EMERG, "PRC%d : Store/AMO access fault.", prcid);
        break;
    case EXCNO_USER_ECALL:
        syslog_1(LOG_EMERG, "PRC%d : Environment call from U-mode.", prcid);
        break;
    case EXCNO_SUPERVISOR_ECALL:
        syslog_1(LOG_EMERG, "PRC%d : Environment call from S-mode.", prcid);
        break;
    case EXCNO_HYPERVISOR_ECALL:
        syslog_1(LOG_EMERG, "PRC%d : Environment call from H-mode.", prcid);
        break;
    case EXCNO_MACHINE_ECALL:
        syslog_1(LOG_EMERG, "PRC%d : Environment call from M-mode.", prcid);
        break;
    case EXCNO_FETCH_PAGE_FAULT:
        syslog_1(LOG_EMERG, "PRC%d : Instruction page fault.", prcid);
        break;
    case EXCNO_LOAD_PAGE_FAULT:
        syslog_1(LOG_EMERG, "PRC%d : Load page fault.", prcid);
        break;
    case EXCNO_STORE_PAGE_FAULT:
        syslog_1(LOG_EMERG, "PRC%d : Store/AMO page fault.", prcid);
        break;        
        
    }
    xlog_sys(p_excinf);
#endif /* OMIT_XLOG_SYS */
    ext_ker();
}

#endif /* OMIT_DEFAULT_EXC_HANDLER */
