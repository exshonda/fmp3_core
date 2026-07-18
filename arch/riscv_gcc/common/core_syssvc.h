/*
 *  TOPPERS/FMP Kernel
 *      Toyohashi Open Platform for Embedded Real-Time Systems/
 *      Flexible MultiProcessor Kernel
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
 *  $Id: core_syssvc.h 335 2023-04-18 10:50:40Z ertl-honda $
 */

/*
 *     システムサービスのコア依存部（RISC-V用）
 *
 *  このヘッダファイルは，target_syssvc.h（または，そこからインクルード
 *  されるファイル）のみからインクルードされる．他のファイルから直接イ
 *  ンクルードしてはならない．
 */

#ifndef TOPPERS_CORE_SYSSVC_H
#define TOPPERS_CORE_SYSSVC_H

#include <t_stddef.h>
#include "riscv.h"

/*
 *  計測前のキャッシュと分岐予測の無効化(ToDo)
 *
 *  実行時間を計測する前に，キャッシュ，TLB，分岐予測を無効化する．
 */
#ifdef HIST_INVALIDATE_CACHE

Inline void
riscv_invalidate_all(void)
{

}

#define HIST_BM_HOOK()		riscv_invalidate_all()

#endif /* HIST_INVALIDATE_CACHE */

/*
 *  パフォーマンスモニタによる性能評価
 */

/*
 *  パフォーマンスモニタのカウンタのデータ型
 */
typedef uint64_t  PMCNT;

/*
 *  パフォーマンスモニタのカウンタの読み込み
 */
Inline void 
riscv_get_pmcnt(PMCNT *p_count)
{
    *p_count = riscv_read_ccount();
}

/*
 *  カウンタ値の時間計測単位への変換
 *
 *  時間計測の単位は，0.1マイクロ秒としている．
 */
Inline uint_t
riscv_conv_pmcnt(PMCNT count) {
    return(((uint_t) count) * 10 / CORE_CLK_MHZ);
}

/*
 *  実行時間分布集計サービスの設定
 */
#define HISTTIM                 PMCNT
#define HIST_GET_TIM(p_time)    (riscv_get_pmcnt(p_time))
#define HIST_CONV_TIM(time)     (riscv_conv_pmcnt(time))

#endif /* TOPPERS_CORE_SYSSVC_H */
