/*
 *  TOPPERS/FMP Kernel
 *      Toyohashi Open Platform for Embedded Real-Time Systems/
 *      Flexible MultiProcessor Kernel
 *
 *  Copyright (C) 2018 by Embedded and Real-Time Systems Laboratory
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
 *  @(#) $Id $
 */

#ifndef TOPPERS_PSCI_SUPPORT_H
#define TOPPERS_PSCI_SUPPORT_H

#include "atf.h"

/*
 *  SMC Function call IDs
 */
#define PSCISMC_RV(func_num) \
		((SMC_TYPE_FAST << FUNCID_TYPE_SHIFT) | \
		((SMC_64) << FUNCID_CC_SHIFT) | \
		(4 << FUNCID_OEN_SHIFT) | \
		((func_num) & FUNCID_NUM_MASK))

/* CPUON:  Trun on a cpu core from NS-EL1 */
#define PSCISMC_SMC_CPU_ON		PSCISMC_RV(3)

/*
 *  SMC Function call return codes
 */
#define PSCISMC_RET_SUCCESS				0
#define PSCISMC_RET_NOT_SUPPORTED		-1
#define PSCISMC_RET_INVALID_PARAM		-2
#define PSCISMC_RET_DENIED				-3
#define PSCISMC_RET_ALREADY_ON			-4
#define PSCISMC_RET_ON_PENDING			-5
#define PSCISMC_RET_INTERNAL_FAILURE	-6
#define PSCISMC_RET_NOT_PRESENT			-7
#define PSCISMC_RET_DISABLED			-8
#define PSCISMC_RET_INVALID_ADDRESS		-9

/*
 *  Type defination
 */
typedef int64_t		psci_ret_t;

/*
 *  Proto types
 */
/* CPUON:    Trun on a cpu core from NS-EL1 */
extern psci_ret_t psci_smc_cpuon(uint64_t target_cpu, uint64_t entry_addr, uint64_t context_id);
#endif  /* TOPPERS_PSCI_SUPPORT_H */
