/*
 *  TOPPERS/FMP Kernel
 *      Toyohashi Open Platform for Embedded Real-Time Systems/
 *      Flexible MultiProcessor Kernel
 *
 *  Copyright (C) 2018-2019 by Embedded and Real-Time Systems Laboratory
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

#ifndef TOPPERS_ATF_H
#define TOPPERS_ATF_H

/*
 *  SMC Function call return codes
 */
#define	ATFSMC_RET_SUCCESS			0
#define	ATFSMC_RET_INVALID_STATE	-1
#define	ATFSMC_RET_PARAM_ERROR		-2
#define	ATFSMC_RET_NOT_READY		-3
#define ATFSMC_RET_FULL				-4
#define ATFSMC_RET_INEXISTENT		-5
#define ATFSMC_RET_NOT_SUPPORTED	-6
#define ATFSMC_RET_UNEXPECTED_ERROR	-7

/*
 *  Type defination
 */
/* Information of entry points */
typedef uint32_t		vector_t;
typedef int64_t			atf_ret_t;

typedef struct
{
	vector_t	cpu_on_entry;
	vector_t	fiq_entry;
} atfsmc_vectors_t;

/* Definition of a system call for registration through safeg_syscall_reg() */
struct atfsmc_syscall {
	uint32_t is_t_callable;
	uint32_t is_nt_callable;
	uint8_t  name[8];
	uint32_t (*function)(uint32_t core_id, uint32_t ns,
						 uint32_t a, uint32_t b, uint32_t c);
};

/*
 *  Proto types
 */
/* ATF Trusted OS Vector Table */
extern atfsmc_vectors_t atf_vector_table;
/* SETVCT: Set TOPPERS kernel entry points (fiq vector etc.) */
extern atf_ret_t atf_smc_setvct(atfsmc_vectors_t *vectors);
/* INIT: Notify TOPPERS kernel initialize complete */
extern atf_ret_t atf_smc_init(void);
/* CPUON:    Trun on a cpu core */
extern atf_ret_t atf_smc_cpuon(uint64_t target_cpu);
/* SWITCH: Initiates a switch to the opposite world */
extern atf_ret_t atf_smc_switch(void);
/* GENINT:  Generate interrupt */
extern atf_ret_t atf_smc_gen_int(uint64_t irq_no);
/* GETID: obtains the ID of a dynamic system call by name */
extern atf_ret_t atf_smc_getid(const char *name, uint32_t *id);
/* SETPERM: set permissions for a safeg system call */
extern atf_ret_t atf_smc_setperm(uint32_t id, uint32_t is_t_callable, uint32_t is_nt_callable);
/* SIGNAL: signals an interrupt to the opposite world */
extern atf_ret_t atf_smc_signal(void);
/* REGDYN: register a dynamic system call */
extern atf_ret_t atf_smc_regdyn(const struct atfsmc_syscall *call, uint32_t *id);
/* READL: read specified address */
extern atf_ret_t atf_smc_readl(uint64_t address, uint32_t *rdata_ref);
/* WRITEL: write specified address */
extern atf_ret_t atf_smc_writel(uint64_t address, uint32_t wdata);
/* RESTARNT: restart NT OS */
extern atf_ret_t atf_smc_restarnt(void);
/* INVOKE: invokes a system call */
extern atf_ret_t atf_smc_invoke(uint32_t id, uint64_t a, uint64_t b, uint64_t c);

#endif  /* TOPPERS_ATF_H */
