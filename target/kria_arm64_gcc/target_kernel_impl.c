/*
 *  TOPPERS/FMP Kernel
 *      Toyohashi Open Platform for Embedded Real-Time Systems/
 *      Flexible MultiProcessor Kernel
 *
 *  Copyright (C) 2007-2024 by Embedded and Real-Time Systems Laboratory
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
 *  @(#) $Id: $
 */

/*
 *  ターゲット依存モジュール（KRIA Cortex-A53(AArch64)用）
 */
#include "kernel_impl.h"
#include <sil.h>

#ifdef TOPPERS_WITH_ATF
#include "atf.h"
#include "psci.h"
#endif /* TOPPERS_WITH_ATF */

/*
 *  システムログの低レベル出力のための初期化
 */
extern void	sio_initialize(EXINF exinf);
extern void	target_fput_initialize(void);

/*
 *  タイマの周波数を保持する変数
 *  単位はkHz
 *  target_initialize() で初期化される
 */
uint32_t	timer_clock_mhz;

/*
 *  EL3で行う初期化処理
 */
void
target_el3_initialize(void)
{
	chip_el3_initialize();
}

/*
 *  EL2で行う初期化処理
 */
void
target_el2_initialize(void)
{
	chip_el2_initialize();
}

/*
 *  str_ker() の前でマスタプロセッサで行う初期化
 */
void
target_mprc_initialize(void)
{
	chip_mprc_initialize();
}

/*
 *  MMUへの設定情報（メモリエリアの情報）
 *
 *  core依存部のmmu_init()が本テーブル（arm64_memory_area）を走査して
 *  メモリマップを設定する（USE_ARM64_MMU_CONFIG_TABLE指定時）．
 *  weak定義であり，アプリケーション側で同名のテーブルを定義することで
 *  テーブル全体を差し替えることができる．
 *  全領域は物理アドレス = 仮想アドレス
 */
#ifdef TOPPERS_TZ_S
#define MMU_MEM_NS		MEM_NS_SECURE
#else  /* TOPPERS_TZ_S */
#define MMU_MEM_NS		MEM_NS_NONSECURE
#endif /* TOPPERS_TZ_S */

__attribute__((weak))
ARM64_MMU_CONFIG arm64_memory_area[] = {
	/* { va, pa, size, attr, ap, ns } */
	/* TOPPERS/FMPが動作するメモリ
	 * : Normal, Outer and Inner Write-Back No-transient */
	{ TOPPERS_MEM_BASE, TOPPERS_MEM_BASE, TOPPERS_MEM_SIZE,
				MEM_ATTR_NML_C,  MEM_AP_RW_EL1, MMU_MEM_NS },
	/* Registers : Device */
	{ 0x80000000,  0x80000000,  0x20000000,
				MEM_ATTR_DEV,    MEM_AP_RW_EL1, MMU_MEM_NS },
	/* FPGA-AXI メモリ : Normal, Non-cacheable */
	{ 0xa0000000,  0xa0000000,  0x00040000,
				MEM_ATTR_NML_NC, MEM_AP_RW_EL1, MMU_MEM_NS },
	/* FPGA-AXI ペリフェラル : Device */
	{ 0xa0040000,  0xa0040000,  0x5FBC0000,
				MEM_ATTR_DEV,    MEM_AP_RW_EL1, MMU_MEM_NS },
	/* OCRAM : Normal, Non-cacheable（常にNonSecure） */
	{ 0xFFC00000,  0xFFC00000,  0x00400000,
				MEM_ATTR_NML_NC, MEM_AP_RW_EL1, MEM_NS_NONSECURE },
};

/*
 *  MMUの設定情報の数（メモリエリアの数）
 */
__attribute__((weak))
const uint_t arm64_tnum_memory_area
					= sizeof(arm64_memory_area) / sizeof(ARM64_MMU_CONFIG);

/*
 *  ターゲット依存の初期化
 */
void
target_initialize(PCB *p_my_pcb)
{
	uint32_t timer_clock_hz;

	/*
	 *  タイマのクロックの取得
	 */
	if (p_my_pcb->prcid == TOPPERS_MASTER_PRCID) {    
		CNTFRQ_EL0_READ(timer_clock_hz);
		timer_clock_mhz = timer_clock_hz / 1000000;
	}

	/*
	 * チップ依存の初期化
	 */
	chip_initialize(p_my_pcb);

	/*
	 *  バナー表示，低レベル出力用にUARTを初期化
	 */
	if (p_my_pcb->prcid == TOPPERS_MASTER_PRCID) {
		sio_initialize(0);
		target_fput_initialize();
	}   
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
	 */
	while (true) ;
}

/*
 *		システムログの低レベル出力（本来は別のファイルにすべき）
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
kria_uart_fput(char c)
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
		kria_uart_fput('\r');
	}
	kria_uart_fput(c);
}
