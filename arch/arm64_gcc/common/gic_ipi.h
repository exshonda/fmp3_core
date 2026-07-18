/*
 *  TOPPERS/FMP Kernel
 *      Toyohashi Open Platform for Embedded Real-Time Systems/
 *      Flexible MultiProcessor Kernel
 *
 *  Copyright (C) 2009-2025 by Embedded and Real-Time Systems Laboratory
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
 *  @(#) $Id: gic_ipi.h 446 2025-06-27 07:54:07Z ertl-honda $
 */

/*
 *  プロセッサ間割込みに関する定義（GIC用）
 */

#ifndef TOPPERS_GIC_IPI_H
#define TOPPERS_GIC_IPI_H

/*
 *  プロセッサ間割込みに使用する割込みの番号
 */
#ifdef TOPPERS_TZ_S
#define IPINO_DISPATCH			UINT_C(12)		/* ディスパッチ要求 */
#define IPINO_EXT_KER			UINT_C(13)		/* カーネル終了要求 */
#define IPINO_SET_HRT_EVT		UINT_C(14)		/* 高分解能タイマ設定要求 */
#define IPINO_START_SCYC		UINT_C(15)		/* システム周期開始要求 */
#else /* TOPPERS_TZ_S */
#define IPINO_DISPATCH			UINT_C(0)		/* ディスパッチ要求 */
#define IPINO_EXT_KER			UINT_C(1)		/* カーネル終了要求 */
#define IPINO_SET_HRT_EVT		UINT_C(2)		/* 高分解能タイマ設定要求 */
#define IPINO_START_SCYC		UINT_C(3)		/* システム周期開始要求 */
#endif /* TOPPERS_TZ_S */

/*
 *  ディスパッチ要求プロセッサ間割込みの割込み番号
 */
#define INTNO_IPI_DISPATCH_PRC1		(0x00010000 | IPINO_DISPATCH)
#define INTNO_IPI_DISPATCH_PRC2		(0x00020000 | IPINO_DISPATCH)
#define INTNO_IPI_DISPATCH_PRC3		(0x00030000 | IPINO_DISPATCH)
#define INTNO_IPI_DISPATCH_PRC4		(0x00040000 | IPINO_DISPATCH)
#define INTNO_IPI_DISPATCH_PRC5		(0x00050000 | IPINO_DISPATCH)
#define INTNO_IPI_DISPATCH_PRC6		(0x00060000 | IPINO_DISPATCH)

/*
 *  カーネル終了要求プロセッサ間割込みの割込み番号
 */
#define INTNO_IPI_EXT_KER_PRC1		(0x00010000 | IPINO_EXT_KER)
#define INTNO_IPI_EXT_KER_PRC2		(0x00020000 | IPINO_EXT_KER)
#define INTNO_IPI_EXT_KER_PRC3		(0x00030000 | IPINO_EXT_KER)
#define INTNO_IPI_EXT_KER_PRC4		(0x00040000 | IPINO_EXT_KER)
#define INTNO_IPI_EXT_KER_PRC5		(0x00050000 | IPINO_EXT_KER)
#define INTNO_IPI_EXT_KER_PRC6		(0x00060000 | IPINO_EXT_KER)

/*
 *  高分解能タイマ設定要求プロセッサ間割込みの割込み番号
 */
#define INTNO_IPI_SET_HRT_EVT_PRC1	(0x00010000 | IPINO_SET_HRT_EVT)
#define INTNO_IPI_SET_HRT_EVT_PRC2	(0x00020000 | IPINO_SET_HRT_EVT)
#define INTNO_IPI_SET_HRT_EVT_PRC3	(0x00030000 | IPINO_SET_HRT_EVT)
#define INTNO_IPI_SET_HRT_EVT_PRC4	(0x00040000 | IPINO_SET_HRT_EVT)
#define INTNO_IPI_SET_HRT_EVT_PRC5	(0x00050000 | IPINO_SET_HRT_EVT)
#define INTNO_IPI_SET_HRT_EVT_PRC6	(0x00060000 | IPINO_SET_HRT_EVT)

/*
 *  システム周期開始要求プロセッサ間割込みの割込み番号
 */
#define INTNO_IPI_START_SCYC_PRC1	(0x00010000 | IPINO_START_SCYC)
#define INTNO_IPI_START_SCYC_PRC2	(0x00020000 | IPINO_START_SCYC)
#define INTNO_IPI_START_SCYC_PRC3	(0x00030000 | IPINO_START_SCYC)
#define INTNO_IPI_START_SCYC_PRC4	(0x00040000 | IPINO_START_SCYC)
#define INTNO_IPI_START_SCYC_PRC5	(0x00050000 | IPINO_START_SCYC)
#define INTNO_IPI_START_SCYC_PRC6	(0x00060000 | IPINO_START_SCYC)

/*
 *  ディスパッチ要求プロセッサ間割込みの割込みハンドラ番号
 */
#define INHNO_IPI_DISPATCH_PRC1		(0x00010000 | IPINO_DISPATCH)
#define INHNO_IPI_DISPATCH_PRC2		(0x00020000 | IPINO_DISPATCH)
#define INHNO_IPI_DISPATCH_PRC3		(0x00030000 | IPINO_DISPATCH)
#define INHNO_IPI_DISPATCH_PRC4		(0x00040000 | IPINO_DISPATCH)
#define INHNO_IPI_DISPATCH_PRC5		(0x00050000 | IPINO_DISPATCH)
#define INHNO_IPI_DISPATCH_PRC6		(0x00060000 | IPINO_DISPATCH)

/*
 *  カーネル終了要求プロセッサ間割込みの割込みハンドラ番号
 */
#define INHNO_IPI_EXT_KER_PRC1		(0x00010000 | IPINO_EXT_KER)
#define INHNO_IPI_EXT_KER_PRC2		(0x00020000 | IPINO_EXT_KER)
#define INHNO_IPI_EXT_KER_PRC3		(0x00030000 | IPINO_EXT_KER)
#define INHNO_IPI_EXT_KER_PRC4		(0x00040000 | IPINO_EXT_KER)
#define INHNO_IPI_EXT_KER_PRC5		(0x00050000 | IPINO_EXT_KER)
#define INHNO_IPI_EXT_KER_PRC6		(0x00060000 | IPINO_EXT_KER)

/*
 *  高分解能タイマ設定要求プロセッサ間割込みの割込みハンドラ番号
 */
#define INHNO_IPI_SET_HRT_EVT_PRC1	(0x00010000 | IPINO_SET_HRT_EVT)
#define INHNO_IPI_SET_HRT_EVT_PRC2	(0x00020000 | IPINO_SET_HRT_EVT)
#define INHNO_IPI_SET_HRT_EVT_PRC3	(0x00030000 | IPINO_SET_HRT_EVT)
#define INHNO_IPI_SET_HRT_EVT_PRC4	(0x00040000 | IPINO_SET_HRT_EVT)
#define INHNO_IPI_SET_HRT_EVT_PRC5	(0x00050000 | IPINO_SET_HRT_EVT)
#define INHNO_IPI_SET_HRT_EVT_PRC6	(0x00060000 | IPINO_SET_HRT_EVT)

/*
 *  システム周期開始要求プロセッサ間割込みの割込みハンドラ番号
 */
#define INHNO_IPI_START_SCYC_PRC1	(0x00010000 | IPINO_START_SCYC)
#define INHNO_IPI_START_SCYC_PRC2	(0x00020000 | IPINO_START_SCYC)
#define INHNO_IPI_START_SCYC_PRC3	(0x00030000 | IPINO_START_SCYC)
#define INHNO_IPI_START_SCYC_PRC4	(0x00040000 | IPINO_START_SCYC)
#define INHNO_IPI_START_SCYC_PRC5	(0x00050000 | IPINO_START_SCYC)
#define INHNO_IPI_START_SCYC_PRC6	(0x00060000 | IPINO_START_SCYC)

/*
 *  プロセッサ間割込み発行関数の定義
 *
 *  TOPPERS_OMIT_GIC_IPI_REQUESTが定義されている場合，以下の発行関数を
 *  定義しない．タイマドライバシミュレータ対応ターゲットなど，発行関数
 *  をターゲット依存部で独自に定義する場合に用いる．未定義時は従来通り
 *  以下の発行関数を定義する（additive，他ターゲット非影響）．
 */
#if !defined(TOPPERS_MACRO_ONLY) && !defined(TOPPERS_OMIT_GIC_IPI_REQUEST)

/*
 *  ディスパッチ要求プロセッサ間割込みの発行
 */
Inline void
request_dispatch_prc(ID prcid)
{
	gicd_raise_sgi(IPINO_DISPATCH, prcid);
}

/*
 *  カーネル終了要求プロセッサ間割込みの発行
 */
Inline void
request_ext_ker(ID prcid)
{
	gicd_raise_sgi(IPINO_EXT_KER, prcid);
}

/*
 *  高分解能タイマ設定要求プロセッサ間割込みの発行
 */
Inline void
request_set_hrt_event(ID prcid)
{
	gicd_raise_sgi(IPINO_SET_HRT_EVT, prcid);
}

/*
 *  システム周期開始要求プロセッサ間割込みの発行
 */
Inline void
request_start_scyc_event(PCB *p_my_pcb)
{
	ID	prcid;

	for (prcid = TMIN_PRCID; prcid <= TMAX_PRCID; prcid++) {
		if (prcid != p_my_pcb->prcid) {
			gicd_raise_sgi(IPINO_START_SCYC, prcid);
		}
	}
}

#endif /* TOPPERS_MACRO_ONLY */
#endif /* TOPPERS_GIC_IPI_H */
