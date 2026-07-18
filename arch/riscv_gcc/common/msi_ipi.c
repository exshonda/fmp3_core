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
 *  $Id: mpcore_timer.c 335 2023-04-18 10:50:40Z ertl-honda $
 */

/*
 *   プロセッサ間割込みドライバ（Machine Software Interrupt 用）
 */

#include "kernel_impl.h"
#include "target_ipi.h"
#include "time_event.h"
#ifdef TOPPERS_SIMTIMER
#include "target_timer.h"
#endif /* TOPPERS_SIMTIMER */

/*
 *  IPI(MSI)受信処理の入口フック．msi_handler の先頭(clear_msip の前)で呼ばれる．
 *  割込みコントローラ依存部(チップ)が実体を提供する．必要な処理が無ければ空に
 *  定義する．本汎用 IPI ハンドラに割込みコントローラ依存を持ち込まないための関数．
 */
extern void irc_begin_ipi(PCB *p_my_pcb);

/*
 *  ext_ker 要求フラグ
 */
bool_t ext_ker_req_flg_table[TNUM_PRCID];

/*
 *  set_hrt_event 要求フラグ
 */
bool_t set_hrt_event_req_flg_table[TNUM_PRCID];

#ifdef TOPPERS_SIMTIMER
/*
 *  高分解能タイマローカル割込み要求フラグ（タイマドライバシミュレータ用）
 *
 *  target_timer.c（simtimer_polarfire_soc_kit_gcc）で定義される．
 *  simtim_target_raise_hrt_int()が自プロセッサ分をセットし，本ハンドラが
 *  自プロセッサ分をクリアして target_hrt_handler() を呼び出す．
 */
extern bool_t sim_hrt_local_req_flg_table[TNUM_PRCID];
#endif /* TOPPERS_SIMTIMER */

/*
 *  Machine Software Interrupt Handler
 * 
 *  MSIは1本しかないため，全ての要求で同じハンドラを実行する
 *   ・ディスパッチ要求
 *   ・カーネル終了要求
 *   ・高分解能タイマ設定要求
 */
void
msi_handler(void)
{
    PCB *p_my_pcb = get_my_pcb();

    /*
     *  IPI 受信処理の入口フック(clear_msip の前)．割込みコントローラ依存部が
     *  必要な処理を行う(不要なら空)．
     */
    irc_begin_ipi(p_my_pcb);

    /*
     *  MSIのクリア
     */
    clear_msip(p_my_pcb->prcid);
    
    /*
     *  特になしもしないため全ての要因で呼び出して問題ない
     */
    dispatch_handler();

    /*
     *  ext_kerの要求があれば呼び出す
     */
    if (ext_ker_req_flg_table[INDEX_PRC(p_my_pcb->prcid)]) {
         ext_ker_handler();
    }

    /*
     *  set_hrt_event の要求があれば呼び出す
     */
    if (set_hrt_event_req_flg_table[INDEX_PRC(p_my_pcb->prcid)]) {
        set_hrt_event_req_flg_table[INDEX_PRC(p_my_pcb->prcid)] = false;
        set_hrt_event_handler();
    }

#ifdef TOPPERS_SIMTIMER
    /*
     *  高分解能タイマローカル割込みの要求があれば，シミュレートされた
     *  高分解能タイマ割込みハンドラを呼び出す（タイマドライバシミュレータ
     *  用．msipをHRTローカル割込み源として流用する）．
     */
    if (sim_hrt_local_req_flg_table[INDEX_PRC(p_my_pcb->prcid)]) {
        sim_hrt_local_req_flg_table[INDEX_PRC(p_my_pcb->prcid)] = false;
        target_hrt_handler();
    }
#endif /* TOPPERS_SIMTIMER */
}
