/*
 *  ターゲット依存モジュール（RP2350 / RaspberryPi Pico 2 / FMP3 用）
 *
 *  pico-sdk 非依存（自己完結）の RP2350 ターゲット依存部．
 *
 *  ・hardware_init_hook()：クロック（XOSC→PLL 150MHz）・TICKS ブロック・
 *    UART 用 GPIO/PADS の初期化（マスタコアのみ）と，IMAGE_DEF ブロックの
 *    リンク強制．
 *  ・target_mprc_initialize()：RP2350 bootROM の multicore launch コマンド列
 *    （0,0,1,VTOR,SP,entry）を SIO FIFO で送信して Core1 を起動する．
 *  ・自コア番号は SIO CPUID で得る（chip_asm.inc / chip_kernel_impl.h）．
 */
#include "kernel_impl.h"
#include "target_syssvc.h"
#include "rpi_pico2.h"
#include <sil.h>
#include "RP2350.h"
#ifdef TOPPERS_OMIT_TECS
#include "target_serial.h"
#endif /* TOPPERS_OMIT_TECS */

/*
 *  IMAGE_DEF ブロック（image_def.S）
 *
 *  libkernel.a 中に置かれるため，参照されないとリンカがアーカイブから
 *  取り出さず，IMAGE_DEF が ELF に入らない（KEEP() だけでは抽出を強制
 *  できない）．hardware_init_hook() で参照してリンクを強制する．
 */
extern volatile int image_def_block;

/*
 *  コンフィギュレータ生成シンボル
 */
extern const FP  _kernel_vector_table_prc1[];
#if TNUM_PRCID >= 2
extern const FP  _kernel_vector_table_prc2[];
#endif /* TNUM_PRCID >= 2 */
extern STK_T *const _kernel_istkpt_table[TNUM_PRCID];

/*
 *  M33 CPACR の FPU 有効化ビット（CP10/CP11 フルアクセス）
 */
#define RP2350_M33_CPACR_FPU_ENABLE (RP2350_M33_CPACR_CP10_BITS | (RP2350_M33_CPACR_CP10_BITS << 2))

/*
 *  RP2350 のクロック初期化（XOSC→PLL_SYS 150MHz，clk_peri/TICKS 設定）
 */
static void
rp2350_clock_initialize(void)
{
	/*
	 *  PLL_SYS / PADS_QSPI / IO_QSPI を除く周辺をリセット解除する
	 *  （XIP 実行中のフラッシュアクセスを壊さないため除外）．
	 */
	uint32_t reset_mask = RP2350_RESETS_RESET_BITS
		& ~(RP2350_RESETS_RESET_PLL_SYS
			| RP2350_RESETS_RESET_PADS_QSPI
			| RP2350_RESETS_RESET_IO_QSPI);
	sil_clrw(RP2350_RESETS_RESET, reset_mask);

	/*
	 *  XOSC（12MHz）を起動する．
	 */
	if ((sil_rew_mem(RP2350_XOSC_STATUS) & RP2350_XOSC_STATUS_STABLE) == 0U) {
		sil_wrw_mem(RP2350_XOSC_CTRL, RP2350_XOSC_CTRL_FREQ_RANGE_MAGIC);
		sil_wrw_mem(RP2350_XOSC_STARTUP, 47); /* ~1ms */
		sil_orw(RP2350_XOSC_CTRL, RP2350_XOSC_CTRL_ENABLE_MAGIC);
		while ((sil_rew_mem(RP2350_XOSC_STATUS) & RP2350_XOSC_STATUS_STABLE) == 0U) ;
	}

	/*
	 *  PLL_SYS を 12MHz × 125（VCO 1500MHz）÷5 ÷2 = 150MHz に設定する．
	 */
	sil_orw(RP2350_RESETS_RESET, RP2350_RESETS_RESET_PLL_SYS);
	sil_clrw(RP2350_RESETS_RESET, RP2350_RESETS_RESET_PLL_SYS);
	while ((sil_rew_mem(RP2350_RESETS_RESET_DONE) & RP2350_RESETS_RESET_PLL_SYS) == 0U) ;

	sil_wrw_mem(RP2350_PLL_SYS_CS, RP2350_PLL_SYS_CS_REFDIV(1));
	sil_wrw_mem(RP2350_PLL_SYS_FBDIV_INT, 125);
	sil_clrw(RP2350_PLL_SYS_PWR, RP2350_PLL_SYS_PWR_PD | RP2350_PLL_SYS_PWR_VCOPD);
	while ((sil_rew_mem(RP2350_PLL_SYS_CS) & RP2350_PLL_SYS_CS_LOCK) == 0U) ;
	sil_wrw_mem(RP2350_PLL_SYS_PRIM,
				RP2350_PLL_SYS_PRIM_POSTDIV1(5) | RP2350_PLL_SYS_PRIM_POSTDIV2(2));
	sil_wrw_mem(RP2350_PLL_SYS_PWR, 0);

	/*
	 *  clk_ref→XOSC，clk_sys→PLL_SYS（分周整数フィールドは RP2350 では
	 *  bit16〜）．
	 */
	sil_wrw_mem(RP2350_CLOCKS_CLK_REF_CTRL, RP2350_CLOCKS_CLK_REF_CTRL_SRC_XOSC);
	sil_wrw_mem(RP2350_CLOCKS_CLK_REF_DIV, (1U << 16));
	while ((sil_rew_mem(RP2350_CLOCKS_CLK_REF_SELECTED) & 0x4U) == 0U) ;

	sil_wrw_mem(RP2350_CLOCKS_CLK_SYS_CTRL, RP2350_CLOCKS_CLK_SYS_CTRL_SRC_REF);
	sil_wrw_mem(RP2350_CLOCKS_CLK_SYS_DIV, (1U << 16));
	sil_wrw_mem(RP2350_CLOCKS_CLK_SYS_CTRL,
				RP2350_CLOCKS_CLK_SYS_CTRL_AUXSRC_PLL_SYS | RP2350_CLOCKS_CLK_SYS_CTRL_SRC_AUX);

	/*
	 *  TICKS ブロックを設定する（RP2350 では TIMER0 はこれを設定しないと
	 *  歩進しない．clk_ref 12MHz / 12 = 1MHz）．
	 */
	while ((sil_rew_mem(RP2350_RESETS_RESET_DONE) & RP2350_RESETS_RESET_TIMER0) == 0U) ;
	sil_wrw_mem(RP2350_TIMER0_SOURCE, RP2350_TIMER0_SOURCE_TICK);
	sil_wrw_mem(RP2350_TICKS_TIMER0_CYCLES, 12);
	sil_wrw_mem(RP2350_TICKS_TIMER0_CTRL, RP2350_TICKS_TIMER0_CTRL_ENABLE);
	while ((sil_rew_mem(RP2350_TICKS_TIMER0_CTRL) & RP2350_TICKS_TIMER0_CTRL_RUNNING) == 0U) ;
}

/*
 *  UART0（GP0=TX/GP1=RX，FUNCSEL=2）用の GPIO/PADS 初期化
 */
static void
rp2350_uart_gpio_initialize(void)
{
	/* IO_BANK0 / PADS_BANK0 のリセット解除 */
	sil_clrw(RP2350_RESETS_RESET, RP2350_RESETS_RESET_IO_BANK0 | RP2350_RESETS_RESET_PADS_BANK0);
	while ((sil_rew_mem(RP2350_RESETS_RESET_DONE)
			& (RP2350_RESETS_RESET_IO_BANK0 | RP2350_RESETS_RESET_PADS_BANK0))
			!= (RP2350_RESETS_RESET_IO_BANK0 | RP2350_RESETS_RESET_PADS_BANK0)) ;

	/* GP0=UART0 TX, GP1=UART0 RX（FUNCSEL=2） */
	sil_wrw_mem(RP2350_IO_BANK0_GPIO_CTRL(0), 2);
	sil_wrw_mem(RP2350_IO_BANK0_GPIO_CTRL(1), 2);

	/* PADS の ISO 解除と IE セット（RP2350 固有） */
	sil_orw(RP2350_PADS_BANK0_GPIO(0), RP2350_PADS_BANK0_GPIOX_IE);
	sil_clrw(RP2350_PADS_BANK0_GPIO(0), RP2350_PADS_BANK0_GPIOX_ISO | RP2350_PADS_BANK0_GPIOX_OD);
	sil_orw(RP2350_PADS_BANK0_GPIO(1), RP2350_PADS_BANK0_GPIOX_IE);
	sil_clrw(RP2350_PADS_BANK0_GPIO(1), RP2350_PADS_BANK0_GPIOX_ISO | RP2350_PADS_BANK0_GPIOX_OD);
}

/*
 *  起動時のハードウェア初期化処理
 *
 *  start.S から，BSS/DATA の初期化に先立って各コアで呼び出される．クロック
 *  等の初期化はマスタコア（SIO CPUID==0）でのみ行い，Core1 では何もしない．
 */
void
hardware_init_hook(void)
{
	/* IMAGE_DEF ブロックのリンクを強制 */
	if (image_def_block) {
		;
	}

#if TNUM_PRCID >= 2
	if ((*RP2350_SIO_CPUID) != 0U) {
		/* Core1 ではハードウェア初期化を行わない */
		return;
	}
#endif /* TNUM_PRCID >= 2 */

	rp2350_clock_initialize();
	rp2350_uart_gpio_initialize();

#ifdef TOPPERS_FPU_ENABLE
	/*
	 *  FPU を有効化する．アトミックエイリアスは M33 PPB に効かないため
	 *  直接 read-modify-write する．
	 */
	sil_wrw_mem(RP2350_M33_CPACR,
				sil_rew_mem(RP2350_M33_CPACR) | RP2350_M33_CPACR_FPU_ENABLE);
#endif /* TOPPERS_FPU_ENABLE */
}

/*
 *  ソフトウェア環境（ライブラリ等）の初期化処理
 */
void
software_init_hook(void)
{
}

#if TNUM_PRCID >= 2
/*
 *  SIO FIFO 1 ワード送信（送信可能になるまで待つ）
 */
Inline void
sio_fifo_push(uint32_t data)
{
	while ((sil_rew_mem(RP2350_SIO_FIFO_ST) & RP2350_SIO_FIFO_ST_RDY) == 0U) ;
	sil_wrw_mem(RP2350_SIO_FIFO_WR, data);
	Asm("sev"); /* Core1 を WFE から起こす */
}

/*
 *  SIO FIFO 1 ワード受信（受信できるまで待つ）
 */
Inline uint32_t
sio_fifo_pop(void)
{
	while ((sil_rew_mem(RP2350_SIO_FIFO_ST) & RP2350_SIO_FIFO_ST_VLD) == 0U) {
		Asm("wfe");
	}
	return sil_rew_mem(RP2350_SIO_FIFO_RD);
}

/*
 *  受信 FIFO を空にする
 */
Inline void
sio_fifo_drain(void)
{
	while ((sil_rew_mem(RP2350_SIO_FIFO_ST) & RP2350_SIO_FIFO_ST_VLD) != 0U) {
		(void) sil_rew_mem(RP2350_SIO_FIFO_RD);
	}
}
#endif /* TNUM_PRCID >= 2 */

/*
 *  マスタプロセッサ依存の初期化（二次コア Core1 の起動）
 *
 *  RP2350 の bootROM は，リセット後 Core1 を SIO FIFO 待ち受けループに入れる．
 *  pico-sdk の multicore_launch_core1 と同じハンドシェイク列
 *  （0, 0, 1, VTOR, SP, entry）を送って Core1 を起動する．各ワードは Core1 の
 *  bootROM がエコーバックし，一致しなければ最初からやり直す．
 *
 *  Core1 の起動先（entry）は _kernel_start とする．Core1 は SIO CPUID==1 を
 *  読んで自分が PRC2 であると判定し，BSS/DATA 初期化を行わず sta_ker へ進む．
 */
void
target_mprc_initialize(void)
{
#if TNUM_PRCID >= 2
	extern void _kernel_start(void);

	uint32_t vtor  = (uint32_t)(uintptr_t) _kernel_vector_table_prc2;
	uint32_t sp    = (uint32_t)(uintptr_t) _kernel_istkpt_table[PRC2 - 1];
	uint32_t entry = ((uint32_t)(uintptr_t) _kernel_start) | 1U; /* Thumb ビット */

	const uint32_t cmd_seq[6] = { 0U, 0U, 1U, vtor, sp, entry };
	uint_t seq = 0;

	do {
		uint32_t cmd = cmd_seq[seq];
		if (cmd == 0U) {
			sio_fifo_drain();
			Asm("sev");
		}
		sio_fifo_push(cmd);
		uint32_t resp = sio_fifo_pop();
		seq = (cmd == resp) ? (seq + 1) : 0;
	} while (seq < 6);
#endif /* TNUM_PRCID >= 2 */
}

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
#ifdef TOPPERS_OMIT_TECS
		sio_initialize(0);
		(void) sio_opn_por(SIOPID_FPUT, 0);
#endif /* TOPPERS_OMIT_TECS */
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
