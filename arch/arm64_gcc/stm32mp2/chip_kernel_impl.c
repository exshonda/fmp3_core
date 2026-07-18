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
 */

/*
 *		カーネルのチップ依存部（STM32MP2用）
 *
 *  STM32MP2 は TF-A(BAREMETAL_IMAGE_LOADER) により EL3 セキュアで起動さ
 *  れる．BL31/PSCI ランタイムは存在しないため，サブコアの起動は PSCI で
 *  はなく EL3 で直接行う（StepM で実装）．
 */
#include "kernel_impl.h"

/*
 *  entry point (start.S)
 */
extern void start(void);

/*
 *  EL3で行う初期化処理
 */
void
chip_el3_initialize(void)
{
	volatile uint32_t scr;
	volatile uint32_t cpuectlr;

	/*
	 *  例外・割込みを下位ELで処理し，セキュア状態を維持する．
	 */
	SCR_EL3_READ(scr);
	scr &= ~(SCR_EA_BIT | SCR_FIQ_BIT | SCR_IRQ_BIT | SCR_NS_BIT);
	SCR_EL3_WRITE(scr);

	/*
	 *  SMP コヒーレンシの有効化
	 */
	CPUECTLR_EL1_READ(cpuectlr);
	cpuectlr |= CPUECTLR_EL1_SMPEN;
	CPUECTLR_EL1_WRITE(cpuectlr);

	/*
	 *  CPTR_EL3 のトラップを解除する．
	 *
	 *  TF-A(FSBL) は CPTR_EL3 の TCPAC(bit31)/TAM(bit30)/TTA(bit20)/
	 *  TFP(bit10) をセットした状態でベアメタルイメージへ制御を渡す．
	 *  この状態のまま EL1 へドロップすると，start.S の FPU 有効化
	 *  (mrs/msr cpacr_el1) や FP/SIMD 命令が EL3 へトラップ(EC=0x18)
	 *  されてしまう．EL1 から CPACR/FP を使用できるようトラップを解除する．
	 */
	{
		volatile uint64_t cptr;
		__asm__ volatile("mrs %0, cptr_el3" : "=r"(cptr));
		cptr &= ~(((uint64_t)1 << 31) | ((uint64_t)1 << 30)
				  | ((uint64_t)1 << 20) | ((uint64_t)1 << 10));
		__asm__ volatile("msr cptr_el3, %0" :: "r"(cptr));
		__asm__ volatile("isb");
	}

#if TOPPERS_GIC_VER >= 3
	{
		volatile uint32_t reg32_val;
		/*  GIC System register interface の有効化（GICv3/4のみ） */
		ICC_SRE_EL3_READ(reg32_val);
		reg32_val |= (uint32_t)((1 << 3) | (1 << 0));
		ICC_SRE_EL3_WRITE(reg32_val);
	}
#endif /* TOPPERS_GIC_VER >= 3 */
}

/*
 *  EL2で行う初期化処理
 *
 *  TZ_S では start.S が EL3 から EL1 へ直接ドロップするため，本処理は
 *  呼ばれない（TZ_NS 構成のための保持）．
 */
void
chip_el2_initialize(void)
{
	volatile uint32_t   reg32_val;

#if TOPPERS_GIC_VER >= 3
	/*  GIC System register interface の有効化（GICv3/4のみ） */
	ICC_SRE_EL2_READ(reg32_val);
	reg32_val |= (uint32_t)((1 << 3) | (1 << 0));
	ICC_SRE_EL2_WRITE(reg32_val);
#endif /* TOPPERS_GIC_VER >= 3 */

	/* CPUECTLR_EL1レジスタのNS-EL1からのアクセス許可 */
	ACTLR_EL2_READ(reg32_val);
	reg32_val |= (1 << 1);
	ACTLR_EL2_WRITE(reg32_val);

	/*
	 *  Generic Timerの初期化
	 *  Physical Counter, Physical Timerを EL1/EL0 からアクセス可能に
	 */
	CNTHCTL_EL2_WRITE(CNTHCTL_EL1PCEN_BIT | CNTHCTL_EL1PCTEN_BIT);
	CNTVOFF_EL2_WRITE(0);

	inst_sync_barrier();
}

/*
 *  str_ker() の実行前にマスタプロセッサのみ実行される初期化処理
 */
void
chip_mprc_initialize(void)
{
	dcache_disable();
	icache_disable();

#if TNUM_PRCID >= 2
	/*
	 *  サブコア(Core1 = a35_1)の起動．
	 *
	 *  STM32MP2 には BL31/PSCI ランタイムが無いため，CA35SYSCFG の 64bit
	 *  リセットベクタ(VBAR_CR)に start.S のエントリを設定し，RCC のコア
	 *  リセット(C1P1RST)を解除して直接起動する．Core1 は EL3 から start.S
	 *  を実行し，start_el1 の my_core_index 判定で slave_wait に入り，
	 *  マスタが start_sync を設定するのを待って sta_ker へ合流する．
	 *
	 *  本関数 chip_mprc_initialize はマスタプロセッサのみ・bss クリア後に
	 *  呼ばれる(start.S)ため，ここで起動するのが安全．
	 *
	 *  注: OpenOCD でデバッグする際は a35_1 を examine させないため
	 *      `openocd -c "set EN_CA35_1 0" -f openocd.cfg` で起動すること．
	 */
	{
		extern void start(void);
		uint64_t entry = (uint64_t)(uintptr_t)start;

		/* Core1 のリセットベクタ(64bitモード)を設定 */
		sil_wrw_mem((void *)CA35SYSCFG_VBAR_CR, (uint32_t)(entry & ~0x3ULL));

		/* Core1 のプロセッサリセットを解除 */
		sil_wrw_mem((void *)RCC_C1P1RSTCSETR, RCC_C1P1RSTCSETR_C1P1RST);
		while ((sil_rew_mem((void *)RCC_C1P1RSTCSETR)
				& RCC_C1P1RSTCSETR_C1P1RST) != 0U) {
			;
		}
	}
#endif /* TNUM_PRCID >= 2 */

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
