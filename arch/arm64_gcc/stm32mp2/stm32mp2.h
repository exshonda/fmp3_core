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
 *		STM32MP25xx(Cortex-A35)のハードウェア資源の定義
 *
 *  対象は STMicroelectronics STM32MP257F (Cortex-A35 x2 / GICv2(GIC-400)).
 *  ベースアドレス等は stm32mp2-baremetal の CMSIS デバイスヘッダおよび
 *  TF-A のデバイスツリー (stm32mp251.dtsi) に基づく．
 */
#ifndef TOPPERS_STM32MP2_H
#define TOPPERS_STM32MP2_H

/*
 *  GIC (GIC-400 / GICv2 互換, dtsi: "arm,cortex-a7-gic")
 *    intc@4ac00000 : GICD=0x4ac10000, GICC=0x4ac20000,
 *                    GICH=0x4ac40000, GICV=0x4ac60000
 */
#define TOPPERS_GIC_VER         2
#define GIC_BASE                0x4AC00000
#define GICD_BASE               0x4AC10000
#define GICC_BASE               0x4AC20000

#define GIC_PRI_LEVEL           32

#ifndef GIC_TNUM_INTNO
#define GIC_TNUM_INTNO		UINT_C(416)		/* STM32MP2: 13*32 = 416 lines */
#endif /* GIC_TNUM_INTNO */

/*
 *  Generic timer (PPI: dtsi より sec-phys=PPI13->INTID29,
 *                         non-sec-phys=PPI14->INTID30,
 *                         virt=PPI11->INTID27, hyp=PPI10->INTID26)
 */
#define GIC_IRQNO_HYPTIMER      26
#define GIC_IRQNO_VIRTIMER      27
#define GIC_IRQNO_SPHYTIMER     29
#define GIC_IRQNO_NSPHYTIMER    30

/*
 *  RCC (Reset and Clock Control)
 */
#define RCC_BASE                0x44200000

/*
 *  サブコア(Core1 = a35_1)起動用レジスタ
 *
 *  STM32MP2 には BL31/PSCI ランタイムが無いため，セカンダリコアは
 *  CA35SYSCFG の 64bit リセットベクタ(VBAR_CR)を設定し，RCC のコアリセットを
 *  解除して直接起動する(stm32mp2-baremetal/multicore_smp の start_cpu1 方式)．
 */
/* CA35SYSCFG 64-bit mode reset vector register (AHB5 + 0x602000 + 0x84) */
#define CA35SYSCFG_VBAR_CR      0x48802084
/* RCC CPU1 Processor1 Reset Control Set Register (RCC_BASE + 0x408) */
#define RCC_C1P1RSTCSETR        (RCC_BASE + 0x408)
#define RCC_C1P1RSTCSETR_C1P1RST    (1U << 1)	/* CPU1 processor1 reset */

/*
 *  UART (STM32 USART)
 *    USART2 : ST-LINK 仮想COMポート (FSBL コンソール, 既に 115200 8N1 設定済み)
 *    USART6 : GPIO ヘッダ
 */
/* Base address */
#define USART2_BASE             0x400E0000
#define USART6_BASE             0x40220000

/* Interrupt Number (GIC INTID = SPI番号 + 32) */
#define IRQ_USART2              147
#define IRQ_USART6              168

#endif /* TOPPERS_STM32MP2_H */
