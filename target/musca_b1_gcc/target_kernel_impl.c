/*
 *  ターゲット依存モジュール（ARM Musca-B1 / FMP3 用）
 *
 *  QEMU の musca-b1 マシンでは，クロックや GPIO/パッドの初期化は不要であ
 *  り，SysTick・NVIC・FPU の設定はコア依存部（core_initialize）が行う．
 *  SSE-200 を中核とするが，UART は PL011 を用いる．
 */
#include "kernel_impl.h"
#include "target_syssvc.h"
#include "musca_b1.h"
#include <sil.h>

/*
 *  起動時のハードウェア初期化処理
 *
 *  start.S から，BSS/DATA の初期化に先立って呼び出される（弱いデフォルト
 *  定義を上書きする）．本ターゲットでは特に処理は不要．
 */
void
hardware_init_hook(void)
{
}

/*
 *  ソフトウェア環境（ライブラリ等）の初期化処理
 *
 *  start.S から，BSS/DATA の初期化後に呼び出される．本ターゲットでは特に
 *  処理は不要．
 */
void
software_init_hook(void)
{
}

/*
 *  マスタプロセッサ依存の初期化（二次コアの起動）
 *
 *  start.S から，マスタコア（CPU0）が BSS/DATA 初期化・software_init_hook
 *  の後，sta_ker() の前に呼び出す．
 *
 *  TNUM_PRCID >= 2 のときは，二次コア（CPU1）を起動する．SSE-200 では
 *  CPUWAIT レジスタの bit1 を 1→0 にすると，QEMU 内部で CPU1 が init_svtor
 *  （0x10000000）からリセット起動する．起動した CPU1 は再び _kernel_start を
 *  通り，my_prcidx_boot マクロでブートフラグ（!=0）を観測して自分が CPU1
 *  （prcidx=1）であると判定するため，解放に先立ってフラグをセットする．
 *
 *  単一プロセッサ（TNUM_PRCID==1）では何もしない．
 */
void
target_mprc_initialize(void)
{
#if TNUM_PRCID >= 2
	extern volatile uint32_t _kernel_mp_boot_flag;

	/*
	 *  二次コアが _kernel_start 冒頭で観測するブートフラグをセットする．
	 *  CPUWAIT 書込みより前に，かつメモリバリアで順序を保証する．
	 */
	_kernel_mp_boot_flag = 1U;
	data_synchronization_barrier();

	/*
	 *  CPUWAIT の bit1（CPU1 のホールド）をクリアして CPU1 を解放する．
	 *  bit0（CPU0=自コア）は 0 のまま維持する．
	 */
	sil_wrw_mem((void *) MUSCA_B1_SYSCTL_CPUWAIT, MUSCA_B1_CPUWAIT_CPU0);
	data_synchronization_barrier();
#endif /* TNUM_PRCID >= 2 */
}

/*
 *  システムログの低レベル出力のための初期化
 */
extern void sio_initialize(EXINF exinf);
extern void target_fput_initialize(void);

/*
 *  ターゲット依存部 初期化処理
 */
void
target_initialize(PCB *p_my_pcb)
{
	/*
	 *  コア依存部の初期化（VTOR・例外優先度・FPU の設定）
	 */
	core_initialize(p_my_pcb);

	/*
	 *  SIO を初期化（マスタプロセッサのみ）
	 */
	if (p_my_pcb->prcid == TOPPERS_MASTER_PRCID) {
		sio_initialize(0);
		target_fput_initialize();
	}
}

/*
 *  ターゲット依存部 終了処理
 */
void
target_exit(void)
{
	extern void	software_term_hook(void);
	void (*volatile fp)(void) = software_term_hook;

	/*
	 *  software_term_hookへのポインタを，一旦volatile指定のあるfpに代
	 *  入してから使うのは，0との比較が最適化で削除されないようにするた
	 *  めである．
	 */
	if (fp != 0) {
		(*fp)();
	}

	/*
	 *  コア依存部の終了処理
	 */
	core_terminate();

#ifdef TOPPERS_USE_QEMU
	/*
	 *  QEMU をセミホスティングで終了させる（SYS_EXIT）．
	 *  実行時は -semihosting-config enable=on を指定すること．
	 */
	{
		register uint32_t r0 __asm__("r0") = 0x18U;       /* SYS_EXIT */
		register uint32_t r1 __asm__("r1") = 0x20026U;    /* ADP_Stopped_ApplicationExit */
		__asm__ volatile ("bkpt 0xab" : : "r"(r0), "r"(r1) : "memory");
	}
#endif /* TOPPERS_USE_QEMU */

	while (1) ;
}

/*
 *  エラー発生時の処理
 */
void
Error_Handler(void)
{
	while (1) ;
}

/*
 *  デフォルトの software_term_hook（弱い定義）
 */
__attribute__((weak)) void
software_term_hook(void)
{
}

/*
 *		システムログの低レベル出力
 */

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
	p_siopcb_target_fput = pl011_uart_opn_por(SIOPID_FPUT, 0);
}

/*
 *  SIOポートへのポーリング出力
 */
static void
musca_b1_uart_fput(char c)
{
	while (!(pl011_uart_snd_chr(p_siopcb_target_fput, c))) {
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
		musca_b1_uart_fput('\r');
	}
	musca_b1_uart_fput(c);
}
