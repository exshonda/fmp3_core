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
 *		チップ依存のスタートアップモジュール（ZynqMP RPU用）
 *
 *  Cortex-R5のlockstep動作のためのレジスタ初期化と，ベクタテーブルの
 *  LOVEC（0x0）への設定を，共通部のstartの前に実行する．
 *  リンカスクリプトのENTRYと，共通部core_support.Sのリセットベクタ
 *  （TARGET_RESET_ENTRYマクロで差替え）から参照される．
 */

#define TOPPERS_MACRO_ONLY
#define TOPPERS_ASM_MACRO
#define UINT_C(val)			(val)		/* uint_t型の定数を作るマクロ */
#define ULONG_C(val)		(val)		/* ulong_t型の定数を作るマクロ */
#define CAST(type, val)		(val)		/* 型キャストを行うマクロ */
#include "kernel_impl.h"
#include "core_asm.inc"

#if defined(USE_ARM_FPU_ALWAYS) || defined(USE_ARM_FPU_SELECTIVE)
	.fpu	ASM_ARM_FPU_TYPE
#endif /* defined(USE_ARM_FPU_ALWAYS) || defined(USE_ARM_FPU_SELECTIVE) */

	ATEXT
	AALIGN(2)
	AGLOBAL(start_r5)
ALABEL(start_r5)
	/*
	 *  lockstep動作のための全レジスタ初期化
	 *
	 *  lockstepの2つのコアは電源投入後のレジスタ値が一致している保証が
	 *  ないため，比較ロジックの誤検出を防ぐために全レジスタを初期化す
	 *  る．QEMUでは不要だが実行しても無害である．
	 */
	mov		r0, #0
	mov		r1, #0
	mov		r2, #0
	mov		r3, #0
	mov		r4, #0
	mov		r5, #0
	mov		r6, #0
	mov		r7, #0
	mov		r8, #0
	mov		r9, #0
	mov		r10, #0
	mov		r11, #0
	mov		r12, #0

	msr		cpsr_cxsf, #(CPSR_IRQ_MODE AOR CPSR_INTLOCK)
	mov		r13, #0
	mov		r14, #0

	msr		cpsr_cxsf, #(CPSR_ABT_MODE AOR CPSR_INTLOCK)
	mov		r13, #0
	mov		r14, #0

	msr		cpsr_cxsf, #(CPSR_FIQ_MODE AOR CPSR_INTLOCK)
	mov		r8, #0
	mov		r9, #0
	mov		r10, #0
	mov		r11, #0
	mov		r12, #0
	mov		r13, #0
	mov		r14, #0

	msr		cpsr_cxsf, #(CPSR_UND_MODE AOR CPSR_INTLOCK)
	mov		r13, #0
	mov		r14, #0

	msr		cpsr_cxsf, #(CPSR_SYS_MODE AOR CPSR_INTLOCK)
	mov		r13, #0
	mov		r14, #0

	msr		cpsr_cxsf, #(CPSR_SVC_MODE AOR CPSR_INTLOCK)
	mov		r13, #0
	mov		r14, #0

#if defined(USE_ARM_FPU_ALWAYS) || defined(USE_ARM_FPU_SELECTIVE)
	/*
	 *  FPU（CP10とCP11）をアクセス可能にし，FPUレジスタも初期化する．
	 */
	ldr		r0, =(CP15_CPACR_CP10_FULLACCESS AOR CP15_CPACR_CP11_FULLACCESS)
	mcr		p15, 0, r0, c1, c0, 2
	asm_inst_sync_barrier r0

	mov		r0, #FPEXC_ENABLE
	vmsr	fpexc, r0
	asm_inst_sync_barrier r0

	mov		r1, #0
	vmov	d0, r1, r1
	vmov	d1, r1, r1
	vmov	d2, r1, r1
	vmov	d3, r1, r1
	vmov	d4, r1, r1
	vmov	d5, r1, r1
	vmov	d6, r1, r1
	vmov	d7, r1, r1
	vmov	d8, r1, r1
	vmov	d9, r1, r1
	vmov	d10, r1, r1
	vmov	d11, r1, r1
	vmov	d12, r1, r1
	vmov	d13, r1, r1
	vmov	d14, r1, r1
	vmov	d15, r1, r1
#endif /* defined(USE_ARM_FPU_ALWAYS) || defined(USE_ARM_FPU_SELECTIVE) */

	/*
	 *  ベクタテーブルをローベクタ（0x0）に設定（SCTLR.Vクリア）
	 *
	 *  実機はブート時にVINITHI=0（LOVEC）となるが，QEMU（xlnx-zcu102）
	 *  のR5Fはreset-hivecs=trueで起動するため明示的にクリアする．
	 *  実機では冪等な操作である．
	 */
	mrc		p15, 0, r0, c1, c0, 0
	bic		r0, r0, #CP15_SCTLR_VECTOR
	mcr		p15, 0, r0, c1, c0, 0
	asm_inst_sync_barrier r0

	/*
	 *  共通部のスタートアップモジュールへ
	 */
	b		start
