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
 *  $Id: core_test.h 335 2023-04-18 10:50:40Z ertl-honda $
 */

/*
 *    テストプログラムのコア依存部（RISC-V用）
 *
 *  このヘッダファイルは，target_test.h（または，そこからインクルードさ
 *  れるファイル）のみからインクルードされる．他のファイルから直接イン
 *  クルードしてはならない．
 */

#ifndef TOPPERS_CORE_TEST_H
#define TOPPERS_CORE_TEST_H

#include <t_stddef.h>
#include "riscv.h"

/*
 *  マルチプロセッサ向けのCPU例外ハンドラ番号の定義
 */
#define CPUEXC1_PRC1        (0x10000U | _CPUEXC1)
#define CPUEXC1_PRC2        (0x20000U | _CPUEXC1)
#define CPUEXC1_PRC3        (0x30000U | _CPUEXC1)
#define CPUEXC1_PRC4        (0x40000U | _CPUEXC1)

/*
 *  サンプルプログラムで用いるCPU例外の発生
 */
#define _CPUEXC1               EXCNO_IINST     /* 未定義命令 */
#define RAISE_CPU_EXCEPTION    RAISE_CPU_EXCEPTION_IINST
#define PREPARE_RETURN_CPUEXC  PREPARE_RETURN_CPUEXC_IINST

/*
 *  違反命令によるCPU例外の発生
 *
 *  未定義命令によりCPU例外を発生させる．使用している未定義命令は，
 *  オール0の命令である．
 *   
 */
#ifdef __riscv_compressed
#define RAISE_CPU_EXCEPTION_IINST  Asm(".short 0x0000")
#else /* __riscv_compressed */
#define RAISE_CPU_EXCEPTION_IINST  Asm(".word 0x00000000")
#endif /* __riscv_compressed */

/*
 *  違反命令によるCPU例外からのリターンアドレスの設定
 */
#ifdef __riscv_compressed
#define PREPARE_RETURN_CPUEXC_IINST  (((T_EXCINF *)p_excinf)->pc += 2)
#else /* __riscv_compressed */
#define PREPARE_RETURN_CPUEXC_IINST  (((T_EXCINF *)p_excinf)->pc += 4)
#endif /* __riscv_compressed */

#endif /* TOPPERS_CORE_TEST_H */
