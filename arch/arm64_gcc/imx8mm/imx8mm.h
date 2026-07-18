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
 */

/*
 *		IMX8MMのハードウェア資源の定義
 */
#ifndef TOPPERS_IMX8MM_H
#define TOPPERS_IMX8MM_H

#define OCRAM_ADDR				UINT_C(0x0000900000)
#define OCRAM_SIZE				UINT_C(0x0000020000)

/*
 *  Cortex-A53 memory mapped register base
 */
#define A53_GIC_CPUIF_BASE		UINT_C(0x00000000)
#define A53_GIC_V_IFCTL_BASE	UINT_C(0x00010000)
#define A53_GIC_V_CPUIF_BASE	UINT_C(0x00020000)

/*
 *  GIC
 */
#define TOPPERS_GIC_VER         4
#define GICD_BASE               0x38800000
#define GICR_BASE               0x38880000
#define GICR_SIZE               0x00020000
#define GICR_RD_OFFSET          0x00000000
#define GICR_SGI_OFFSET         0x00010000

#define GIC_PRI_LEVEL           32

#ifndef GIC_TNUM_INTNO
#define GIC_TNUM_INTNO		UINT_C(160)
#endif /* GIC_TNUM_INTNO */

/*
 *  Generic timer
 */
/* Interrupt Number */
#define GIC_IRQNO_HYPTIMER      26
#define GIC_IRQNO_VIRTIMER      27
#define GIC_IRQNO_SPHYTIMER     29
#define GIC_IRQNO_NSPHYTIMER    30

/*
 *  UART
 */
/* Base address */
#define UART1_BASE              0x30860000
#define UART2_BASE              0x30890000
#define UART3_BASE              0x30880000
#define UART4_BASE              0x30A60000

/* Interrupt Number */
#define IRQ_UART1               (26 + 32)
#define IRQ_UART2               (27 + 32)
#define IRQ_UART3               (28 + 32)
#define IRQ_UART4               (29 + 32)

/*
 *  IOMUXC
 */
#define IOMUXC_BASE 0x30330000

#define IOMUXC_PAD_CTL_PKE      (1 << 12)
#define IOMUXC_PAD_CTL_PUE      (1 << 13 | IOMUXC_PAD_CTL_PKE)
#define IOMUXC_PAD_CTL_PUS_100K_UP  (2 << 14 | IOMUXC_PAD_CTL_PUE)
#define IOMUXC_PAD_CTL_SPEED_MED    (2 << 6)
#define IOMUXC_PAD_CTL_DSE_40ohm    (6 << 3)
#define IOMUXC_PAD_CTL_SRE_FAST (1 << 0)
#define IOMUXC_PAD_CTL_HYS      (1 << 16)

#define IOMUXC_MUX_MODE_ALT0				0x00000000
#define IOMUXC_MUX_MODE_ALT1				0x00000001
#define IOMUXC_MUX_MODE_ALT2				0x00000002
#define IOMUXC_MUX_MODE_ALT3				0x00000003
#define IOMUXC_MUX_MODE_ALT4				0x00000004
#define IOMUXC_MUX_MODE_ALT5				0x00000005
#define IOMUXC_MUX_MODE_ALT6				0x00000006
#define IOMUXC_MUX_MODE_ALT7				0x00000007

#define IOMUXC_SW_MUX_CTL_PAD_ECSPI1_SCLK	(IOMUXC_BASE + 0x01F4)
#define IOMUXC_SW_MUX_CTL_PAD_ECSPI1_MOSI	(IOMUXC_BASE + 0x01F8)
#define IOMUXC_SW_MUX_CTL_PAD_UART3_RXD		(IOMUXC_BASE + 0x0244)
#define IOMUXC_SW_MUX_CTL_PAD_UART3_TXD		(IOMUXC_BASE + 0x0248)
#define IOMUXC_SW_PAD_CTL_PAD_ECSPI1_SCLK	(IOMUXC_BASE + 0x045C)
#define IOMUXC_SW_PAD_CTL_PAD_ECSPI1_MOSI	(IOMUXC_BASE + 0x0460)

#define IOMUXC_SW_MUX_CTL_PAD_UART4_RXD		(IOMUXC_BASE + 0x024C)
#define IOMUXC_SW_MUX_CTL_PAD_UART4_TXD		(IOMUXC_BASE + 0x0250)
#define IOMUXC_UART4_RXD_SELECT_INPUT		(IOMUXC_BASE + 0x050C)

/*
 *  System Reset Controller (SRC)
 */
#define SRC_BASE        0x30390000

#define IMX8REG_SRC_SCR     (SRC_BASE + 0x0000)
#define IMX8REG_SRC_A53RCR0 (SRC_BASE + 0x0004)
#define IMX8REG_SRC_A53RCR1 (SRC_BASE + 0x0008)
#define IMX8REG_SRC_M4RCR   (SRC_BASE + 0x000C)
#define IMX8REG_SRC_SRSR    (SRC_BASE + 0x005C)
#define IMX8REG_SRC_SISR    (SRC_BASE + 0x0068)
#define IMX8REG_SRC_SIMR    (SRC_BASE + 0x006C)
#define IMX8REG_SRC_GPR1    (SRC_BASE + 0x0074)
#define IMX8REG_SRC_GPR2    (SRC_BASE + 0x0078)
#define IMX8REG_SRC_GPR3    (SRC_BASE + 0x007C)
#define IMX8REG_SRC_GPR4    (SRC_BASE + 0x0080)
#define IMX8REG_SRC_GPR5    (SRC_BASE + 0x0084)
#define IMX8REG_SRC_GPR6    (SRC_BASE + 0x0088)
#define IMX8REG_SRC_GPR7    (SRC_BASE + 0x008C)
#define IMX8REG_SRC_GPR8    (SRC_BASE + 0x0090)

/*
 *  General Power Controller (GPC)
 */
#define GPC_BASE        0x303A0000
#define IMX8REG_GPC_CPU_PGC_SW_PUP_REQ	(GPC_BASE + 0xF0)

/*
 *  GPC Power Gating Controller (GPC_PGC)
 */
#define GPC_PGC_BASE        0x303A0800
#define IMX8REG_GPC_PGC_nCTRL     (GPC_PGC_BASE + 0x0000)

/*
 *  Resouce Domain Controller（RDC）
 */
#define RDC_PDAP70   0x303D0518   /* for UART4 */

#endif /* TOPPERS_IMX8M_A53_H */
