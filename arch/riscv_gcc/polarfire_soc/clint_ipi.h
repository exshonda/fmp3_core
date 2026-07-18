/*
 *  TOPPERS/FMP Kernel
 *      Toyohashi Open Platform for Embedded Real-Time Systems/
 *      Flexible MultiProcessor Kernel
 *
 *  Copyright (C) 2024 by Embedded and Real-Time Systems Laboratory
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
 *  $Id: gic_ipi.h 335 2023-04-18 10:50:40Z ertl-honda $
 */

/*
 *    プロセッサ間割込みに関する定義（CLINT用）
 */

#ifndef TOPPERS_CLINT_IPI_H
#define TOPPERS_CLINT_IPI_H

/*
 *  PolarFire SoC のハードウェア資源の定義
 */
#include "polarfire_soc.h"

/*
 *  プロセッサ間割込みに使用する割込みの番号
 */
#define IPINO_DISPATCH			UINT_C(0)		/* ディスパッチ要求 */
#define IPINO_EXT_KER			UINT_C(1)		/* カーネル終了要求 */
#define IPINO_SET_HRT_EVT		UINT_C(2)		/* 高分解能タイマ設定要求 */
#define IPINO_START_SCYC		UINT_C(3)		/* システム周期開始要求 */

#ifndef TOPPERS_MACRO_ONLY

/*
 *  Machine Software Interrupt の発生
 */
Inline void
raise_msip(ID prcid)
{
    sil_swrw_mem(CLINT_MSIP(prcid), 1U);
    (void)sil_rew_mem(CLINT_MSIP(prcid));
}

/*
 *  Machine Software Interrupt のクリア
 */
Inline void
clear_msip(ID prcid)
{
    sil_swrw_mem(CLINT_MSIP(prcid), 0U);
    (void)sil_rew_mem(CLINT_MSIP(prcid));
}


/*
 *  MSI IPI 依存部
 */
#include "msi_ipi.h"

#endif /* TOPPERS_MACRO_ONLY */
#endif /* TOPPERS_CLINT_IPI_H */
