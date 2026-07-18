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
 *  @(#) $Id: gic_kernel_impl.c 465 2026-06-03 08:51:58Z ertl-honda $
 */

/*
 *		カーネルの割込みコントローラ依存部（GIC用）
 * 
 * 		初期化処理・終了処理のみで呼び出される関数には，最後にメモリ・命令同期は入れていない．
 */

#include "kernel_impl.h"
#include "interrupt.h"
#include <sil.h>
#include "arm64.h"
#include "gic_ipi.h"			/* IPINO_EXT_KER / IPINO_DISPATCH（残留IPIクリア用） */

/* 前方参照 */
static void gicc_init(void);
static void gic_sgi_ppi_init(void);

/*
 *  GICv2,3,4 で共通の関数
 */
/*
 *  Distributor 終了
 */
void
gicd_terminate(void)
{
	uint32_t ctlr_val;

	/* 現在値を確認 */
	ctlr_val = sil_rew_mem((void *)GICD_CTLR);

	/* ARE ビットは保持し、残りはクリア */
	ctlr_val &= GICD_CTLR_ARE_MASK;
	
	/* Distributor を無効に */
	sil_wrw_mem((void *)(GICD_CTLR), ctlr_val);
}

/*
 *  Distributor 初期化共通処理
 */
static void
gicd_initialize_common(void)
{
	int32_t i;
	uint32_t ctlr_val;

	/* 現在値を確認 */
	ctlr_val = sil_rew_mem((void *)GICD_CTLR);

	/* ARE ビットは保持し、残りはクリア */
	ctlr_val &= GICD_CTLR_ARE_MASK;

	/* Distributor を無効に */
	sil_wrw_mem((void *)(GICD_CTLR), ctlr_val);

#ifdef TOPPERS_TZ_S
	/* 割込みを全てグループ1(IRQ)に */
	for(i = TMIN_GLOBAL_INTNO/32; i < GIC_TNUM_INT/32; i++){
		sil_wrw_mem((void *)(GICD_IGROUPRn + (uintptr_t)(4 * i)), 0xffffffff);
		sil_wrw_mem((void *)(GICD_IGRPMODRn + (uintptr_t)(4 * i)), 0x00000000);
	}
#endif /* TOPPERS_TZ_S */

	/* 割込みを全て禁止 */
	for(i = TMIN_GLOBAL_INTNO/32; i < GIC_TNUM_INT/32; i++){
		sil_wrw_mem((void *)(GICD_ICENABLERn + (uintptr_t)(4 * i)), 0xffffffff);
	}

	/* ペンディングをクリア */
	for(i = TMIN_GLOBAL_INTNO/32; i < GIC_TNUM_INT/32; i++){
		sil_wrw_mem((void *)(GICD_ICPENDRn + (uintptr_t)(4 * i)), 0xffffffff);
	}

	/* 優先度最低に設定  */
	for(i = TMIN_GLOBAL_INTNO/4; i < GIC_TNUM_INT/4; i++){
		sil_wrw_mem((void *)(GICD_IPRIORITYRn + (uintptr_t)(4 * i)), 0xffffffff);
	}

	/* ターゲット初期化（全てCPU0へ） */
	for(i = TMIN_GLOBAL_INTNO/4; i < GIC_TNUM_INT/4; i++){
		sil_wrw_mem((void *)(GICD_ITARGETSRn + (uintptr_t)(4 * i)), 0x01010101);
	}

	/* モード初期化(1-N Level) */
	for(i = TMIN_GLOBAL_INTNO/16; i < GIC_TNUM_INT/16; i++){
		sil_wrw_mem((void *)(GICD_ICFGRn + (uintptr_t)(4 * i)), 0x55555555);
	}
}

#if TOPPERS_GIC_VER == 2
/*
 *  GIC初期化（GICv2）
 */
void
gic_init(void)
{
	/*
	 *  SGIとPPIの 初期化
	 */
	gic_sgi_ppi_init();

	/*
	 *  GIC CPUインタフェース初期化
	 */
	gicc_init();
}

/*
 *  CPU Interface の初期化（GICv2）
 */
static void
gicc_init(void)
{
	/* CPUインタフェースを無効に */
	sil_wrw_mem((void *)GICC_CTLR, 0);

	/* 最低優先度に設定 */
	gicc_set_priority(INT_IPM(TIPM_ENAALL));

	/* 割込み優先度の全ビット有効に */
	gicc_set_bp(0);

	/* ペンディングしている可能性があるので，EOI によりクリア */
	sil_wrw_mem((void *)GICC_EOIR, sil_rew_mem((void *)GICC_IAR));

	/* CPUインタフェースを有効に */
#ifdef TOPPERS_TZ_S
#ifdef GIC_NO_FIQ_IN_SECURE
	/*
	 *  セキュア(Group0)割込みを FIQ ではなく IRQ で配送するため，FIQEN を立てず
	 *  ENABLEGRP0 のみとする．FIQEN を立てるとハンドラが GIC ack 前に FIQ を再許可して
	 *  同一割込みが暴走再入する（カーネルの CPU ロックは IRQ マスクモデルのため）．
	 *  既定（マクロ未定義）の挙動は従来どおり（FIQEN を立てる）で他ターゲットに影響しない．
	 */
	sil_wrw_mem((void *)GICC_CTLR, GICC_CTLR_ENABLEGRP0);
#else  /* !GIC_NO_FIQ_IN_SECURE */
	sil_wrw_mem((void *)GICC_CTLR, (GICC_CTLR_FIQEN|GICC_CTLR_ENABLEGRP0));
#endif /* GIC_NO_FIQ_IN_SECURE */
#else  /* !TOPPERS_TZ_S */
	sil_wrw_mem((void *)GICC_CTLR, GICC_CTLR_ENABLE);
#endif /* TOPPERS_TZ_S */

	/*
	 *  残留するペンディング/アクティブ割込みの正規化（GICv2）
	 *
	 *  リセットを介さずにカーネルを再起動した場合（JTAGによる再ロード等）に，
	 *  前回実行時のペンディング割込みや，完了せずに残ったアクティブ割込みが
	 *  CPUインタフェースに残存することがある．特にカーネル終了IPI（ext_ker）
	 *  は irc_end_int を実行せずに停止するためEOIされず，アクティブのまま残
	 *  る．アクティブ割込みが残るとその実行優先度により以降のIPI（他プロセッ
	 *  サからのディスパッチ要求等）がマスクされ，マルチプロセッサ動作がデッ
	 *  ドロックする．これを正常な状態へ正規化する．
	 *
	 *  通常のリセット起動時はペンディング/アクティブ割込みは存在せず（GICC_RPR
	 *  はアイドル0xffを示す），本処理は実質的な副作用を持たない．
	 *  （ARM32版 arm_gcc/common/gic_kernel_impl.c の同等処理をGICv2へ適用）
	 */
	{
		uint_t		i;
		uint32_t	iar;

		/* ペンディング割込みのドレイン（IARでack→EOIで完了） */
		for (i = 0; i < 64U; i++) {
			iar = sil_rew_mem((void *)GICC_IAR);
			if ((iar & 0x03ffU) == 0x03ffU) {
				break;					/* spurious(1023)＝ペンディング無し */
			}
			sil_wrw_mem((void *)GICC_EOIR, iar);
		}

		/*
		 *  残留アクティブ割込みのクリア．GICC_RPRがアイドル(0xff)以外なら，
		 *  前回実行で完了しなかったIPIハンドラのアクティブ状態が残っている．
		 *  カーネルが用いるIPI(SGI)をEOIして解除する（SGIのEOIRは送信元CPU
		 *  を含むため，両プロセッサ分＝CPUID=0とCPUID=1を発行する）．
		 */
		for (i = 0; i < 16U; i++) {
			if ((sil_rew_mem((void *)GICC_RPR) & 0xffU) == 0xffU) {
				break;					/* アクティブ割込み無し */
			}
			sil_wrw_mem((void *)GICC_EOIR, IPINO_EXT_KER);
			sil_wrw_mem((void *)GICC_EOIR, (1U << 10) | IPINO_EXT_KER);
			sil_wrw_mem((void *)GICC_EOIR, IPINO_DISPATCH);
			sil_wrw_mem((void *)GICC_EOIR, (1U << 10) | IPINO_DISPATCH);
		}
	}
}

/*
 *  CPU Interface の終了（GICv2）
 */
void
gicc_stop(void)
{
	sil_wrw_mem((void *)(GICC_CTLR), 0);
}

/*
 *  Distoributor 関連
 */

/*
 *  Distributor 初期化（GICv2）
 * 
 *  この関数は，他のプロセッサが実行を開始する前に，マスタプロセッサの
 *  みから呼び出されるため，プロセッサ間排他制御は必要ない．
 */
void
gicd_initialize(void)
{
	/*
	 *  共通処理を呼び出す
	 */
	gicd_initialize_common();

	/* Distibutor を有効に */
	sil_wrw_mem((void *)(GICD_CTLR), GICD_CTLR_ENABLE);
}

/*
 *  割込み禁止（GICv2）
 * 
 *  ディストリビュータのレジスタへの1回の書き込みのみであるため，プロ
 *  セッサ間排他制御は必要ない．
 */
void
gicd_disable_int(uint8_t id)
{
	uintptr_t offset_addr = (id / 32) * 4;
	uint16_t offset_bit   = id % 32;

	sil_swrw_mem((void *)(GICD_ICENABLERn + offset_addr), (1 << offset_bit));
}

/*
 *  割込み許可（GICv2）
 * 
 *  ディストリビュータのレジスタへの1回の書き込みのみであるため，プロ
 *  セッサ間排他制御は必要ない．  
 */
void
gicd_enable_int(uint8_t id)
{
	uintptr_t offset_addr = (id / 32) * 4;
	uint16_t offset_bit  = id % 32;

	sil_swrw_mem((void *)(GICD_ISENABLERn + offset_addr), (1 << offset_bit));
}

/*
 *  割込みペンディングクリア（GICv2）
 * 
 *  ディストリビュータのレジスタへの1回の書き込みのみであるため，プロ
 *  セッサ間排他制御は必要ない．  
 */
void
gicd_clear_pending(uint8_t id)
{
	uintptr_t offset_addr = (id / 32) * 4;
	uint16_t offset_bit  = id % 32;

	sil_swrw_mem((void *)(GICD_ICPENDRn + offset_addr), (1 << offset_bit));
}

/*
 *  割込みペンディングセット（GICv2）
 * 
 *  ディストリビュータのレジスタへの1回の書き込みのみであるため，プロ
 *  セッサ間排他制御は必要ない．  
 */
void
gicd_set_pending(uint8_t id)
{
	uintptr_t offset_addr = (id / 32) * 4;
	uint16_t offset_bit  = id % 32;

	sil_swrw_mem((void *)(GICD_ISPENDRn + offset_addr), (1 << offset_bit));
}

/*
 *  割込み要求のチェック（GICv2）
 */
bool_t
gicd_probe_pending(uint8_t id)
{
	uintptr_t offset_addr = (id / 32) * 4;
	uint16_t offset_bit  = id % 32;
	uint32_t state;

	state = sil_rew_mem((void *)(GICD_ISPENDRn + offset_addr));

	return ((state & (1 << offset_bit)) == (1 << offset_bit));
}

/*
 *  割込みコンフィギュレーション設定（GICv2）
 * 
 *  この関数は，プロセッサ間排他制御を行った状態で呼び出さなければなら
 *  ない．
 */
void
gicd_config(uint8_t id,  bool_t is_edge, bool_t is_1_n)
{
	uintptr_t offset_addr;
	uint16_t offset_bit;
	uint32_t cfgr_reg_val;
	uint8_t  config;
	SIL_PRE_LOC;

	if (is_edge) {
		config = GICD_ICFGRn_EDGE;
	}
	else {
		config = GICD_ICFGRn_LEVEL;
	}

	if (is_1_n) {
		config |= GICD_ICFGRn_1_N;
	}
	else {
		config |= GICD_ICFGRn_N_N;
	}

	offset_addr = (id / 16) * 4;
	offset_bit  = (id % 16) * 2;

	SIL_LOC_SPN();

	cfgr_reg_val  = sil_rew_mem((void *)(GICD_ICFGRn + offset_addr));
	cfgr_reg_val &= ~(0x03U << offset_bit);
	cfgr_reg_val |= (0x03U & config) << offset_bit;
	sil_wrw_mem((void *)(GICD_ICFGRn + offset_addr), cfgr_reg_val);

#ifdef TOPPERS_TZ_S
	offset_addr = (id / 32) * 4;
	offset_bit  = id % 32;
	sil_wrw_mem((void *)(GICD_IGROUPRn + offset_addr),
				sil_rew_mem((void *)(GICD_IGROUPRn+ offset_addr)) & ~(1 << offset_bit));
#endif /* TOPPERS_TZ_S */

	SIL_UNL_SPN();
}

/*
 *  割込み優先度のセット（GICv2）
 *  内部表現で渡す．
 * 
 *  この関数は，プロセッサ間排他制御を行った状態で呼び出さなければなら
 *  ない．  
 */
void
gicd_set_priority(INTNO intno, uint_t pri)
{
	uintptr_t offset_addr = (intno / 4) * 4;
	uint16_t shift  = ((intno % 4) * 8);
	uint32_t pr_reg_val;
	SIL_PRE_LOC;

	SIL_LOC_SPN();

	pr_reg_val  = sil_rew_mem((void *)(GICD_IPRIORITYRn + offset_addr));
	pr_reg_val &= ~(0xffU << shift);
	pr_reg_val |= (pri << shift);
	sil_wrw_mem((void *)(GICD_IPRIORITYRn + offset_addr), pr_reg_val);

	SIL_UNL_SPN();
}

/*
 *  SGIとPPIの 初期化（GICv2）
 */
static void
gic_sgi_ppi_init(void)
{
	int32_t i;

	/* 割込みを全てグループ1(IRQ)に */
	sil_wrw_mem((void *)(GICD_IGROUPRn + (uintptr_t)(4 * 0)), 0xffffffff);

	/* 割込みを全て禁止 */
	sil_wrw_mem((void *)(GICD_ICENABLERn + (uintptr_t)(4 * 0)), 0xffffffff);

	/* ペンディングをクリア */
	sil_wrw_mem((void *)(GICD_ICPENDRn + (uintptr_t)(4 * 0)), 0xffff0000);

	/*
	 *  アクティブをクリア（PPI: INTID16-31）
	 *
	 *  リセットを介さずにカーネルを再起動した場合（JTAGによる再ロード等）に，
	 *  前回実行で完了せずに残ったPPI（特にタイマ割込み）のアクティブ状態が
	 *  CPUインタフェースのバンク化レジスタに残存することがある．アクティブが
	 *  残ると以降の同一PPI（タイマ割込み）が配送されず，時間イベント（アラー
	 *  ム/周期ハンドラ/タイムアウト）が一切発火しなくなる．本関数は各プロセッ
	 *  サ上で実行されるため，ICACTIVERへの書き込みは当該CPUのバンクに作用する．
	 *  通常のリセット起動時はアクティブ割込みは存在せず本処理は副作用を持たない．
	 *  （Kria/KR260等，システムリセットを介さず起動するターゲットで必要）
	 */
	sil_wrw_mem((void *)(GICD_ICACTIVERn + (uintptr_t)(4 * 0)), 0xffff0000);

	/* 優先度最低に設定  */
	for(i = 0; i < TMIN_GLOBAL_INTNO/4; i++){
		sil_wrw_mem((void *)(GICD_IPRIORITYRn + (uintptr_t)(4 * i)), 0xffffffff);
	}

	/* モード初期化(1-N Level) */
	sil_wrw_mem((void *)(GICD_ICFGRn + (uintptr_t)(4 * 1)), 0x55555555);
}

/*
 *  GIC割込みターゲットの設定（GICv2）
 * 
 *  この関数は，プロセッサ間排他制御を行った状態で呼び出さなければなら
 *  ない．  
 */
void
gicd_set_target(uint8_t id, ID iprcid, uint8_t cpus)
{
	uintptr_t	offset_addr = (id / 4) * 4;
	uint32_t	shift  = (id % 4) * 8;
	uint32_t itr_reg_val;
	SIL_PRE_LOC;

	SIL_LOC_SPN();
    
	itr_reg_val  = sil_rew_mem((void *)(GICD_ITARGETSRn + offset_addr));
	itr_reg_val &= ~(0xf << shift);
	itr_reg_val |= (conv_prcid_to_gicdtarget(iprcid) << shift);
	sil_wrw_mem((void *)(GICD_ITARGETSRn + offset_addr), itr_reg_val);

	SIL_UNL_SPN();
}
#elif (TOPPERS_GIC_VER == 3) || (TOPPERS_GIC_VER == 4)

/*
 *  GICv3,4向けの関数
 */

/*
 *  Redistributor フレーム先頭アドレスは GICR_BASE_ADDR(x)（= GICR_BASE +
 *  GICR_SIZE * x）で毎回計算する（gic_kernel_impl.h 参照）．以前は共有可変配列
 *  gicr_base[] に保存していたが，false sharing 不具合の根絶のため配列を廃止した
 *  （doc/arm64_design.txt 参照）．
 */
static bool_t	gicr_init(void);

/*
 *  GIC初期化（GICv3,4）
 */
void
gic_init(void)
{
	/*
	 *  GIC Redistributor初期化
	 */
	if (!gicr_init()) {
		target_exit();
	}

	/*
	 *  SGIとPPIの 初期化
	 */
	gic_sgi_ppi_init();

	/*
	 *  GIC CPUインタフェース初期化
	 */
	gicc_init();
}

/*
 *  CPU Interface の初期化（GICv3,4）
 */
static void
gicc_init(void)
{
	volatile uint32_t   reg32_val;

	/* Enable System Register */
	ICC_SRE_EL1_WRITE(1);

#ifdef TOPPERS_TZ_S
	/* Group0,1割込を無効に */
	ICC_IGRPEN0_EL1_WRITE(0);
	ICC_IGRPEN1_EL1_WRITE(0);

	/* 最低優先度に設定 */
	gicc_set_priority(INT_IPM(TIPM_ENAALL));

	/* 割込み優先度の全ビット有効に */
	gicc_set_bp(0);

	/* ペンディングしている可能性があるので，EOI によりクリア */
	ICC_IAR0_EL1_READ(reg32_val);
	ICC_EOIR0_EL1_WRITE(reg32_val);

	/* Group0,1割込を有効に */
	ICC_IGRPEN0_EL1_WRITE(1);
	ICC_IGRPEN1_EL1_WRITE(1);

	/* Disable IRQ,FIQ bypass */
	ICC_SRE_EL1_WRITE(3);

	/*
	 *  残留するペンディング/アクティブ割込みの正規化（GICv3）
	 *
	 *  GICv2版gicc_initの同等処理をCPUインタフェースのシステムレジスタ
	 *  （ICC_IAR1/EOIR1/RPR_EL1）で実施する．リセットを介さずにカーネルを
	 *  再起動した場合（JTAGによる再ロード等）や，カーネル終了IPI（ext_ker）
	 *  がirc_end_intを実行せず停止した場合，前回実行のペンディング割込みや，
	 *  EOIされずに残ったアクティブ割込みがCPUインタフェースに残存する．
	 *  アクティブ割込みが残るとその実行優先度により以降のIPI（他プロセッサ
	 *  からのディスパッチ要求等）がマスクされ，マルチプロセッサ動作がデッ
	 *  ドロックする．本処理で正常な状態へ正規化する（Secure Group1割込みは
	 *  ICC_IAR1_EL1でackする）．通常のリセット起動時は副作用を持たない．
	 */
	{
		uint_t		i;
		uint32_t	iar;

		/* ペンディング割込みのドレイン（IAR1でack→EOI1で完了） */
		for (i = 0; i < 64U; i++) {
			ICC_IAR1_EL1_READ(iar);
			if ((iar & 0x03ffU) == 0x03ffU) {
				break;					/* spurious(1023)＝ペンディング無し */
			}
			ICC_EOIR1_EL1_WRITE(iar);
		}

		/*
		 *  残留アクティブ割込みのクリア．ICC_RPR_EL1がアイドル(0xff)以外なら，
		 *  前回実行で完了しなかったIPIハンドラのアクティブ状態が残っている．
		 *  カーネルが用いるIPI(SGI)をEOIして実行優先度を解除する．
		 */
		for (i = 0; i < 16U; i++) {
			ICC_RPR_EL1_READ(reg32_val);
			if ((reg32_val & 0xffU) == 0xffU) {
				break;					/* アクティブ割込み無し */
			}
			ICC_EOIR1_EL1_WRITE(IPINO_EXT_KER);
			ICC_EOIR1_EL1_WRITE(IPINO_DISPATCH);
		}
	}
#else  /* !TOPPERS_TZ_S */
	/* Group1割込を無効に */
	ICC_IGRPEN1_EL1_WRITE(0);

	/* 最低優先度に設定 */
	gicc_set_priority(INT_IPM(TIPM_ENAALL));

	/* 割込み優先度の全ビット有効に */
	gicc_set_bp(0);

	/* ペンディングしている可能性があるので，EOI によりクリア */
	ICC_IAR1_EL1_READ(reg32_val);
	ICC_EOIR1_EL1_WRITE(reg32_val);

	/* Group1割込を有効に */
	ICC_IGRPEN1_EL1_WRITE(1);

	/* Disable IRQ,FIQ bypass */
	ICC_SRE_EL1_WRITE(7);
#endif /* TOPPERS_TZ_S */
}

/*
 *  CPU Interface の終了（GICv3,4）
 */
void
gicc_stop(void)
{
#ifdef TOPPERS_TZ_S
	ICC_IGRPEN0_EL1_WRITE(0);
	ICC_IGRPEN1_EL1_WRITE(0);
#else  /* !TOPPERS_TZ_S */
	ICC_IGRPEN1_EL1_WRITE(0);
#endif /* TOPPERS_TZ_S */
}

/*
 *  Redistoributor初期化（GICv3,4）
 */
static bool_t
gicr_init(void)
{
	volatile uint32_t   reg32_val;
	uint_t  cnt;
	uint_t	prc_id = get_my_prcidx();

	/* Redistoributor起動 */
	reg32_val = sil_rew_mem((void *)GICR_WAKER(prc_id));
	reg32_val &= ~GICR_ProcSleep;
	sil_wrw_mem((void *)GICR_WAKER(prc_id), reg32_val);

	/* 起動を待つ */
	for( cnt = 0 ; cnt < 1000 ; cnt++ ) {
		reg32_val = sil_rew_mem((void *)GICR_WAKER(prc_id));
		if ((reg32_val & GICR_ChildrenAsleep) == 0) {
			return true;
		}
		sil_dly_nse(1000000);
	}

	return false;
}

/*
 *  ディストリビュータの操作
 * 
 *  ディストリビュータはプロセッサ間で共有しているため，それを操作する
 *  場合には，必要に応じて，プロセッサ間排他制御を行う必要がある．
 */

/*
 *  ディストリビュータ初期化（GICv3,4）
 * 
 *  この関数は，他のプロセッサが実行を開始する前に，マスタプロセッサの
 *  みから呼び出されるため，プロセッサ間排他制御は必要ない．  
 */
void
gicd_initialize(void) {
	int32_t i;

	/* 共通処理を呼び出す */
	gicd_initialize_common();

	/* SPIの設定値をクリア */
#ifdef TOPPERS_TZ_S
	/* 全てNonSecure Group1に */
	for(i = 1; i < GIC_TNUM_INT/32; i++){
		sil_wrw_mem((void *)(GICD_IGROUPRn + (uintptr_t)(4 * i)),  0xffffffff);
		sil_wrw_mem((void *)(GICD_IGRPMODRn + (uintptr_t)(4 * i)), 0x00000000);
	}
#endif /* TOPPERS_TZ_S */

	/* 割込みを全て禁止 */
	for(i = 1; i < GIC_TNUM_INT/32; i++){
		sil_wrw_mem((void *)(GICD_ICENABLERn + (uintptr_t)(4 * i)), 0xffffffff);
	}

	/* ペンディングをクリア */
	for(i = 1; i < GIC_TNUM_INT/32; i++){
		sil_wrw_mem((void *)(GICD_ICPENDRn + (uintptr_t)(4 * i)), 0xffffffff);
	}

	/* 優先度最低に設定  */
	for(i = 1; i < GIC_TNUM_INT/32; i++){
		sil_wrw_mem((void *)(GICD_IPRIORITYRn + (uintptr_t)(4 * i)), 0xffffffff);
	}

	/* ターゲット初期化（全てCPU0へ） */
	/*   SPIs */
	for(i = 1; i < GIC_TNUM_INT/32; i++){
		sil_wrw_mem((void *)(GICD_ITARGETSRn + (uintptr_t)(4 * i)), 0x01010101);
	}

	/* モード初期化(1-N Level) */
	for(i = 1; i < GIC_TNUM_INT/32; i++){
		sil_wrw_mem((void *)(GICD_ICFGRn + (uintptr_t)(4 * i)), 0x55555555);
	}

	/*
	 *  Distibutor を有効に
	 *
	 *  GICv3では GICD_CTLR への書込みが反映されるまで RWP(bit31)が立つ．
	 *  ARE設定とグループ有効化を連続書込みする際，RWPの完了を待たないと
	 *  2回目の書込み（グループ有効化）が取りこぼされ，Secure Group1の
	 *  SPIが配送されない（ras_int等が効かない）．各書込み後にRWPを待つ．
	 */
#ifdef TOPPERS_TZ_S
	sil_wrw_mem((void *)(GICD_CTLR), sil_rew_mem((void *)(GICD_CTLR)) | GICD_CTLR_ARE_S);
	while ((sil_rew_mem((void *)GICD_CTLR) & GICD_CTLR_RWP) != 0U) ;
	sil_wrw_mem((void *)(GICD_CTLR), sil_rew_mem((void *)(GICD_CTLR)) | GICD_CTLR_ENABLEGRP1S);
	while ((sil_rew_mem((void *)GICD_CTLR) & GICD_CTLR_RWP) != 0U) ;
#else  /* !TOPPERS_TZ_S */
	sil_wrw_mem((void *)(GICD_CTLR), sil_rew_mem((void *)(GICD_CTLR)) | GICD_CTLR_ARE_NS);
	while ((sil_rew_mem((void *)GICD_CTLR) & GICD_CTLR_RWP) != 0U) ;
	sil_wrw_mem((void *)(GICD_CTLR), sil_rew_mem((void *)(GICD_CTLR)) | GICD_CTLR_ENABLEGRP1NS);
	while ((sil_rew_mem((void *)GICD_CTLR) & GICD_CTLR_RWP) != 0U) ;
#endif /* TOPPERS_TZ_S */
}

/*
 *  割込み禁止（GICv3,4）
 * 
 *  ディストリビュータのレジスタへの1回の書き込みのみであるため，プロ
 *  セッサ間排他制御は必要ない．
 */
void
gicd_disable_int(uint8_t id)
{
	uintptr_t offset_addr = (id / 32) * 4;
	uint16_t shift   = id % 32;
	uint_t	prc_id = get_my_prcidx();

	if (id < 32) {
		sil_swrw_mem((void *)GICR_ICENABLER0(prc_id), (1 << shift));
	}
	else {
		sil_swrw_mem((void *)(GICD_ICENABLERn + offset_addr), (1 << shift));
	}
}

/*
 *  割込み許可（GICv3,4）
 * 
 *  ディストリビュータのレジスタへの1回の書き込みのみであるため，プロ
 *  セッサ間排他制御は必要ない．  
 */
void
gicd_enable_int(uint8_t id)
{
	uintptr_t offset_addr = (id / 32) * 4;
	uint16_t shift  = id % 32;
	uint_t	prc_id = get_my_prcidx();

	if (id < 32) {
		sil_swrw_mem((void *)GICR_ISENABLER0(prc_id), (1 << shift));
	}
	else {
		sil_swrw_mem((void *)(GICD_ISENABLERn + offset_addr), (1 << shift));
	}
}

/*
 *  割込みペンディングクリア（GICv3,4）
 * 
 *  ディストリビュータのレジスタへの1回の書き込みのみであるため，プロ
 *  セッサ間排他制御は必要ない．  
 */
void
gicd_clear_pending(uint8_t id)
{
	uint16_t shift  = id % 32;
	uint_t	prc_id = get_my_prcidx();

	if (id < 32) {
		sil_swrw_mem((void *)GICR_ICPENDR0(prc_id), (1 << shift));
	}
	else {
#ifdef TOPPERS_TZ_S
		sil_swrw_mem((void *)GICD_CLRSPI_SR, id);
#else  /* !TOPPERS_TZ_S */
		sil_swrw_mem((void *)GICD_CLRSPI_NSR, id);
#endif /* TOPPERS_TZ_S */
	}
}

/*
 *  割込みペンディングセット（GICv3,4）
 * 
 *  ディストリビュータのレジスタへの1回の書き込みのみであるため，プロ
 *  セッサ間排他制御は必要ない．  
 */
void
gicd_set_pending(uint8_t id)
{
	uint16_t	shift  = id % 32;
	uint_t		prc_id = get_my_prcidx();

	if (id < 32) {
		sil_swrw_mem((void *)GICR_ISPENDR0(prc_id), (1 << shift));
	}
    else {
#ifdef TOPPERS_TZ_S
		sil_swrw_mem((void *)GICD_SETSPI_SR, id);
#else  /* !TOPPERS_TZ_S */
		sil_swrw_mem((void *)GICD_SETSPI_NSR, id);
#endif /* TOPPERS_TZ_S */
	}
}

/*
 *  割込み要求のチェック（GICv3,4）
 */
bool_t
gicd_probe_pending(uint8_t id)
{
	uintptr_t offset_addr = (id / 32) * 4;
	uint16_t shift  = id % 32;
	uint32_t state;
	uint_t	prc_id = get_my_prcidx();

	if (id < 32) {
		state = sil_rew_mem((void *)GICR_ISPENDR0(prc_id));
	}
	else {
		state = sil_rew_mem((void *)(GICD_ISPENDRn + offset_addr));
	}

	return ((state & (1 << shift)) == (1 << shift));
}

/*
 *  割込みコンフィギュレーション設定（GICv3,4）
 *
 *  この関数は，プロセッサ間排他制御を行った状態で呼び出さなければなら
 *  ない．
 */
void
gicd_config(uint8_t id,  bool_t is_edge, bool_t is_1_n)
{
	uintptr_t offset_addr;
	uint16_t shift;
	uint32_t cfgr_reg_val;
	uint8_t  config;
	uint_t	prc_id = get_my_prcidx();
	SIL_PRE_LOC;

	if (is_edge) {
		config = GICD_ICFGRn_EDGE;
	}
	else {
		config = GICD_ICFGRn_LEVEL;
	}

	if (is_1_n) {
		config |= GICD_ICFGRn_1_N;
	}
	else {
		config |= GICD_ICFGRn_N_N;
	}

	offset_addr = (id / 16) * 4;
	shift  = (id % 16) * 2;

	SIL_LOC_SPN();

	if (id < 32) {
		cfgr_reg_val  = sil_rew_mem((void *)(GICR_ICFGR0(prc_id) + offset_addr));
		cfgr_reg_val &= ~(0x03U << shift);
		cfgr_reg_val |= (0x03U & config) << shift;
		sil_wrw_mem((void *)(GICR_ICFGR0(prc_id) + offset_addr), cfgr_reg_val);
#ifdef TOPPERS_TZ_S
		/* 割込をSecure Group1に設定 */
		shift  = id % 32;
		sil_wrw_mem((void *)GICR_IGROUPR0(prc_id),
					sil_rew_mem((void *)GICR_IGROUPR0(prc_id)) & ~(1U << shift));
		sil_wrw_mem((void *)GICR_IGRPMODR0(prc_id),
					sil_rew_mem((void *)GICR_IGRPMODR0(prc_id)) | (1U << shift));
#endif /* TOPPERS_TZ_S */
	}
	else {
		cfgr_reg_val  = sil_rew_mem((void *)(GICD_ICFGRn + offset_addr));
		cfgr_reg_val &= ~(0x03U << shift);
		cfgr_reg_val |= (0x03U & config) << shift;
		sil_wrw_mem((void *)(GICD_ICFGRn + offset_addr), cfgr_reg_val);
#ifdef TOPPERS_TZ_S
		/* 割込をSecure Group1に設定 */
		offset_addr = (id / 32) * 4;
		shift  = id % 32;
		sil_wrw_mem((void *)(GICD_IGROUPRn + offset_addr),
					sil_rew_mem((void *)(GICD_IGROUPRn + offset_addr)) & ~(1U << shift));
		sil_wrw_mem((void *)(GICD_IGRPMODRn + offset_addr),
					sil_rew_mem((void *)(GICD_IGRPMODRn + offset_addr)) | (1U << shift));
#endif /* TOPPERS_TZ_S */
	}

	SIL_UNL_SPN();
}

/*
 *  割込み優先度のセット（GICv3,4）
 *  内部表現で渡す．
 *
 *  この関数は，プロセッサ間排他制御を行った状態で呼び出さなければなら
 *  ない．
 */
void
gicd_set_priority(INTNO intno, uint_t pri)
{
	uintptr_t offset_addr = (intno / 4) * 4;
	uint16_t shift  = ((intno % 4) * 8);
	uint32_t pr_reg_val;
	uint_t	prc_id = get_my_prcidx();
	SIL_PRE_LOC;

	SIL_LOC_SPN();

	if (intno < GIC_INTNO_SPI0) {
		pr_reg_val  = sil_rew_mem((void *)(GICR_IPRIORITYRn(prc_id) + offset_addr));
		pr_reg_val &= ~(0xffU << shift);
		pr_reg_val |= (pri << shift);
		sil_wrw_mem((void *)(GICR_IPRIORITYRn(prc_id) + offset_addr), pr_reg_val);
	}
	else {
		pr_reg_val  = sil_rew_mem((void *)(GICD_IPRIORITYRn + offset_addr));
		pr_reg_val &= ~(0xffU << shift);
		pr_reg_val |= (pri << shift);
		sil_wrw_mem((void *)(GICD_IPRIORITYRn + offset_addr), pr_reg_val);
	}

	SIL_UNL_SPN();
}

/*
 *  SGIとPPIの 初期化（GICv3,4）
 */
static void
gic_sgi_ppi_init(void)
{
	int_t i;
	uint_t	prc_id = get_my_prcidx();

#ifdef TOPPERS_TZ_S
	/* SGIとPPIを全て Non-Secure Group 1 に */
	sil_wrw_mem((void *)GICR_IGROUPR0(prc_id), 0xffffffff);
	sil_wrw_mem((void *)GICR_IGRPMODR0(prc_id), 0x00000000);
#endif /* TOPPERS_TZ_S */

	/* 割込みを全て禁止 */
	sil_wrw_mem((void *)GICR_ICENABLER0(prc_id), 0xffffffff);

	/* ペンディングをクリア */
	sil_wrw_mem((void *)GICR_ICPENDR0(prc_id), 0xffff0000);

	/* 優先度最低に設定  */
	for(i = 0; i < TMIN_GLOBAL_INTNO/4; i++){
		sil_wrw_mem((void *)(GICR_IPRIORITYRn(prc_id) + (uintptr_t)(4 * i)), 0xffffffff);
	}

	/* モード初期化(1-N Level) */
	/*   SGIs */
	sil_wrw_mem((void *)GICR_ICFGR0(prc_id), 0x55555555);
	/*   PPIs */
	sil_wrw_mem((void *)GICR_ICFGR1(prc_id), 0x55555555);
}

/*
 *  GIC割込みターゲットの設定（GICv3,4）
 *  cpusはターゲットとするCPUのビットパターンで指定
 *
 *  この関数は，プロセッサ間排他制御を行った状態で呼び出さなければなら
 *  ない．  
 */
void
gicd_set_target(uint8_t id, ID iprcid, uint8_t cpus)
{
	uintptr_t offset_addr = id * 8;
	SIL_PRE_LOC;

	SIL_LOC_SPN();

	sil_wrw_mem((void *)(GICD_IROUTERn + offset_addr), conv_prcid_to_mpidr(iprcid));

	SIL_UNL_SPN();
}

/*
 *  割込みグループの設定（セキュリティ拡張）
 *
 *  この関数は，プロセッサ間排他制御を行った状態で呼び出さなければなら
 *  ない．  
 */
Inline void
gicd_config_group(INTNO intno, uint_t group)
{
	uint_t		shift = intno % 32;
	uint32_t	reg;
	SIL_PRE_LOC;

	SIL_LOC_SPN();
    
	reg = sil_rew_mem((void *)(GICD_IGROUPRn + (uintptr_t)((intno / 32) * 4)));
	reg &= ~(0x01U << shift);
	reg |= (group << shift);
	sil_wrw_mem((void *)(GICD_IGROUPRn + (uintptr_t)((intno / 32) * 4)), reg);

	SIL_UNL_SPN();    
}

/*
 *  割込みグループモディファイアの設定（セキュリティ拡張）
 *
 *  この関数は，プロセッサ間排他制御を行った状態で呼び出さなければなら
 *  ない．  
 */
Inline void
gicd_config_group_modifier(INTNO intno, uint_t group)
{
	uint_t		shift = intno % 32;
	uint32_t	reg;
	SIL_PRE_LOC;

	SIL_LOC_SPN();

	reg = sil_rew_mem((void *)(GICD_IGRPMODRn + (uintptr_t)((intno / 32) * 4)));
	reg &= ~(0x01U << shift);
	reg |= (group << shift);
	sil_wrw_mem((void *)(GICD_IGRPMODRn + (uintptr_t)((intno / 32) * 4)), reg);

	SIL_UNL_SPN();
}

#endif /* (TOPPERS_GIC_VER == 3) || (TOPPERS_GIC_VER == 4) */

#ifndef OMIT_GIC_INITIALIZE_INTERRUPT

/*
 *  割込み要求ラインの属性の設定
 *
 *  FMP3カーネルでの利用を想定して，パラメータエラーはアサーションでチェッ
 *  クしている．
 */
Inline void
config_int(PCB *p_my_pcb, INTNO intno, ATR intatr, PRI intpri, ID iprcid, uint_t affinity)
{
	SIL_PRE_LOC;

	assert(VALID_INTNO(p_my_pcb->prcid, intno));
	assert(TMIN_INTPRI <= intpri && intpri <= TMAX_INTPRI);

	/*
	 *  SILスピンロックを取得
	 */
	SIL_LOC_SPN();

	/*
	 *  割込みを禁止
	 *
	 *  割込みを受け付けたまま，レベルトリガ／エッジトリガの設定や，割
	 *  込み優先度の設定を行うのは危険なため，割込み属性にかかわらず，
	 *  一旦マスクする．
	 */
	disable_int(intno);

	/*
	 *  割込みをコンフィギュレーション
	 */
	if ((intatr & TA_EDGE) != 0U) {
		gicd_config(INTNO_MASK(intno), GICD_ICFGRn_EDGE, true);
		clear_int(intno);
	}
	else {
		gicd_config(INTNO_MASK(intno), GICD_ICFGRn_LEVEL, true);
	}

	/*
	 *  割込み優先度とターゲットプロセッサを設定
	 */
	gicd_set_priority(INTNO_MASK(intno), INT_IPM(intpri));
	if (INTNO_MASK(intno) >= TMIN_GLOBAL_INTNO) {
		gicd_set_target(INTNO_MASK(intno), iprcid, affinity);
	}

	/*
	 * 割込みを許可
	 */
	if ((intatr & TA_ENAINT) != 0U) {
		enable_int(intno);
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
initialize_interrupt(PCB *p_my_pcb)
{
	uint_t			i;
	const INTINIB	*p_intinib;

	for (i = 0; i < tnum_cfg_intno; i++) {
		p_intinib = &(intinib_table[i]);
		if (p_intinib->iprcid == p_my_pcb->prcid) {
			config_int(p_my_pcb, p_intinib->intno, p_intinib->intatr,
							p_intinib->intpri, p_intinib->iprcid, p_intinib->affinity);
		}
	}
}

#endif /* OMIT_GIC_INITIALIZE_INTERRUPT */
