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
 *  プロセッサ間割込みに関する定義（MSI用）
 */

#ifndef TOPPERS_MSI_IPI_H
#define TOPPERS_MSI_IPI_H

#ifndef TOPPERS_MACRO_ONLY

/*
 *  ext_ker 要求フラグ
 */
extern bool_t ext_ker_req_flg_table[TNUM_PRCID];

/*
 *  set_hrt_event 要求フラグ
 */
extern bool_t set_hrt_event_req_flg_table[TNUM_PRCID];

/*
 *  プロセッサ間割込み発行関数
 *
 *  TOPPERS_OMIT_MSI_IPI_REQUESTが定義されている場合（例：タイマドライバ
 *  シミュレータ用ターゲット simtimer_polarfire_soc_kit_gcc）は，ここでの
 *  発行関数の定義を抑止し，ターゲット依存部（target_ipi.h）でsimtim_set_
 *  int_flag()付きの発行関数を定義する．未定義時は従来通りの定義となり，
 *  既存ターゲットに影響しない．
 */
#ifndef TOPPERS_OMIT_MSI_IPI_REQUEST

/*
 *  ディスパッチ要求プロセッサ間割込みの発行
 */
Inline void
request_dispatch_prc(ID prcid)
{
    raise_msip(prcid);
}

/*
 *  カーネル終了要求プロセッサ間割込みの発行
 */
Inline void
request_ext_ker(ID prcid)
{
    ext_ker_req_flg_table[INDEX_PRC(prcid)] = true;
    raise_msip(prcid);
}

/*
 *  高分解能タイマ設定要求プロセッサ間割込みの発行
 */
Inline void
request_set_hrt_event(ID prcid)
{
    set_hrt_event_req_flg_table[INDEX_PRC(prcid)] = true;
    raise_msip(prcid);
}

#endif /* TOPPERS_OMIT_MSI_IPI_REQUEST */

/*
 *  Machine Software Interrupt Handler
 */
extern void msi_handler(void);

#endif /* TOPPERS_MACRO_ONLY */
#endif /* TOPPERS_MSI_IPI_H */
