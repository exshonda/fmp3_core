/*
 *  TOPPERS Software
 *      Toyohashi Open Platform for Embedded Real-Time Systems
 *
 *  Copyright (C) 2000-2003 by Embedded and Real-Time Systems Laboratory
 *                              Toyohashi Univ. of Technology, JAPAN
 *  Copyright (C) 2005-2023 by Embedded and Real-Time Systems Laboratory
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
 *  @(#) $Id: start.S 345 2023-04-20 12:58:48Z ertl-honda $
 */

/*
 *   カーネル用のスタートアップモジュール(ZYNQMP-A53-ARM64用)
 */
#define TOPPERS_MACRO_ONLY
#define UINT_C(val)		(val)		/* uint_t型の定数を作るマクロ */
#define ULONG_C(val)	(val)		/* ulong_t型の定数を作るマクロ */

#include "kernel_impl.h"
#include "core_asm.inc"

/*
 *  同期用のマジックナンバー
 */
#define MAGIC_START 0x87654321

	ASECTION(.text.start)
	AALIGN(4)
	AGLOBAL(start)
ALABEL(start)
	/*
	 *  FIQ / IRQの禁止
	 */
	mov   x0, #(DAIF_F_BIT AOR DAIF_I_BIT)
	msr   daif, x0

	/*
	 *  現在のELによって処理を変える
	 */
	mrs   x0, CurrentEL
	and   x0, x0, #(CURRENTEL_EL_MASK)
	cmp   x0, #(CURRENTEL_EL_EL3)
	b.eq  start_el3
	cmp   x0, #(CURRENTEL_EL_EL2)
	b.eq  start_el2
	cmp   x0, #(CURRENTEL_EL_EL1)
	b.eq  start_el1

	/*
	 *  EL3の処理
	 */
ALABEL(start_el3)

	/*
	 *  EL3の設定
	 *  EL2 をAArch64に設定，セキュアタイマ，セキュリティ状態の設定
	 */
#ifdef TOPPERS_TZ_S
	mov w0, #(SCR_RW_BIT | SCR_ST_BIT)
#else  /* TOPPERS_TZ_NS */
	mov w0, #(SCR_RW_BIT | SCR_NS_BIT)
#endif /* TOPPERS_TZ_S */
	msr scr_el3, x0
	isb

	/*
	 *  スタックポインタと，フレームポインタの初期化
	 */
	my_istkpt x0, x1		/* FMP */
	mov   sp, x0
	mov   x29, xzr			/* フレームポインタ(r29) */

	/*
	 *
	 *  EL3で行う初期化を実行
	 */
	bl    target_el3_initialize

	/*
	 *  Secure時はEL1 へドロップ / NonSecure時はEL2
	 */
ALABEL(drop_el2)
#ifdef TOPPERS_TZ_S
	adr   x0, start_el1
	msr   elr_el3, x0
	mov   x0, #(DAIF_F_BIT AOR DAIF_I_BIT AOR MODE_AARCH64_EL1H)
	msr   spsr_el3, x0
#else /* TOPPERS_TZ_NS */
	adr   x0, start_el2
	msr   elr_el3, x0
	mov   x0, #(DAIF_F_BIT AOR DAIF_I_BIT AOR MODE_AARCH64_EL2H)
	msr   spsr_el3, x0
#endif /* TOPPERS_TZ_S */
	eret

	/*
	 *  EL2の処理
	 */
ALABEL(start_el2)

	/*
	 *  EL2の設定
	 *  EL1とEL0 をAArch64に設定
	 */
	mov x0, #(HCR_RW_BIT)
	msr hcr_el2, x0
	isb

	/*
	 *  スタックポインタと，フレームポインタの初期化
	 */
	my_istkpt x0, x1		/* FMP */
	mov   sp, x0
	mov   x29, xzr			/* フレームポインタ(r29) */

	/*
	 *
	 *  EL2で行う初期化を実行
	 */
	bl    target_el2_initialize

	/*
	 *  EL1へドロップ
	 */
ALABEL(drop_el1)
	adr   x0, start_el1
	msr   elr_el2, x0
	mov   x0, #(DAIF_F_BIT AOR DAIF_I_BIT AOR MODE_AARCH64_EL1H)
	msr   spsr_el2, x0
	eret

	/*
	 *  EL1の処理
	 */
ALABEL(start_el1)

	/*
	 *  スタックポインタと，フレームポインタの初期化
	 */
	my_istkpt x0, x1		/* FMP */
	mov   sp, x0
	mov   x29, xzr			/* フレームポインタ(r29) */

	/*
	 *  hardware_init_hookの呼び出し
	 *
	 *  ターゲットハードウェアに依存して必要な初期化処理がある場合
	 *  は，hardware_init_hook という関数を用意する
	 */
	bl    hardware_init_hook

ALABEL(start_1)

	/*
	 *  マスタプロセッサ以外は初期化待ち
	 */
	my_core_index x0, x1
	cmp   x0, #(TOPPERS_MASTER_PRCID - 1)
	b.ne  slave_wait

	/*
	 *  マスタプロセッサは変数を初期化
	 */
	ldr   x0, =start_sync
	str   wzr, [x0]

#ifndef TOPPERS_OMIT_BSS_INIT
	/*
	 *  bssセクションのクリア
	 */
	ldr   x1, =BSS_START
	ldr   x2, =BSS_END
	cmp   x1, x2
	b.hs  start_3
ALABEL(start_2)
	str   xzr, [x1], #8
	cmp   x1, x2
	b.lo  start_2
ALABEL(start_3)
#endif /* TOPPERS_OMIT_BSS_INIT */

#ifndef TOPPERS_OMIT_DATA_INIT
	/*
	 *  dataセクションの初期化(ROM化対応)
	 *  IDATA_START から IDATA_END までを，DATA_START 以降に
	 *  コピーする
	 */
	ldr   x1, =DATA_START
	ldr   x3, =DATA_END
	cmp   x1, x3
	b.hs  start_5
	ldr   x2, =IDATA_START
ALABEL(start_4)
	ldr   x0, [x2], #8
	str   x0, [x1], #8
	cmp   x1, x3
	b.lo  start_4
ALABEL(start_5)
#endif /* TOPPERS_OMIT_DATA_INIT */

	/*
	 *  software_init_hook を呼出し
	 *
	 *  ソフトウェア環境（特にライブラリ）に依存して必要な初期化処
	 *  理がある場合は，software_init_hook という関数を用意すれば
	 *  よい．
	 */
	bl  software_init_hook

	/*
	 *  target_mprc_initialize の呼び出し
	 *
	 *  sta_ker 以前にマスタプロセッサのみで行う必要のある初期化を実行
	 */
ALABEL(start_6)
	bl	target_mprc_initialize

	/*
	 *  カーネルを起動する
	 */
ALABEL(start_7)
	ldr   x0, =start_sync
	ldr   w1, =MAGIC_START
	mov   x2, xzr
ALABEL(start_8)
	str   w1, [x0, x2]
	add   x2, x2, #4
	cmp   x2, #(TNUM_PRCID * 4)
	b.ne  start_8

	/* 全コアに通知 */
	sev

	/* マスタプロセッサ用のstart_syncを初期化 */
	my_core_index x1, x3
	lsl   x1, x1, #3
	str   w2, [x0, x1]
	bl    sta_ker

	/*
	 *  マスタプロセッサ以外の初期化待ちルーチン
	 */
ALABEL(slave_wait)
	my_core_index x0, x1
	lsl   x0, x0, #2
	ldr   x1, =start_sync
ALABEL(slave_wait_1)
	ldr   w2, =MAGIC_START
	ldr   w3, [x1, x0]
	cmp   w3, w2
	b.ne  slave_wait_1
	str   wzr, [x1, x0]
	bl    sta_ker

	/*
	 *  デフォルトのhardware_init_hook（weak定義）
	 */
	ATEXT
	AALIGN(2)
	AWEAK(hardware_init_hook)
ALABEL(hardware_init_hook)
	ret

	/*
	 *  デフォルトのsoftware_init_hook（weak定義）
	 */
	ATEXT
	AALIGN(2)
	AWEAK(software_init_hook)
ALABEL(software_init_hook)
	ret
