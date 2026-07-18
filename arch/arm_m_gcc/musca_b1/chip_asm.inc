/*
 *  アセンブリ言語用マクロ定義のチップ依存部（Musca-B1 / SSE-200 用）
 */

#ifndef TOPPERS_CHIP_ASM_INC
#define TOPPERS_CHIP_ASM_INC

#if TNUM_PRCID >= 2

/*
 * dstに自プロセッサのインデックス（0オリジン）を代入するマクロ．
 * dst = get_my_prcidx()
 *
 * スタック設定後に用いる版．現在のスタックポインタ（MSP）が二次コア
 * （prc2）の割込みスタック範囲内なら 1，そうでなければ 0 を返す．
 *   _kernel_istk_table[1]   : prc2 の istack の先頭番地
 *   _kernel_istkpt_table[1] : prc2 の istack のスタックポインタ初期値（上限）
 * 例外・割込みハンドラも MSP（=istack）上で動作するため，呼出し文脈に依らず
 * 正しい値を返す（冪等）．
 *
 * このマクロは my_pcb / my_exc_tbl / my_istkpt 等から間接的に呼ばれ，それら
 * は \reg 以外のレジスタを破壊しないことを期待している．そのため一時レジスタ
 * は ip(r12) のみを用い（AAPCS のスクラッチかつ本コードの呼出し文脈で非生
 * 存），\reg への結果書込みは比較完了後に行う．
 */
.macro my_prcidx reg
    push {\reg, ip}                 /* \reg と ip を退避 */
    mrs  ip, msp                    /* ip = MSP（自コアの istack を指す） */
    ldr  \reg, =_kernel_istk_table
    ldr  \reg, [\reg, #4]           /* \reg = istk_table[1] (prc2 base) */
    cmp  ip, \reg
    blo  1f                         /* msp < base → prc1(0) */
    ldr  \reg, =_kernel_istkpt_table
    ldr  \reg, [\reg, #4]           /* \reg = istkpt_table[1] (prc2 top) */
    cmp  ip, \reg
    bhi  1f                         /* msp > top  → prc1(0) */
    movs ip, #1                     /* base <= msp <= top → prc2(1) */
    b    2f
1:
    movs ip, #0
2:
    str  ip, [sp, #0]               /* 退避領域の \reg を結果で上書き */
    pop  {\reg, ip}                 /* \reg = 結果，ip を復元 */
.endm

/*
 * dstに自プロセッサのインデックス（0オリジン）を代入する起動時専用マクロ．
 * dst = get_my_prcidx() 相当（スタック設定前）
 *
 * SSE-200 には自コア番号を読めるレジスタが無いため，ブートハンドシェイク用
 * フラグ _kernel_mp_boot_flag で判定する．
 *   フラグ == 0 : 最初に走るマスタコア（CPU0） → 0
 *   フラグ != 0 : CPU0 が二次コア解放直前にセット済 → 二次コア（CPU1） → 1
 */
.macro my_prcidx_boot reg
    ldr  \reg, =_kernel_mp_boot_flag
    ldr  \reg, [\reg]
    cmp  \reg, #0
    beq  3f
    movs \reg, #1
    b    4f
3:
    movs \reg, #0
4:
.endm

/* core_asm.inc のデフォルト my_prcidx_boot を抑止する */
#define MY_PRCIDX_BOOT_DEFINED

#else /* TNUM_PRCID >= 2 */

/*
 * 単一プロセッサ（TNUM_PRCID == 1）なので常に 0．
 */
.macro my_prcidx reg
    movs \reg, #0
.endm

#endif /* TNUM_PRCID >= 2 */

/*
 *  コア依存部で共通な定義
 */
#include "core_asm.inc"

#endif /* TOPPERS_CHIP_ASM_INC */
