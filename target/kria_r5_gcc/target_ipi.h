/*
 *  TOPPERS/FMP Kernel
 *      Toyohashi Open Platform for Embedded Real-Time Systems/
 *      Flexible MultiProcessor Kernel
 *
 *  Copyright (C) 2009-2019 by Embedded and Real-Time Systems Laboratory
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
 *  $Id: target_ipi.h 263 2021-01-08 06:08:59Z ertl-honda $
 */

/*  
 *		プロセッサ間割込みに関する定義（ZynqMP R5用）
 */

#ifndef TOPPERS_TARGET_IPI_H
#define TOPPERS_TARGET_IPI_H

/*
 *  プロセッサ間割込みの割込み優先度
 *
 *  システム周期開始要求割込みの優先度は，高分解能タイマ割込みの優先度
 *  と同じでなければならない．
 */
#ifdef TOPPERS_SAFEG_SECURE

#define INTPRI_IPI_DISPATCH_PRC1		(-16)
#define INTPRI_IPI_DISPATCH_PRC2		(-16)
#define INTPRI_IPI_DISPATCH_PRC3		(-16)
#define INTPRI_IPI_DISPATCH_PRC4		(-16)
#define INTPRI_IPI_EXT_KER_PRC1			(-31)
#define INTPRI_IPI_EXT_KER_PRC2			(-31)
#define INTPRI_IPI_EXT_KER_PRC3			(-31)
#define INTPRI_IPI_EXT_KER_PRC4			(-31)
#define INTPRI_IPI_SET_HRT_EVT_PRC1		(-16)
#define INTPRI_IPI_SET_HRT_EVT_PRC2		(-16)
#define INTPRI_IPI_SET_HRT_EVT_PRC3		(-16)
#define INTPRI_IPI_SET_HRT_EVT_PRC4		(-16)
#define INTPRI_IPI_START_SCYC_PRC1		(-16)
#define INTPRI_IPI_START_SCYC_PRC2		(-16)
#define INTPRI_IPI_START_SCYC_PRC3		(-16)
#define INTPRI_IPI_START_SCYC_PRC4		(-16)

#else /* TOPPERS_SAFEG_SECURE */

#define INTPRI_IPI_DISPATCH_PRC1		(-2)
#define INTPRI_IPI_DISPATCH_PRC2		(-2)
#define INTPRI_IPI_DISPATCH_PRC3		(-2)
#define INTPRI_IPI_DISPATCH_PRC4		(-2)
#define INTPRI_IPI_EXT_KER_PRC1			(-15)
#define INTPRI_IPI_EXT_KER_PRC2			(-15)
#define INTPRI_IPI_EXT_KER_PRC3			(-15)
#define INTPRI_IPI_EXT_KER_PRC4			(-15)
#define INTPRI_IPI_SET_HRT_EVT_PRC1		(-2)
#define INTPRI_IPI_SET_HRT_EVT_PRC2		(-2)
#define INTPRI_IPI_SET_HRT_EVT_PRC3		(-2)
#define INTPRI_IPI_SET_HRT_EVT_PRC4		(-2)
#define INTPRI_IPI_START_SCYC_PRC1		(-2)
#define INTPRI_IPI_START_SCYC_PRC2		(-2)
#define INTPRI_IPI_START_SCYC_PRC3		(-2)
#define INTPRI_IPI_START_SCYC_PRC4		(-2)

#endif /* TOPPERS_SAFEG_SECURE */

/*
 *  GIC依存部
 */
#include "gic_ipi.h"

#endif /* TOPPERS_TARGET_IPI_H */
