/*
 *  TOPPERS/FMP Kernel
 *      Toyohashi Open Platform for Embedded Real-Time Systems/
 *      Flexible MultiProcessor Kernel
 * 
 *  Copyright (C) 2000-2003 by Embedded and Real-Time Systems Laboratory
 *                              Toyohashi Univ. of Technology, JAPAN
 *  Copyright (C) 2006-2023 by Embedded and Real-Time Systems Laboratory
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
 *  $Id: chip_kernel_impl.h 335 2023-04-18 10:50:40Z ertl-honda $
 */

/*
 *		kernel_impl.hのチップ依存部（Zynq7000用）
 *
 *  このヘッダファイルは，target_kernel_impl.h（または，そこからインク
 *  ルードされるファイル）のみからインクルードされる．他のファイルから
 *  直接インクルードしてはならない．
 */

#ifndef TOPPERS_CHIP_KERNEL_IMPL_H
#define TOPPERS_CHIP_KERNEL_IMPL_H

/*
 *  Zynq7000のハードウェア資源の定義
 */
#include "zynq7000.h"
#include "pl310.h"

/*
 *  Use the dispatch_handler bypass processing
 *
 *  Note: After one core (e.g. core0) changes the status of a task
 *  in another core (e.g., act_tsk(CORE1TASK)) usually an interrupt is
 *  sent to that core (e.g., core1) for dispatching. If we use the
 *  USE_IPI_DIS_HANDER_BYPASS macro, instead of going to an interrupt
 *  handler the dispatcher will be called automatically for reducing
 *  overhead.
 */
//#define USE_DISPATCH_HANDER_BYPASS

/*
 *  デフォルトの非タスクコンテキスト用のスタック領域の定義
 */
#ifndef DEFAULT_ISTKSZ
#define DEFAULT_ISTKSZ  0x2000U			/* 8KB */
#endif /* DEFAULT_ISTKSZ */

/*
 *  FPUに関する設定
 */
#define ASM_ARM_FPU_TYPE	vfpv3

/*
 *  デフォルトのアイドル処理用のスタック領域の定義
 */
#define DEFAULT_IDSTKSZ		0x0100U

/*
 *  ARM Cortex-A9 GTC Errataへの対策を実施
 */
#define ARM_CA9_GTC_ERRATA

/*
 *  MPCoreで共通な定義
 */
#include "mpcore_kernel_impl.h"

#ifndef TOPPERS_MACRO_ONLY

/*
 *  チップ依存の初期化
 */
extern void chip_initialize(PCB *p_my_pcb);

/*
 *  チップ依存の終了処理
 */
extern void chip_terminate(void);

/*
 *  L2キャッシュのイネーブル
 */
Inline void
arm_enable_outer_cache(void) 
{
	pl310_initialize(0x0U, ~0x0U);
}

/*
 *  L2キャッシュのディスエーブル
*/
Inline void
arm_disable_outer_cache(void)
{
	pl310_disable();
}

/*
 *  L2キャッシュの無効化
 */
Inline void
arm_invalidate_outer_cache(void)
{
	pl310_invalidate_all();
}

/*
 *  L2キャッシュのクリーン
 */
Inline void
arm_clean_outer_cache(void) 
{
	pl310_clean_and_invalidate_all();
}

#endif /* TOPPERS_MACRO_ONLY */
#endif /* TOPPERS_CHIP_KERNEL_IMPL_H */
