/*
 *  TOPPERS Software
 *      Toyohashi Open Platform for Embedded Real-Time Systems
 * 
 *  Copyright (C) 2011-2024 by Embedded and Real-Time Systems Laboratory
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
 *  $Id: core_asm.inc 64 2018-08-20 06:57:46Z ertl-honda $
 */

/*
 *		アセンブリ言語用マクロ定義（RISC-V GNU開発環境用）
 */

#ifndef TOPPERS_CORE_ASM_INC
#define TOPPERS_CORE_ASM_INC

/*
 *  ディレクティブのマクロ定義
 */
#define ASECTION(name)  .section name, "a"
#define ATEXT           .text
#define AGLOBAL(name)   .global name
#define AALIGN(size)    .align size
#define ABALIGN(size)   .balign size
#define ALONG           .long
#define AINT            .int
#define ALABEL(label)   label:
#define AOR             |
#define AWEAK(name)      .weak name
#define AO_PUSH         .option push
#define AO_POP          .option pop
#define AO_NORELAX      .option norelax
#define AO_NORVC        .option norvc

/*
 *  セクション情報
 */
#define DATA_START		__start_data
#define DATA_END		__end_data
#define IDATA_START		__start_idata
#define BSS_START		__start_bss
#define BSS_END			__end_bss


#if __riscv_xlen == 64
#define XREG_SIZE 8
#define INTPTR_T_SHIFT 3
#define lx ld
#define sx sd
#elif __riscv_xlen == 32
#define XREG_SIZE 4
#define INTPTR_T_SHIFT 2
#define lx lw
#define sx sw
#define lwu lw
#endif /* __riscv_xlen == 64 */

#if __riscv_flen == 64
#define FREG_SIZE 8
#define lf fld
#define sf fsd
#elif __riscv_flen == 32
#define FREG_SIZE 4
#define lf flw
#define sf fsw
#endif /* __riscv_flen == 64 */


.macro my_pcb re_reg sc_reg1
    my_pidx \sc_reg1
    la    \re_reg, p_pcb_table
    slli  \sc_reg1, \sc_reg1, INTPTR_T_SHIFT
    add   \sc_reg1,\sc_reg1,\re_reg    
    lx    \re_reg, 0(\sc_reg1)
.endm

.macro my_istkpt re_reg sc_reg1
    my_pidx  \sc_reg1
    slli  \sc_reg1, \sc_reg1, INTPTR_T_SHIFT
    la    \re_reg,  _kernel_istkpt_table
    add   \re_reg,  \re_reg, \sc_reg1
    lx    \re_reg,  0(\re_reg)
.endm

/*
 *  データ同期バリア
 */
.macro riscv_dmb
    fence iorw, iorw
.endm


#ifndef TOPPERS_SUPPORT_NONKERNELINT
/*
 *  CPUロック状態へ
 */
.macro lock_cpu_asm reg
    csrrc \reg, mstatus, MSTATUS_MIE
.endm

/*
 *  CPUロック解除状態へ
 */
.macro unlock_cpu_asm reg
    csrrs \reg, mstatus, MSTATUS_MIE
.endm

/* 
 *  CPUロック解除状態 & 全割込みロック解除状態 とする
 */
.macro unlock_cpu_and_unlock_allint_asm reg
    csrrs \reg, mstatus, MSTATUS_MIE
.endm

/* 
 *  CPUロック状態 & 全割込みロック解除状態 とする
 */
.macro lock_cpu_and_unlock_allint_asm reg
.endm

/*
 *  割込みからのリターンでCPUロック解除状態とする
 */
.macro ret_int_prepare_unlock_cpu_asm reg
.endm

/*
 *  例外からのリターンでCPUロック解除状態とする
 */
.macro ret_exc_prepare_unlock_cpu_asm reg
.endm

/*
 *  カーネル管理外の例外か判断
 *   カーネル管理   : none 0
 *   カーネル管理外 : 0
 */
.macro is_kernel_exception_asm reg1 reg2
    csrr \reg1, mstatus
    li   \reg2, MSTATUS_MPIE
    and  \reg1, \reg1, \reg2
.endm

#endif /* TOPPERS_SUPPORT_NONKERNELINT */

#endif /* TOPPERS_CORE_ASM_INC */
