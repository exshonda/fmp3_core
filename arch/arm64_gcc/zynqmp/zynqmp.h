/*
 *  TOPPERS/FMP Kernel
 *      Toyohashi Open Platform for Embedded Real-Time Systems/
 *      Flexible MultiProcessor Kernel
 *
 *  Copyright (C) 2020 by Embedded and Real-Time Systems Laboratory
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
 *  @(#) $Id: zynqmp.h 215 2020-02-17 05:45:15Z ertl-honda $
 */

/*
 *		ZynqMPのハードウェア資源の定義
 */
#ifndef TOPPERS_ZYNQMP_H
#define TOPPERS_ZYNQMP_H

/*
 *  Memory Address
 */
#define OCRAM_ADDR 				UINT_C(0xFFFC0000)
#define OCRAM_SIZE 				UINT_C(0x00040000)

/*
 *  GIC
 */
#define TOPPERS_GIC_VER         2
#define GICD_BASE				UINT_C(0xF9010000)
#define GICC_BASE				UINT_C(0xF9020000)
#define GICH_BASE				UINT_C(0x2c04f000)
#define GICV_BASE				UINT_C(0x2c06f000)

#define GIC_PRI_LEVEL			32

#ifndef GIC_TNUM_INTNO
#define GIC_TNUM_INTNO		UINT_C(352)
#endif /* GIC_TNUM_INTNO */

/*
 *  Interrupt Number
 */
#define GIC_IRQNO_HYPTIMER		26
#define GIC_IRQNO_VIRTIMER		27
#define GIC_IRQNO_SPHYTIMER		29
#define GIC_IRQNO_NSPHYTIMER	30
#define GIC_IRQNO_APTIMER0		91
#define GIC_IRQNO_APTIMER1		92

/*
 *  System Timestamp Generator - Secure
 */
#define XIOU_SCNTRS_FREQ_HZ				99998999

#define XIOU_SCNTRS_BASEADDR			0xFF260000U
#define XIOU_SCNTRS_CNT_CNTRL_REG		((XIOU_SCNTRS_BASEADDR) + 0x00000000U)
#define XIOU_SCNTRS_FREQ_REG			((XIOU_SCNTRS_BASEADDR) + 0x00000020U)
#define XIOU_SCNTRS_CNT_CNTRL_REG_EN	0x00000001U

/*
 *  UART
 */
#define ZYNQMP_UART0_BASE		0xFF000000U
#define ZYNQMP_UART1_BASE		0xFF010000U

#define ZYNQMP_UART0_IRQ		UINT_C(21 + 32)
#define ZYNQMP_UART1_IRQ		UINT_C(22 + 32)


/*
 *  Peripherals
 */
#define PMU_GLOBAL_BASEADDR					0xFFD80000U
#define PMU_GLOBAL_REQ_PWRUP_INT_EN			((PMU_GLOBAL_BASEADDR) + 0x00000118U)
#define PMU_GLOBAL_REQ_PWRUP_TRIG			((PMU_GLOBAL_BASEADDR) + 0x00000120U)
#define PMU_GLOBAL_REQ_PWRUP_STATUS			((PMU_GLOBAL_BASEADDR) + 0x00000110U)
#define PMU_GLOBAL_PWR_STATE_FP_MASK		0x00400000U
#define PMU_GLOBAL_PWR_STATE_L2_BANK0_MASK	0x00000080U

#define CRF_APB_BASEADDR					0xFD1A0000U
#define CRF_APB_ACPU_CTRL					((CRF_APB_BASEADDR) + 0x00000060U)
#define CRF_APB_ACPU_CTRL_CLKACT_FULL_MASK	0x01000000U
#define CRF_APB_ACPU_CTRL_CLKACT_HALF_MASK	0x02000000U

#define CRF_APB_RST_FPD_APU						((CRF_APB_BASEADDR) + 0x00000104U)
#define CRF_APB_RST_FPD_APU_ACPU0_PWRON_OFFSET	10U
#define CRF_APB_RST_FPD_APU_APU_L2_RESET_MASK	0x00000100U

#define APU_BASEADDR		0xFD5C0000U
#define APU_CONFIG_0		((APU_BASEADDR) + 0x00000020U)
#define APU_VINITHI_MASK	0x00000f00

#define APU_RVBARADDR0L		((APU_BASEADDR) + 0X00000040U)
#define APU_RVBARADDR0H		((APU_BASEADDR) + 0X00000044U)
#define APU_RVBARADDR1L		((APU_BASEADDR) + 0X00000048U)
#define APU_RVBARADDR1H		((APU_BASEADDR) + 0X0000004CU)
#define APU_RVBARADDR2L		((APU_BASEADDR) + 0X00000050U)
#define APU_RVBARADDR2H		((APU_BASEADDR) + 0X00000054U)
#define APU_RVBARADDR3L		((APU_BASEADDR) + 0X00000058U)
#define APU_RVBARADDR3H		((APU_BASEADDR) + 0X0000005CU)

#endif /* TOPPERS_ZYNQMP_H */
