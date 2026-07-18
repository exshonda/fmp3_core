/*
 *  TOPPERS Software
 *      Toyohashi Open Platform for Embedded Real-Time Systems
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
 *  $Id: core_asm.inc 64 2018-08-20 06:57:46Z ertl-honda $
 */

/*
 *    アセンブリ言語用マクロ定義（PolarFire SoC GNU開発環境用）
 */

#ifndef TOPPERS_CHIP_ASM_INC
#define TOPPERS_CHIP_ASM_INC

/*
 *  自プロセッサのプロセッサIDの取得
 */
.macro my_pid reg
    csrr \reg, mhartid    
.endm

/*
 *  自プロセッサのプロセッサINDEXの取得
 */
.macro my_pidx reg
    csrr \reg, mhartid    
    addi \reg, \reg, -1
.endm

/*
 *  自プロセッサのPLICのコンテキストINDEXの取得（U54のみ）
 *   コンテキストINDEX = ((hartid - 1) * 2) + 1
 */
.macro my_cidx reg
    csrr \reg, mhartid
    addi \reg, \reg, -1
    slli \reg, \reg,  1
    addi \reg, \reg,  1
.endm

/*
 *  ディスパッチ時の追加コンテキストの保存
 */
.macro save_additional_regs_disp
.endm

/*
 *  ディスパッチ時に保存した追加コンテキストの復帰
 */
.macro restore_additional_regs_disp
.endm

/*
 *  タスク実行開始時(start_r)の追加コンテキスト初期化．PolarFire SoC Kit は既定不要(空)．
 *  (ESP32-S31 の TLS 初期化と同じフック点．a0=p_runtsk は不変とする契約)
 */
.macro init_additional_regs_start_r tcb_reg
.endm

/*
 *  割込み・例外ハンドラ入口処理での追加コンテキストの保存
 */
.macro save_additional_regs_int
.endm

/*
 *  割込み・例外ハンドラ入口処理で保存した追加コンテキストの復帰
 */
.macro restore_additional_regs_int
.endm

/*
 *  割込み・例外ハンドラからのディスパッチ時の追加コンテキストの保存
 */
.macro save_additional_regs_int_disp
.endm
	
/*
 *  割込み・例外ハンドラからのディスパッチ時に保存した追加コンテキストの復帰
 */
.macro restore_additional_regs_int_disp
.endm

/*
 *  CPU例外番号(exccode)を mcause から reg に取り出す
 *    非CLIC(標準 RISC-V)では mcause 全体が exccode のためマスク不要。reg のみ破壊。
 *    core_support.S の CPU例外入口で用いる(関数フックでなくマクロでインライン展開)。
 */
.macro core_get_exccode_asm reg
    csrr  \reg, mcause
.endm

/*
 *  コア依存部で共通な定義
 */
#include "core_asm.inc"

#endif /* TOPPERS_CORE_ASM_INC */
