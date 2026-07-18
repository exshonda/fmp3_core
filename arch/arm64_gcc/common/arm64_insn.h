/*
 *  TOPPERS/FMP Kernel
 *      Toyohashi Open Platform for Embedded Real-Time Systems/
 *      Flexible MultiProcessor Kernel
 *
 *  Copyright (C) 2000-2003 by Embedded and Real-Time Systems Laboratory
 *                              Toyohashi Univ. of Technology, JAPAN
 *  Copyright (C) 2006-2020 by Embedded and Real-Time Systems Laboratory
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
 *  @(#) $Id: arm64_insn.h 260 2020-10-26 09:11:16Z ertl-honda $
 */

/*
 *		ARM64コアの特殊命令のインライン関数定義
 *
 *  このヘッダファイルは，arm.hからインクルードされる．arm.hから分離し
 *  ているのは，コンパイラによるインラインアセンブラの記述方法の違いを
 *  吸収するために，このファイルのみを置き換えればよいようにするためで
 *  ある．
 */

#ifndef ARM64_INSN_H
#define ARM64_INSN_H

#include <t_stddef.h>

/*
 *  CLZ（Count Leading Zero）命令
 */
Inline uint32_t
count_leading_zero(uint32_t val)
{
	uint32_t	count;

	Asm("clz %w0, %w1" : "=r"(count) : "r"(val));
	return(count);
}

/*
 *  メモリが変更されることをコンパイラに伝えるためのマクロ
 */
#define ARM64_MEMORY_CHANGED Asm("":::"memory")

/*
 *  ステータスレジスタ（DAIF）の現在値の読出し
 */
Inline uint32_t
current_daif(void)
{
	uint32_t sr;
	Asm("mrs %0, daif" : "=r"(sr));
	return(sr);
}

/*
 *  ステータスレジスタ（DAIF）の現在値の変更
 */
Inline void
set_daif(uint32_t sr)
{
	Asm("msr daif, %0" : : "r"(sr) : "cc");
}

/*
 *  割込み禁止／許可関数
 *
 */
/*
 *  IRQの禁止
 */
Inline void
disable_irq(void)
{
	Asm("msr DAIFSet, #2" ::: "cc");
}

/*
 *  IRQの許可
 */
Inline void
enable_irq(void)
{
	Asm("msr DAIFClr, #2" ::: "cc");
}

/*
 *  FIQの禁止
 */
Inline void
disable_fiq(void)
{
	Asm("msr DAIFSet, #1" ::: "cc");
}

/*
 *  FIQの許可
 */
Inline void
enable_fiq(void)
{
	Asm("msr DAIFClr, #1" ::: "cc");
}

/*
 *  FIQとIRQの禁止
 */
Inline void
disable_fiq_irq(void)
{
	Asm("msr DAIFSet, #3" ::: "cc");
}

/*
 *  WFE（Wait For Event）命令
 */
Inline void
arm64_wait_for_event(void)
{
	Asm("wfe");
}

/*
 *  SEV（Send Event）命令
 */
Inline void
arm64_send_event(void)
{
	Asm("sev");
}

/*
 *  Test&Set操作
 */
Inline bool_t
test_and_set_uint32(volatile uint32_t *p_var)
{
	uint32_t		locked;

	Asm("ldxr	w1, [%1]			\n"
	"	cmp		w1, #0				\n"
	"	b.ne	1f					\n"
	"	stxr	w1, %w2, [%1]		\n"
	"1:								\n"
	"	mov		%0, x1				\n"
	: "=r"(locked) : "r"(p_var),"r"(1) : "x1", "cc");

	return(locked != 0);
}

/*
 *  浮動小数点例外制御レジスタ（FPEXC）の現在値の読出し
 */
Inline uint32_t
current_fpexc(void)
{
	uint32_t	fpexc;

	Asm("vmrs %0, fpexc" : "=r"(fpexc));
	return(fpexc);
}

/*
 *  浮動小数点例外制御レジスタ（FPEXC）の現在値の変更
 */
Inline void
set_fpexc(uint32_t fpexc)
{
	Asm("vmsr fpexc, %0" : : "r"(fpexc));
}
#endif /*  ARM64_INSN_H */
