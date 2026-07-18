/*
 *  TOPPERS/FMP Kernel
 *      Toyohashi Open Platform for Embedded Real-Time Systems/
 *      Flexible MultiProcessor Kernel
 *
 *  Copyright (C) 2006-2020 by Embedded and Real-Time Systems Laboratory
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
 *  @(#) $Id: chip_kernel_impl.c 248 2020-07-09 06:43:11Z ertl-honda $
 */

/*
 *		カーネルのチップ依存部（IMX8MM用）
 */
#include "kernel_impl.h"

#ifdef TOPPERS_WITH_ATF
#include "atf.h"
#include "psci.h"
#endif /* TOPPERS_WITH_ATF */

/*
 *	前方参照
 */
#ifndef SYSMON
static void cpu_power_on(uint32_t core_id);
#endif /* SYSMON */

/*
 *  EL3で行う初期化処理
 */
void
chip_el3_initialize(void)
{
	volatile uint32_t scr;
	volatile uint32_t cpuectlr;
	volatile uint32_t reg32_val;

	SCR_EL3_READ(scr);
	scr &= ~(SCR_EA_BIT|SCR_FIQ_BIT|SCR_IRQ_BIT|SCR_NS_BIT);
	SCR_EL3_WRITE(scr);

	CPUECTLR_EL1_READ(cpuectlr);
	cpuectlr |= CPUECTLR_EL1_SMPEN;
	CPUECTLR_EL1_WRITE(cpuectlr);

	/*
	 *  GIC System register enable
	 */
	ICC_SRE_EL3_READ(reg32_val);
	reg32_val |= (uint32_t)((1 << 3) | (1 << 0));
	ICC_SRE_EL3_WRITE(reg32_val);
}

/*
 *  EL2で行う初期化処理
 */
void
chip_el2_initialize(void)
{
	volatile uint32_t   reg32_val;

	/*
	 *  GIC System register enable
	 */
	ICC_SRE_EL2_READ(reg32_val);
	reg32_val |= (uint32_t)((1 << 3) | (1 << 0));
	ICC_SRE_EL2_WRITE(reg32_val);

	/* CPUECTLR_EL1レジスタのNS-EL1からのアクセス許可 */
	ACTLR_EL2_READ(reg32_val);
	reg32_val |= (1 << 1);
	ACTLR_EL2_WRITE(reg32_val);

	/*
	 *  Generic Timerの初期化
	 */
	/* Physical Counter, Physical TimerをEL1NSとEL0NSからアクセス可能に */
	CNTHCTL_EL2_WRITE(CNTHCTL_EL1PCEN_BIT | CNTHCTL_EL1PCTEN_BIT);

	/* Virtual Counterのオフセットを0に */
	CNTVOFF_EL2_WRITE(0);

	inst_sync_barrier();
}

/*
 *  entry point (start.S)
 */
extern void start(void);

/*
 *  str_ker() の実行前にマスタプロセッサのみ実行される初期化処理
 */
void
chip_mprc_initialize(void)
{
#ifdef SYSMON
	uint32_t i;
#else
	uint64_t tmp_addr = (uint64_t)start;
#endif

	dcache_disable();
	icache_disable();

#ifdef SYSMON
#ifdef TOPPERS_WITH_ATF
#ifdef TOPPERS_TZ_S
	/*
	 *  ATFの BL32 (Secure-EL1 Payload) として動作させる場合
	 */
	/* ATFからのTOPPERS/FMPのエントリポイントを設定 */
	atf_smc_setvct(&atf_vector_table);

	/*
	 *  サブコア ON
	 */
	for (i = 1; i < TNUM_PRCID; i++) {
		atf_smc_cpuon(i);
	}

#else /* TOPPERS_TZ_NS */

	/*
	 *  ARM Trusted Firmware(None Secure) 使用時
	 */

	/*
	 *  サブコア ON
	 */
	for(i = 1; i < TNUM_PRCID; i++) {
		psci_smc_cpuon(i, (uint64_t)start, 0);
	}

#endif /* TOPPERS_TZ_S */
#endif /* TOPPERS_WITH_ATF */
#else /* SYSMON */
	/*
	 * SafeG64 未使用時
	 */
#if TNUM_PRCID >= 2
	/* Wake up processor 2 */
	sil_wrw_mem((void *)IMX8REG_SRC_GPR3, (uint32_t)((tmp_addr >> 24) & 0xffff));
	sil_wrw_mem((void *)IMX8REG_SRC_GPR4, (uint32_t)((tmp_addr >> 2) & 0x003fffff));

	cpu_power_on(1);
#endif /* TNUM_PRCID >= 2 */
#if TNUM_PRCID >= 3
	/* Wake up processor 3 */
	sil_wrw_mem((void *)IMX8REG_SRC_GPR5, (uint32_t)((tmp_addr >> 24) & 0xffff));
	sil_wrw_mem((void *)IMX8REG_SRC_GPR6, (uint32_t)((tmp_addr >> 2) & 0x003fffff));

	cpu_power_on(2);
#endif /* TNUM_PRCID >= 3 */
#if TNUM_PRCID >= 4
	/* Wake up processor 4 */
	sil_wrw_mem((void *)IMX8REG_SRC_GPR7, (uint32_t)((tmp_addr >> 24) & 0xffff));
	sil_wrw_mem((void *)IMX8REG_SRC_GPR8, (uint32_t)((tmp_addr >> 2) & 0x003fffff));

	cpu_power_on(3);
#endif /* TNUM_PRCID >= 4 */
#endif /* SYSMON */
	
#ifdef TOPPERS_BOOT_M4
	/*
	 *  Cortex-M4 On
	 */
	/* Enable ARM_M4_CLK ROOT */
	/* Enable Cortex-M4 */
	reg32_val = sil_rew_mem((void*)IMX8REG_SRC_M4RCR);
	sil_wrw_mem((void*)IMX8REG_SRC_M4RCR, reg32_val | (1 << 3));
	/* Reset Cortex-M4 core and platform */
	reg32_val = sil_rew_mem((void*)IMX8REG_SRC_M4RCR);
	sil_wrw_mem((void*)IMX8REG_SRC_M4RCR, reg32_val | (1 << 2) | (1 << 1));
	do {
		reg32_val = sil_rew_mem((void*)IMX8REG_SRC_M4RCR);
	} while( (reg32_val & ((1 << 2) | (1 << 1))) != 0 );
#endif /* TOPPERS_BOOT_M4 */

	core_mprc_initialize();
}

/*
 *  チップ依存の初期化
 */
void
chip_initialize(PCB *p_my_pcb)
{

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
	 *  コア依存の終了処理
	 */
	core_terminate();
}

#ifndef SYSMON
/*
 * サブコア電源ON処理
 */
static void
cpu_power_on(uint32_t core_id)
{
	uint32_t val;

	/* コアを無効に */
	val = sil_rew_mem((void *)IMX8REG_SRC_A53RCR1);
	val &= ~(1 << core_id);
	sil_wrw_mem((void *)IMX8REG_SRC_A53RCR1, val);

	/* GPC_PGC_nCTRL の PCR ビットをセット */
	val = sil_rew_mem((void *)(IMX8REG_GPC_PGC_nCTRL + (uintptr_t)(0x40 * core_id)));
	val |= (1 << 0);
	sil_wrw_mem((void *)(IMX8REG_GPC_PGC_nCTRL + (uintptr_t)(0x40 * core_id)), val);

	/* software powerup trigger ビットをセット */
	val = sil_rew_mem((void *)IMX8REG_GPC_CPU_PGC_SW_PUP_REQ);
	val |= (1 << core_id);
	sil_wrw_mem((void *)IMX8REG_GPC_CPU_PGC_SW_PUP_REQ, val);

	/* Power UP 完了まで待つ */
	while ((sil_rew_mem((void *)IMX8REG_GPC_CPU_PGC_SW_PUP_REQ) & (1 << core_id)) != 0);
		;
	/* GPC_PGC_nCTRL の PCR ビットをクリア */
		val = sil_rew_mem((void *)(IMX8REG_GPC_PGC_nCTRL + (uintptr_t)(0x40 * core_id)));
	val &= ~(1 << 0);
	sil_wrw_mem((void *)(IMX8REG_GPC_PGC_nCTRL + (uintptr_t)(0x40 * core_id)), val);

	/* コアを有効に */
	val = sil_rew_mem((void *)IMX8REG_SRC_A53RCR1);
	val |= (1 << core_id);
	sil_wrw_mem((void *)IMX8REG_SRC_A53RCR1, val);
}
#endif /* SYSMON */
