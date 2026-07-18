/*
 *		cfg1_out.cのリンクに必要なスタブの定義
 *
 *  $Id: core_cfg1_out.h 214 2020-02-15 19:05:52Z ertl-honda $
 */

#include <kernel.h>

void sta_ker(void){}
void _kernel_target_el3_initialize(void){}
void _kernel_target_el2_initialize(void){}
void _kernel_target_mprc_initialize(void){}
uint_t _kernel_start_sync;
STK_T *const	_kernel_istkpt_table[TNUM_PRCID];
