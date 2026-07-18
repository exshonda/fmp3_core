/*
 *  TOPPERS/FMP Kernel
 *      Toyohashi Open Platform for Embedded Real-Time Systems/
 *      Flexible MultiProcessor Kernel
 * 
 *  Copyright (C) 2006-2023 by Embedded and Real-Time Systems Laboratory
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
 *		テストプログラムのコア依存部（ARM用）
 *
 *  このヘッダファイルは，target_test.h（または，そこからインクルードさ
 *  れるファイル）のみからインクルードされる．他のファイルから直接イン
 *  クルードしてはならない．
 */

#ifndef TOPPERS_CORE_TEST_H
#define TOPPERS_CORE_TEST_H

#include <t_stddef.h>
#include "arm.h"

/*
 *  不正アドレスの定義（メモリのない番地に設定する）
 */
#ifndef ILLEGAL_IADDR
#define ILLEGAL_IADDR			0xd0000000U		/* 不正命令アドレス */
#endif /* ILLEGAL_IADDR */

#ifndef ILLEGAL_DADDR
#define ILLEGAL_DADDR			0xd0000000U		/* 不正データアドレス */
#endif /* ILLEGAL_DADDR */

/*
 *  マルチプロセッサ向けのCPU例外ハンドラ番号の定義
 */
#define EXCNO_MACV_INST_PRC1	(0x10000U | EXCNO_PABORT)
#define EXCNO_MACV_INST_PRC2	(0x20000U | EXCNO_PABORT)
#define EXCNO_MACV_INST_PRC3	(0x30000U | EXCNO_PABORT)
#define EXCNO_MACV_INST_PRC4	(0x40000U | EXCNO_PABORT)
#define EXCNO_MACV_DATA_PRC1	(0x10000U | EXCNO_DABORT)
#define EXCNO_MACV_DATA_PRC2	(0x20000U | EXCNO_DABORT)
#define EXCNO_MACV_DATA_PRC3	(0x30000U | EXCNO_DABORT)
#define EXCNO_MACV_DATA_PRC4	(0x40000U | EXCNO_DABORT)

#define CPUEXC1_PRC1		(0x10000U | _CPUEXC1)
#define CPUEXC1_PRC2		(0x20000U | _CPUEXC1)
#define CPUEXC1_PRC3		(0x30000U | _CPUEXC1)
#define CPUEXC1_PRC4		(0x40000U | _CPUEXC1)

#define EXCNO_UNDEF_PRC1	(0x10000U | EXCNO_UNDEF)
#define EXCNO_UNDEF_PRC2	(0x20000U | EXCNO_UNDEF)
#define EXCNO_UNDEF_PRC3	(0x30000U | EXCNO_UNDEF)
#define EXCNO_UNDEF_PRC4	(0x40000U | EXCNO_UNDEF)
#define EXCNO_SVC_PRC1		(0x10000U | EXCNO_SVC)
#define EXCNO_SVC_PRC2		(0x20000U | EXCNO_SVC)
#define EXCNO_SVC_PRC3		(0x30000U | EXCNO_SVC)
#define EXCNO_SVC_PRC4		(0x40000U | EXCNO_SVC)
#define EXCNO_PABORT_PRC1	(0x10000U | EXCNO_PABORT)
#define EXCNO_PABORT_PRC2	(0x20000U | EXCNO_PABORT)
#define EXCNO_PABORT_PRC3	(0x30000U | EXCNO_PABORT)
#define EXCNO_PABORT_PRC4	(0x40000U | EXCNO_PABORT)
#define EXCNO_DABORT_PRC1	(0x10000U | EXCNO_DABORT)
#define EXCNO_DABORT_PRC2	(0x20000U | EXCNO_DABORT)
#define EXCNO_DABORT_PRC3	(0x30000U | EXCNO_DABORT)
#define EXCNO_DABORT_PRC4	(0x40000U | EXCNO_DABORT)
#define EXCNO_FATAL_PRC1	(0x10000U | EXCNO_FATAL)
#define EXCNO_FATAL_PRC2	(0x20000U | EXCNO_FATAL)
#define EXCNO_FATAL_PRC3	(0x30000U | EXCNO_FATAL)
#define EXCNO_FATAL_PRC4	(0x40000U | EXCNO_FATAL)

/*
 *  サンプルプログラムで用いるCPU例外の発生
 */
#if defined(USE_CPUEXC_SVC)

#define _CPUEXC1					EXCNO_SVC		/* スーパバイザコール */
#define RAISE_CPU_EXCEPTION		RAISE_CPU_EXCEPTION_SVC
#define PREPARE_RETURN_CPUEXC	PREPARE_RETURN_CPUEXC_SVC

#elif defined(USE_CPUEXC_PABORT)

#define _CPUEXC1				EXCNO_PABORT	/* プリフェッチアボート */
#define RAISE_CPU_EXCEPTION		RAISE_CPU_EXCEPTION_PABORT
#define PREPARE_RETURN_CPUEXC	PREPARE_RETURN_CPUEXC_PABORT

#elif defined(USE_CPUEXC_DABORT)

#define _CPUEXC1				EXCNO_DABORT	/* データアボート */
#define RAISE_CPU_EXCEPTION		RAISE_CPU_EXCEPTION_DABORT
#define PREPARE_RETURN_CPUEXC	PREPARE_RETURN_CPUEXC_DABORT

#elif defined(USE_CPUEXC_FATAL)

#define _CPUEXC1				EXCNO_FATAL		/* フェイタルデータアボート */
#define RAISE_CPU_EXCEPTION		RAISE_CPU_EXCEPTION_FATAL
/* フェイタルデータアボート例外ハンドラからリターンしてはならない */

#else

#define _CPUEXC1				EXCNO_UNDEF		/* 未定義命令 */
#define RAISE_CPU_EXCEPTION		RAISE_CPU_EXCEPTION_UNDEF
#define PREPARE_RETURN_CPUEXC	PREPARE_RETURN_CPUEXC_UNDEF

#endif

/*
 *  スーパバイザコールによるCPU例外の発生
 *
 *  svc #0によりCPU例外を発生させる．スーパバイザモードでCPU例外を発生
 *  させる場合には，svc命令によりlrレジスタが上書きされるため，lrレジ
 *  スタは破壊されるものと指定している．CPU例外ハンドラからそのままリ
 *  ターンすることで，svc命令の次の命令から実行が継続する．
 */
#define RAISE_CPU_EXCEPTION_SVC			Asm("svc #0":::"lr")
#define PREPARE_RETURN_CPUEXC_SVC

/*
 *  プリフェッチアボートによるCPU例外の発生（スーパバイザモードでCPU例
 *  外を発生させる場合）
 *
 *  不正な番地を関数の先頭番地として呼び出すことで，プリフェッチアボー
 *  トによるCPU例外を発生させる．不正な番地に分岐した命令をスキップし
 *  てCPU例外ハンドラからリターンするために，戻り番地には，lrレジスタ
 *  の値（CPU例外がスーパバイザモードで発生した場合には，不正な番地へ
 *  の分岐命令からのリターン番地が入っている）を設定する．
 */
#define RAISE_CPU_EXCEPTION_PABORT		(((void (*)(void)) ILLEGAL_IADDR)())
#define PREPARE_RETURN_CPUEXC_PABORT	(((T_EXCINF *) p_excinf)->pc \
											= ((T_EXCINF *) p_excinf)->lr)

/*
 *  データアボートによるCPU例外の発生
 *
 *  不正な番地をリードすることで，データアボートによるCPU例外を発生さ
 *  せる．データアボートを起こした命令をスキップしてCPU例外ハンドラか
 *  らリターンするために，戻り番地から4を減算する（ARMモードで使うこと
 *  を想定している）．
 */
#define RAISE_CPU_EXCEPTION_DABORT		(void)(*((volatile uint32_t *) \
															ILLEGAL_DADDR))
#define PREPARE_RETURN_CPUEXC_DABORT	(((T_EXCINF *) p_excinf)->pc -= 4U)

/*
 *  フェイタルデータアボートによるCPU例外の発生
 *
 *  スタックポインタを不正にして，未定義命令を実行することで，フェイタ
 *  ルデータアボートによるCPU例外を発生させる．CPU例外ハンドラからリター
 *  ンしてはならない．
 */
#define RAISE_CPU_EXCEPTION_FATAL		Asm("mov sp, %0\n" \
										  "\t.word 0xf0500090" \
										  ::"I"(ILLEGAL_DADDR))

/*
 *  未定義命令によるCPU例外の発生
 *
 *  未定義命令によりCPU例外を発生させる．使用している未定義命令は，
 *  "Multiply and multiply accumulate"命令群のエンコーディング内におけ
 *  る未定義命令である．CPU例外ハンドラからそのままリターンすることで，
 *  未定義命令の次の命令から実行が継続する（ARMモードで使うことを想定
 *  している）．
 */
#define RAISE_CPU_EXCEPTION_UNDEF		Asm(".word 0xf0500090")
#define PREPARE_RETURN_CPUEXC_UNDEF

#endif /* TOPPERS_CORE_TEST_H */
