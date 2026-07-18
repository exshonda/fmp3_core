/*
 *  TOPPERS/FMP Kernel
 *      Toyohashi Open Platform for Embedded Real-Time Systems/
 *      Flexible MultiProcessor Kernel
 * 
 *  Copyright (C) 2000-2003 by Embedded and Real-Time Systems Laboratory
 *                              Toyohashi Univ. of Technology, JAPAN
 *  Copyright (C) 2005-2020 by Embedded and Real-Time Systems Laboratory
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
 *  $Id: time_event.c 508 2026-06-16 06:29:17Z ertl-honda $
 */

/*
 *		タイムイベント管理モジュール
 */

#include "kernel_impl.h"
#include "time_event.h"
#include "task.h"
#include "target_ipi.h"

/*
 *  TCYC_HRTCNTの定義のチェック
 */
#if defined(USE_64BIT_HRTCNT) && defined(TCYC_HRTCNT)
#error TCYC_HRTCNT must not be defined when USE_64BIT_HRTCNT.
#endif

/*
 *  TSTEP_HRTCNTの範囲チェック
 */
#if TSTEP_HRTCNT > 4000U
#error TSTEP_HRTCNT is too large.
#endif /* TSTEP_HRTCNT > 4000U */

/*
 *  HRTCNT_BOUNDの定義のチェック
 */
#ifndef USE_64BIT_HRTCNT

#if HRTCNT_BOUND >= 4294000000U
#error HRTCNT_BOUND is too large.
#endif /* HRTCNT_BOUND >= 4294000000U */

#ifdef TCYC_HRTCNT
#if HRTCNT_BOUND >= TCYC_HRTCNT
#error HRTCNT_BOUND is too large.
#endif /* HRTCNT_BOUND >= TCYC_HRTCNT */
#endif /* TCYC_HRTCNT */

#else /* USE_64BIT_HRTCNT */

#ifdef HRTCNT_BOUND
#error USE_64BIT_HRTCNT is not supported on this target.
#endif /* HRTCNT_BOUND */

#endif /* USE_64BIT_HRTCNT */

/*
 *  タイムイベントヒープ操作マクロ
 */
#define	ROOT_INDEX			(1)					/* ルートノードのインデックス */
#define	PARENT(index)		((index) >> 1)		/* 親ノードを求める */
#define	LCHILD(index)		((index) << 1)		/* 左の子ノードを求める */
#define	LAST_INDEX(p_tmevt_heap) \
							((p_tmevt_heap)->last_index)
#define	HEAP_NODE(index, p_tmevt_heap) \
							(((p_tmevt_heap) + (index))->p_tmevtb)

/*
 *  タイムイベントヒープ中の先頭のタイムイベントの発生時刻
 */
#define top_evttim(p_tmevt_heap)	(HEAP_NODE(ROOT_INDEX, p_tmevt_heap) \
																->evttim)

/*
 *  イベント時刻の前後関係の判定［ASPD1009］
 *
 *  イベント時刻は，boundary_evttimからの相対値で比較する．すなわち，
 *  boundary_evttimを最も早い時刻，boundary_evttim−1が最も遅い時刻とみ
 *  なして比較する．
 */
#define EVTTIM_ADVANCE(t)	((t) - boundary_evttim)
#define EVTTIM_LT(t1, t2)	(EVTTIM_ADVANCE(t1) < EVTTIM_ADVANCE(t2))
#define EVTTIM_LE(t1, t2)	(EVTTIM_ADVANCE(t1) <= EVTTIM_ADVANCE(t2))

#ifdef TOPPERS_tmeini

/*
 *  境界イベント時刻［ASPD1008］
 */
EVTTIM	boundary_evttim;

/*
 *  最後に現在時刻を算出した時点でのイベント時刻［ASPD1012］
 */
EVTTIM	current_evttim;

/*
 *  最後に現在時刻を算出した時点での高分解能タイマのカウント値［ASPD1012］
 */
HRTCNT	current_hrtcnt;

/*
 *  最も進んでいた時のイベント時刻［ASPD1041］
 */
EVTTIM	monotonic_evttim;

/*
 *  システム時刻のオフセット［ASPD1043］
 */
SYSTIM	systim_offset;

/*
 *  タイムイベント管理モジュールの初期化［ASPD1061］
 */
void
initialize_tmevt(PCB *p_my_pcb)
{
	if (TOPPERS_TEPP_PRC & (1 << get_my_prcidx())) {
		p_my_pcb->p_tevtcb = p_tevtcb_table[get_my_prcidx()];
		p_my_pcb->p_tevtcb->p_tmevt_heap = p_tmevt_heap_table[get_my_prcidx()];

		/*
		 *  タイムマスタプロセッサであればグローバル変数を初期化
		 */
		if (p_my_pcb->prcid == TOPPERS_TMASTER_PRCID) {
			current_evttim = 0U;							/*［ASPD1047］*/
			boundary_evttim = current_evttim - BOUNDARY_MARGIN;
			/*［ASPD1048］*/
			monotonic_evttim = 0U;							/*［ASPD1046］*/
			systim_offset = 0U;								/*［ASPD1044］*/
		}
		p_my_pcb->p_tevtcb->in_signal_time = false;							/*［ASPD1033］*/
		LAST_INDEX(p_my_pcb->p_tevtcb->p_tmevt_heap) = 0;
	}
	else {
		p_my_pcb->p_tevtcb = NULL;
	}
}

#endif /* TOPPERS_tmeini */

/*
 *  タイムイベントの挿入位置を上向きに探索
 *
 *  時刻evttimに発生するタイムイベントを挿入するノードを空けるために，
 *  ヒープの上に向かって空ノードを移動させる．移動前の空ノードの位置を
 *  indexに渡すと，移動後の空ノードの位置（すなわち挿入位置）を返す．
 */
#ifdef TOPPERS_tmeup

uint_t
tmevt_up(uint_t index, EVTTIM evttim, TMEVTN *p_tmevt_heap)
{
	uint_t	parent;

	while (index > ROOT_INDEX) {
		/*
		 *  親ノードのイベント発生時刻の方が早い（または同じ）ならば，
		 *  indexが挿入位置なのでループを抜ける．
		 */
		parent = PARENT(index);
		if (EVTTIM_LE(HEAP_NODE(parent, p_tmevt_heap)->evttim, evttim)) {
			break;
		}

		/*
		 *  親ノードをindexの位置に移動させる．
		 */
		HEAP_NODE(index, p_tmevt_heap) = HEAP_NODE(parent, p_tmevt_heap);
		HEAP_NODE(index, p_tmevt_heap)->index = index;

		/*
		 *  indexを親ノードの位置に更新．
		 */
		index = parent;
	}
	return(index);
}

#endif /* TOPPERS_tmeup */

/*
 *  タイムイベントの挿入位置を下向きに探索
 *
 *  時刻evttimに発生するタイムイベントを挿入するノードを空けるために，
 *  ヒープの下に向かって空ノードを移動させる．移動前の空ノードの位置を
 *  indexに渡すと，移動後の空ノードの位置（すなわち挿入位置）を返す．
 */
#ifdef TOPPERS_tmedown

uint_t
tmevt_down(uint_t index, EVTTIM evttim, TMEVTN *p_tmevt_heap)
{
	uint_t	child;

	while ((child = LCHILD(index)) <= LAST_INDEX(p_tmevt_heap)) {
		/*
		 *  左右の子ノードのイベント発生時刻を比較し，早い方の子ノード
		 *  の位置をchildに設定する．以下の子ノードは，ここで選ばれた
		 *  方の子ノードのこと．
		 */
		if (child + 1 <= LAST_INDEX(p_tmevt_heap)
					&& EVTTIM_LT(HEAP_NODE(child + 1, p_tmevt_heap)->evttim,
									HEAP_NODE(child, p_tmevt_heap)->evttim)) {
			child = child + 1;
		}

		/*
		 *  子ノードのイベント発生時刻の方が遅い（または同じ）ならば，
		 *  indexが挿入位置なのでループを抜ける．
		 */
		if (EVTTIM_LE(evttim, HEAP_NODE(child, p_tmevt_heap)->evttim)) {
			break;
		}

		/*
		 *  子ノードをindexの位置に移動させる．
		 */
		HEAP_NODE(index, p_tmevt_heap) = HEAP_NODE(child, p_tmevt_heap);
		HEAP_NODE(index, p_tmevt_heap)->index = index;

		/*
		 *  indexを子ノードの位置に更新．
		 */
		index = child;
	}
	return(index);
}

#endif /* TOPPERS_tmedown */

/*
 *  タイムイベントヒープへの追加
 *
 *  p_tmevtbで指定したタイムイベントブロックを，タイムイベントヒープに
 *  追加する．
 */
Inline void
tmevtb_insert(TMEVTB *p_tmevtb, TMEVTN *p_tmevt_heap)
{
	uint_t	index;

	/*
	 *  last_indexをインクリメントし，そこから上に挿入位置を探す．
	 */
	index = tmevt_up(++LAST_INDEX(p_tmevt_heap), p_tmevtb->evttim,
														p_tmevt_heap);

	/*
	 *  タイムイベントをindexの位置に挿入する．
	 */
	HEAP_NODE(index, p_tmevt_heap) = p_tmevtb;
	p_tmevtb->index = index;
}

/*
 *  タイムイベントヒープからの削除
 */
Inline void
tmevtb_delete(TMEVTB *p_tmevtb, TMEVTN *p_tmevt_heap)
{
	uint_t	index = p_tmevtb->index;
	uint_t	last_index = LAST_INDEX(p_tmevt_heap);
	uint_t	parent;
	EVTTIM	event_evttim;

	/*
	 *  削除によりタイムイベントヒープが空になる場合は何もしない．
	 */
	if (--LAST_INDEX(p_tmevt_heap) > 0) {
		/*
		 *  削除したノードの位置に最後のノード（last_index の位置の
		 *  ノード）を挿入し，それを適切な位置へ移動させる．実際には，
		 *  最後のノードを実際に挿入するのではなく，削除したノードの位
		 *  置が空ノードになるので，最後のノードを挿入すべき位置へ向け
		 *  て空ノードを移動させる．
		 *
		 *  最後のノードのイベント発生時刻が，削除したノードの親ノード
		 *  のイベント発生時刻より前の場合には，上に向かって挿入位置を
		 *  探す．そうでない場合には，下に向かって探す．
		 */
		event_evttim = HEAP_NODE(last_index, p_tmevt_heap)->evttim;
		if (index > ROOT_INDEX
				&& EVTTIM_LT(event_evttim,
		 			HEAP_NODE(parent = PARENT(index), p_tmevt_heap)->evttim)) {
			/*
			 *  親ノードをindexの位置に移動させる．
			 */
			HEAP_NODE(index, p_tmevt_heap) = HEAP_NODE(parent, p_tmevt_heap);
			HEAP_NODE(index, p_tmevt_heap)->index = index;

			/*
			 *  削除したノードの親ノードから上に向かって挿入位置を探す．
			 */
			index = tmevt_up(parent, event_evttim, p_tmevt_heap);
		}
		else {
			/*
			 *  削除したノードから下に向かって挿入位置を探す．
			 */
			index = tmevt_down(index, event_evttim, p_tmevt_heap);
		}

		/*
		 *  最後のノードをindexの位置に挿入する．
		 */
		HEAP_NODE(index, p_tmevt_heap) = HEAP_NODE(last_index, p_tmevt_heap);
		HEAP_NODE(index, p_tmevt_heap)->index = index;
	}
}

/*
 *  タイムイベントヒープの先頭のノードの削除
 */
Inline TMEVTB *
tmevtb_delete_top(TMEVTN *p_tmevt_heap)
{
	uint_t	index;
	uint_t	last_index = LAST_INDEX(p_tmevt_heap);
	TMEVTB	*p_top_tmevtb = HEAP_NODE(ROOT_INDEX, p_tmevt_heap);
	EVTTIM	event_evttim;

	/*
	 *  削除によりタイムイベントヒープが空になる場合は何もしない．
	 */
	if (--LAST_INDEX(p_tmevt_heap) > 0) {
		/*
		 *  ルートノードに最後のノード（last_index の位置のノード）を
		 *  挿入し，それを適切な位置へ移動させる．実際には，最後のノー
		 *  ドを実際に挿入するのではなく，ルートノードが空ノードになる
		 *  ので，最後のノードを挿入すべき位置へ向けて空ノードを移動さ
		 *  せる．
		 */
		event_evttim = HEAP_NODE(last_index, p_tmevt_heap)->evttim;
		index = tmevt_down(ROOT_INDEX, event_evttim, p_tmevt_heap);

		/*
		 *  最後のノードをindexの位置に挿入する．
		 */
		HEAP_NODE(index, p_tmevt_heap) = HEAP_NODE(last_index, p_tmevt_heap);
		HEAP_NODE(index, p_tmevt_heap)->index = index;
	}
	return(p_top_tmevtb);
}

/*
 *  現在のイベント時刻の更新
 */
#ifdef TOPPERS_tmecur

void
update_current_evttim(void)
{
	HRTCNT	new_hrtcnt, hrtcnt_advance;
	EVTTIM	previous_evttim;

	new_hrtcnt = target_hrt_get_current();			/*［ASPD1013］*/
	hrtcnt_advance = new_hrtcnt - current_hrtcnt;	/*［ASPD1014］*/
#ifdef TCYC_HRTCNT
	if (new_hrtcnt < current_hrtcnt) {
		hrtcnt_advance += TCYC_HRTCNT;
	}
#endif /* TCYC_HRTCNT */
	current_hrtcnt = new_hrtcnt;					/*［ASPD1016］*/

	previous_evttim = current_evttim;
	current_evttim += (EVTTIM) hrtcnt_advance;		/*［ASPD1015］*/
	boundary_evttim = current_evttim - BOUNDARY_MARGIN;	/*［ASPD1011］*/

	if (monotonic_evttim - previous_evttim < (EVTTIM) hrtcnt_advance) {
#ifdef UINT64_MAX
		if (current_evttim < monotonic_evttim) {
			systim_offset += 1LLU << 32;			/*［ASPD1045］*/
		}
#endif /* UINT64_MAX */
		monotonic_evttim = current_evttim;			/*［ASPD1042］*/
	}
}

#endif /* TOPPERS_tmecur */

/*
 *  高分解能タイマ割込みの発生タイミングの設定
 *  引数pcbは操作対象とするタイムイベント処理プロセッサのPCBである． 
 */
#ifdef TOPPERS_tmeset

void
set_hrt_event(PCB *p_pcb)
{
	HRTCNT	hrtcnt;

#ifndef TOPPERS_SUPPORT_CONTROL_OTHER_HRT
	if (p_pcb != get_my_pcb()) {
		/*
		 *  他プロセッサの高分解能タイマを設定したい場合は，割込みを要
		 *  求する．
		 */
		request_set_hrt_event(p_pcb->prcid);
		return;
	}
#endif /* TOPPERS_SUPPORT_CONTROL_OTHER_HRT */

	if (LAST_INDEX(p_pcb->p_tevtcb->p_tmevt_heap) == 0) {
		/*
		 *  タイムイベントがない場合
		 */
#ifdef USE_64BIT_HRTCNT
		target_hrt_clear_event(p_pcb->prcid);
#else /* USE_64BIT_HRTCNT */
		if (p_pcb->prcid == TOPPERS_TMASTER_PRCID) {
			/*
			 *  タイムマスタプロセッサでは，HRTCNT_BOUND後に割込みを発
			 *  生させる．［ASPD1007］
			 */
			target_hrt_set_event(p_pcb->prcid, HRTCNT_BOUND);
		}
		else {
			/*
			 *  他のプロセッサでは，割込みを発生させないようにする．
			 */
			target_hrt_clear_event(p_pcb->prcid);
		}
#endif /* USE_64BIT_HRTCNT */
	}
	else if (EVTTIM_LE(top_evttim(p_pcb->p_tevtcb->p_tmevt_heap), current_evttim)) {
		/*
		 *  タイムイベントの発生時刻を過ぎている場合［ASPD1017］
		 */
		target_hrt_raise_event(p_pcb->prcid);
	}
	else {
		hrtcnt = (HRTCNT)(top_evttim(p_pcb->p_tevtcb->p_tmevt_heap) - current_evttim);
#ifdef USE_64BIT_HRTCNT
		target_hrt_set_event(p_pcb->prcid, hrtcnt);
#else /* USE_64BIT_HRTCNT */
		if (hrtcnt > HRTCNT_BOUND) {
			/*
			 *  イベントがHRTCNT_BOUNDより先の場合なので，タイムマスタ
			 *  プロセッサ以外でも割込みを発生させる必要がある．［ASPD1006］
			 */
			target_hrt_set_event(p_pcb->prcid, HRTCNT_BOUND);
		}
		else {
			target_hrt_set_event(p_pcb->prcid, hrtcnt);	/*［ASPD1002］*/
		}
#endif /* USE_64BIT_HRTCNT */
	}
}

#ifndef TOPPERS_SUPPORT_CONTROL_OTHER_HRT
/*
 *  高分解能タイマ設定ハンドラ
 *
 *  非タスクコンテキストで実行されるため，他のプロセッサへマイグレート
 *  することはなく，CPUロック状態にせずに自プロセッサのPCBにアクセスし
 *  てよい．
 */
void
set_hrt_event_handler(void)
{
	PCB		*p_my_pcb = get_my_pcb();

	assert(sense_context(p_my_pcb));
	assert(!sense_lock());

	lock_cpu();
	acquire_glock();
	set_hrt_event(p_my_pcb);
	release_glock();
	unlock_cpu();
}

#endif /* TOPPERS_SUPPORT_CONTROL_OTHER_HRT */
#endif /* TOPPERS_tmeset */

/*
 *  タイムイベントブロックのヒープへの挿入
 */
#ifdef TOPPERS_tmereg

void
tmevtb_register(TMEVTB *p_tmevtb, PCB *p_pcb)
{
	/*
	 *  タイムイベント処理プロセッサでない場合はタイムマスタプロセッサに
	 *  依頼．
	 */
	if (p_pcb->p_tevtcb == NULL) {
		p_pcb = P_TM_PCB;
	}

	tmevtb_insert(p_tmevtb, p_pcb->p_tevtcb->p_tmevt_heap);
}

#endif /* TOPPERS_tmereg */

/*
 *  タイムイベントの登録
 *  
 */
#ifdef TOPPERS_tmeenq

void
tmevtb_enqueue(TMEVTB *p_tmevtb, PCB *p_pcb)
{
	TMEVTN *p_tmevt_heap;

	/*
	 *  タイムイベント処理プロセッサでない場合はタイムマスタプロセッサに
	 *  依頼．
	 */
	if (p_pcb->p_tevtcb == NULL) {
		p_pcb = P_TM_PCB;
	}

	p_tmevt_heap = p_pcb->p_tevtcb->p_tmevt_heap;

	/*
	 *  タイムイベントブロックをヒープに挿入する．
	 */
	tmevtb_insert(p_tmevtb, p_tmevt_heap);

	/*
	 *  高分解能タイマ割込みの発生タイミングを設定する．
	 */
	if (!(p_pcb->p_tevtcb->in_signal_time)
				&& p_tmevtb->index == ROOT_INDEX) {
		set_hrt_event(p_pcb);
	}
}

#endif /* TOPPERS_tmeenq */

/*
 *  相対時間指定によるタイムイベントの登録
 *  
 */
#ifdef TOPPERS_tmeenqrel

void
tmevtb_enqueue_reltim(TMEVTB *p_tmevtb, RELTIM time, PCB *p_pcb)
{
	TMEVTN *p_tmevt_heap;

	/*
	 *  タイムイベント処理プロセッサでない場合はタイムマスタプロセッサに
	 *  依頼．
	 */
	if (p_pcb->p_tevtcb == NULL) {
		p_pcb = P_TM_PCB;
	}

	p_tmevt_heap = p_pcb->p_tevtcb->p_tmevt_heap;

	/*
	 *  現在のイベント時刻とタイムイベントの発生時刻を求める［ASPD1026］．
	 */
	update_current_evttim();
	p_tmevtb->evttim = calc_current_evttim_ub() + time;

	/*
	 *  タイムイベントブロックをヒープに挿入する［ASPD1030］．
	 */
	tmevtb_insert(p_tmevtb, p_tmevt_heap);

	/*
	 *  高分解能タイマ割込みの発生タイミングを設定する［ASPD1031］
	 *  ［ASPD1034］．
	 */
	if (!(p_pcb->p_tevtcb->in_signal_time)
				&& p_tmevtb->index == ROOT_INDEX) {
		set_hrt_event(p_pcb);
	}
}

#endif /* TOPPERS_tmeenqrel */

/*
 *  タイムイベントの登録解除
 */
#ifdef TOPPERS_tmedeq

void
tmevtb_dequeue(TMEVTB *p_tmevtb, PCB *p_pcb)
{
	uint_t	index;
	TMEVTN *p_tmevt_heap;

	/*
	 *  タイムイベント処理プロセッサでない場合はタイムマスタプロセッサに
	 *  依頼．
	 */
	if (p_pcb->p_tevtcb == NULL) {
		p_pcb = P_TM_PCB;
	}

	p_tmevt_heap = p_pcb->p_tevtcb->p_tmevt_heap;

	/*
	 *  タイムイベントブロックをヒープから削除する［ASPD1039］．
	 */
	index = p_tmevtb->index;
	tmevtb_delete(p_tmevtb, p_tmevt_heap);

	/*
	 *  高分解能タイマ割込みの発生タイミングを設定する［ASPD1040］．
	 */
	if (!(p_pcb->p_tevtcb->in_signal_time)
				&& index == ROOT_INDEX) {
		update_current_evttim();
		set_hrt_event(p_pcb);
	}
}

#endif /* TOPPERS_tmedeq */

/*
 *  システム時刻の調整時のエラーチェック
 */
#ifdef TOPPERS_tmechk

bool_t
check_adjtim(int32_t adjtim, PCB *p_pcb)
{
	if (adjtim > 0) {							/*［NGKI3588］*/
		return(LAST_INDEX(p_pcb->p_tevtcb->p_tmevt_heap) > 0	/*［NGKI3588］*/
					&& EVTTIM_LE(top_evttim(p_pcb->p_tevtcb->p_tmevt_heap) + TMAX_ADJTIM, current_evttim));
	}
	else if (adjtim < 0) {						/*［NGKI3589］*/
		return(monotonic_evttim - current_evttim >= -TMIN_ADJTIM);
	}
	return(false);
}

#endif /* TOPPERS_tmechk */

/*
 *  タイムイベントが発生するまでの時間の計算
 */
#ifdef TOPPERS_tmeltim

RELTIM
tmevt_lefttim(TMEVTB *p_tmevtb)
{
	EVTTIM	evttim, current_evttim_ub;

	/*
	 *  現在のイベント時刻を遅い方に丸めた時刻を求める［ASPD1050］．
	 */
	update_current_evttim();
	current_evttim_ub = calc_current_evttim_ub();

	/*
	 *  タイムイベント発生までの相対時間を求める［ASPD1049］．
	 */
	evttim = p_tmevtb->evttim;
	if (EVTTIM_LE(evttim, current_evttim_ub)) {
		/*
		 *  タイムイベントの発生時刻を過ぎている場合には0を返す［NGKI0552］．
		 */
		return(0U);
	}
	else {
		return((RELTIM)(evttim - current_evttim_ub));
	}
}

#endif /* TOPPERS_tmeltim */

/*
 *  高分解能タイマ割込みの処理
 */
#ifdef TOPPERS_sigtim

void
signal_time(void)
{
	TMEVTB	*p_tmevtb;
	bool_t	callflag;
#ifndef TOPPERS_OMIT_SYSLOG
	uint_t	nocall = 0;
#endif /* TOPPERS_OMIT_SYSLOG */
	PCB		*p_my_pcb = get_my_pcb();

	assert(sense_context(p_my_pcb));
	assert(!sense_lock());

	lock_cpu();
	acquire_glock();
	p_my_pcb->p_tevtcb->in_signal_time = true;						/*［ASPD1033］*/

	do {
		/*
		 *  コールバック関数を呼び出さなければループを抜ける［ASPD1020］．
		 */
		callflag = false;

		/*
		 *  現在のイベント時刻を求める［ASPD1022］．
		 */
		update_current_evttim();

		/*
		 *  発生時刻がcurrent_evttim以前のタイムイベントがあれば，タイ
		 *  ムイベントヒープから削除し，コールバック関数を呼び出す
		 *  ［ASPD1018］［ASPD1019］．
		 */
		while (LAST_INDEX(p_my_pcb->p_tevtcb->p_tmevt_heap) > 0
							&& EVTTIM_LE(top_evttim(p_my_pcb->p_tevtcb->p_tmevt_heap), current_evttim)) {
			p_tmevtb = tmevtb_delete_top(p_my_pcb->p_tevtcb->p_tmevt_heap);
			(*(p_tmevtb->callback))(p_my_pcb, p_tmevtb->arg);
			callflag = true;
#ifndef TOPPERS_OMIT_SYSLOG
			nocall += 1;
#endif /* TOPPERS_OMIT_SYSLOG */
		}
	} while (callflag);								/*［ASPD1020］*/

#ifndef TOPPERS_OMIT_SYSLOG
	/*
	 *  タイムイベントが処理されなかった場合．
	 */
	if (nocall == 0) {
		syslog_1(LOG_NOTICE, "no time event is processed in hrt interrupt " \
												"on PRC%d.", p_my_pcb->prcid);
	}
#endif /* TOPPERS_OMIT_SYSLOG */

	/*
	 *  高分解能タイマ割込みの発生タイミングを設定する［ASPD1025］．
	 */
	set_hrt_event(p_my_pcb);

	p_my_pcb->p_tevtcb->in_signal_time = false;				/*［ASPD1033］*/
	release_glock();
	unlock_cpu();
}

#endif /* TOPPERS_sigtim */
