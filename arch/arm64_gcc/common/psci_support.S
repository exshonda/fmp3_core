/*
 *  TOPPERS/FMP Kernel
 *      Toyohashi Open Platform for Embedded Real-Time Systems/
 *      Flexible MultiProcessor Kernel
 *
 *  Copyright (C) 2018 by Embedded and Real-Time Systems Laboratory
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
 *  @(#) $Id: psci_support.S 216 2020-02-17 16:17:15Z ertl-honda $
 */

#define TOPPERS_MACRO_ONLY
#define TOPPERS_ASM_MACRO
#include "kernel_impl.h"
#include "core_asm.inc"

/*
 *		PSCI SMCサービルコール
 */
/*
 * [C-lang function] CPUON: Turn on a cpu core from NS-EL1
 *
 * Argument :
 *   [In]  X0 - Target cpu core
 *                bit63-40 : 0
 *                bit39-32 : Aff3 of target core MPIDR
 *                bit31-24 : 0
 *                bit23-16 : Aff2 of target core MPIDR
 *                bit15-8  : Aff1 of target core MPIDR
 *                bit7-0   : Aff0 of target core MPIDR
 *   [In]  X1 - Entry points address
 *   [In]  X2 - context_id
 *
 * Return value:
 *   0 -           Success
 *   Less than 0 - Failed
 */
	AALIGN(4)
	AGLOBAL(psci_smc_cpuon)
ALABEL(psci_smc_cpuon)
	/* Execute SMC Function call */
	mov   x3, x2
	mov   x2, x1
	mov   x1, x0
	mov   x0, #0x0003   /* PSCISMC_SET_CPUON */
	movk  x0, #((1 << 15) | (1 << 14) | (4 << 8)), lsl #16
	dsb   sy
	smc   #0
	ret
