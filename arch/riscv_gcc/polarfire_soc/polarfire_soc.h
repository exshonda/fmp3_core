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
 *  $Id: zynq7000.h 117 2018-12-21 08:59:55Z ertl-honda $
 */

/*
 *    PolarFire SoC のハードウェア資源の定義
 */
#ifndef TOPPERS_POLAFIRE_SOC_H
#define TOPPERS_POLAFIRE_SOC_H

/*
 * n はプロセッサIDが指定される
 */
#define CLINT_MSIP(pid)       (uint32_t *)(0x02000000UL + (pid * 4UL))
#define MTIMER_MTIMECMP(pid)  (uint64_t *)(0x02004000UL + (pid * 8UL))
#define MTIMER_MTIME          (uint64_t *)(0x0200BFF8UL)

/*
 *  PLICに関する定義
 */
#define PLIC_BASE  ULONG_C(0x0C000000)

/*
 *  PLICの割込み数
 */
#ifndef PLIC_TNUM_INTNO
#define PLIC_TNUM_INTNO    UINT_C(182)
#endif /* GIC_TNUM_INTNO */

/*
 *  MMUARTに関する定義
 */
#define MMUART0_BASE  0x20000000UL
#define MMUART1_BASE  0x20100000UL
#define MMUART2_BASE  0x20102000UL
#define MMUART3_BASE  0x20104000UL
#define MMUART4_BASE  0x20106000UL

#define MMUART0_INTNO  UINT_C(90)
#define MMUART1_INTN1  UINT_C(91)
#define MMUART1_INTN2  UINT_C(92)
#define MMUART1_INTN3  UINT_C(93)
#define MMUART1_INTN4  UINT_C(94)

/*
 *  システムレジスタの定義
 */
#define SYSREG_BASE 0x20002000UL
#define SYSREG_SUBBLK_CLOCK_CR (uint32_t*)(SYSREG_BASE + 0x84U)
#define SYSREG_SOFT_RESET_CR   (uint32_t*)(SYSREG_BASE + 0x88U)

#define SYSREG_SUBBLK_CLOCK_CR_UART0  UINT_C(0x0020)
#define SYSREG_SOFT_RESET_CR_UART0    UINT_C(0x0020)
#define SYSREG_SUBBLK_CLOCK_CR_UART1  UINT_C(0x0040)
#define SYSREG_SOFT_RESET_CR_UART1    UINT_C(0x0040)
#define SYSREG_SUBBLK_CLOCK_CR_UART2  UINT_C(0x0080)
#define SYSREG_SOFT_RESET_CR_UART2    UINT_C(0x0080)
#define SYSREG_SUBBLK_CLOCK_CR_UART3  UINT_C(0x0100)
#define SYSREG_SOFT_RESET_CR_UART3    UINT_C(0x0100)
#define SYSREG_SUBBLK_CLOCK_CR_UART4  UINT_C(0x0200)
#define SYSREG_SOFT_RESET_CR_UART4    UINT_C(0x0200)

#endif /* TOPPERS_POLARFIRE_SOC_H */
