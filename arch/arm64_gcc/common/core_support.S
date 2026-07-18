/*
 *  TOPPERS/FMP Kernel
 *      Toyohashi Open Platform for Embedded Real-Time Systems/
 *      Flexible MultiProcessor Kernel
 *
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
 *  @(#) $Id: core_support.S 461 2026-05-06 12:29:29Z ertl-honda $
 */

/*
 *		カーネルのコア依存部のアセンブリ言語部（ARM64用）
 */

#define TOPPERS_MACRO_ONLY
#define TOPPERS_ASM_MACRO
#define UINT_C(val)			(val)		/* uint_t型の定数を作るマクロ */
#define ULONG_C(val)		(val)		/* ulong_t型の定数を作るマクロ */
#define CAST(type, val)		(val)		/* 型キャストを行うマクロ */
#include "kernel_impl.h"
#include "target_ipi.h"
#include "offset.h"
#include "target_asm.inc"

/*
 *  例外ベクタ
 */
	ASECTION(vector)
	AALIGN(11)
	AGLOBAL(vector_table)
ALABEL(vector_table)
	b		cur_sp0_sync_handler
	ABALIGN(0x80)
	b		cur_sp0_irq_handler
	ABALIGN(0x80)
	b		cur_sp0_fiq_handler
	ABALIGN(0x80)
	b		cur_sp0_serr_handler
	ABALIGN(0x80)
	b		cur_spx_sync_handler
	ABALIGN(0x80)
	b		cur_spx_irq_handler
	ABALIGN(0x80)
	b		cur_spx_fiq_handler
	ABALIGN(0x80)
	b		cur_spx_serr_handler
	ABALIGN(0x80)
	b		l64_sync_handler
	ABALIGN(0x80)
	b		l64_irq_handler
	ABALIGN(0x80)
	b		l64_fiq_handler
	ABALIGN(0x80)
	b		l64_serr_handler
	ABALIGN(0x80)
	b		l32_sync_handler
	ABALIGN(0x80)
	b		l32_irq_handler
	ABALIGN(0x80)
	b		l32_fiq_handler
	ABALIGN(0x80)
	b		l32_serr_handler

/*
 *  タスクディスパッチャ
 */
	ATEXT
	AALIGN(2)
	AGLOBAL(dispatch)
ALABEL(dispatch)
	/*
	 *  このルーチンは，タスクコンテキスト・CPUロック状態・割込み優先度
	 *  マスク全解除状態・ディスパッチ許可状態で呼び出される．
	 */
	sub		sp, sp, #(16 * 6)
	stp		x29, x30, [sp, #0x50]	/* 戻り番地（x30(lr)）を保存 */
#ifdef TOPPERS_SUPPORT_OVRHDR
	bl		ovrtimer_stop
#endif /* TOPPERS_SUPPORT_OVRHDR */
	stp		x19, x20, [sp, #0x00]	/* 非スクラッチレジスタの保存 */
	stp		x21, x22, [sp, #0x10]
	stp		x23, x24, [sp, #0x20]
	stp		x25, x26, [sp, #0x30]
	stp		x27, x28, [sp, #0x40]
#ifdef USE_ARM64_FPU
	sub		sp, sp, #(16 * 8)		/* 非スクラッチFPUレジスタの保存 */
	stp		q8, q9, [sp, #0x00]
	stp		q10, q11, [sp, #0x20]
	stp		q12, q13, [sp, #0x40]
	stp		q14, q15, [sp, #0x60]
#endif /* USE_ARM64_FPU */
	my_pcb	x8, x0
	ldr		x0, [x8, #PCB_p_runtsk]	/* p_runtsk → x0 */
	mov		x1, sp					/* スタックポインタを保存 */
	str		x1, [x0, #TCB_sp]
	adr		x1, dispatch_r
	str		x1, [x0, #TCB_pc]		/* 実行再開番地を保存 */
	b		dispatcher

ALABEL(dispatch_r)
#ifdef USE_ARM64_FPU
	ldp		q14, q15, [sp, #0x60]	/* 非スクラッチFPUレジスタの復帰 */
	ldp		q12, q13, [sp, #0x40]
	ldp		q10, q11, [sp, #0x20]
	ldp		q8, q9, [sp, #0x00]
	add		sp, sp, #(16 * 8)
#endif /* USE_ARM64_FPU */
	ldp		x27, x28, [sp, #0x40]	/* 非スクラッチレジスタの復帰 */
	ldp		x25, x26, [sp, #0x30]
	ldp		x23, x24, [sp, #0x20]
	ldp		x21, x22, [sp, #0x10]
	ldp		x19, x20, [sp, #0x00]
#ifdef TOPPERS_SUPPORT_OVRHDR
	bl		ovrtimer_start
#endif /* TOPPERS_SUPPORT_OVRHDR */
	ldp		x29, x30, [sp, #0x50]	/* 戻り番地（x30(lr)）を復帰 */
	add		sp, sp, #(16 * 6)
	ret

/*
 *  ディスパッチャの動作開始
 */
	AGLOBAL(start_dispatch)
ALABEL(start_dispatch)
	/*
	 *  このルーチンは，カーネル起動時に，非タスクコンテキストで，NMIを
	 *  除くすべての割込みを禁止した状態（全割込みロック状態と同等）で
	 *  呼び出される．
	 *
	 *  dispatcher_0へ分岐する前に，タスクコンテキスト・CPUロック状態・
	 *  割込み優先度マスク全解除状態になければならない．
	 */

	/*
	 *  各種のデバイス（特に割込みコントローラ）の設定が完了するのを待つ．
	 */
	dsb		sy

	/*
	 *	タスクコンテキストに切り換える．
	 */
	my_pcb	x0, x1
	str		wzr, [x0, #PCB_excpt_nest_count]	/* 例外ネストカウントを0に */

	/*
	 *  CPUロック状態にして，ディスパッチャ本体へ分岐する．
	 */
	msr		DAIFClr, #DAIFSCBIT(DAIF_MASK AAND ANOT(DAIF_CPULOCK))
	b		dispatcher_0

/*
 *  現在のコンテキストを捨ててディスパッチ
 */
	AGLOBAL(exit_and_dispatch)
ALABEL(exit_and_dispatch)
	/*
	 *  このルーチンは，タスクコンテキスト・CPUロック状態・割込み優先度
	 *  マスク全解除状態・ディスパッチ許可状態で呼び出される．
	 */
#ifdef LOG_DSP_ENTER
	my_pcb	x2, x1
	ldr		x0, [x2,#PCB_p_runtsk]	/* p_runtsk → x0 */
	ldr		x1, [x2,#PCB_istkpt]	/* 非タスクコンテキスト用スタックに */
	mov		sp, x1
#endif /* LOG_DSP_ENTER */
	/* ディスパッチャ本体（dispatcher）へ */

/*
 *  ディスパッチャ本体
 */
ALABEL(dispatcher)
#ifdef LOG_DSP_ENTER
	/*
	 * 【この時点のレジスタ状態】
	 *  x0：p_runtsk
	 */
	bl		log_dsp_enter
#endif /* LOG_DSP_ENTER */

ALABEL(dispatcher_0)
	/*
	 *  このルーチンは，タスクコンテキスト・CPUロック状態・割込み優先度
	 *  マスク全解除状態・ディスパッチ許可状態で呼び出される．実行再開番
	 *  地へもこの状態のまま分岐する．
	 */
	my_pcb	x0, x1
	ldr		x19, [x0, #PCB_p_schedtsk]	/* p_schedtsk → x19 → p_runtsk */
	str		x19, [x0, #PCB_p_runtsk]
	cbz		x19, dispatcher_1			/* p_runtskがNULLならdispatcher_1へ */
	ldr		x0, [x19, #TCB_sp]			/* タスクスタックを復帰 */
	mov		sp, x0
#ifdef LOG_DSP_LEAVE
	mov		x0, x19				/* 引数 */
	bl		log_dsp_leave
#endif /* LOG_DSP_LEAVE */
	ldr		x0, [x19, #TCB_pc]	/* 実行再開番地を復帰 */
	br		x0

	/*
	 *  アイドル処理
	 *
	 *  割込みをすべて許可し，CPUロック解除状態にして割込みを待つ．
	 */
ALABEL(dispatcher_1)
	/*
	 *  CPUロック状態の解除と，非タスクコンテキスト実行状態への
	 *  準備をする
	 */
	my_pcb	x0, x1
	ldr		x1, [x0, #PCB_idstkpt]	/* アイドルスタックへ */
	mov		sp, x1

	/*
	 *  ターゲットによっては，省電力モード等に移行するため，標準の方法
	 *  と異なる手順が必要な場合がある．そのようなターゲットでは，ター
	 *  ゲット依存部でTOPPERS_CUSTOM_IDLEを定義し，アセンブラマクロとし
	 *  て，toppers_asm_custom_idleを用意すればよい．
	 */
ALABEL(dispatcher_2)
#ifdef TOPPERS_CUSTOM_IDLE
	toppers_asm_custom_idle
#else /* TOPPERS_CUSTOM_IDLE */
	msr		DAIFClr, #DAIFSCBIT(DAIF_CPULOCK) 
#endif /* TOPPERS_CUSTOM_IDLE */
	b		dispatcher_2			/* 割込み待ち */

/*
 *  カーネルの終了処理の呼出し
 *   引数1(x0) : p_my_pcb
 *
 *  使用するスタックを非タスクコンテキスト用のスタック領域に切り替え，
 *  例外ネストカウントを1にして，exit_kernelを呼び出す．
 */
	AGLOBAL(call_exit_kernel)
ALABEL(call_exit_kernel)
	ldr		x1, [x0, #PCB_istkpt]	/* 非タスクコンテキスト用スタックに */
	mov		sp, x1
	mov		x1, #1
	str		x1, [x0, #PCB_excpt_nest_count]	/* 例外ネストカウントを1に */
	b		exit_kernel

/*
 *  自タスクのマイグレート
 *   引数1(x0) : p_my_pcb
 *   引数2(x1) : p_selftsk
 */
	ATEXT
	AALIGN(2)
	AGLOBAL(dispatch_and_migrate)
ALABEL(dispatch_and_migrate)
	sub		sp, sp, #(16 * 6)			/* 非スクラッチレジスタ用の領域の確保 */
	stp		x29, x30, [sp, #0x50]		/* 戻り番地（x30(lr)）を保存 */
#ifdef TOPPERS_SUPPORT_OVRHDR
	sub		sp, sp, #(16 * 2)
	stp		x0, x1, [sp, #0x00]
	stp		x2, x3, [sp, #0x10]
	bl		ovrtimer_stop
	ldp		x2, x3, [sp, #0x10]
	ldp		x0, x1, [sp, #0x00]
	add		sp, sp, #(16 * 2)
#endif /* TOPPERS_SUPPORT_OVRHDR */
	stp		x19, x20, [sp, #0x00]		/* 非スクラッチレジスタの保存 */
	stp		x21, x22, [sp, #0x10]
	stp		x23, x24, [sp, #0x20]
	stp		x25, x26, [sp, #0x30]
	stp		x27, x28, [sp, #0x40]
#ifdef USE_ARM64_FPU
	sub		sp, sp, #(16 * 8)			/* 非スクラッチFPUレジスタの保存 */
	stp		q8, q9, [sp, #0x00]
	stp		q10, q11, [sp, #0x20]
	stp		q12, q13, [sp, #0x40]
	stp		q14, q15, [sp, #0x60]
#endif /* USE_ARM64_FPU */
	mov		x3, sp
	str		x3, [x1, #TCB_sp]		/* タスクスタックを保存 */
	adr		x3, dispatch_r			/* 実行再開番地を保存 */
	str		x3, [x1, #TCB_pc]		/* 実行再開番地を保存 */
	ldr		x3, [x0, #PCB_istkpt]
	mov		sp, x3					/* 非タスクコンテキスト用スタックに */
	b		migrate_self			/* 自タスクのマイグレート */

/*
 *  現在のコンテキストを捨ててマイグレート
 *   引数1(x0) : p_my_pcb
 *   引数2(x1) : p_selftsk
 */
	ATEXT
	AALIGN(2)
	AGLOBAL(exit_and_migrate)
ALABEL(exit_and_migrate)
	ldr		x2, [x0, #PCB_istkpt]
	mov		sp, x2					/* 非タスクコンテキスト用スタックに */
	b		migrate_activate_self	/* 自タスクのマイグレートと起動 */

/*
 *  タスクの実行開始時処理
 */
	ATEXT
	AALIGN(2)
	AGLOBAL(start_r)
	/*
	 * 【この時点のレジスタ状態】
	 *  x19：p_runtsk（タスク切換え後）
	 */
ALABEL(start_r)
#ifdef TOPPERS_SUPPORT_OVRHDR
	bl		ovrtimer_start
#endif /* TOPPERS_SUPPORT_OVRHDR */
	msr		DAIFClr, #DAIFSCBIT(DAIF_CPULOCK) 
	ldr		x30, =ext_tsk			/* タスク本体からの戻り番地を設定 */
	ldr		x2, [x19,#TCB_p_tinib]	/* p_runtsk->p_tinib → x2 */
	ldr		x0, [x2, #TINIB_exinf]	/* exinfをパラメータに */
	ldr		x1, [x2, #TINIB_task]	/* タスク起動番地にジャンプ */
	br		x1

/*
 *  割込みハンドラの出入口処理
 *  
 *  x19,x20はノンスクラッチレジスタであるが，割込みハンドラの呼び出しを
 *  を跨いで割込み番号をx19に保持するために保存する．
 */
#ifndef OMIT_IRQ_HANDLER
	ATEXT
	AALIGN(2)
	AGLOBAL(cur_spx_irq_handler)
ALABEL(cur_spx_irq_handler)
	/*
	 *  スクラッチレジスタ+αを保存する
	 */
	sub		sp, sp, #(16 * 11)
	stp		x0, x1, [sp, #0x00]
	stp		x2, x3, [sp, #0x10]
	stp		x4, x5, [sp, #0x20]
	stp		x6, x7, [sp, #0x30]
	stp		x8, x9, [sp, #0x40]
	stp		x10, x11, [sp, #0x50]
	stp		x12, x13, [sp, #0x60]
	stp		x14, x15, [sp, #0x70]
	stp		x16, x17, [sp, #0x80]
	stp		x18, x30, [sp, #0x90]
	stp		x19, x20, [sp, #0xa0]

	/*
	 *  elr(戻り先), spsrを保存．
	 */
	mrs		x0, elr_el1
	mrs		x1, spsr_el1
	stp		x0, x1, [sp, #-16]!

#ifdef USE_ARM64_FPU
	sub		sp, sp, #(16 * 26)		/* スクラッチFPUレジスタの保存 */
	mrs		x9, fpsr
	str		x9, [sp, #0x00]
	mrs		x10, fpcr
	str		x10, [sp, #0x10]
	stp		q0, q1, [sp, #0x20]
	stp		q2, q3, [sp, #0x40]
	stp		q4, q5, [sp, #0x60]
	stp		q6, q7, [sp, #0x80]
	stp		q16, q17, [sp, #0xa0]
	stp		q18, q19, [sp, #0xc0]
	stp		q20, q21, [sp, #0xe0]
	stp		q22, q23, [sp, #0x100]
	stp		q24, q25, [sp, #0x120]
	stp		q26, q27, [sp, #0x140]
	stp		q28, q29, [sp, #0x160]
	stp		q30, q31, [sp, #0x180]
#endif /* USE_ARM64_FPU */

	/*
	 *  カーネル管理の割込みのみを禁止
	 */
	msr		DAIFClr, #DAIFSCBIT(DAIF_MASK AAND ANOT(DAIF_CPULOCK))

	/*
	 *  スタックポインタの調整
	 */
	mov		x2, sp
	and		x1, x2, #8
	sub		sp, sp, x1
	stp		x0, x1, [sp, #-16]!	/* スタックポインタの調整値を保存 */
								/* x0はアライメント調整のため */

	/*
	 *  例外ネストカウントをインクリメントする．割込みが非タスクコンテキ
	 *  ストで発生した場合には，irq_handler_1へ分岐する．
	 */
	my_pcb	x0, x1
	ldr		w3, [x0, #PCB_excpt_nest_count]
	add		w3, w3, #1
	str		w3, [x0, #PCB_excpt_nest_count]
	cmp		x3, #1			/* 割込み発生前が非タスクコンテキスト(x3>1) */
	b.ne	irq_handler_1	/* ならirq_handler_1に分岐 */


#ifdef TOPPERS_SUPPORT_OVRHDR
	/*
	 *  オーバランタイマを停止する．
	 */
	bl		ovrtimer_stop
#endif /* TOPPERS_SUPPORT_OVRHDR */

	/*
	 *  非タスクコンテキスト用のスタックに切り換える．
	 */
	mov		x3, sp					/* この時点のスタックポインタをx3に */
	my_pcb	x2, x1
	ldr		x1, [x2, #PCB_istkpt]	/* 非タスクコンテキスト用スタックに */
	mov		sp, x1
	stp		x0, x3, [sp, #-16]!		/* 切換え前のスタックポインタを保存 */
									/* x0はアライメント制約のためのダミー */
ALABEL(irq_handler_1)
	/*
	 *  割込みコントローラを操作し，割込み番号を取得する．
	 *
	 *  irc_begin_intは，x0に割込みハンドラ番号を入れてリターンする
	 */
	bl		irc_begin_int
	mov		x19, x0					/* 関数呼び出しをまたいで保持するためノンスクラッチレジスタx19に保存 */
#ifdef USE_BYPASS_IPI_DISPATCH_HANDER
	cmp		w19, #IPINO_DISPATCH
	b.eq	irq_handler_2			/* ディスパッチ要求なら */
									/*  irq_handler_2に分岐 */
#endif /* USE_BYPASS_IPI_DISPATCH_HANDER */
	cmp		w19, #0x03ff
	b.eq	irq_handler_2		/* スプリアス割込(INT_ID=1023(0x03ff))みなら */
								/*  irq_handler_2に分岐 */
#ifdef TOPPERS_TZ_S
	cmp		w19, #0x03fe		/* 割込みグループをサポートしている場合は */
	b.eq	irq_handler_2		/* (INT_ID=1022(0x03fe))も スプリアス割込み */
#endif /* TOPPERS_TZ_S */

#ifdef TOPPERS_SAFEG_LOAD
	bl	safeg_load_int
#endif /* TOPPERS_SAFEG_LOAD */

	/*
	 *  CPUロック解除状態にする．
	 */
	msr		DAIFClr, #DAIFSCBIT(DAIF_CPULOCK) 

#ifdef LOG_INH_ENTER
	/*
	 *  ログ出力の呼出し
	 */
	mov		x0, x19					/* 割込み番号をパラメータに渡す */
	bl	log_inh_enter
#endif /* LOG_INH_ENTER */

	/*
	 *  割込みハンドラの呼出し
	 */
	my_pcb	x1, x2
	ldr		x2, [x1, #PCB_p_inh_tbl]	/* 割込みハンドラテーブルの読み込み */
	ldr		x1, [x2, x19, lsl #3]		/* 割込みハンドラの番地 → x1 */
	mov		x0, x19						/* 割込み番号をパラメータに渡す */
	blr		x1							/* 割込みハンドラの呼出し */

#ifdef LOG_INH_LEAVE
	/*
	 *  ログ出力の呼出し
	 */
	mov		x0, x19						/* 割込み番号をパラメータに渡す */
	bl		log_inh_leave
#endif /* LOG_INH_LEAVE */

	/*
	 *  スピンロックを取得していれば解放する．
	 */
	my_pcb	x0, x1		/* 引数 */
	bl		force_unlock_spin

	/*
	 *  カーネル管理の割込みを禁止する
	 */
	msr		DAIFSet, #DAIFSCBIT(DAIF_CPULOCK)

	/*
	 *  割込みコントローラを操作する．
	 */
ALABEL(irq_handler_2)
	bl		irc_end_int

	/*
	 *  例外ネストカウントをデクリメントする．
	 */
	my_pcb	x2, x1
	ldr	w3, [x2, #PCB_excpt_nest_count]
	sub	w3, w3, #1
	str	w3, [x2, #PCB_excpt_nest_count]
	cbnz	w3, irq_handler_5		/* 戻り先が非タスクコンテキストなら */
									/*			ならirq_handler_5に分岐 */

	/*
	 *  タスク用のスタックに戻す．
	 */
	ldp		x0, x3, [sp], #16
	mov		sp, x3

	/*
	 *  p_runtskがNULLか判定する
	 */
	ldr		x0, [x2, #PCB_p_runtsk]	/* p_runtsk → x0 */
	cbnz	x0, irq_handler_3		/* p_runtskがNULLでなければ */
									/*		irq_handler_3に分岐 */

	/*
	 * タスクのスタックに保存したスクラッチレジスタを捨てる
	 */
	ldp		x0, x1, [sp], #16		/* スタックポインタの調整を元に戻す */
	add		sp, sp, x1
	add		sp, sp, #(16 * 12)	/* スクラッチレジスタ等を捨てる */
#ifdef USE_ARM64_FPU
	add		sp, sp, #(16 * 26)	/* スクラッチFPUレジスタを捨てる */
#endif /* USE_ARM64_FPU */
	b		dispatcher_0

	/*
	 *  ディスパッチが必要か判定する．
	 */
ALABEL(irq_handler_3)
	/*
	 * 【この時点のレジスタ状態】
	 *  x0：p_runtsk
	 *  x2：p_my_pcb
	 */
	ldr		x1, [x2, #PCB_p_schedtsk]	/* p_schedtsk → x1 */
	cmp		x0, x1						/* p_runtskとp_schedtskが同じなら */
	b.eq	irq_handler_4				/*				  irq_handler_4へ */

	/*
	 *  コンテキストを保存する．
	 */
	sub		sp, sp, #(16 * 5)			/* 残りのレジスタの保存 */
	stp		x21, x22, [sp, #0x00]
	stp		x23, x24, [sp, #0x10]
	stp		x25, x26, [sp, #0x20]
	stp		x27, x28, [sp, #0x30]
	str		x29,[sp, #0x50]
#ifdef USE_ARM64_FPU
	sub		sp, sp, #(16 * 8)			/* 残りのFPUレジスタ保存 */
	stp		q8, q9, [sp, #0x00]
	stp		q10, q11, [sp, #0x20]
	stp		q12, q13, [sp, #0x40]
	stp		q14, q15, [sp, #0x60]
#endif /* USE_ARM64_FPU */

	mov		x1, sp
	str		x1, [x0, #TCB_sp]		/* スタックポインタを保存 */
	adr		x1, ret_int_r			/* 実行再開番地を保存 */
	str		x1, [x0, #TCB_pc]
	b		dispatcher				/* x0にはp_runtskが格納されている */

ALABEL(ret_int_r)
	/*
	 *  コンテキストを復帰する．
	 */
#ifdef USE_ARM64_FPU
	ldp		q14, q15, [sp, #0x60]	/* 残りのFPUレジスタの復帰 */
	ldp		q12, q13, [sp, #0x40]
	ldp		q10, q11, [sp, #0x20]
	ldp		q8, q9, [sp, #0x00]
	add		sp, sp, #(16 * 8)
#endif /* USE_ARM64_FPU */
	ldr		x29, [sp, #0x50]		/* 残りのレジスタの復帰 */
	ldp		x27, x28, [sp, #0x30]
	ldp		x25, x26, [sp, #0x20]
	ldp		x23, x24, [sp, #0x10]
	ldp		x21, x22, [sp, #0x00]
	add		sp, sp, #(16 * 5)

ALABEL(irq_handler_4)
#ifdef TOPPERS_SUPPORT_OVRHDR
	/*
	 *  オーバランタイマを動作開始する．
	 */
	bl		ovrtimer_start
#endif /* TOPPERS_SUPPORT_OVRHDR */

	/*
	 *  割込み処理からのリターン
	 *
	 *  割込み処理からのリターンにより，CPUロック解除状態に遷移するよ
	 *  うにする必要があるが，ARM64はDAIFのビットによってCPUロック状態を
	 *  表しているため，SPSRを元に戻してリターンすればよい．
	 */
ALABEL(irq_handler_5)
	ldp		x0, x1, [sp], #16		/* スタックポインタの調整を元に戻す */
	add		sp, sp, x1
#ifdef USE_ARM64_FPU
	ldp		q30, q31, [sp, #0x180]	/* スクラッチFPUレジスタの復帰 */
	ldp		q28, q29, [sp, #0x160]
	ldp		q26, q27, [sp, #0x140]
	ldp		q24, q25, [sp, #0x120]
	ldp		q22, q23, [sp, #0x100]
	ldp		q20, q21, [sp, #0xe0]
	ldp		q18, q19, [sp, #0xc0]
	ldp		q16, q17, [sp, #0xa0]
	ldp		q6, q7, [sp, #0x80]
	ldp		q4, q5, [sp, #0x60]
	ldp		q2, q3, [sp, #0x40]
	ldp		q0, q1, [sp, #0x20]
	ldr		x10, [sp, #0x10]
	msr		fpcr, x10
	ldr		x9,  [sp, #0x00]
	msr		fpsr, x9
	add		sp, sp, #(16 * 26)
#endif /* USE_ARM64_FPU */
	ldp		x0, x1, [sp], #16
	msr		elr_el1, x0
	msr		spsr_el1, x1
	ldp		x19, x20, [sp, #0xa0]	/* ノンスクラッチレジスタの復帰 */
	ldp		x18, x30, [sp, #0x90]	/* スクラッチレジスタの復帰 */
	ldp		x16, x17, [sp, #0x80]
	ldp		x14, x15, [sp, #0x70]
	ldp		x12, x13, [sp, #0x60]
	ldp		x10, x11, [sp, #0x50]
	ldp		x8, x9, [sp, #0x40]
	ldp		x6, x7, [sp, #0x30]
	ldp		x4, x5, [sp, #0x20]
	ldp		x2, x3, [sp, #0x10]
	ldp		x0, x1, [sp]
	add		sp, sp, #(16 * 11)
	eret
#endif /* OMIT_IRQ_HANDLER */

/*
 *  CPU例外ハンドラ出入口処理
 */
ALABEL(start_exc_entry)

/*
 *  全てのコンテキストを保存
 */
.macro save_all_registers
	sub		sp, sp, #(16 * 16)
	stp		x0, x1, [sp, #0x00]
	stp		x2, x3, [sp, #0x10]
	stp		x4, x5, [sp, #0x20]
	stp		x6, x7, [sp, #0x30]
	stp		x8, x9, [sp, #0x40]
	stp		x10, x11, [sp, #0x50]
	stp		x12, x13, [sp, #0x60]
	stp		x14, x15, [sp, #0x70]
	stp		x16, x17, [sp, #0x80]
	stp		x18, x19, [sp, #0x90]
	stp		x20, x21, [sp, #0xa0]
	stp		x22, x23, [sp, #0xb0]
	stp		x24, x25, [sp, #0xc0]
	stp		x26, x27, [sp, #0xd0]
	stp		x28, x29, [sp, #0xe0]
	add		x0, sp, #(16 * 16)
	stp		x30, x0, [sp, #0xf0]
	mrs		x0, elr_el1
	mrs		x1, spsr_el1
	stp		x0, x1, [sp, #-16]!
#ifdef USE_ARM64_FPU
	sub		sp, sp, #(16 * 34)
	mrs		x9,  fpsr
	str		x9,  [sp, #0x00]
	mrs		x10, fpcr
	str		x10, [sp, #0x10]
	stp		q0, q1, [sp, #0x20]
	stp		q2, q3, [sp, #0x40]
	stp		q4, q5, [sp, #0x60]
	stp		q6, q7, [sp, #0x80]
	stp		q8, q9, [sp, #0xa0]
	stp		q10, q11, [sp, #0xc0]
	stp		q12, q13, [sp, #0xe0]
	stp		q14, q15, [sp, #0x100]
	stp		q16, q17, [sp, #0x120]
	stp		q18, q19, [sp, #0x140]
	stp		q20, q21, [sp, #0x160]
	stp		q22, q23, [sp, #0x180]
	stp		q24, q25, [sp, #0x1a0]
	stp		q26, q27, [sp, #0x1c0]
	stp		q28, q29, [sp, #0x1e0]
	stp		q30, q31, [sp, #0x200]
#endif /* USE_ARM64_FPU */
.endm

/*
 *  同期 例外ハンドラ
 */
	ATEXT
	AALIGN(2)
	AGLOBAL(cur_sp0_sync_handler)
ALABEL(cur_sp0_sync_handler)
	/*
	 *  全てのレジスタをスタックへ保存
	 */
	save_all_registers

	/*
	 *  例外入口処理の呼び出し
	 */
	mov		x19, #EXCNO_CUR_SP0_SYNC
	b		exc_handler_1

/*
 *  IRQ 例外ハンドラ
 */
	ATEXT
	AALIGN(2)
	AGLOBAL(cur_sp0_irq_handler)
ALABEL(cur_sp0_irq_handler)
	/*
	 *  全てのレジスタをスタックへ保存
	 */
	save_all_registers

	/*
	 *  例外入口処理の呼び出し
	 */
	mov		x19, #EXCNO_CUR_SP0_IRQ
	b		exc_handler_1

/*
 *  FIQ 例外ハンドラ
 */
	ATEXT
	AALIGN(2)
	AGLOBAL(cur_sp0_fiq_handler)
ALABEL(cur_sp0_fiq_handler)
	/*
	 *  全てのレジスタをスタックへ保存
	 */
	save_all_registers

	/*
	 *  例外入口処理の呼び出し
	 */
	mov		x19, #EXCNO_CUR_SP0_FIQ
	b		exc_handler_1

/*
 *  システムエラー 例外ハンドラ
 */
	ATEXT
	AALIGN(2)
	AGLOBAL(cur_sp0_serr_handler)
ALABEL(cur_sp0_serr_handler)
	/*
	 *  全てのレジスタをスタックへ保存
	 */
	save_all_registers

	/*
	 *  例外入口処理の呼び出し
	 */
	mov		x19, #EXCNO_CUR_SP0_SERR
	b		exc_handler_1

/*
 *  同期 例外ハンドラ
 */
	ATEXT
	AALIGN(2)
	AGLOBAL(cur_spx_sync_handler)
ALABEL(cur_spx_sync_handler)
	/*
	 *  全てのレジスタをスタックへ保存
	 */
	save_all_registers

	/*
	 *  例外入口処理の呼び出し
	 */
	mov		x19, #EXCNO_CUR_SPX_SYNC
	b		exc_handler_1

#ifndef TARGET_FIQ_HANDLER
/*
 *  FIQ 例外ハンドラ
 */
	ATEXT
	AALIGN(2)
	AGLOBAL(cur_spx_fiq_handler)
ALABEL(cur_spx_fiq_handler)
#ifdef TOPPERS_TZ_S
	b cur_spx_irq_handler
#else /* TOPPERS_TZ_S */

	/*
	 *  全てのレジスタをスタックへ保存
	 */
	save_all_registers

	/*
	 *  例外入口処理の呼び出し
	 */
	mov		x19, #EXCNO_CUR_SPX_FIQ
	b		exc_handler_1
#endif /* TOPPERS_TZ_S */
#endif /* TARGET_FIQ_HANDLER */

/*
 *  システムエラー 例外ハンドラ
 */
	ATEXT
	AALIGN(2)
	AGLOBAL(cur_spx_serr_handler)
ALABEL(cur_spx_serr_handler)
	/*
	 *  全てのレジスタをスタックへ保存
	 */
	save_all_registers

	/*
	 *  例外入口処理の呼び出し
	 */
	mov		x19, #EXCNO_CUR_SPX_SERR
	b		exc_handler_1

/*
 *  同期 例外ハンドラ
 */
	ATEXT
	AALIGN(2)
	AGLOBAL(l64_sync_handler)
ALABEL(l64_sync_handler)
	/*
	 *  全てのレジスタをスタックへ保存
	 */
	save_all_registers

	/*
	 *  例外入口処理の呼び出し
	 */
	mov		x19, #EXCNO_L64_SYNC
	b		exc_handler_1

/*
 *  IRQ 例外ハンドラ
 */
	ATEXT
	AALIGN(2)
	AGLOBAL(l64_irq_handler)
ALABEL(l64_irq_handler)
	/*
	 *  全てのレジスタをスタックへ保存
	 */
	save_all_registers

	/*
	 *  例外入口処理の呼び出し
	 */
	mov		x19, #EXCNO_L64_IRQ
	b		exc_handler_1

/*
 *  FIQ 例外ハンドラ
 */
	ATEXT
	AALIGN(2)
	AGLOBAL(l64_fiq_handler)
ALABEL(l64_fiq_handler)
	/*
	 *  全てのレジスタをスタックへ保存
	 */
	save_all_registers

	/*
	 *  例外入口処理の呼び出し
	 */
	mov		x19, #EXCNO_L64_FIQ
	b		exc_handler_1

/*
 *  システムエラー 例外ハンドラ
 */
	ATEXT
	AALIGN(2)
	AGLOBAL(l64_serr_handler)
ALABEL(l64_serr_handler)
	/*
	 *  全てのレジスタをスタックへ保存
	 */
	save_all_registers

	/*
	 *  例外入口処理の呼び出し
	 */
	mov		x19, #EXCNO_L64_SERR
	b		exc_handler_1

/*
 *  同期 例外ハンドラ
 */
	ATEXT
	AALIGN(2)
	AGLOBAL(l32_sync_handler)
ALABEL(l32_sync_handler)
	/*
	 *  全てのレジスタをスタックへ保存
	 */
	save_all_registers

	/*
	 *  例外入口処理の呼び出し
	 */
	mov		x19, #EXCNO_L32_SYNC
	b		exc_handler_1

/*
 *  IRQ 例外ハンドラ
 */
	ATEXT
	AALIGN(2)
	AGLOBAL(l32_irq_handler)
ALABEL(l32_irq_handler)
	/*
	 *  全てのレジスタをスタックへ保存
	 */
	save_all_registers

	/*
	 *  例外入口処理の呼び出し
	 */
	mov		x19, #EXCNO_L32_IRQ
	b		exc_handler_1

/*
 *  FIQ 例外ハンドラ
 */
	ATEXT
	AALIGN(2)
	AGLOBAL(l32_fiq_handler)
ALABEL(l32_fiq_handler)
	/*
	 *  全てのレジスタをスタックへ保存
	 */
	save_all_registers

	/*
	 *  例外入口処理の呼び出し
	 */
	mov		x19, #EXCNO_L32_FIQ
	b		exc_handler_1

/*
 *  システムエラー 例外ハンドラ
 */
	ATEXT
	AALIGN(2)
	AGLOBAL(l32_serr_handler)
ALABEL(l32_serr_handler)
	/*
	 *  全てのレジスタをスタックへ保存
	 */
	save_all_registers

	/*
	 *  例外入口処理の呼び出し
	 */
	mov		x19, #EXCNO_L32_SERR
	b		exc_handler_1

ALABEL(end_exc_entry)

/*
 *  CPU例外ハンドラ出入口処理の共通部分
 */
ALABEL(exc_handler_1)
	/*
	 * 【この時点のレジスタ状態】
	 *  x19：CPU例外ハンドラ番号
	 *
	 *  CPU例外が発生した状況の判断に用いるために，CPU例外発生前の
	 *  割込み優先度マスク，例外ネストカウントをスタッ
	 *  に保存する．
	 */
	bl		irc_get_intpri
	my_pcb	x4, x1
	ldr		w3, [x4, #PCB_excpt_nest_count]
	stp		x0, x3, [sp, #-16]!
					/* 例外発生前の割込み優先度マスクをスタックに保存 */
					/* excpt_nest_count をスタックに保存．*/
	mov		x20, sp					/* CPU例外の情報を記憶している領域の */
									/*				  先頭番地をx5に保存 */

	/*
	 *  スタックポインタの調整
	 */
	mov		x4, sp
	and		x1, x4, #8
	sub		x0, x4, x1
	mov		sp, x0
	stp		x0, x1, [sp, #-16]!	/* スタックポインタの調整値を保存 */
									/* r0はスペース確保のため */

	/*
	 *  カーネル管理外のCPU例外か判定する
	 *
	 *  カーネル管理外のCPU例外は，カーネル実行中，全割込みロック状態，
	 *  CPUロック状態，カーネル管理外の割込みハンドラ実行中に発生した
	 *  CPU例外である．ARMの場合は，SPSRのI/Fフラグのいずれかがセット
	 *  されているなら，これに該当する．
	 */
	ldr		x0, [x20, #EXC_FRAME_pstate]	/* 例外フレームからpstateを取得 */
	tst		w0, #(DAIF_CPULOCK AOR DAIF_INTLOCK)
	my_pcb	x0, x1						/* これ以降x0=p_my_pcbを前提としているため */
	b.ne	nk_exc_handler_1			/* カーネル管理外のCPU例外の処理へ */

	/*
	 * 【この時点のレジスタ状態】
	 *  x0 ：p_my_pcb
	 *  w3 ：excpt_nest_countの値
	 *  w19：CPU例外ハンドラ番号
	 *  x20：CPU例外の情報を記憶している領域の先頭番地
	 */

	/*
	 *  例外ネストカウントをインクリメントする．
	 */
	add		w3, w3, #1
	str		w3, [x0, #PCB_excpt_nest_count]
	cmp		x3, #1			/* CPU例外発生前が非タスクコンテキスト(x3>1) */
	b.ne	exc_handler_2	/* ならexc_handler_2に分岐 */

#ifdef TOPPERS_SUPPORT_OVRHDR
	/*
	 *  オーバランタイマを停止する．
	 */
	bl		ovrtimer_stop
#endif /* TOPPERS_SUPPORT_OVRHDR */

	/*
	 *  非タスクコンテキスト用のスタックに切り換える．
	 */
	mov		x3, sp					/* この時点のスタックポインタをr3に */
	my_pcb	x2, x1
	ldr		x1, [x2, #PCB_istkpt]	/* 非タスクコンテキスト用スタックに */
	mov		sp, x1
	stp		x0, x3, [sp, #-16]!		/* 切換え前のスタックポインタを保存 */
									/* r0はスペース確保のため */

ALABEL(exc_handler_2)
	/*
	 * 【この時点のレジスタ状態】
	 *  w19：CPU例外ハンドラ番号
	 *  x20：CPU例外の情報を記憶している領域の先頭番地
	 */

	/*
	 *  （必要なら）割込みコントローラを操作する．
	 *
	 *  irc_begin_excは，スタックトップ（r0の場所）に，irc_end_excで用
	 *  いる情報を保存する．
	 */
	bl		irc_begin_exc

	/*
	 *  CPUロック解除状態にする．
	 *
	 *  カーネル管理外のCPU例外ハンドラは別ルーチンで呼び出すため，単純
	 *  に割込みを許可するだけでよい．
	 */
	msr		DAIFClr, #DAIFSCBIT(DAIF_MASK)

	/*
	 *  ログ出力の呼出し
	 */
#ifdef LOG_EXC_ENTER
	mov		x0, x19					/* CPU例外番号をパラメータに渡す */
	bl		log_exc_enter
#endif /* LOG_EXC_ENTER */

	/*
	 *  CPU例外ハンドラの呼出し
	 */
	my_pcb	x1, x2
	ldr		x2, [x1,#PCB_p_exc_tbl]	/* CPU例外ハンドラテーブルの読込み */
	ldr		x3, [x2, x19, lsl #3]	/* CPU例外ハンドラの番地 → x3 */
	mov		x0, x20					/* CPU例外の情報を記憶している領域の */
									/*		先頭番地を第1パラメータに渡す */
	mov		x1, x19					/* CPU例外番号を第2パラメータに渡す */
	blr		x3

	/*
	 *  ログ出力の呼出し
	 */
#ifdef LOG_EXC_LEAVE
	mov		x0, x19					/* CPU例外番号をパラメータに渡す */
	bl		log_exc_leave
#endif /* LOG_EXC_LEAVE */

	/*
	 *  スピンロックを取得していれば解放する．
	 */
	my_pcb	x0, x1		/* 引数 */
	bl		force_unlock_spin

	/*
	 *  カーネル管理の割込みを禁止する
	 */
	msr		DAIFSet, #DAIFSCBIT(DAIF_CPULOCK)

	/*
	 *  割込みコントローラを操作して，割込み優先度マスクを，CPU例外発
	 *  生時の値に設定する．
	 */
	bl		irc_end_exc

	/*
	 *  例外ネストカウントをデクリメントする．
	 */
	my_pcb	x2, x1
	ldr		w3, [x2, #PCB_excpt_nest_count]
	sub		w3, w3, #1
	str		w3, [x2, #PCB_excpt_nest_count]
	cbnz	w3, exc_handler_5	/* 戻り先が非タスクコンテキスト */
								/*		ならexc_handler_5に分岐 */

	/*
	 *  タスク用のスタックに戻す．
	 */
	ldp		x0, x3, [sp], #16
	mov		sp, x3

	/*
	 *  p_runtskがNULLか判定する．
	 */
	ldr		x0, [x2,#PCB_p_runtsk]	/* p_runtsk → x0 */
	cbnz	x0, exc_handler_3		/* p_runtskがNULLでなければ */
									/*		exc_handler_3に分岐 */

	/*
	 *  タスクのスタックに保存したスクラッチレジスタ等を捨てる．
	 */
	ldp		x0, x1, [sp], #16		/* スタックポインタの調整を元に戻す */
	add		sp, sp, x1				/* レジスタとCPU例外が発生した */
	add		sp, sp, #(EXC_FRAME)	/* 状況を判断するための追加情報を捨てる */
	b		dispatcher_0

	/*
	 *  ディスパッチが必要か判定する．
	 */
ALABEL(exc_handler_3)

	/*
	 * 【この時点のレジスタ状態】
	 *  x0：p_runtsk
	 *  x2：p_my_pcb
	 */
	ldr		x1, [x2,#PCB_p_schedtsk]	/* p_schedtsk → x1 */
	cmp		x0, x1						/* p_runtskとp_schedtskが同じなら */
	b.eq	exc_handler_4				/*				  exc_handler_4へ */

	/*
	 *  コンテキストを保存する．
	 */
	mov		x1, sp					/* スタックポインタを保存 */
	str		x1, [x0,#TCB_sp]
	adr		x1, ret_exc_r			/* 実行再開番地を保存 */
	str		x1, [x0,#TCB_pc]
	b		dispatcher				/* x0にはp_runtskが格納されている */

ALABEL(ret_exc_r)
ALABEL(exc_handler_4)
#ifdef TOPPERS_SUPPORT_OVRHDR
	/*
	 *  オーバランタイマを動作開始する．
	 */
	bl		ovrtimer_start
#endif /* TOPPERS_SUPPORT_OVRHDR */

	/*
	 *  例外み処理からのリターン
	 *
	 *  例外み処理からのリターンにより，CPUロック解除状態に遷移するよ
	 *  うにする必要があるが，ARM64はDAIFのビットによってCPUロック状態を
	 *  表しているため，SPSRを元に戻してリターンすればよい．
	 */
ALABEL(exc_handler_5)
	ldp		x0, x1, [sp], #16	/* スタックポインタの調整を元に戻す */
	add		sp, sp, x1
	add		sp, sp, #(8*2)		/* 例外発生前の割込み優先度マスクとexcpt_nest_countを捨てる */
#ifdef USE_ARM64_FPU
	ldp		q30, q31, [sp, #0x200]
	ldp		q28, q29, [sp, #0x1e0]
	ldp		q26, q27, [sp, #0x1c0]
	ldp		q24, q25, [sp, #0x1a0]
	ldp		q22, q23, [sp, #0x180]
	ldp		q20, q21, [sp, #0x160]
	ldp		q18, q19, [sp, #0x140]
	ldp		q16, q17, [sp, #0x120]
	stp		q14, q15, [sp, #0x100]
	stp		q12, q13, [sp, #0xe0]
	stp		q10, q11, [sp, #0xc0]
	stp		q8, q9, [sp, #0xa0]
	ldp		q6, q7, [sp, #0x80]
	ldp		q4, q5, [sp, #0x60]
	ldp		q2, q3, [sp, #0x40]
	ldp		q0, q1, [sp, #0x20]
	ldr		x10, [sp, #0x10]
	msr		fpcr, x10
	ldr		x9,  [sp, #0x00]
	msr		fpsr, x9
	add		sp, sp, #(16 * 34)
#endif /* USE_ARM64_FPU */
	ldp		x0, x1, [sp], #16
	msr		elr_el1, x0
	msr		spsr_el1, x1
	ldp		x30, x0, [sp, #0xf0]
	ldp		x28, x29, [sp, #0xe0]
	ldp		x26, x27, [sp, #0xd0]
	ldp		x24, x25, [sp, #0xc0]
	ldp		x22, x23, [sp, #0xb0]
	ldp		x20, x21, [sp, #0xa0]
	ldp		x18, x19, [sp, #0x90]
	ldp		x16, x17, [sp, #0x80]
	ldp		x14, x15, [sp, #0x70]
	ldp		x12, x13, [sp, #0x60]
	ldp		x10, x11, [sp, #0x50]
	ldp		x8, x9, [sp, #0x40]
	ldp		x6, x7, [sp, #0x30]
	ldp		x4, x5, [sp, #0x20]
	ldp		x2, x3, [sp, #0x10]
	ldp		x0, x1, [sp]
	add		sp, sp, #(16 * 16)
	eret

/*
 *  カーネル管理外のCPU例外の出入口処理
 */
ALABEL(nk_exc_handler_1)
	/*
	 * 【この時点のレジスタ状態】
	 *  x0 ：p_my_pcb
	 *  x3 ：excpt_nest_countの値
	 *  x19：CPU例外ハンドラ番号
	 *  x20：CPU例外の情報を記憶している領域の先頭番地
	 */

	/*
	 *  例外ネストカウントをインクリメントする．
	 */
	add		x3, x3, #1
	str		x3, [x0,#PCB_excpt_nest_count]
	tbnz	x3, #1, nk_exc_handler_2	/* CPU例外発生前が非タスクコンテキスト */
										/* (x3==1)	ならnk_exc_handler_2に分岐 */

	/*
	 *  非タスクコンテキスト用のスタックに切り換える．
	 *
	 * 【この時点のレジスタ状態】
	 *  x0 ：p_my_pcb
	 */
	mov		x3, sp					/* この時点のスタックポインタをx3に */
	ldr		x1, [x0,#PCB_istkpt]	/* 非タスクコンテキスト用スタックに */
	mov		sp, x1
	stp		x0, x3, [sp, #-16]!		/* 切換え前のスタックポインタを保存 */
									/* r0はアラインメントのため */
ALABEL(nk_exc_handler_2)
	/*
	 *  システム状態（コンテキストは除く）を，CPU例外発生時の状態へ
	 */
	ldr		x0, [x20, #EXC_FRAME_pstate]		/* CPU例外発生前のDAIFの取得 */
	and		x0, x0, #DAIF_MASK
	msr		DAIF, x0

	/*
	 *  CPU例外ハンドラの呼出し
	 */
	my_pcb	x1, x2
	ldr		x2, [x1,#PCB_p_exc_tbl]	/* CPU例外ハンドラテーブルの読込み */
	ldr		x3, [x2, x19, lsl #3]	/* CPU例外ハンドラの番地 → x3 */
	mov		x0, x20					/* CPU例外の情報を記憶している領域の */
									/*		先頭番地を第1パラメータに渡す */
	mov		x1, x19					/* CPU例外番号を第2パラメータに渡す */
	blr		x3

	/*
	 *  例外ネストカウントをデクリメントする．
	 */
	my_pcb	x2, x1
	ldr	w3, [x2, #PCB_excpt_nest_count]
	sub	w3, w3, #1
	str	w3, [x2, #PCB_excpt_nest_count]
	cmp	w3, wzr					/* 戻り先が非タスクコンテキスト */
	bne		exc_handler_5		/*		ならexc_handler_5に分岐 */

	/*
	 *  タスク用のスタックに戻す．
	 */
	ldp		x0, x3, [sp], #16
	mov		sp, x3

	b		exc_handler_5


/*
 *  微少時間待ち
 */
	ATEXT
	AALIGN(3)
	AGLOBAL(sil_dly_nse)
ALABEL(sil_dly_nse)
	isb
	sub		x0, x0, #SIL_DLY_TIM1
	cmp		x0, xzr
	b.gt  _sil_dly_nse1
	ret
ALABEL(_sil_dly_nse1)
	isb
	sub		x0, x0, #SIL_DLY_TIM2
	cmp		x0, xzr
	b.gt  _sil_dly_nse1
	ret

/*
 *  エントリ時キャッシュ／MMU状態の正規化（ARM64, EL1）
 *
 *  ブートローダ（TF-A等）やデバッガがキャッシュ／MMU有効のまま制御を渡し
 *  た場合（リセットを介さない再ロード等）でも，カーネル初期化が前提とする
 *  「MMU・D/Iキャッシュ無効」状態をカーネル自身が作り出す．ターゲット依存
 *  部の hardware_init_hook（start.Sのweak定義を上書き）から tail-call(b)で
 *  呼ぶこと（スタック未設定でも安全．ロード／ストアを一切発行しない）．
 *  マルチコアでは各コアが他コア起動前に自コアで実行する（set/wayは自PEのみ）．
 *  設計根拠・前提・再現方法は stm32mp2_init/work/asp3_core/arch/arm64_gcc/
 *  docs/entry-cache-normalization.md（ARM64版の同問題と対策）を参照．
 */
	ATEXT
	AALIGN(2)
	AGLOBAL(arm64_normalize_cache_state_el1)
ALABEL(arm64_normalize_cache_state_el1)
	/*
	 *  D/統合キャッシュの全レベルを clean&invalidate（DC CISW）
	 *
	 *  CLIDR_EL1 の LoC までの各レベルについて，CCSIDR_EL1 から
	 *  way数・set数・ラインサイズを取得し全ラインを掃引する
	 *  （TF-A lib/aarch64/cache_helpers.S の dcsw_op_all と同構造）．
	 */
	mrs	x0, clidr_el1
	ubfx	x3, x0, #24, #3			/* x3 = LoC */
	cbz	x3, ncs_dcsw_done
	mov	x10, xzr			/* x10 = CSSELR値（level << 1） */
ALABEL(ncs_dcsw_level)
	add	x2, x10, x10, lsr #1		/* x2 = level * 3 */
	lsr	x1, x0, x2
	and	x1, x1, #7			/* x1 = ctype */
	cmp	x1, #2
	b.lt	ncs_dcsw_next			/* Dキャッシュ無しレベルはスキップ */
	msr	csselr_el1, x10
	isb
	mrs	x1, ccsidr_el1
	and	x2, x1, #7
	add	x2, x2, #4			/* x2 = log2(ラインサイズ) */
	ubfx	x4, x1, #3, #10			/* x4 = way数 - 1 */
	clz	w5, w4				/* x5 = wayフィールドのシフト量 */
	ubfx	x6, x1, #13, #15		/* x6 = set数 - 1 */
	mov	x7, x4				/* x7 = wayカウンタ */
ALABEL(ncs_dcsw_way)
	lsl	x8, x7, x5
	orr	x8, x8, x10			/* x8 = way | level */
	mov	x9, x6				/* x9 = setカウンタ */
ALABEL(ncs_dcsw_set)
	lsl	x11, x9, x2
	orr	x11, x11, x8			/* x11 = way | set | level */
	dc	cisw, x11
	subs	x9, x9, #1
	b.ge	ncs_dcsw_set
	subs	x7, x7, #1
	b.ge	ncs_dcsw_way
ALABEL(ncs_dcsw_next)
	add	x10, x10, #2			/* 次のレベルへ */
	lsl	x1, x3, #1
	cmp	x1, x10
	b.gt	ncs_dcsw_level
	msr	csselr_el1, xzr
	dsb	sy
	isb
ALABEL(ncs_dcsw_done)

	/*
	 *  MMU・D/Iキャッシュの無効化（SCTLR_EL1.{M,C,I} クリア）
	 *
	 *  論理即値の制約上，1ビットずつクリアする．
	 */
	mrs	x0, sctlr_el1
	bic	x0, x0, #SCTLR_M_BIT
	bic	x0, x0, #SCTLR_C_BIT
	bic	x0, x0, #SCTLR_I_BIT
	msr	sctlr_el1, x0
	dsb	sy
	isb

	/*
	 *  Iキャッシュの全無効化（stale命令の排除）
	 */
	ic	iallu
	dsb	nsh
	isb

	/*
	 *  TLBの全無効化（旧実行の変換の排除）
	 */
	tlbi	vmalle1
	dsb	nsh
	isb

	ret

