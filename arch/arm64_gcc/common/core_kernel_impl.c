/*
 *  TOPPERS/FMP Kernel
 *      Toyohashi Open Platform for Embedded Real-Time Systems/
 *      Flexible MultiProcessor Kernel
 *
 *  Copyright (C) 2000-2003 by Embedded and Real-Time Systems Laboratory
 *                              Toyohashi Univ. of Technology, JAPAN
 *  Copyright (C) 2006-2025 by Embedded and Real-Time Systems Laboratory
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
 *  @(#) $Id: core_kernel_impl.c 458 2026-05-06 06:01:33Z ertl-honda $
 */

/*
 *		カーネルのコア依存部（ARM64用）
 */

#include "kernel_impl.h"
#include "check.h"
#include "task.h"
#include "arm64.h"
#include "gic_ipi.h"

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
 *  str_ker() の実行前にマスタプロセッサのみ実行される初期化処理
 */
void
core_mprc_initialize(void)
{
	/*
	 *  GIC Distributor初期化
	 */
	gicd_initialize();

	/*
	 *  SILのスピンロック用変数の初期化
	 */
	TOPPERS_sil_spn_var[0] = 0U;
}

/*
 *  パフォーマンスモニタによる性能評価
 */
#ifdef USE_ARM64_PMCNT

/*
 *  パフォーマンスモニタの初期化
 */
Inline void
arm64_pmcnt_initialize(void)
{
	volatile uint64_t	reg;

	/*
	 *  全カウンターの有効化
	 */
	PMCR_EL0_READ(reg);
	reg |= PMCR_LC_BIT | PMCR_E_BIT;
	PMCR_EL0_WRITE(reg);

	INST_SYNC_BARRIER();
	DATA_SYNC_BARRIER();

	/*
	 *  パフォーマンスカウンタの有効化
	 */
	PMCNTENSET_EL0_WRITE(PMCNTENSET_C_BIT);

	INST_SYNC_BARRIER();
	DATA_SYNC_BARRIER();

	/*
	 *  全カウンターのリセット
	 */
	PMCR_EL0_WRITE(reg | PMCR_C_BIT);

	INST_SYNC_BARRIER();
	DATA_SYNC_BARRIER();
}

#endif /* USE_ARM64_PMCNT */

/*
 *  プロセッサ依存の初期化
 */
void
core_initialize(PCB *p_my_pcb)
{
	TPCB	*p_my_tpcb = &(p_my_pcb->target_pcb);
	uint32_t tmp;

#ifdef USE_THREAD_ID_PCB
	/*
	 *  Thread IDレジスタへのPCBへのポインタの設定
	 */
	TPIDR_EL1_WRITE((uint64_t) p_my_pcb);
#endif /* USE_THREAD_ID_PCB */

	/*
	 *  カーネル起動時は非タスクコンテキストとして動作させるために，例外
	 *  のネスト回数を1に初期化する．
	 */ 
	p_my_tpcb->excpt_nest_count = 1U;

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

	/*
	 *  例外ベクタテーブルをVECTOR BASE ADDRESS レジスタに設定する
	 */
	extern void *vector_table;
	VBAR_EL1_WRITE((uint64_t)&vector_table);

	/*
	 *  アラインメントチェックの設定
	 */
#ifdef ENABLE_ALIGNMENT_CHECK
	SCTLR_EL1_READ(tmp);
	SCTLR_EL1_WRITE(tmp | SCTLR_SA_BIT | SCTLR_A_BIT);
#else /* ENABLE_ALIGNMENT_CHECK */
	SCTLR_EL1_READ(tmp);
	SCTLR_EL1_WRITE(tmp & ~(SCTLR_SA_BIT | SCTLR_A_BIT));
#endif /* ENABLE_ALIGNMENT_CHECK */
	inst_sync_barrier();

	/*
	 *  キャッシュを無効にする前にフラッシュする
	 */
	cache_flush();
    
	/*
	 *  キャッシュを無効に
	 */
	cache_disable();

#if defined(TOPPERS_CORTEX_A53) || defined(TOPPERS_CORTEX_A57)
	/*
	 *  SMPモードに設定
	 */
	enable_smp();
#endif /* defined(TOPPERS_CORTEX_A53) || defined(TOPPERS_CORTEX_A57) */

	/*
	 *  MMUを有効に
	 */
	mmu_init();

	/*
	 *  キャッシュを有効に
	 */
	cache_enable();

	/*
	 *  GIC初期化
	 */
	gic_init();

	/*
	 *  パフォーマンスモニタの初期化
	 */
#ifdef USE_ARM64_PMCNT
	arm64_pmcnt_initialize();
#endif /* USE_ARM64_PMCNT */
}

/*
 *  コア依存の終了処理
 */
void
core_terminate(void)
{
	/*
	 *  GICのCPUインタフェースを停止
	 */
	gicc_stop();

	/*
	 *  GIC Distributorを停止
	 */
	gicd_terminate();
}

/*
 *  CPU例外の発生状況のログ出力
 */
#ifndef OMIT_XLOG_SYS

/*
 *  CPU例外ハンドラの中から，CPU例外情報ポインタ（p_excinf）を引数とし
 *  て呼び出すことで，CPU例外の発生状況をシステムログに出力する．
 */
void
xlog_sys(void *p_excinf)
{
	T_EXCINF *p_exc_frame = (T_EXCINF*)p_excinf;
	ID prcid = ID_PRC(get_my_prcidx());

	syslog_3(LOG_EMERG, "Processor %d : nest_count = %d, intpri = %d.",
			 prcid, (int)p_exc_frame->nest_count, (int)p_exc_frame->intpri);
	syslog_3(LOG_EMERG, "Processor %d : pc = 0x%016lx, pstate = 0x%08x",
			 prcid, p_exc_frame->pc, (uint32_t)p_exc_frame->pstate);
	syslog_3(LOG_EMERG, "Processor %d : x0 = 0x%016lx, x1 = 0x%016lx",
			 prcid, p_exc_frame->x0, p_exc_frame->x1);
	syslog_3(LOG_EMERG, "Processor %d : x2 = 0x%016lx, x3 = 0x%016lx",
			 prcid, p_exc_frame->x2, p_exc_frame->x3);
	syslog_3(LOG_EMERG, "Processor %d : x4 = 0x%016lx, x5 = 0x%016lx",
			 prcid, p_exc_frame->x4, p_exc_frame->x5);
	syslog_3(LOG_EMERG, "Processor %d : x6 = 0x%016lx, x7 = 0x%016lx",
			 prcid, p_exc_frame->x6, p_exc_frame->x7);
	syslog_3(LOG_EMERG, "Processor %d : x8 = 0x%016lx, x9 = 0x%016lx",
			 prcid, p_exc_frame->x8, p_exc_frame->x9);
	syslog_3(LOG_EMERG, "Processor %d : x10 = 0x%016lx, x11 = 0x%016lx",
			 prcid, p_exc_frame->x10, p_exc_frame->x11);
	syslog_3(LOG_EMERG, "Processor %d : x12 = 0x%016lx, x13 = 0x%016lx",
			 prcid, p_exc_frame->x12, p_exc_frame->x13);
	syslog_3(LOG_EMERG, "Processor %d : x14 = 0x%016lx, x15 = 0x%016lx",
			 prcid, p_exc_frame->x14, p_exc_frame->x15);
	syslog_3(LOG_EMERG, "Processor %d : x16 = 0x%016lx, x17 = 0x%016lx",
			 prcid, p_exc_frame->x16, p_exc_frame->x17);
	syslog_3(LOG_EMERG, "Processor %d : x18 = 0x%016lx, x19 = 0x%016lx",
			 prcid, p_exc_frame->x18, p_exc_frame->x19);
	syslog_3(LOG_EMERG, "Processor %d : x20 = 0x%016lx, x21 = 0x%016lx",
			 prcid, p_exc_frame->x20, p_exc_frame->x21);
	syslog_3(LOG_EMERG, "Processor %d : x22 = 0x%016lx, x23 = 0x%016lx",
			 prcid, p_exc_frame->x22, p_exc_frame->x23);
	syslog_3(LOG_EMERG, "Processor %d : x24 = 0x%016lx, x25 = 0x%016lx",
			 prcid, p_exc_frame->x24, p_exc_frame->x25);
	syslog_3(LOG_EMERG, "Processor %d : x26 = 0x%016lx, x27 = 0x%016lx",
			 prcid, p_exc_frame->x26, p_exc_frame->x27);
	syslog_3(LOG_EMERG, "Processor %d : x28 = 0x%016lx, x29 = 0x%016lx",
			 prcid, p_exc_frame->x28, p_exc_frame->x29);
	syslog_3(LOG_EMERG, "Processor %d : x30(lr) = 0x%016lx, sp = 0x%016lx",
			 prcid, p_exc_frame->x30, p_exc_frame->sp);
}
#endif /* OMIT_XLOG_SYS */

#ifndef OMIT_DEFAULT_INT_HANDLER

/*
 *  未定義の割込みが入った場合の処理
 */
void
default_int_handler(uint32_t intno){
	ID prcid = ID_PRC(get_my_prcidx());

	syslog_2(LOG_EMERG, "Processor %d : Unregistered Interrupt occurs at %d.", prcid, intno);
	ext_ker();
}

#endif /* OMIT_DEFAULT_INT_HANDLER */

#ifndef OMIT_DEFAULT_EXC_HANDLER

void
default_exc_handler(void *p_excinf, EXCNO excno)
{
	ID prcid = ID_PRC(get_my_prcidx());
	volatile uint32_t	esr_el1;

#ifdef OMIT_XLOG_SYS
	syslog_1(LOG_EMERG, "\nProcessor %d : Unregistered exception %d occurs.", prcid, excno);
#else /* OMIT_XLOG_SYS */

	switch (excno) {
	  case EXCNO_CUR_SP0_SYNC:
		syslog_1(LOG_EMERG, "Processor %d : Synchronous exception with current EL with SP0", prcid);
		break;
	  case EXCNO_CUR_SP0_IRQ:
		syslog_1(LOG_EMERG, "Processor %d : IRQ exception with current EL with SP0", prcid);
		break;
	  case EXCNO_CUR_SP0_FIQ:
		syslog_1(LOG_EMERG, "Processor %d : FIQ exception with current EL with SP0", prcid);
		break;
	  case EXCNO_CUR_SP0_SERR:
		syslog_1(LOG_EMERG, "Processor %d : SError exception with current EL with SP0", prcid);
		break;
	  case EXCNO_CUR_SPX_SYNC:
		syslog_1(LOG_EMERG, "Processor %d : Synchronous exception with current EL with SPx", prcid);
		break;
	  case EXCNO_CUR_SPX_IRQ:
		syslog_1(LOG_EMERG, "Processor %d : IRQ exception with current EL with SPx", prcid);
		break;
	  case EXCNO_CUR_SPX_FIQ:
		syslog_1(LOG_EMERG, "Processor %d : FIQ exception with current EL with SPx", prcid);
		break;
	  case EXCNO_CUR_SPX_SERR:
		syslog_1(LOG_EMERG, "Processor %d : SError exception with current EL with SPx", prcid);
		break;
	  case EXCNO_L64_SYNC:
		syslog_1(LOG_EMERG, "Processor %d : Synchronous exception with lower EL using AArch64", prcid);
		break;
	  case EXCNO_L64_IRQ:
		syslog_1(LOG_EMERG, "Processor %d : IRQ exception with lower EL using AArch64", prcid);
		break;
	  case EXCNO_L64_FIQ:
		syslog_1(LOG_EMERG, "Processor %d : FIQ exception with lower EL using AArch64", prcid);
		break;
	  case EXCNO_L64_SERR:
		syslog_1(LOG_EMERG, "Processor %d : SError exception with lower EL using AArch64", prcid);
		break;
	  case EXCNO_L32_SYNC:
		syslog_1(LOG_EMERG, "Processor %d : Synchronous exception with lower EL using AArch32", prcid);
		break;
	  case EXCNO_L32_IRQ:
		syslog_1(LOG_EMERG, "Processor %d : IRQ exception with lower EL using AArch32", prcid);
		break;
	  case EXCNO_L32_FIQ:
		syslog_1(LOG_EMERG, "Processor %d : FIQ exception with lower EL using AArch32", prcid);
		break;
	  case EXCNO_L32_SERR:
		syslog_1(LOG_EMERG, "Processor %d : SError exception with lower EL using AArch32", prcid);
		break;
	}

	Asm("mrs %0, esr_el1":"=r"(esr_el1));
	syslog_2(LOG_EMERG, "Processor %d : ESR_EL1.EC  = 0x%06x", prcid, esr_el1 >> 26);
	syslog_2(LOG_EMERG, "Processor %d : ESR_EL1.IL  = 0x%x", prcid, (esr_el1 >> 25) & 0x01);
	syslog_2(LOG_EMERG, "Processor %d : ESR_EL1.ISS = 0x%x", prcid, esr_el1 & 0x1ffffff);

	xlog_sys(p_excinf);
#endif /* OMIT_XLOG_SYS */
	ext_ker();
}
#endif /* OMIT_DEFAULT_EXC_HANDLER */
