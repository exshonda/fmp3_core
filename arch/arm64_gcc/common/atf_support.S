/*
 *  TOPPERS/FMP Kernel
 *      Toyohashi Open Platform for Embedded Real-Time Systems/
 *      Flexible MultiProcessor Kernel
 *
 *  Copyright (C) 2000-2003 by Embedded and Real-Time Systems Laboratory
 *                              Toyohashi Univ. of Technology, JAPAN
 *  Copyright (C) 2006-2018 by Embedded and Real-Time Systems Laboratory
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
 *  @(#) $Id: atf_support.S 216 2020-02-17 16:17:15Z ertl-honda $
 */
#define TOPPERS_MACRO_ONLY
#define TOPPERS_ASM_MACRO
#include "kernel_impl.h"
#include "core_asm.inc"

#define ATF_SAVE_REGS_SIZE  (18 * 8)

    .bss
    AALIGN(16)

/*
 * ATFのSMC実行時に使用されるレジスタの値を保存する領域
 */
    AGLOBAL(atf_smc_uses_regs)
ALABEL(atf_smc_uses_regs)
    .skip   ATF_SAVE_REGS_SIZE * TNUM_PRCID


/*
 * [Macro] Save AFT SMC uses regiters
 *
 * Parameter (Registers) :
 *   None
 *
 * Return value (Registers) :
 *   None
 */
    .macro save_atf_regs
    stp   x13, x14, [sp, #-16]!

    /* Get core ID */
    mrs   x13, mpidr_el1
    and   x13, x13, #0xFF
    /* Set save address to x18 */
    mov   x14, #ATF_SAVE_REGS_SIZE
    mul   x14, x13, x14
    ldr   x13, =atf_smc_uses_regs
    add   x13, x13, x14
    /* Save registers */
    stp   x1,  x2,  [x13, #0x00]
    stp   x3,  x4,  [x13, #0x10]
    stp   x5,  x6,  [x13, #0x20]
    stp   x7,  x8,  [x13, #0x30]
    stp   x9,  x10, [x13, #0x40]
    stp   x11, x12, [x13, #0x50]
    stp   x15, x16, [x13, #0x70]
    str   x17,      [x13, #0x80]
    mov   x15, x13
    ldp   x13, x14, [sp], #16
    stp   x13, x14, [x15, #0x60]
    .endm

/*
 * [Macro] Restore AFT SMC uses regiters
 *
 * Parameter (Registers) :
 *   None
 *
 * Return value (Registers) :
 *   None
 */
    .macro restore_atf_regs
    /* Get core ID */
    mrs   x13, mpidr_el1
    and   x13, x13, #0xFF
    /* Set restore address to x18 */
    mov   x14, #ATF_SAVE_REGS_SIZE
    mul   x14, x13, x14
    ldr   x13, =atf_smc_uses_regs
    add   x13, x13, x14
    /* Restore registers */
    ldp   x1,  x2,  [x13, #0x00]
    ldp   x3,  x4,  [x13, #0x10]
    ldp   x5,  x6,  [x13, #0x20]
    ldp   x7,  x8,  [x13, #0x30]
    ldp   x9,  x10, [x13, #0x40]
    ldp   x11, x12, [x13, #0x50]
    ldp   x15, x16, [x13, #0x70]
    ldr   x17,      [x13, #0x80]
    ldp   x13, x14, [x13, #0x60]
    .endm

    ATEXT
    AALIGN(4)

/*
 * ATFのTrusted OSベクタテーブル
 */
    AGLOBAL(atf_vector_table)
ALABEL(atf_vector_table)
    b start                 /* PSCI CPU_ONでCPUコアON直後に実行されるコールバック */
    b atf_fiq_entry

/*
 * ATFのTrusted OSベクタエントリ
 */
    AGLOBAL(atf_fiq_entry)
ALABEL(atf_fiq_entry)
    /* 復帰アドレスを設定
     *    ATFでeret実行前にELR_EL1を設定しても、eret実行時にELR_EL1がCPUにより
     *    書き換えられるため、ここで設定する必要がある */
    msr elr_el1, x0
    /* atf_smc_int(), atf_smc_switch()の戻り値を設定 */
    mov x0, xzr
    /* IRQハンドラへジャンプ */
    b cur_spx_irq_handler

/*
 * ATFのNon-secure OS ダミーエントリ
 */
    AGLOBAL(atf_ns_dummy_entry)
ALABEL(atf_ns_dummy_entry)
    b   atf_ns_dummy_entry

/*
 *  SPD SMCサービルコール
 */
/*
 * [C-lang function] SETVCT: Set TOPPERS kernel entry points (fiq vector etc.)
 *
 * Argument :
 *   [In]  X0 - TOPPERS kernel entry points (fiq vector etc.)
 *
 * Return value:
 *   ATFSMC_RET_SUCCESS -       Success
 *   ATFSMC_RET_INVALID_STATE - Called from Non-secure state
 */
    AALIGN(4)
    AGLOBAL(atf_smc_setvct)
ALABEL(atf_smc_setvct)
    /* SPD SMCサービルコール実行前のレジスタを保存 */
    save_atf_regs
    /* Execute SMC Function call */
    mov   x1, x0
    mov   x0, #0x0000   /* ATFSMC_SET_VECTOR */
    movk  x0, #((1 << 15) | (62 << 8)), lsl #16
    dsb   sy
    smc   #0
    /* SPD SMCサービルコール実行前のレジスタを復帰 */
    restore_atf_regs
    ret

/*
 * [C-lang function] INIT:   Notify TOPPERS kernel initialize complete
 *
 * Argument :
 *   None
 *
 * Return value :
 *   ATFSMC_RET_SUCCESS -       Success
 *   ATFSMC_RET_INVALID_STATE - Called from non secure state
 *   ATFSMC_RET_NOT_READY -     NS-EL1 OS isn't ready
 */
    AALIGN(4)
    AGLOBAL(atf_smc_init)
ALABEL(atf_smc_init)
    /* SPD SMCサービルコール実行前のレジスタを保存 */
    save_atf_regs
    /* Execute SMC Function call */
    ldr   x1, =atf_ns_dummy_entry
    mov   x0, #0x0001   /* ATFSMC_INIT_COMPLETE */
    movk  x0, #((1 << 15) | (62 << 8)), lsl #16
    dsb   sy
    smc   #0
    /* SPD SMCサービルコール実行前のレジスタを復帰 */
    restore_atf_regs
    ret

/*
 * [C-lang function] CPUON:  Trun on a cpu core
 *
 * Argument :
 *   [In]  X0 - target cpu core (value of mpidr)
 *
 * Return value:
 *   ATFSMC_RET_SUCCESS -          Success
 *   ATFSMC_RET_INVALID_STATE -    Called from non-secure state
 *   ATFSMC_RET_PARAM_ERROR -      Paramter error
 *   ATFSMC_RET_UNEXPECTED_ERROR - Unexpected error
 */
    AALIGN(4)
    AGLOBAL(atf_smc_cpuon)
ALABEL(atf_smc_cpuon)
    /* SPD SMCサービルコール実行前のレジスタを保存 */
    save_atf_regs
    /* Execute SMC Function call */
    mov   x3, xzr
    ldr   x2, =atf_ns_dummy_entry
    mov   x1, x0
    mov   x0, #0x0002   /* ATFSMC_CPU_ON */
    movk  x0, #((1 << 15) | (62 << 8)), lsl #16
    dsb   sy
    smc   #0
    /* SPD SMCサービルコール実行前のレジスタを復帰 */
    restore_atf_regs
    ret

/*
 * [C-lang function] SWITCH: Initiates a switch to the opposite world
 *
 * Argument :
 *   None
 *
 * Return value:
 *   ATFSMC_RET_SUCCESS -          Success
 *   ATFSMC_RET_INVALID_STATE -    Called from non permmison state
 */
    AALIGN(4)
    AGLOBAL(atf_smc_switch)
ALABEL(atf_smc_switch)
    /* SPD SMCサービルコール実行前のレジスタを保存 */
    save_atf_regs
    /* Execute SMC Function call */
    mov   x0, #0x0003   /* ATFSMC_SWITCH_SECURE_STATE */
    movk  x0, #((1 << 15) | (62 << 8)), lsl #16
    dsb   sy
    smc   #0
    /* SPD SMCサービルコール実行前のレジスタを復帰 */
    restore_atf_regs
    ret

/*
 * [C-lang function] GENINT:  Generate interrupt to Secure state
 *
 * Argument :
 *   [In]  X0 - IRQ number
 *
 * Return value:
 *   ATFSMC_RET_SUCCESS -          Success
 *   ATFSMC_RET_INVALID_STATE -    Called from non secure state
 *   ATFSMC_RET_PARAM_ERROR -      Paramter error
 */
    AALIGN(4)
    AGLOBAL(atf_smc_gen_int)
ALABEL(atf_smc_gen_int)
    /* SPD SMCサービルコール実行前のレジスタを保存 */
    save_atf_regs
    /* Execute SMC Function call */
    mov   x1, x0
    mov   x0, #0x0004   /* ATFSMC_GEN_INT */
    movk  x0, #((1 << 15) | (62 << 8)), lsl #16
    dsb   sy
    smc   #0
    /* SPD SMCサービルコール実行前のレジスタを復帰 */
    restore_atf_regs
    ret

/*
 * [C-lang function] Obtains the ID of a system call
 *
 * Argument :
 *   [In]  X0 - System call name
 *   [Out] X1 - System call ID
 *
 * Return value:
 *   ATFSMC_RET_SUCCESS -          Success
 *   ATFSMC_RET_PARAM_ERROR -      Paramter error
 */
    AALIGN(4)
    AGLOBAL(atf_smc_getid)
ALABEL(atf_smc_getid)
    /* SPD SMCサービルコール実行前のレジスタを保存 */
    save_atf_regs
    /* Execute SMC Function call */
    mov   x2, x1
    mov   x1, x0
    mov   x0, #0x0005   /* ATFSMC_GETID */
    movk  x0, #((1 << 15) | (62 << 8)), lsl #16
    dsb   sy
    smc   #0
    /* Get core ID */
    mrs   x13, mpidr_el1
    and   x13, x13, #0xFF
    /* Set restore address to x18 */
    mov   x14, #ATF_SAVE_REGS_SIZE
    mul   x14, x13, x14
    ldr   x13, =atf_smc_uses_regs
    add   x13, x13, x14
    /* Store system call id */
    ldr   x14,      [x13, #0x00]
    str   w1,       [x14]
    /* Restore registers */
    ldp   x1,  x2,  [x13, #0x00]
    ldp   x3,  x4,  [x13, #0x10]
    ldp   x5,  x6,  [x13, #0x20]
    ldp   x7,  x8,  [x13, #0x30]
    ldp   x9,  x10, [x13, #0x40]
    ldp   x11, x12, [x13, #0x50]
    ldp   x15, x16, [x13, #0x70]
    ldr   x17,      [x13, #0x80]
    ldp   x13, x14, [x13, #0x60]
    ret

/*
 * [C-lang function] SETPERM: set permissions for a safeg system call
 *
 * Argument :
 *   [In]  X0 - System call ID
 *   [In]  X1 - Execution permisson of secure state
 *   [In]  X2 - Execution permisson of non secure state
 *
 * Return value:
 *   ATFSMC_RET_SUCCESS -          Success
 *   ATFSMC_RET_INVALID_STATE -    Called from non permmison state
 *   ATFSMC_RET_PARAM_ERROR -      Paramter error
 *   ATFSMC_RET_UNEXPECTED_ERROR - Unexpected error
 */
    AALIGN(4)
    AGLOBAL(atf_smc_setperm)
ALABEL(atf_smc_setperm)
    /* SPD SMCサービルコール実行前のレジスタを保存 */
    save_atf_regs
    /* Execute SMC Function call */
    mov   x3, x2
    mov   x2, x1
    mov   x1, x0
    mov   x0, #0x0006   /* ATFSMC_SETPERM */
    movk  x0, #((1 << 15) | (62 << 8)), lsl #16
    dsb   sy
    smc   #0
    /* SPD SMCサービルコール実行前のレジスタを復帰 */
    restore_atf_regs
    ret

/*
 * [C-lang function] SIGNAL: signals an interrupt to the opposite world
 *
 * Argument :
 *   None
 *
 * Return value:
 *   ATFSMC_RET_SUCCESS -          Success
 *   ATFSMC_RET_UNEXPECTED_ERROR - Unexpected error
 */
    AALIGN(4)
    AGLOBAL(atf_smc_signal)
ALABEL(atf_smc_signal)
    /* SPD SMCサービルコール実行前のレジスタを保存 */
    save_atf_regs
    /* Execute SMC Function call */
    mov   x0, #0x0007   /* ATFSMC_SIGNAL */
    movk  x0, #((1 << 15) | (62 << 8)), lsl #16
    dsb   sy
    smc   #0
    /* SPD SMCサービルコール実行前のレジスタを復帰 */
    restore_atf_regs
    ret

/*
 * [C-lang function] REGDYN: register a dynamic system call
 *
 * Argument :
 *   [In]  X0 - Dynamic system call and initial parameters
 *   [In]  X1 - System call ID of registered dynamic system call
 *
 * Return value:
 *   ATFSMC_RET_SUCCESS -          Success
 *   ATFSMC_RET_INVALID_STATE -    Called from non permmison state
 *   ATFSMC_RET_PARAM_ERROR -      Paramter error
 *   ATFSMC_RET_FULL -             Already registered maximum number of dynamic system call
 */
    AALIGN(4)
    AGLOBAL(atf_smc_regdyn)
ALABEL(atf_smc_regdyn)
    /* SPD SMCサービルコール実行前のレジスタを保存 */
    save_atf_regs
    /* Execute SMC Function call */
    mov   x2, x1
    mov   x1, x0
    mov   x0, #0x0008   /* ATFSMC_REGDYN */
    movk  x0, #((1 << 15) | (62 << 8)), lsl #16
    dsb   sy
    smc   #0
    /* Get core ID */
    mrs   x13, mpidr_el1
    and   x13, x13, #0xFF
    /* Set restore address to x18 */
    mov   x14, #ATF_SAVE_REGS_SIZE
    mul   x14, x13, x14
    ldr   x13, =atf_smc_uses_regs
    add   x13, x13, x14
    /* Store system call id */
    ldr   x14,      [x13, #0x00]
    str   w1,       [x14]
    /* Restore registers */
    ldp   x1,  x2,  [x13, #0x00]
    ldp   x3,  x4,  [x13, #0x10]
    ldp   x5,  x6,  [x13, #0x20]
    ldp   x7,  x8,  [x13, #0x30]
    ldp   x9,  x10, [x13, #0x40]
    ldp   x11, x12, [x13, #0x50]
    ldp   x15, x16, [x13, #0x70]
    ldr   x17,      [x13, #0x80]
    ldp   x13, x14, [x13, #0x60]
    ret

/*
 * [C-lang function] READL: read specified address
 *
 * Argument :
 *   [In]  X0 - Read address (Flat model)
 *   [In]  X1 - Read buffer address
 *
 * Return value:
 *   ATFSMC_RET_SUCCESS -          Success
 *   ATFSMC_RET_PARAM_ERROR -      Paramter error
 */
    AALIGN(4)
    AGLOBAL(atf_smc_readl)
ALABEL(atf_smc_readl)
    /* SPD SMCサービルコール実行前のレジスタを保存 */
    save_atf_regs
    /* Execute SMC Function call */
    mov   x2, x1
    mov   x1, x0
    mov   x0, #0x000A   /* ATFSMC_READL */
    movk  x0, #((1 << 15) | (62 << 8)), lsl #16
    dsb   sy
    smc   #0
    /* Get core ID */
    mrs   x13, mpidr_el1
    and   x13, x13, #0xFF
    /* Set restore address to x18 */
    mov   x14, #ATF_SAVE_REGS_SIZE
    mul   x14, x13, x14
    ldr   x13, =atf_smc_uses_regs
    add   x13, x13, x14
    /* Store read data to read buffer */
    ldr   x14,      [x13, #0x00]
    str   w1,       [x14]
    /* Restore registers */
    ldp   x1,  x2,  [x13, #0x00]
    ldp   x3,  x4,  [x13, #0x10]
    ldp   x5,  x6,  [x13, #0x20]
    ldp   x7,  x8,  [x13, #0x30]
    ldp   x9,  x10, [x13, #0x40]
    ldp   x11, x12, [x13, #0x50]
    ldp   x15, x16, [x13, #0x70]
    ldr   x17,      [x13, #0x80]
    ldp   x13, x14, [x13, #0x60]
    ret

/*
 * [C-lang function] WRITEL: write specified address
 *
 * Argument :
 *   [In]  X0 - Write address (Flat model)
 *   [In]  X1 - Write data
 *
 * Return value:
 *   ATFSMC_RET_SUCCESS -          Success
 *   ATFSMC_RET_PARAM_ERROR -      Paramter error
 */
    AALIGN(4)
    AGLOBAL(atf_smc_writel)
ALABEL(atf_smc_writel)
    /* SPD SMCサービルコール実行前のレジスタを保存 */
    save_atf_regs
    /* Execute SMC Function call */
    mov   x2, x1
    mov   x1, x0
    mov   x0, #0x000B   /* ATFSMC_WRITEL */
    movk  x0, #((1 << 15) | (62 << 8)), lsl #16
    dsb   sy
    smc   #0
    /* SPD SMCサービルコール実行前のレジスタを復帰 */
    restore_atf_regs
    ret

/*
 * [C-lang function] RESTARNT: restart NT OS
 *
 * Argument :
 *   None
 *
 * Return value:
 *   ATFSMC_RET_SUCCESS -          Success
 *   ATFSMC_RET_INVALID_STATE -    Called from NonSecure state
 *   ATFSMC_RET_NOT_SUPPORTED -    Not supported (No implemented)
 */
    AALIGN(4)
    AGLOBAL(atf_smc_restarnt)
ALABEL(atf_smc_restarnt)
    /* SPD SMCサービルコール実行前のレジスタを保存 */
    save_atf_regs
    /* Execute SMC Function call */
    mov   x1, x0
    mov   x0, #0x000C   /* ATFSMC_RESTARNT */
    movk  x0, #((1 << 15) | (62 << 8)), lsl #16
    dsb   sy
    smc   #0
    /* SPD SMCサービルコール実行前のレジスタを復帰 */
    restore_atf_regs
    ret

/*
 * [C-lang function] INVOKE: invokes a system call
 *
 * Argument :
 *   [In]  X0 - System call ID
 *   [I/O] X1 - 1st parameter
 *   [I/O] X2 - 2nd parameter
 *   [I/O] X3 - 3rd parameter
 *
 * Return value:
 *   System call return code
 */
    AALIGN(4)
    AGLOBAL(atf_smc_invoke)
ALABEL(atf_smc_invoke)
    /* SPD SMCサービルコール実行前のレジスタを保存 */
    save_atf_regs
    /* Execute SMC Function call */
    mov   x4, x3
    mov   x3, x2
    mov   x2, x1
    mov   x1, x0
    mov   x0, #0x000D   /* ATFSMC_INVOKE */
    movk  x0, #((1 << 15) | (62 << 8)), lsl #16
    dsb   sy
    smc   #0
    /* SPD SMCサービルコール実行前のレジスタを復帰 */
    restore_atf_regs
    ret
