/*
 *  TOPPERS/FMP Kernel
 *      Toyohashi Open Platform for Embedded Real-Time Systems/
 *      Flexible MultiProcessor Kernel
 *
 *  Copyright (C) 2026 by Embedded and Real-Time Systems Laboratory
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
 *  $Id$
 */

/*
 *		カーネルのターゲット依存部（ZynqMP R5用）
 */

#include "kernel_impl.h"
#include <sil.h>
#include "arm.h"

/*
 *  カーネル動作時のメモリマップと関連する定義
 *
 *  0x00000000 - 0x7fffffff：DDR（低位2GB）
 *  0x80000000 - 0xbfffffff：PL（プログラマブルロジック）領域
 *  0xc0000000 - 0xdfffffff：QSPI
 *  0xe0000000 - 0xefffffff：PCIe Low
 *  0xf8000000 - 0xffffffff：周辺デバイス・OCM等
 */

/*
 *  MPUの静的リージョンの初期化
 *
 *  タスク間のメモリ保護は行わず，全域をアクセス可能とした上で，
 *  (1) MPU無効時のデフォルトメモリマップによるXN制約の回避，
 *  (2) メモリ／デバイスの属性の明示，のためにMPUを使用する．
 */
static void
init_mpu_region(void)
{
	uint32_t	number = 0;

	mpu_disable_allregion();

	/*
	 *  DDR : 0x00000000 - 0x7FFFFFFF
	 */
	mpu_set_region(0x00000000U, RSAE_2G, number++,
					RAC_AP_PRW_URW | RAC_OI_NCACHE | RAC_S);

	/*
	 *  PL : 0x80000000 - 0xBFFFFFFF
	 */
	mpu_set_region(0x80000000U, RSAE_1G, number++,
					RAC_AP_PRW_URW | RAC_STRONGO_SHAR | RAC_XN);

	/*
	 *  QSPI : 0xC0000000 - 0xDFFFFFFF
	 */
	mpu_set_region(0xC0000000U, RSAE_512M, number++,
					RAC_AP_PRW_URW | RAC_DEV_NSHAR | RAC_XN);

	/*
	 *  PCIe Low : 0xE0000000 - 0xEFFFFFFF
	 */
	mpu_set_region(0xE0000000U, RSAE_256M, number++,
					RAC_AP_PRW_URW | RAC_DEV_NSHAR | RAC_XN);

	/*
	 *  STM CoreSight : 0xF8000000 - 0xF8FFFFFF
	 */
	mpu_set_region(0xF8000000U, RSAE_16M, number++,
					RAC_AP_PRW_URW | RAC_DEV_NSHAR | RAC_XN);

	/*
	 *  RPU/A53 GIC : 0xF9000000 - 0xF9FFFFFF
	 */
	mpu_set_region(0xF9000000U, RSAE_16M, number++,
					RAC_AP_PRW_URW | RAC_DEV_NSHAR | RAC_XN);

	/*
	 *  FPS slaves : 0xFD000000 - 0xFDFFFFFF
	 */
	mpu_set_region(0xFD000000U, RSAE_16M, number++,
					RAC_AP_PRW_URW | RAC_DEV_NSHAR | RAC_XN);

	/*
	 *  Upper LPS : 0xFE000000 - 0xFEFFFFFF
	 */
	mpu_set_region(0xFE000000U, RSAE_16M, number++,
					RAC_AP_PRW_URW | RAC_DEV_NSHAR | RAC_XN);

	/*
	 *  Lower LPS : 0xFF000000 - 0xFFFFFFFF（OCMを含む）
	 */
	mpu_set_region(0xFF000000U, RSAE_16M, number++,
					RAC_AP_PRW_URW | RAC_DEV_NSHAR | RAC_XN);

	/*
	 *  OCM RAM : 0xFFFC0000 - 0xFFFFFFFF（前リージョンより優先される）
	 */
	mpu_set_region(0xFFFC0000U, RSAE_256K, number++,
					RAC_AP_PRW_URW | RAC_OI_WB_WA | RAC_S);
}

/*
 *  システムログの低レベル出力のための初期化
 */
extern void	sio_initialize(EXINF exinf);
extern void	target_fput_initialize(void);

/*
 *  ハードウェアの初期化（start_r5 → start から呼ばれる）
 *
 *  キャッシュとMPUを一旦無効化した上で，MPUの静的リージョンを設定し，
 *  キャッシュ・MPU・分岐予測を有効にする．
 */
void
hardware_init_hook(void)
{
	uint32_t	reg;

	/*
	 *  キャッシュとMPUの無効化
	 */
	CP15_READ_SCTLR(reg);
	if ((reg & CP15_SCTLR_DCACHE) != 0U) {
		arm_clean_dcache();
	}
	arm_disable_dcache();
	arm_disable_icache();
	mpu_disable();

	/*
	 *  TCMのECCチェックとキャッシュのパリティチェックを無効化
	 *  （初期化されていないRAMの読出しによる誤検出を防ぐ）
	 */
	CP15_READ_ACTLR(reg);
	reg &= ~(ACTLR_ATCMPCEN | ACTLR_B0TCMPCEN | ACTLR_B1TCMPCEN);
	reg &= ~ACTLR_CEC_MASK;
	reg |= ACTLR_CEC_NPE;
	CP15_WRITE_ACTLR(reg);
	data_sync_barrier();

	/*
	 *  キャッシュ全体の無効化
	 */
	arm_invalidate_icache();
	CP15_INVALIDATE_DCACHE_R5();
	data_sync_barrier();

	/*
	 *  lockstepモードの場合，フォールトログを有効にする．
	 */
	if ((sil_rew_mem((uint32_t *) RPU_RPU_GLBL_CNTL)
							& RPU_RPU_GLBL_CNTL_SLSPLIT_MASK) == 0U) {
		sil_wrw_mem((uint32_t *) RPU_RPU_ERR_INJ,
					sil_rew_mem((uint32_t *) RPU_RPU_ERR_INJ)
									| RPU_RPU_ERR_INJ_FAULTLOGENABLE);
	}

	/*
	 *  MPUの静的リージョンの設定
	 */
	init_mpu_region();

	/*
	 *  分岐予測の有効化（DBWRはerrata 780125対策で無効化）
	 */
	CP15_READ_ACTLR(reg);
	reg &= ~ACTLR_RSDIS;
	reg &= ~ACTLR_BP_MASK;
	reg |= ACTLR_DBWR;
	CP15_WRITE_ACTLR(reg);
	data_sync_barrier();

	/*
	 *  キャッシュとMPUの有効化
	 */
	arm_enable_icache();
	arm_enable_dcache();
	mpu_enable();
}

/*
 *  ターゲット依存の初期化（マスタプロセッサ，sta_ker前）
 */
void
target_mprc_initialize(void)
{
	chip_mprc_initialize();
}

/*
 *  ターゲット依存の初期化
 */
void
target_initialize(PCB *p_my_pcb)
{
	/*
	 *  チップ依存の初期化
	 *
	 *  ベクタテーブルはリンカスクリプトで0x0に配置し，SCTLR.V=0
	 *  （start_r5で設定）で参照するため，VBARの設定は行わない
	 *  （Cortex-R5はVBARを持たない）．
	 */
	chip_initialize(p_my_pcb);

	/*
	 *  SIOを初期化
	 */
	sio_initialize(0);
	target_fput_initialize();
}

/*
 *  デフォルトのsoftware_term_hook（weak定義）
 */
__attribute__((weak))
void software_term_hook(void)
{
}

/*
 *  ターゲット依存の終了処理
 */
void
target_exit(void)
{
	/*
	 *  software_term_hookの呼出し
	 */
	software_term_hook();

	/*
	 *  チップ依存の終了処理
	 */
	chip_terminate();

	/*
	 *  ターゲット依存の終了処理
	 *
	 *  QEMU上で実行する場合，セミホスティングによりQEMUを終了させる
	 *  （テストの自動実行用）．実機では何もしない．
	 */
#if defined(TOPPERS_USE_QEMU) && !defined(TOPPERS_OMIT_QEMU_SEMIHOSTING)
	Asm("ldr r1, =#0x20026\n\t"		/* ADP_Stopped_ApplicationExit */
		"mov r0, #0x18\n\t"			/* angel_SWIreason_ReportException */
		"svc 0x00123456");
#endif
	while (true) ;
}

/*
 *		システムログの低レベル出力
 */

#include "target_syssvc.h"
#include "target_serial.h"

/*
 *  低レベル出力用のSIOポート管理ブロック
 */
static SIOPCB	*p_siopcb_target_fput;

/*
 *  SIOポートの初期化
 */
void
target_fput_initialize(void)
{
	p_siopcb_target_fput = sio_opn_por(SIOPID_FPUT, 0);
}

/*
 *  SIOポートへのポーリング出力
 */
Inline void
zynqmp_r5_uart_fput(char c)
{
	/*
	 *  送信できるまでポーリング
	 */
	while (!(sio_snd_chr(p_siopcb_target_fput, c))) {
		sil_dly_nse(100);
	}
}

/*
 *  SIOポートへの文字出力
 */
void
target_fput_log(char c)
{
	if (c == '\n') {
		zynqmp_r5_uart_fput('\r');
	}
	zynqmp_r5_uart_fput(c);
}
