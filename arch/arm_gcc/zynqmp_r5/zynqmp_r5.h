/*
 *  TOPPERS/FMP Kernel
 *      Toyohashi Open Platform for Embedded Real-Time Systems/
 *      Flexible MultiProcessor Kernel
 *
 *  Copyright (C) 2026 by Embedded and Real-Time Systems Laboratory
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
 *  $Id$
 */

/*
 *		ZynqMP RPU（Cortex-R5）のハードウェア資源の定義
 */
#ifndef TOPPERS_ZYNQMP_R5_H
#define TOPPERS_ZYNQMP_R5_H

/*
 *  GIC依存部を使用するための定義（RPU用GIC，PL390/GICv1相当）
 */
#define GICC_BASE			UINT_C(0xF9001000)
#define GICD_BASE			UINT_C(0xF9000000)

#ifndef GIC_TNUM_INTNO
#define GIC_TNUM_INTNO		UINT_C(187)		/* SGI 16 + PPI 16 + SPI 155 */
#endif /* GIC_TNUM_INTNO */

/*
 *  UARTのベースアドレスと割込み番号（Cadence UART）
 */
#define ZYNQMP_UART0_BASE	UINT_C(0xFF000000)
#define ZYNQMP_UART1_BASE	UINT_C(0xFF010000)

#define ZYNQMP_UART0_IRQ	UINT_C(21 + 32)		/* 53 */
#define ZYNQMP_UART1_IRQ	UINT_C(22 + 32)		/* 54 */

/*
 *  TTC（Triple Timer Counter）のベースアドレスと割込み番号
 *
 *  TTC3のカウンタ0を高分解能タイマに使用する．
 */
#define ZYNQMP_TTC3_BASE	UINT_C(0xFF140000)

#define ZYNQMP_TTC3_0_IRQ	UINT_C(45 + 32)		/* 77 */
#define ZYNQMP_TTC3_1_IRQ	UINT_C(46 + 32)		/* 78 */
#define ZYNQMP_TTC3_2_IRQ	UINT_C(47 + 32)		/* 79 */

/*
 *  RPU制御レジスタ（lockstep/split設定等）
 */
#define RPU_BASEADDR					UINT_C(0xFF9A0000)

#define RPU_RPU_GLBL_CNTL				(RPU_BASEADDR + 0x00000000U)
#define RPU_RPU_GLBL_CNTL_SLSPLIT_MASK	0x00000008U
#define RPU_RPU_GLBL_CNTL_SLCLAMP_MASK	0x00000010U
#define RPU_RPU_GLBL_CNTL_TCM_COMB_MASK	0x00000040U

#define RPU_RPU_ERR_INJ					(RPU_BASEADDR + 0x00000020U)
#define RPU_RPU_ERR_INJ_FAULTLOGENABLE	0x00000101U

/*
 *  CRL_APB（低消費電力ドメインのクロック・リセット制御）
 */
#define CRL_APB_BASEADDR				UINT_C(0xFF5E0000)

#define CRL_APB_RST_LPD_IOU2			(CRL_APB_BASEADDR + 0x00000238U)
#define CRL_APB_RST_LPD_IOU2_TTC3_RESET_MASK	0x00004000U

#endif /* TOPPERS_ZYNQMP_R5_H */
