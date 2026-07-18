/*
 *  TOPPERS/FMP Kernel
 *      Toyohashi Open Platform for Embedded Real-Time Systems/
 *      Flexible MultiProcessor Kernel
 *
 *  Copyright (C) 2006-2025 by Embedded and Real-Time Systems Laboratory
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
 *  @(#) $Id: gic_kernel_impl.h 446 2025-06-27 07:54:07Z ertl-honda $
 */

/*
 *		カーネルの割込みコントローラ依存部（GIC用）
 *
 *  このヘッダファイルは，target_kernel_impl.h（または，そこからインク
 *  ルードされるファイル）のみからインクルードされる．他のファイルから
 *  直接インクルードしてはならない．
 */

#ifndef TOPPERS_GIC_KERNEL_IMPL_H
#define TOPPERS_GIC_KERNEL_IMPL_H

#include <sil.h>
#include "arm64.h"

/*
 *  割込み番号の数，最小値と最大値
 */
#define TNUM_INTNO		GIC_TNUM_INTNO
#define TMIN_INTNO		UINT_C(0)
#define TMAX_INTNO		(GIC_TNUM_INTNO - 1)

/*
 *  割込みハンドラ番号の数
 */
#define TNUM_INHNO		GIC_TNUM_INTNO

/*
 *  割込み番号の定義
 */
#define GIC_INTNO_SGI0		UINT_C(0)
#define GIC_INTNO_PPI0		UINT_C(16)
#define GIC_INTNO_SPI0		UINT_C(32)

#ifdef TOPPERS_TZ_S
#if GIC_PRI_LEVEL == 16
#define GIC_PRI_SHIFT		4
#define GIC_PRI_MASK		UINT_C(0x0f)
#elif GIC_PRI_LEVEL == 32
#define GIC_PRI_SHIFT		3
#define GIC_PRI_MASK		UINT_C(0x1f)
#elif GIC_PRI_LEVEL == 64
#define GIC_PRI_SHIFT		2
#define GIC_PRI_MASK		UINT_C(0x3f)
#elif GIC_PRI_LEVEL == 128
#define GIC_PRI_SHIFT		1
#define GIC_PRI_MASK		UINT_C(0x7f)
#elif GIC_PRI_LEVEL == 256
#define GIC_PRI_SHIFT		0
#define GIC_PRI_MASK		UINT_C(0xff)
#else
#error Invalid number of priority levels for GIC.
#endif /* GIC_PRI_LEVEL == 16 */
#else /* TOPPERS_TZ_S */
#if GIC_PRI_LEVEL == 16
#define GIC_PRI_SHIFT		5
#define GIC_PRI_MASK		UINT_C(0x0f)
#elif GIC_PRI_LEVEL == 32
#define GIC_PRI_SHIFT		4
#define GIC_PRI_MASK		UINT_C(0x1f)
#elif GIC_PRI_LEVEL == 64
#define GIC_PRI_SHIFT		3
#define GIC_PRI_MASK		UINT_C(0x3f)
#elif GIC_PRI_LEVEL == 128
#define GIC_PRI_SHIFT		2
#define GIC_PRI_MASK		UINT_C(0x7f)
#elif GIC_PRI_LEVEL == 256
#define GIC_PRI_SHIFT		1
#define GIC_PRI_MASK		UINT_C(0xff)
#else
#error Invalid number of priority levels for GIC.
#endif /* GIC_PRI_LEVEL == 16 */
#endif /* TOPPERS_TZ_S */

/* 外部表現への変換 */
#define EXT_IPM(pri) \
			(((PRI)((pri) >> GIC_PRI_SHIFT)) - ((GIC_PRI_LEVEL >> 1) - 1))

/* 内部表現への変換 */
#define INT_IPM(ipm) \
			(((uint_t)((ipm) + ((GIC_PRI_LEVEL >> 1) - 1))) << GIC_PRI_SHIFT)

/*
 *  GICレジスタのアドレスを定義するためのマクロ
 *
 *  GICレジスタのアドレスを，アセンブリ言語からも参照できるようにするた
 *  めのマクロ．
 */
#ifndef GIC_REG
#define GIC_REG(base, offset)	((uint32_t *)((base) + (offset ## U)))
#endif /* GIC_REG */

/*
 *  CPUインタフェース関連の定義
 */
#if TOPPERS_GIC_VER == 2
#define GICC_CTLR		GIC_REG(GICC_BASE, 0x00)
#define GICC_PMR		GIC_REG(GICC_BASE, 0x04)
#define GICC_BPR		GIC_REG(GICC_BASE, 0x08)
#define GICC_IAR		GIC_REG(GICC_BASE, 0x0C)
#define GICC_EOIR		GIC_REG(GICC_BASE, 0x10)
#define GICC_RPR		GIC_REG(GICC_BASE, 0x14)
#define GICC_HPPIR		GIC_REG(GICC_BASE, 0x18)
#elif (TOPPERS_GIC_VER == 3) || (TOPPERS_GIC_VER == 4)
#define ICC_CTLR_EL1_WRITE(x)		Asm("msr S3_0_C12_C12_4, %0"::"r"(x))
#define ICC_CTLR_EL1_READ(x)		Asm("mrs %0, S3_0_C12_C12_4":"=r"(x))
#define ICC_SRE_EL1_WRITE(x)		Asm("msr S3_0_C12_C12_5, %0"::"r"(x))
#define ICC_SRE_EL1_READ(x)			Asm("mrs %0, S3_0_C12_C12_5":"=r"(x))
#define ICC_SRE_EL2_WRITE(x)		Asm("msr S3_4_C12_C9_5, %0"::"r"(x))
#define ICC_SRE_EL2_READ(x)			Asm("mrs %0, S3_4_C12_C9_5":"=r"(x))
#define ICC_SRE_EL3_WRITE(x)		Asm("msr S3_6_C12_C12_5, %0"::"r"(x))
#define ICC_SRE_EL3_READ(x)			Asm("mrs %0, S3_6_C12_C12_5":"=r"(x))
#define ICC_IGRPEN0_EL1_WRITE(x)	Asm("msr S3_0_C12_C12_6, %0"::"r"(x))
#define ICC_IGRPEN0_EL1_READ(x)		Asm("mrs %0, S3_0_C12_C12_6":"=r"(x))
#define ICC_IGRPEN1_EL1_WRITE(x)	Asm("msr S3_0_C12_C12_7, %0"::"r"(x))
#define ICC_IGRPEN1_EL1_READ(x)		Asm("mrs %0, S3_0_C12_C12_7":"=r"(x))
#define ICC_PMR_EL1_WRITE(x)		Asm("msr S3_0_C4_C6_0, %0"::"r"(x))
#define ICC_PMR_EL1_READ(x)			Asm("mrs %0, S3_0_C4_C6_0":"=r"(x))
#define ICC_BPR0_EL1_WRITE(x)		Asm("msr S3_0_C12_C8_3, %0"::"r"(x))
#define ICC_BPR0_EL1_READ(x)		Asm("mrs %0, S3_0_C12_C8_3":"=r"(x))
#define ICC_BPR1_EL1_WRITE(x)		Asm("msr S3_0_C12_C12_3, %0"::"r"(x))
#define ICC_BPR1_EL1_READ(x)		Asm("mrs %0, S3_0_C12_C12_3":"=r"(x))
#define ICC_IAR0_EL1_WRITE(x)		Asm("msr S3_0_C12_C8_0, %0"::"r"(x))
#define ICC_IAR0_EL1_READ(x)		Asm("mrs %0, S3_0_C12_C8_0":"=r"(x))
#define ICC_IAR1_EL1_WRITE(x)		Asm("msr S3_0_C12_C12_0, %0"::"r"(x))
#define ICC_IAR1_EL1_READ(x)		Asm("mrs %0, S3_0_C12_C12_0":"=r"(x))
#define ICC_EOIR0_EL1_WRITE(x)		Asm("msr S3_0_C12_C8_1, %0"::"r"(x))
#define ICC_EOIR0_EL1_READ(x)		Asm("mrs %0, S3_0_C12_C8_1":"=r"(x))
#define ICC_EOIR1_EL1_WRITE(x)		Asm("msr S3_0_C12_C12_1, %0"::"r"(x))
#define ICC_EOIR1_EL1_READ(x)		Asm("mrs %0, S3_0_C12_C12_1":"=r"(x))
#define ICC_RPR_EL1_WRITE(x)		Asm("msr S3_0_C12_C11_3, %0"::"r"(x))
#define ICC_RPR_EL1_READ(x)			Asm("mrs %0, S3_0_C12_C11_3":"=r"(x))
#define ICC_HPPIR0_EL1_WRITE(x)		Asm("msr S3_0_C12_C8_2, %0"::"r"(x))
#define ICC_HPPIR0_EL1_READ(x)		Asm("mrs %0, S3_0_C12_C8_2":"=r"(x))
#define ICC_HPPIR1_EL1_WRITE(x)		Asm("msr S3_0_C12_C12_2, %0"::"r"(x))
#define ICC_HPPIR1_EL1_READ(x)		Asm("mrs %0, S3_0_C12_C12_2":"=r"(x))

/*
 *  アセンブリ言語用の定義
 */
#define ICC_RPR_EL1			S3_0_C12_C11_3
#define ICC_PMR_EL1			S3_0_C4_C6_0
#define ICC_IAR1_EL1		S3_0_C12_C12_0
#define ICC_EOIR1_EL1		S3_0_C12_C12_1
#endif /* TOPPERS_GIC_VER == 2 */

#define GICC_CTLR_ENABLE		0x01
#define GICC_CTLR_ENABLEGRP0	0x01
#define GICC_CTLR_ENABLEGRP1	0x02
#define GICC_CTLR_ACKCTL		0x04
#define GICC_CTLR_CBPR			0x10
#define GICC_CTLR_FIQEN			0x08
#define GICC_CTLR_IRQBYPDISGRP1		(1 << 8)
#define GICC_CTLR_FIQBYPDISGRP1		(1 << 7)
#define GICC_CTLR_IRQBYPDISGRP0		(1 << 6)
#define GICC_CTLR_FIQBYPDISGRP0		(1 << 5)

#define GICC_IAR_INTERRUPTID_MASK		0x03ff
#define GICC_IAR_CPUID_MASK				0x1c00

#if (TOPPERS_GIC_VER == 3) || (TOPPERS_GIC_VER == 4)
/*
 *  SGI
 */
#define ICC_SGI0R_EL1_WRITE(x)		Asm("msr S3_0_C12_C11_7, %0"::"r"(x))
#define ICC_SGI1R_EL1_WRITE(x)		Asm("msr S3_0_C12_C11_5, %0"::"r"(x))

#define BITPOS_ICC_SGInR_AFF3		48
#define BITPOS_ICC_SGInR_RS			44
#define BITPOS_ICC_SGInR_IRM		40
#define BITPOS_ICC_SGInR_AFF2		32
#define BITPOS_ICC_SGInR_INTID		24
#define BITPOS_ICC_SGInR_AFF1		16
#define BITPOS_ICC_SGInR_TLIST		0

/*
 *  Redistributor
 *
 *  各PEの Redistributor フレーム先頭は GICR_BASE + GICR_SIZE * PEインデックス の
 *  固定ストライドで一意に決まる（gicr_init() の初期化式と同一）．以前は計算結果を
 *  共有可変配列 gicr_base[] に保存していたが，キャッシュONで書く同配列が，キャッシュ
 *  OFFで書く per-PE ページテーブル(arm64.c の tt_pri)と同一キャッシュラインに同居すると
 *  false sharing でテーブルを破壊する不具合があった（doc/arm64_design.txt 参照）．
 *  配列を持たず毎回計算することで，共有書込み先そのものを無くし不具合を根絶する．
 */
#define GICR_BASE_ADDR(x)	(GICR_BASE + GICR_SIZE * (uintptr_t)(x))
#define GICR_CTLR(x)		(GICR_BASE_ADDR(x) + GICR_RD_OFFSET + 0x0000)
#define GICR_IIDR(x)		(GICR_BASE_ADDR(x) + GICR_RD_OFFSET + 0x0004)
#define GICR_TYPER(x)		(GICR_BASE_ADDR(x) + GICR_RD_OFFSET + 0x0008)
#define GICR_STATUSR(x)		(GICR_BASE_ADDR(x) + GICR_RD_OFFSET + 0x0010)
#define GICR_WAKER(x)		(GICR_BASE_ADDR(x) + GICR_RD_OFFSET + 0x0014)
 #define GICR_ChildrenAsleep	(uint32_t)(1 << 2)
 #define GICR_ProcSleep			(uint32_t)(1 << 1)

#define GICR_SETLPIR(x)		(GICR_BASE_ADDR(x) + GICR_RD_OFFSET + 0x0040)
#define GICR_CLRLPIR(x)		(GICR_BASE_ADDR(x) + GICR_RD_OFFSET + 0x0048)
#define GICR_PROPBASER(x)	(GICR_BASE_ADDR(x) + GICR_RD_OFFSET + 0x0070)
#define GICR_PENDBASER(x)	(GICR_BASE_ADDR(x) + GICR_RD_OFFSET + 0x0078)
#define GICR_INVLPIR(x)		(GICR_BASE_ADDR(x) + GICR_RD_OFFSET + 0x00A0)
#define GICR_INVALLR(x)		(GICR_BASE_ADDR(x) + GICR_RD_OFFSET + 0x00B0)
#define GICR_SYNCR(x)		(GICR_BASE_ADDR(x) + GICR_RD_OFFSET + 0x00C0)

#define GICR_IGROUPR0(x)	(GICR_BASE_ADDR(x) + GICR_SGI_OFFSET + 0x0080)
#define GICR_ISENABLER0(x)	(GICR_BASE_ADDR(x) + GICR_SGI_OFFSET + 0x0100)
#define GICR_ICENABLER0(x)	(GICR_BASE_ADDR(x) + GICR_SGI_OFFSET + 0x0180)
#define GICR_ISPENDR0(x)	(GICR_BASE_ADDR(x) + GICR_SGI_OFFSET + 0x0200)
#define GICR_ICPENDR0(x)	(GICR_BASE_ADDR(x) + GICR_SGI_OFFSET + 0x0280)
#define GICR_ISACTIVER0(x)	(GICR_BASE_ADDR(x) + GICR_SGI_OFFSET + 0x0300)
#define GICR_ICACTIVER0(x)	(GICR_BASE_ADDR(x) + GICR_SGI_OFFSET + 0x0380)
#define GICR_IPRIORITYRn(x)	(GICR_BASE_ADDR(x) + GICR_SGI_OFFSET + 0x0400)
#define GICR_ICFGR0(x)		(GICR_BASE_ADDR(x) + GICR_SGI_OFFSET + 0x0C00)
#define GICR_ICFGR1(x)		(GICR_BASE_ADDR(x) + GICR_SGI_OFFSET + 0x0C04)
#define GICR_IGRPMODR0(x)	(GICR_BASE_ADDR(x) + GICR_SGI_OFFSET + 0x0D00)
#define GICR_NSACR(x)		(GICR_BASE_ADDR(x) + GICR_SGI_OFFSET + 0x0E00)

#endif /* (TOPPERS_GIC_VER == 3) || (TOPPERS_GIC_VER == 4) */

/*
 *  ディストリビュータ関連の定義
 */
#define GICD_CTLR			GIC_REG(GICD_BASE, 0x000)
#define GICD_TYPER			GIC_REG(GICD_BASE, 0x004)
#define GICD_IIDR			GIC_REG(GICD_BASE, 0x008)
#define GICD_IGROUPR(n)		GIC_REG(GICD_BASE, 0x080 + (n) * 4)
#define GICD_ISENABLER(n)	GIC_REG(GICD_BASE, 0x100 + (n) * 4)
#define GICD_SETSPI_NSR		(GICD_BASE + 0x0040)		/* Set SPI Register */
#define GICD_CLRSPI_NSR		(GICD_BASE + 0x0048)		/* Clear SPI Register */
#define GICD_SETSPI_SR		(GICD_BASE + 0x0050)		/* Set SPI, Secure Register */
#define GICD_CLRSPI_SR		(GICD_BASE + 0x0058)		/* Clear SPI, Secure Register */
#define GICD_IGROUPRn		(GICD_BASE + 0x0080)		/* 割込みセキュリティ */
#define GICD_ISENABLERn		(GICD_BASE + 0x0100)		/* 割込みイネーブルセット   */
#define GICD_ICENABLERn		(GICD_BASE + 0x0180)		/* 割込みイネーブルクリアー */
#define GICD_ISPENDRn		(GICD_BASE + 0x0200)		/* 割込みセットペンディング */
#define GICD_ISPENDR(n)		(GICD_BASE + 0x200 + (n) * 4)
#define GICD_ICPENDRn		(GICD_BASE + 0x0280)		/* 割込みクリアーペンディング */
#define GICD_ISACTIVERn		(GICD_BASE + 0x0300)		/* 割込みセットアクティブレジスター */
#define GICD_ICACTIVERn		(GICD_BASE + 0x0380)		/* 割込みクリアーアクティブレジスター */
#define GICD_IPRIORITYRn	(GICD_BASE + 0x0400)		/* 割込み優先度レジスタ */
#define GICD_IPRIORITYR(n)	(GICD_BASE + 0x0400 + (n) * 4)
#define GICD_ITARGETSRn		(GICD_BASE + 0x0800)		/* 割込みターゲットレジスタ/CA9はSPIターゲットレジスタ */
#define GICD_ITARGETSR(n)	(GICD_BASE + 0x0800 + (n) * 4)
#define GICD_ICFGRn			(GICD_BASE + 0x0C00)		/* 割込みコンフィギュレーションレジスタ */
#define GICD_IGRPMODRn		(GICD_BASE + 0x0D00)		/* Interrupt Group Modifier Registers */
#define GICD_IGRPMODR(n)	(GICD_BASE + 0x0D00 + (n) * 4)
#define GICD_SGIR			(GICD_BASE + 0x0F00)		/* ソフトウェア割込みレジスタ  */
#define GICD_CPENDSGIRn		(GICD_BASE + 0x0F10)		/* SGI Clear-Pending Registers */
#define GICD_SPENDSGIRn		(GICD_BASE + 0x0F20)		/* SGI Set-Pending Registers */
#define GICD_IROUTERn		(GICD_BASE + 0x6000)		/* Interrupt Routing Registers */

#define GICD_PPIS		(GICD_BASE + 0x0D00)		/* PPIステータス */
#define GICD_SPIS		(GICD_BASE + 0x0D04)		/* SPIステータス */

#define GICD_CTLR_ENABLE		0x01
#define GICD_CTLR_ENABLEGRP0	0x01
#define GICD_CTLR_ENABLEGRP1	0x02
#define GICD_CTLR_ENABLEGRP1NS	(1U << 1)
#define GICD_CTLR_ENABLEGRP1S	(1U << 2)
#define GICD_CTLR_ARE_S			(1U << 4)
#ifdef TOPPERS_TZ_S
#define GICD_CTLR_ARE_NS		(1U << 5)
#else  /* !TOPPERS_TZ_S */
#define GICD_CTLR_ARE_NS		(1U << 4)
#endif /* TOPPERS_TZ_S */

#define GICD_CTLR_ARE_MASK		0x00000030
#define GICD_CTLR_RWP			(1U << 31)	/* Register Write Pending */

/*
 *  割込み先のプロセッサの指定
 */
#define GICD_ITARGETSRn_CPU0		0x01
#define GICD_ITARGETSRn_CPU1		0x02
#define GICD_ITARGETSRn_CPU2		0x04
#define GICD_ITARGETSRn_CPU3		0x08

#define GICD_SGIR_CPU0		0x01
#define GICD_SGIR_CPU1		0x02
#define GICD_SGIR_CPU2		0x04
#define GICD_SGIR_CPU3		0x08
#define GICD_SGIR_CPUS		0x0f

#define GICD_SGIR_CPU_OFFSET		16

/*
 *  コンフィギュレーションレジスタの設定値
 */
#define GICD_ICFGRn_EDGE		0x03	/* エッジ割込み */
#define GICD_ICFGRn_LEVEL		0x00	/* レベル割込み */
#define GICD_ICFGRn_N_N			0x00	/* N-Nモデル    */
#define GICD_ICFGRn_1_N			0x01	/* 1-Nモデル    */

/*
 *  GICでサポートしている割込み数
 */
#define GIC_TMIN_INTNO		0U

#ifndef GIC_TMAX_INTNO
#define GIC_TMAX_INTNO		255U
#endif /* GIC_TMAX_INTNO */

#ifndef GIC_TNUM_INT
#define GIC_TNUM_INT		256U
#endif /* GIC_TNUM_INT */

/*
 *  グローバル割込みの開始番号
 */
#define TMIN_GLOBAL_INTNO	32U

#ifndef TOPPERS_MACRO_ONLY

/*
 *  GIC CPU Interface 関連のドライバ
 */
#if TOPPERS_GIC_VER == 2
/*
 *  割込み優先度マスクを設定（priは内部表現）（GICv2）
 */ 
Inline void
gicc_set_priority(uint_t pri)
{
	sil_swrw_mem(GICC_PMR, pri);
}

/*
 *  割込み優先度マスクを取得（内部表現で返す）（GICv2）
 */ 
Inline uint_t
gicc_get_priority(void)
{
	return(sil_rew_mem(GICC_PMR));
}

/*
 *  GICのプロセッサの割込み優先度のどのビットを使用するか（GICv2）
 * 
 *  この関数は，カーネルの初期化中に呼び出すことを想定しているため，
 *  GICの操作後にメモリ同期バリアを入れていない．
 */
Inline void
gicc_set_bp(int mask_bit)
{
	sil_wrw_mem((void *)GICC_BPR, mask_bit);
}

/*
 *  ソフトウェア割込みを発行（GICv2）
 * 
 *  ディストリビュータのレジスタへの1回の書き込みのみであるため，プロ
 *  セッサ間排他制御は必要ない．
 */
Inline void
gicd_raise_sgi(INTNO intno, ID prcid)
{    
	data_sync_barrier();
	sil_swrw_mem((void *)GICD_SGIR, (conv_prcid_to_gicdtarget(prcid) << GICD_SGIR_CPU_OFFSET) | intno);
}

#elif (TOPPERS_GIC_VER == 3) || (TOPPERS_GIC_VER == 4)

/*
 *  CPUの割込み優先度マスクを設定（GICv3/4）
 */ 
Inline void
gicc_set_priority(int pri)
{
	ICC_PMR_EL1_WRITE(pri);
	inst_sync_barrier();
}

/*
 *  CPUの割込み優先度マスクを取得（GICv3/4）
 */ 
Inline int
gicc_get_priority(void)
{
	volatile uint32_t	reg32_val;

	ICC_PMR_EL1_READ(reg32_val);
	return (reg32_val);
}

/*
 *  GICのプロセッサの割込み優先度のどのビットを使用するか（GICv3/4）
 *
 *  この関数は，カーネルの初期化中に呼び出すことを想定しているため，
 *  GICの操作後に命令同期バリアを入れていない．  
 */
Inline void
gicc_set_bp(int mask_bit)
{
#ifdef TOPPERS_TZ_S
	ICC_BPR0_EL1_WRITE(mask_bit);
#else  /* !TOPPERS_TZ_S */
	ICC_BPR1_EL1_WRITE(mask_bit);
#endif /* TOPPERS_TZ_S */
}

/*
 *  ソフトウェア割込みを発行（GICv3/4）
 * 
 *  ディストリビュータのレジスタへの1回の書き込みのみであるため，プロ
 *  セッサ間排他制御は必要ない．  
 */
Inline void
gicd_raise_sgi(INTNO intno, ID prcid)
{
	volatile uint64_t	reg64_val;
	uint64_t	mpidr;
	uint64_t	aff1;
	uint64_t	aff0;
    
	data_sync_barrier();

	mpidr = conv_prcid_to_mpidr(prcid);
	aff1 = (mpidr & MPIDR_AFF1_MASK) >> MPIDR_AFF1_SHIFT;
	aff0 = (mpidr & MPIDR_AFF0_MASK) >> MPIDR_AFF0_SHIFT;    
	reg64_val = ((uint64_t)intno  << BITPOS_ICC_SGInR_INTID) |
					(aff1 << BITPOS_ICC_SGInR_AFF1) | (1 << aff0);

	ICC_SGI1R_EL1_WRITE(reg64_val);
	inst_sync_barrier();
}

#endif /* TOPPERS_GIC_VER == 2 */

/*
 *  GICの初期化
 */
extern void gic_init(void);

/*
  *  GIC CPU Interface の終了
 */
extern void gicc_stop(void);

/*
 *  Distributor 関連のドライバ
 */

/*
 *  割込み禁止
 */
extern void gicd_disable_int(uint8_t id);

/*
 *  割込み許可
 */
extern void gicd_enable_int(uint8_t id);

/*
 *  割込みペンディングクリア
 */
extern void gicd_clear_pending(uint8_t id);

/*
 *  割込みペンディングセット
 */
extern void gicd_set_pending(uint8_t id);

/*
 *  割込み要求のチェック
 */
extern bool_t gicd_probe_pending(uint8_t id);

/*
 *  割込み設定のセット
 */
extern void gicd_config(uint8_t id, bool_t is_edge, bool_t is_1_n);

/*
 *  割込み優先度のセット
 *  内部表現で渡す． 
 */
extern void gicd_set_priority(INTNO intno, uint_t pri);

/*
 *  割込みターゲットの設定
 *  CPUはORで指定  
 */
extern void gicd_set_target(uint8_t id, ID iprcid, uint8_t cpus);

/*
 *  ディストリビュータの初期化
 */
extern void gicd_initialize(void);

/*
 *  ディストリビュータの終了
 */
extern void gicd_terminate(void);

#endif /* TOPPERS_MACRO_ONLY */

/*
 *  割込み番号の範囲の判定
 */
#define VALID_INTNO(prcid, intno) \
			(((TMIN_INTNO <= INTNO_MASK(intno))					\
					&& (INTNO_MASK(intno) < GIC_INTNO_SPI0)		\
					&& (INTNO_PRCID(intno) == prcid))			\
			|| ((GIC_INTNO_SPI0 <= INTNO_MASK(intno))			\
					&& (INTNO_MASK(intno) <= TMAX_INTNO)		\
					&& (INTNO_PRCID(intno) == 0U)))

/*
 *  割込み要求ラインのための標準的な初期化情報を生成する
 */
#define USE_INTINIB_TABLE

/*
 *  割込み要求ライン設定テーブルを生成する
 */
#define USE_INTCFG_TABLE

/*
 *  コア依存部
 */
#include "core_kernel_impl.h"

/*
 *  ターゲット非依存部に提供する関数
 */
#ifndef TOPPERS_MACRO_ONLY

/*
 *  割込み属性の設定のチェック
 */
Inline bool_t
check_intno_cfg(INTNO intno)
{
	uint_t	prcidx = get_my_prcidx();

	return((p_intcfg_table[prcidx])[INTNO_MASK(intno)] != 0U);
}

/*
 *  割込み優先度マスクの設定
 */
Inline void
t_set_ipm(PRI intpri)
{
	gicc_set_priority(INT_IPM(intpri));
}

/*
 *  割込み優先度マスクの参照
 */
Inline PRI
t_get_ipm(void)
{
	return(EXT_IPM(gicc_get_priority()));
}

/*
 *  割込み要求禁止フラグが操作できる割込み番号の範囲の判定
 */
#ifdef GIC_SUPPORT_DISABLE_SGI
#define VALID_INTNO_DISINT(prcid, intno)	VALID_INTNO(prcid, intno)
#else /* GIC_SUPPORT_DISABLE_SGI */
#define VALID_INTNO_DISINT(prcid, intno) \
			(((GIC_INTNO_PPI0 <= INTNO_MASK(intno))				\
					&& (INTNO_MASK(intno) < GIC_INTNO_SPI0)		\
					&& (INTNO_PRCID(intno) == prcid))			\
			|| ((GIC_INTNO_SPI0 <= INTNO_MASK(intno))			\
					&& (INTNO_MASK(intno) <= TMAX_INTNO)		\
					&& (INTNO_PRCID(intno) == 0U)))
#endif /* GIC_SUPPORT_DISABLE_SGI */

/*
 *  割込み要求禁止フラグのセット
 */
Inline void
disable_int(INTNO intno)
{
	gicd_disable_int(INTNO_MASK(intno));
}

/* 
 *  割込み要求禁止フラグのクリア
 */
Inline void
enable_int(INTNO intno)
{
	gicd_enable_int(INTNO_MASK(intno));
}

/*
 *  割込み要求がクリアできる割込み番号の範囲の判定
 */
#define VALID_INTNO_CLRINT(prcid, intno) \
			(((GIC_INTNO_PPI0 <= INTNO_MASK(intno))				\
					&& (INTNO_MASK(intno) < GIC_INTNO_SPI0)		\
					&& (INTNO_PRCID(intno) == prcid))			\
			|| ((GIC_INTNO_SPI0 <= INTNO_MASK(intno))			\
					&& (INTNO_MASK(intno) <= TMAX_INTNO)		\
					&& (INTNO_PRCID(intno) == 0U)))

/*
 *  割込み要求がクリアできる状態か？
 */
Inline bool_t
check_intno_clear(INTNO intno)
{
	return(true);
}

/*
 *  割込み要求のクリア
 */
Inline void
clear_int(INTNO intno)
{
	gicd_clear_pending(INTNO_MASK(intno));
}

/*
 *  割込みが要求できる割込み番号の範囲の判定
 */
#define VALID_INTNO_RASINT(prcid, intno) \
			(((TMIN_INTNO <= INTNO_MASK(intno))					\
					&& (INTNO_MASK(intno) < GIC_INTNO_PPI0)		\
					&& (TMIN_PRCID <= INTNO_PRCID(intno))		\
					&& (INTNO_PRCID(intno) <= TMAX_PRCID))		\
			|| ((GIC_INTNO_PPI0 <= INTNO_MASK(intno))			\
					&& (INTNO_MASK(intno) < GIC_INTNO_SPI0)		\
					&& (INTNO_PRCID(intno) == prcid))			\
			|| ((GIC_INTNO_SPI0 <= INTNO_MASK(intno))			\
					&& (INTNO_MASK(intno) < TMAX_INTNO)			\
					&& (INTNO_PRCID(intno) == 0U)))

/*
 *  割込みが要求できる状態か？
 */
Inline bool_t
check_intno_raise(INTNO intno)
{
	return(true);
}

/*
 *  割込みの要求
 */
Inline void
raise_int(INTNO intno)
{
	if (INTNO_MASK(intno) < GIC_INTNO_PPI0) {
		gicd_raise_sgi(INTNO_MASK(intno), INTNO_PRCID(intno));
	}
	else {
		gicd_set_pending(INTNO_MASK(intno));
	}
}

/*
 *  割込み要求のチェック
 */
Inline bool_t
probe_int(INTNO intno)
{
	return(gicd_probe_pending(INTNO_MASK(intno)));
}

#endif /* TOPPERS_MACRO_ONLY */
#endif /* TOPPERS_GIC_KERNEL_IMPL_H */
