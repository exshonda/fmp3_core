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
 *  @(#) $Id: chip_kernel_impl.c 453 2026-05-06 05:01:16Z ertl-honda $
 */

/*
 *		カーネルのチップ依存部（ZynqMP用）
 */
#include "kernel_impl.h"

#ifdef TOPPERS_WITH_ATF
#include "atf.h"
#include "psci.h"
#endif /* TOPPERS_WITH_ATF */

/*
 *  EL3で行う初期化処理
 */
void
chip_el3_initialize(void)
{
	volatile uint32_t scr;
	volatile uint32_t cpuectlr;

	SCR_EL3_READ(scr);
	scr &= ~(SCR_EA_BIT|SCR_FIQ_BIT|SCR_IRQ_BIT|SCR_NS_BIT);
	SCR_EL3_WRITE(scr);

	CPUECTLR_EL1_READ(cpuectlr);
	cpuectlr |= CPUECTLR_EL1_SMPEN;
	CPUECTLR_EL1_WRITE(cpuectlr);

	/*
	 *  Enable System Timestamp Generator - Secure
	 */
	sil_wrw_mem((void*)XIOU_SCNTRS_CNT_CNTRL_REG,
				sil_rew_mem((void*)XIOU_SCNTRS_CNT_CNTRL_REG) & ~XIOU_SCNTRS_CNT_CNTRL_REG_EN);

	sil_wrw_mem((void*)XIOU_SCNTRS_FREQ_REG, XIOU_SCNTRS_FREQ_HZ);

	sil_wrw_mem((void*)XIOU_SCNTRS_CNT_CNTRL_REG,
				sil_rew_mem((void*)XIOU_SCNTRS_CNT_CNTRL_REG) | XIOU_SCNTRS_CNT_CNTRL_REG_EN);

	/* Initialize Generic Timer Freq */
	CNTFRQ_EL0_WRITE(XIOU_SCNTRS_FREQ_HZ);
}

/*
 *  EL2で行う初期化処理
 */
void
chip_el2_initialize(void)
{
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
	 *  FSBLから起動する場合
	 */
#if TNUM_PRCID >= 2    
	uint32_t mask = 0;
	uint32_t result;
#endif /* TNUM_PRCID >= 2 */
    
#if TNUM_PRCID >= 2
	sil_wrw_mem((void*)APU_RVBARADDR1H, 0x00);
	sil_wrw_mem((void*)APU_RVBARADDR1L, (uint32_t)((uint64_t)start));
	mask |= 0x02;
#if TNUM_PRCID >= 3
	sil_wrw_mem((void*)APU_RVBARADDR2H, 0x00);
	sil_wrw_mem((void*)APU_RVBARADDR2L, (uint32_t)((uint64_t)start));
	mask |= 0x04;
#endif /* TNUM_PRCID == 3 */
#if TNUM_PRCID >= 4
	sil_wrw_mem((void*)APU_RVBARADDR3H, 0x00);
	sil_wrw_mem((void*)APU_RVBARADDR3L, (uint32_t)((uint64_t)start));
	mask |= 0x08;
#endif /* TNUM_PRCID == 4 */

	/* Power Up */
	sil_wrw_mem((void*)PMU_GLOBAL_REQ_PWRUP_INT_EN, mask|PMU_GLOBAL_PWR_STATE_FP_MASK|PMU_GLOBAL_PWR_STATE_L2_BANK0_MASK);
	sil_wrw_mem((void*)PMU_GLOBAL_REQ_PWRUP_TRIG, mask|PMU_GLOBAL_PWR_STATE_FP_MASK|PMU_GLOBAL_PWR_STATE_L2_BANK0_MASK);
	do{
		result = sil_rew_mem((void*)PMU_GLOBAL_REQ_PWRUP_STATUS);
		result &= mask|PMU_GLOBAL_PWR_STATE_FP_MASK|PMU_GLOBAL_PWR_STATE_L2_BANK0_MASK;
	}while(result != 0x00U);

	/* Set AArch64 and Vector is 0x00000000 */
	sil_wrw_mem((void*)APU_CONFIG_0, (sil_rew_mem((void*)APU_CONFIG_0) & ~(APU_VINITHI_MASK)) | mask);

	/* Enable clock */
	sil_wrw_mem((void*)CRF_APB_ACPU_CTRL,
				sil_rew_mem((void*)CRF_APB_ACPU_CTRL)
				| (CRF_APB_ACPU_CTRL_CLKACT_FULL_MASK | CRF_APB_ACPU_CTRL_CLKACT_HALF_MASK));

	sil_wrw_mem((void*)CRF_APB_RST_FPD_APU,
				sil_rew_mem((void*)CRF_APB_RST_FPD_APU)
				& ~(CRF_APB_RST_FPD_APU_APU_L2_RESET_MASK|mask|(mask << CRF_APB_RST_FPD_APU_ACPU0_PWRON_OFFSET)));
#endif /* TNUM_PRCID >= 2 */
#endif /* SYSMON */

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
