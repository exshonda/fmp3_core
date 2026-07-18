/*
 *  TOPPERS/FMP Kernel
 *      Toyohashi Open Platform for Embedded Real-Time Systems/
 *      Flexible MultiProcessor Kernel
 * 
 *  Copyright (C) 2024 by Embedded and Real-Time Systems Laboratory
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
 *  $Id: core_support.S 382 2023-09-14 04:39:49Z ertl-honda $
 */

/*
 *    カーネルのチップ依存部のアセンブリ言語部（PolarFire SoC 用）
 */

#define TOPPERS_MACRO_ONLY
#define TOPPERS_ASM_MACRO
#define UINT_C(val)     (val) /* uint_t型の定数を作るマクロ */
#define ULONG_C(val)    (val) /* ulong_t型の定数を作るマクロ */
#define CAST(type, val) (val) /* 型キャストを行うマクロ */
#define PLIC_REG(base, offset)  ((base) + (offset))

#include "kernel_impl.h"
#include "offset.h"
#include "target_asm.inc"

/*
 *  トラップベクタテーブル(256byteアライン)
 */
    AO_PUSH    /* 設定の保存 */
    AO_NORVC
    ABALIGN(256)
    AGLOBAL(trap_vector_table)
ALABEL(trap_vector_table)
    j core_exc_entry  /*  0:Synchronous Exception */
    j core_int_entry  /*  1:SSI */
    j core_exc_entry  /*  2:Reserved */
    j core_int_entry  /*  3:MSI */
    j core_exc_entry  /*  4:Reserved */
    j core_int_entry  /*  5:STI */
    j core_exc_entry  /*  6:Reserved */
    j core_int_entry  /*  7:MTI */
    j core_exc_entry  /*  8:Reserved */
    j core_int_entry  /*  9:SEI */
    j core_exc_entry  /* 10:Reserved */
    j core_int_entry  /* 11:MEI */
    j core_exc_entry  /* 12:Reserved */
    j core_exc_entry  /* 13:Reserved */
    j core_exc_entry  /* 14:Reserved */
    j core_exc_entry  /* 15:Reserved */
    j core_int_entry  /* 16:Local Interrupt */
    j core_int_entry  /* 17:Local Interrupt */
    j core_int_entry  /* 18:Local Interrupt */
    j core_int_entry  /* 19:Local Interrupt */
    j core_int_entry  /* 20:Local Interrupt */
    j core_int_entry  /* 21:Local Interrupt */
    j core_int_entry  /* 22:Local Interrupt */
    j core_int_entry  /* 23:Local Interrupt */
    j core_int_entry  /* 24:Local Interrupt */
    j core_int_entry  /* 25:Local Interrupt */
    j core_int_entry  /* 26:Local Interrupt */
    j core_int_entry  /* 27:Local Interrupt */
    j core_int_entry  /* 28:Local Interrupt */
    j core_int_entry  /* 29:Local Interrupt */
    j core_int_entry  /* 30:Local Interrupt */
    j core_int_entry  /* 31:Local Interrupt */
    j core_int_entry  /* 32:Local Interrupt */
    j core_int_entry  /* 33:Local Interrupt */
    j core_int_entry  /* 34:Local Interrupt */
    j core_int_entry  /* 35:Local Interrupt */
    j core_int_entry  /* 36:Local Interrupt */
    j core_int_entry  /* 37:Local Interrupt */
    j core_int_entry  /* 38:Local Interrupt */
    j core_int_entry  /* 39:Local Interrupt */
    j core_int_entry  /* 40:Local Interrupt */
    j core_int_entry  /* 41:Local Interrupt */
    j core_int_entry  /* 42:Local Interrupt */
    j core_int_entry  /* 43:Local Interrupt */
    j core_int_entry  /* 44:Local Interrupt */
    j core_int_entry  /* 45:Local Interrupt */
    j core_int_entry  /* 46:Local Interrupt */
    j core_int_entry  /* 47:Local Interrupt */
    AO_POP     /* 設定の復帰 */

/*
 *  割込み入口処理での割込みコントローラ操作
 * 
 *  受け付けた割込みに応じて割込み優先度マスクをセットする
 *  以下の内容をレジスタに入れてリターンする
 *   a0:割込み番号
 *   a1:割込み前の優先度(内部表現)
 *   a2:割込みハンドラアドレス
 */
    ATEXT
    AALIGN(4)
    AGLOBAL(irc_begin_int)
ALABEL(irc_begin_int)
    /*
     *  ここからはCLIC依存となる
     *   CLICになると番号以外の情報が入る
     */
    csrr t0, mcause
    andi t0, t0, 0x3f  /* 割込み番号のみマスク(このマスク幅は実装により異なる) */

    /*
     *  MEIなら分岐
     */
    li  t1, MCAUSE_MEI
    beq t0, t1, do_mei

    my_pid t1                     /* プロセッサID 取得 */
    slli   t1, t1, 16             /* 割込み番号の生成 */
    add    a0, t0, t1

    /*
     *  ローカル割込みの分岐
     */
    li  t1, MCAUSE_MSI
    beq t0, t1, do_msi

    li  t1, MCAUSE_MTI
    beq t0, t1, do_mti

    /* 
     *  それ以外のローカル割込み 
     */
    la     a2, default_int_handler /* 割込みハンドラアドレス */
    li     a3, 7                   /* 割込み優先度     */
    j      irc_begin_int_1


    /*
     *  Machine software interruptの場合
     */
ALABEL(do_msi)
    la     a2, msi_handler        /* 割込みハンドラアドレス */
    li     a3, 1                  /* 割込み優先度     */
    j      irc_begin_int_1

    /*
     *  Machine timer interruptの場合
     */
ALABEL(do_mti)
    la     a2, target_hrt_handler /* 割込みハンドラアドレス */
    li     a3, 1                  /* 割込み優先度     */
    j      irc_begin_int_1

    /*
     *  Machine External interruptの場合
     */
ALABEL(do_mei)
    /* 
     *  割込み番号の取得
     */
    my_cidx t0                       /* PLICのコンテキストINDEX 取得 */
    li      t1, PLIC_CC_BASE         /* 割込み要因の取得 */
    li      t2, PLIC_CC_OFFSET 
    mul     t0, t2, t0
    add     t0, t0, t1
    lwu     a0, 0(t0)                 /* 割込み番号 */

    /*
     *  0ならSpurious割込みとして何もしない
     */
    beqz    a0, irc_begin_int_2
    
    /* 
     *  割込みハンドラアドレスの取得
     */
    my_pcb   t0, t1
    lx       t1, PCB_p_inh_tbl(t0)    /* 割込みハンドラテーブルの読込み */
#if __riscv_xlen == 64
    slli     t2, a0, 3
#elif __riscv_xlen == 32
    slli     t2, a0, 2
#endif /* __riscv_xlen == 64 */
    add      t2, t1, t2
    lx       a2, 0(t2)                /* 割込みハンドラの番地  */

    /* 
     *  割込み優先度の取得 
     *  PLICの優先度レジスタから取得
     */
    slli t2, a0, 2                /* 割込み番号を2倍 */
    li   t3, PLIC_IPRI_BASE
    add  t3, t3, t2
    lwu  a3, 0(t3)

    /*
     *  PLIC依存の割込み優先度設定
     *   a3 : 設定する割込み優先度(内部表現)
     */
ALABEL(irc_begin_int_1)
   /*
    *  PLICの割込み優先度を設定する
    */
    my_cidx t0                       /* PLICのコンテキストINDEX 取得 */
    li      t1, PLIC_TH_BASE         /* 割込み要因の取得 */
    li      t2, PLIC_TH_OFFSET 
    mul     t0, t2, t0
    add     t0, t0, t1
    lw      a1, 0(t0)                /* 割込み前の割込み優先度マスク */
    sw      a3, 0(t0)                /* 新しい割込み優先度マスクを設定 */

   /*
    *  MTI/MSIのmieによる割込み禁止
    */
    la     t0, MIE_MTIE|MIE_MSIE      /* MTIとMSIの禁止 */
    csrrc  t0, mie, t0    

ALABEL(irc_begin_int_2)
    jr       ra

    /*
     *  割込み出口処理での割込みコントローラ操作
     *
     *  a0:割込み番号
     *  a1:割込み前の優先度
     */
    ATEXT
    AALIGN(4)
    AGLOBAL(irc_end_int)
ALABEL(irc_end_int)

    /*
     *  0ならSpurious割込みとして何もしない
     */
    beqz    a0, irc_end_int_2

    /*
     *  グローバル割込みの場合は割込みのクリア
     */
    srli    t0, a0, 16
    bnez    t0, irc_end_int_1

    my_cidx t0                       /* PLICのコンテキストINDEX 取得 */
    li      t1, PLIC_CC_BASE         
    li      t2, PLIC_CC_OFFSET 
    mul     t0, t2, t0
    add     t0, t0, t1
    sw      a0, 0(t0)                 /* 割込み処理終了通知 */

ALABEL(irc_end_int_1)
    my_cidx t0                       /* PLICのコンテキストINDEX 取得 */
    li      t1, PLIC_TH_BASE         /* 割込み要因の取得 */
    li      t2, PLIC_TH_OFFSET 
    mul     t0, t2, t0
    add     t0, t0, t1
    sw      a1, 0(t0)                /* 割込み前の割込み優先度マスクを設定 */

    bnez    a1, irc_end_int_2        /* 全許可でないならリターン */

    la      t0, MIE_MTIE|MIE_MSIE      /* MTIとMSIの許可 */
    csrrs   t0, mie, t0        

ALABEL(irc_end_int_2)
    jr ra    


/*
 *  CPU例外発生前の割込み優先度の取得
 *
 *  CPU例外の発生で割込み優先度が変わることはないため，現在の割込み優先
 *  度を返す．
 */
    ATEXT
    AALIGN(4)
    AGLOBAL(irc_get_intpri)
ALABEL(irc_get_intpri)
    /*
     *  割込み優先度マスクを外部表現に変換して返す．
     */
    my_cidx t0                       /* PLICのコンテキストINDEX 取得 */
    li      t1, PLIC_TH_BASE         /* 割込み要因の取得 */
    li      t2, PLIC_TH_OFFSET 
    mul     t0, t2, t0
    add     t0, t0, t1
    lw      a1, 0(t0)                /* 割込み優先度マスクを取得 */
    neg     a0, a1                   /* 外部表現に変換 */
    jr      ra

/*
 *  CPU例外ハンドラ呼出し前の割込みコントローラ操作
 *
 *  irc_end_excで用いる情報（CPU例外発生前の割込み優先度マスク）を返す．
 */
    ATEXT
    AALIGN(4)
    AGLOBAL(irc_begin_exc)
ALABEL(irc_begin_exc)
    my_cidx t0                       /* PLICのコンテキストINDEX 取得 */
    li      t1, PLIC_TH_BASE         /* 割込み要因の取得 */
    li      t2, PLIC_TH_OFFSET 
    mul     t0, t2, t0
    add     t0, t0, t1
    lw      a0, 0(t0)                /* 割込み優先度マスクを取得 */
    jr      ra


/*
 *  CPU例外ハンドラ呼出し後の割込みコントローラ操作
 */
    ATEXT
    AALIGN(4)
    AGLOBAL(irc_end_exc)
ALABEL(irc_end_exc)
    /*
     *  割込み優先度マスクを元に戻す．
     */
    my_cidx t0                       /* PLICのコンテキストINDEX 取得 */
    li      t1, PLIC_TH_BASE         /* 割込み要因の取得 */
    li      t2, PLIC_TH_OFFSET 
    mul     t0, t2, t0
    add     t0, t0, t1
    sw      a0, 0(t0)                /* 割込み優先度マスクを設定 */
    jr      ra
