/*
 *  cfg1_out.c をリンクするために必要なスタブの定義（ARM Musca-B1 用）
 *
 *  cfg1_out は start.o と cfg1_out.o のみをリンクして生成される（カーネル
 *  本体やターゲット依存部の .o はリンクしない）ため，start.S が参照するシン
 *  ボルのうち，それらの .o でしか定義されないものをスタブとして与える．
 *  なお _kernel_start は start.S が定義するため，ここでは定義しない．
 */

#include <kernel.h>

/*
 *  start.S が参照するシンボルのスタブ
 */
STK_T *const _kernel_istkpt_table[TNUM_PRCID];
void hardware_init_hook(void){}
void software_init_hook(void){}
void _kernel_target_mprc_initialize(void){}

#if TNUM_PRCID >= 2
/*
 *  マルチプロセッサ起動で start.S が参照するシンボルのスタブ
 *    _kernel_istk_table   : my_prcidx（istack 範囲判定）が参照
 *    _kernel_mp_boot_flag : my_prcidx_boot（ブートハンドシェイク）が参照
 */
STK_T *const _kernel_istk_table[TNUM_PRCID];
volatile uint32_t _kernel_mp_boot_flag;
#endif /* TNUM_PRCID >= 2 */

/*
 *  チップ依存のスタブ定義（_kernel_istkpt 等）
 */
#include <chip_cfg1_out.h>
