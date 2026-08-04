/*
 *  TOPPERS/FMP Kernel
 *      Toyohashi Open Platform for Embedded Real-Time Systems/
 *      Flexible MultiProcessor Kernel
 * 
 *  Copyright (C) 2000-2003 by Embedded and Real-Time Systems Laboratory
 *                              Toyohashi Univ. of Technology, JAPAN
 *  Copyright (C) 2005-2023 by Embedded and Real-Time Systems Laboratory
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
 *  $Id: interrupt.c 335 2023-04-18 10:50:40Z ertl-honda $
 */

/*
 *		割込み管理機能
 */

#include "kernel_impl.h"
#include "check.h"
#include "task.h"
#include "interrupt.h"

/*
 *  トレースログマクロのデフォルト定義
 */
#ifndef LOG_ISR_ENTER
#define LOG_ISR_ENTER(isrid)
#endif /* LOG_ISR_ENTER */

#ifndef LOG_ISR_LEAVE
#define LOG_ISR_LEAVE(isrid)
#endif /* LOG_ISR_LEAVE */

#ifndef LOG_DIS_INT_ENTER
#define LOG_DIS_INT_ENTER(intno)
#endif /* LOG_DIS_INT_ENTER */

#ifndef LOG_DIS_INT_LEAVE
#define LOG_DIS_INT_LEAVE(ercd)
#endif /* LOG_DIS_INT_LEAVE */

#ifndef LOG_ENA_INT_ENTER
#define LOG_ENA_INT_ENTER(intno)
#endif /* LOG_ENA_INT_ENTER */

#ifndef LOG_ENA_INT_LEAVE
#define LOG_ENA_INT_LEAVE(ercd)
#endif /* LOG_ENA_INT_LEAVE */

#ifndef LOG_CLR_INT_ENTER
#define LOG_CLR_INT_ENTER(intno)
#endif /* LOG_CLR_INT_ENTER */

#ifndef LOG_CLR_INT_LEAVE
#define LOG_CLR_INT_LEAVE(ercd)
#endif /* LOG_CLR_INT_LEAVE */

#ifndef LOG_RAS_INT_ENTER
#define LOG_RAS_INT_ENTER(intno)
#endif /* LOG_RAS_INT_ENTER */

#ifndef LOG_RAS_INT_LEAVE
#define LOG_RAS_INT_LEAVE(ercd)
#endif /* LOG_RAS_INT_LEAVE */

#ifndef LOG_PRB_INT_ENTER
#define LOG_PRB_INT_ENTER(intno)
#endif /* LOG_PRB_INT_ENTER */

#ifndef LOG_PRB_INT_LEAVE
#define LOG_PRB_INT_LEAVE(ercd)
#endif /* LOG_PRB_INT_LEAVE */

#ifndef LOG_CHG_IPM_ENTER
#define LOG_CHG_IPM_ENTER(intpri)
#endif /* LOG_CHG_IPM_ENTER */

#ifndef LOG_CHG_IPM_LEAVE
#define LOG_CHG_IPM_LEAVE(ercd)
#endif /* LOG_CHG_IPM_LEAVE */

#ifndef LOG_GET_IPM_ENTER
#define LOG_GET_IPM_ENTER(p_intpri)
#endif /* LOG_GET_IPM_ENTER */

#ifndef LOG_GET_IPM_LEAVE
#define LOG_GET_IPM_LEAVE(ercd, p_intpri)
#endif /* LOG_GET_IPM_LEAVE */

/*
 *  割込み番号の範囲の判定
 */
#ifndef VALID_INTNO_DISINT
#define VALID_INTNO_DISINT(prcid, intno)	VALID_INTNO(prcid, intno)
#endif /* VALID_INTNO_DISINT */

#ifndef VALID_INTNO_CLRINT
#define VALID_INTNO_CLRINT(prcid, intno)	VALID_INTNO(prcid, intno)
#endif /* VALID_INTNO_CLRINT */

#ifndef VALID_INTNO_RASINT
#define VALID_INTNO_RASINT(prcid, intno)	VALID_INTNO(prcid, intno)
#endif /* VALID_INTNO_RASINT */

#ifndef VALID_INTNO_PRBINT
#define VALID_INTNO_PRBINT(prcid, intno)	VALID_INTNO(prcid, intno)
#endif /* VALID_INTNO_PRBINT */

/*
 *  割込み優先度の範囲の判定
 */
#ifndef VALID_INTPRI_CHGIPM
#define VALID_INTPRI_CHGIPM(intpri)	\
					(TMIN_INTPRI <= (intpri) && (intpri) <= TIPM_ENAALL)
#endif /* VALID_INTPRI_CHGIPM */

/*
 *  割込みサービスルーチンIDから割込みサービスルーチン管理ブロックを取り
 *  出すためのマクロ
 */
#define INDEX_ISR(isrid)	((uint_t)((isrid) - TMIN_ISRID))
#define get_isrcb(isrid)	(p_isrcb_table[INDEX_ISR(isrid)])

/*
 *  走査キー（isrpri, isrseq）の辞書式比較
 *
 *  「(pri1, seq1) が (pri2, seq2) より真に大きい」を判定する．call_isrの
 *  走査が，ジャイアントロックを外してISR本体を呼んだ後に「次に呼ぶISR」を
 *  再決定するために使う．
 */
#define ISR_KEY_GT(pri1, seq1, pri2, seq2)								\
			(((pri1) > (pri2))											\
				|| (((pri1) == (pri2)) && ((seq1) > (seq2))))

/*
 *  割込みサービスルーチンキューへの登録
 *
 *  キューは(isrpri, isrseq)の辞書式昇順に保たれる．isrpriが自分より真に
 *  大きい最初の要素の直前へ挿入するので，同一isrpriの中ではenqueueした
 *  順（＝isrseqの昇順）に並ぶ（dcre interrupt.c:182-195と同じ形）．
 *
 *  キューが空のときisrseqのカウンタを0へ戻す．これによりu32のラップは
 *  実用上到達しない．★副作用として，走査中にキューが空になった後で
 *  acre_isrされたISRは，走査側の継続キーより小さい世代番号を持つため
 *  その割込みでは呼ばれない（次の割込みで呼ばれる）．安全側の脱落であり，
 *  二重実行は起こらない．
 *
 *  ジャイアントロックを取得した状態で呼び出すこと．
 */
Inline void
enqueue_isr(ISRQCB *p_isr_queue, ISRCB *p_isrcb)
{
	QUEUE	*p_entry;
	PRI		isrpri = p_isrcb->p_isrinib->isrpri;

	if (queue_empty(&(p_isr_queue->isr_queue))) {
		p_isr_queue->isrseq = 0U;
	}
	p_isrcb->isrseq = p_isr_queue->isrseq;
	p_isr_queue->isrseq += 1U;

	for (p_entry = p_isr_queue->isr_queue.p_next;
							p_entry != &(p_isr_queue->isr_queue);
							p_entry = p_entry->p_next) {
		if (isrpri < ((ISRCB *) p_entry)->p_isrinib->isrpri) {
			break;
		}
	}
	queue_insert_prev(p_entry, &(p_isrcb->isr_queue));
}

/*
 *  割込みサービスルーチン呼出しキューの検索
 *
 *  cfgが生成するグローバルな適格intno表（isr_queue_list）を二分探索する．
 *  この表はENA_DYNISRされたintnoだけを昇順に持つので，
 *  ・範囲外のintno
 *  ・CFG_INTの無いintno
 *  ・ENA_DYNISRされていないintno
 *  ・DEF_INHが競合するintno
 *  はいずれもNULLになる．per-coreのビットマップ（check_intno_cfg）を使わ
 *  ないため，判定結果が呼出しコアに依存しない．
 *
 *  dcre interrupt.c:267-293の転写（型がISRQCB *に変わるだけ）．
 */
Inline ISRQCB *
search_isr_queue(INTNO intno)
{
	int_t	left, right, i;

	if (tnum_isr_queue == 0) {
		return(NULL);
	}

	left = 0;
	right = tnum_isr_queue - 1;
	while (left < right) {
		i = (left + right + 1) / 2;
		if (intno < isr_queue_list[i].intno) {
			right = i - 1;
		}
		else {
			left = i;
		}
	}
	if (isr_queue_list[left].intno == intno) {
		return(isr_queue_list[left].p_isr_queue);
	}
	else {
		return(NULL);
	}
}

/*
 *  割込みサービスルーチン機能の初期化
 */
#ifdef TOPPERS_isrini

/*
 *  使用していない割込みサービスルーチン管理ブロックのリスト
 *
 *  ISRCBの先頭フィールドがQUEUE（isr_queue）なので，そのままfree-listの
 *  リンクに流用する（dcre interrupt.c:202,229と同一）．
 */
QUEUE	free_isrcb;

void
initialize_isr(PCB *p_my_pcb)
{
	uint_t	i, j;
	ISRCB	*p_isrcb;
	ISRINIB	*p_isrinib;

	/*
	 *  割込みサービスルーチンはプロセッサ親和を持たない（ISRINIBに
	 *  iprcid/affinityが無く，ISRCBにp_pcbが無い）．実行プロセッサは
	 *  intnoの配線（CFG_INTのクラス）で決まるため，カーネルオブジェクト
	 *  としては非親和である．したがってマスタプロセッサだけが初期化し，
	 *  他プロセッサへの可視性は本関数の呼出し後のbarrier_syncが保証する
	 *  （段階1のfree_tcb・段階3bのfree_dtqcbと同じ論証）．
	 */
	if (p_my_pcb->prcid == TOPPERS_MASTER_PRCID) {
		for (i = 0; i < tnum_isr_queue; i++) {
			queue_initialize(&(isr_queue_table[i].isr_queue));
			isr_queue_table[i].isrseq = 0U;
		}

		/*
		 *  静的生成ISRの初期化
		 *
		 *  isrorder_tableはisrid昇順である（cfgが生成する．dcreの挿入順
		 *  からの意図的な逸脱で，理由はENA_DYNISRの有無で呼出し順序が
		 *  変わらないようにするため）．この順にenqueue_isrすることで，
		 *  キューの並びは「isrid昇順を基底とするisrpriの安定ソート」＝
		 *  インライン連鎖の呼出し順序と完全に一致する．
		 *
		 *  ★p_isr_queueがNULLのISRは，ENA_DYNISRされていないintnoに
		 *  登録された静的ISRである．インライン連鎖から直接呼ばれるので
		 *  キューには入れない（dcreには無い分岐．dcreは全intnoをキュー化
		 *  するためNULLになりえない）．
		 */
		for (i = 0; i < tnum_sisr; i++) {
			j = INDEX_ISR(isrorder_table[i]);
			p_isrcb = p_isrcb_table[j];
			p_isrcb->p_isrinib = &(isrinib_table[j]);
			p_isrcb->isrseq = 0U;
			p_isrcb->running = 0U;
			if (p_isrcb->p_isrinib->p_isr_queue != NULL) {
				enqueue_isr(p_isrcb->p_isrinib->p_isr_queue, p_isrcb);
			}
		}

		/*
		 *  動的生成用スロットの初期化
		 *
		 *  free-listはFIFO（queue_insert_prevで末尾へ．段階1で裁定済み）．
		 *  iは静的ループから引き継ぐ（dcre interrupt.c:217,224と同じ書き方）．
		 */
		queue_initialize(&free_isrcb);
		for (j = 0; i < tnum_isr; i++, j++) {
			p_isrcb = p_isrcb_table[i];
			p_isrinib = &(aisrinib_table[j]);
			p_isrinib->isratr = TA_NOEXS;
			p_isrcb->p_isrinib = ((const ISRINIB *) p_isrinib);
			p_isrcb->isrseq = 0U;
			p_isrcb->running = 0U;
			queue_insert_prev(&free_isrcb, &(p_isrcb->isr_queue));
		}
	}
}

#endif /* TOPPERS_isrini */

/*
 *  割込みサービスルーチンの呼出し
 */
#ifdef TOPPERS_isrcal

/*
 *  ★★これはTask 3の暫定実装である．Task 4でMP対応版へ全面的に置き換える．
 *
 *  この版はdcre interrupt.c:240-260の素朴な単方向走査であり，
 *  「走査とacre_isr/del_isrが時間的に排他である」という単一プロセッサの
 *  前提に依存している．FMP3では成立しないため，このままでは使えない．
 *  型（ISRQCB *）と表の結線が正しいことをTask 3の時点で独立に検証する
 *  ためだけに置いている．
 */
void
call_isr(ISRQCB *p_isr_queue)
{
	QUEUE	*p_queue;
	ISRCB	*p_isrcb;

	for (p_queue = p_isr_queue->isr_queue.p_next;
							p_queue != &(p_isr_queue->isr_queue);
							p_queue = p_queue->p_next) {
		p_isrcb = (ISRCB *) p_queue;
		LOG_ISR_ENTER(ISRID(p_isrcb));
		(*(p_isrcb->p_isrinib->isr))(p_isrcb->p_isrinib->exinf);
		LOG_ISR_LEAVE(ISRID(p_isrcb));

		if (p_queue->p_next != &(p_isr_queue->isr_queue)) {
			/* ISRの呼出し前の状態に戻す */
			if (sense_lock()) {
				unlock_cpu();
			}
		}
	}
}

#endif /* TOPPERS_isrcal */

/*
 *  割込み管理機能の初期化
 */
#ifdef TOPPERS_intini
#ifndef OMIT_INITIALIZE_INTERRUPT

void
initialize_interrupt(PCB *p_my_pcb)
{
	uint_t			i;
	const INHINIB	*p_inhinib;
	const INTINIB	*p_intinib;

	for (i = 0; i < tnum_def_inhno; i++) {
		p_inhinib = &(inhinib_table[i]);
		if (p_inhinib->prcid == p_my_pcb->prcid) {
			define_inh(p_my_pcb, p_inhinib->inhno, p_inhinib->int_entry);
		}
	}
	for (i = 0; i < tnum_cfg_intno; i++) {
		p_intinib = &(intinib_table[i]);
		if (p_intinib->iprcid == p_my_pcb->prcid) {
			config_int(p_my_pcb, p_intinib->intno, p_intinib->intatr,
							p_intinib->intpri, p_intinib->affinity);
		}
	}
}

#endif /* OMIT_INITIALIZE_INTERRUPT */
#endif /* TOPPERS_intini */

/*
 *  割込みの禁止［NGKI3555］
 */
#ifdef TOPPERS_dis_int
#ifdef TOPPERS_SUPPORT_DIS_INT					/*［NGKI3093］*/

ER
dis_int(INTNO intno)
{
	bool_t	locked;
	ER		ercd;

	LOG_DIS_INT_ENTER(intno);

	locked = sense_lock();
	if (!locked) {
		lock_cpu();
	}
	if (!VALID_INTNO_DISINT(get_my_pcb()->prcid, intno)) {
		ercd = E_PAR;							/*［NGKI3083］［NGKI3087］*/
	}
	else if (check_intno_cfg(intno)) {
		disable_int(intno);						/*［NGKI3086］*/
		ercd = E_OK;
	}
	else {
		ercd = E_OBJ;							/*［NGKI3085］*/
	}
	if (!locked) {
		unlock_cpu();
	}

	LOG_DIS_INT_LEAVE(ercd);
	return(ercd);
}

#endif /* TOPPERS_SUPPORT_DIS_INT */
#endif /* TOPPERS_dis_int */

/*
 *  割込みの許可［NGKI3556］
 */
#ifdef TOPPERS_ena_int
#ifdef TOPPERS_SUPPORT_ENA_INT					/*［NGKI3106］*/

ER
ena_int(INTNO intno)
{
	bool_t	locked;
	ER		ercd;

	LOG_ENA_INT_ENTER(intno);

	locked = sense_lock();
	if (!locked) {
		lock_cpu();
	}
	if (!VALID_INTNO_DISINT(get_my_pcb()->prcid, intno)) {
		ercd = E_PAR;							/*［NGKI3096］［NGKI3100］*/
	}
	else if (check_intno_cfg(intno)) {
		enable_int(intno);						/*［NGKI3099］*/
		ercd = E_OK;
	}
	else {
		ercd = E_OBJ;							/*［NGKI3098］*/
	}
	if (!locked) {
		unlock_cpu();
	}

	LOG_ENA_INT_LEAVE(ercd);
	return(ercd);
}

#endif /* TOPPERS_SUPPORT_ENA_INT */
#endif /* TOPPERS_ena_int */

/*
 *  割込み要求のクリア［NGKI3920］
 */
#ifdef TOPPERS_clr_int
#ifdef TOPPERS_SUPPORT_CLR_INT					/*［NGKI3927］*/

ER
clr_int(INTNO intno)
{
	bool_t	locked;
	ER		ercd;

	LOG_CLR_INT_ENTER(intno);

	locked = sense_lock();
	if (!locked) {
		lock_cpu();
	}
	if (!VALID_INTNO_CLRINT(get_my_pcb()->prcid, intno)) {
		ercd = E_PAR;							/*［NGKI3921］［NGKI3930］*/
	}
	else if (check_intno_cfg(intno) && check_intno_clear(intno)) {
		clear_int(intno);						/*［NGKI3924］*/
		ercd = E_OK;
	}
	else {
		ercd = E_OBJ;							/*［NGKI3923］［NGKI3929］*/
	}
	if (!locked) {
		unlock_cpu();
	}

	LOG_CLR_INT_LEAVE(ercd);
	return(ercd);
}

#endif /* TOPPERS_SUPPORT_CLR_INT */
#endif /* TOPPERS_clr_int */

/*
 *  割込みの要求［NGKI3932］
 */
#ifdef TOPPERS_ras_int
#ifdef TOPPERS_SUPPORT_RAS_INT					/*［NGKI3939］*/

ER
ras_int(INTNO intno)
{
	bool_t	locked;
	ER		ercd;

	LOG_RAS_INT_ENTER(intno);

	locked = sense_lock();
	if (!locked) {
		lock_cpu();
	}
	if (!VALID_INTNO_RASINT(get_my_pcb()->prcid, intno)) {
		ercd = E_PAR;							/*［NGKI3933］［NGKI3942］*/
	}
	else if (check_intno_cfg(intno) && check_intno_raise(intno)) {
		raise_int(intno);						/*［NGKI3936］*/
		ercd = E_OK;
	}
	else {
		ercd = E_OBJ;							/*［NGKI3935］［NGKI3941］*/
	}
	if (!locked) {
		unlock_cpu();
	}

	LOG_RAS_INT_LEAVE(ercd);
	return(ercd);
}

#endif /* TOPPERS_SUPPORT_RAS_INT */
#endif /* TOPPERS_ras_int */

/*
 *  割込み要求のチェック［NGKI3944］
 */
#ifdef TOPPERS_prb_int
#ifdef TOPPERS_SUPPORT_PRB_INT					/*［NGKI3951］*/

ER_BOOL
prb_int(INTNO intno)
{
	bool_t	locked;
	ER_BOOL	ercd;

	LOG_PRB_INT_ENTER(intno);

	locked = sense_lock();
	if (!locked) {
		lock_cpu();
	}
	if (!VALID_INTNO_PRBINT(get_my_pcb()->prcid, intno)) {
		ercd = E_PAR;							/*［NGKI3945］［NGKI3952］*/
	}
	else if (check_intno_cfg(intno)) {
		ercd = (ER_BOOL) probe_int(intno);		/*［NGKI5214］［NGKI5215］*/
	}
	else {
		ercd = E_OBJ;							/*［NGKI3947］*/
	}
	if (!locked) {
		unlock_cpu();
	}

	LOG_PRB_INT_LEAVE(ercd);
	return(ercd);
}

#endif /* TOPPERS_SUPPORT_PRB_INT */
#endif /* TOPPERS_prb_int */

/*
 *  割込み優先度マスクの変更［NGKI3107］
 */
#ifdef TOPPERS_chg_ipm

ER
chg_ipm(PRI intpri)
{
	ER		ercd;
	TCB		*p_selftsk;
	PCB		*p_my_pcb;

	LOG_CHG_IPM_ENTER(intpri);
	CHECK_TSKCTX_UNL_MYSTATE(&p_selftsk);		/*［NGKI3108］［NGKI3109］*/
	CHECK_PAR(VALID_INTPRI_CHGIPM(intpri));		/*［NGKI3113］［NGKI3114］*/

	lock_cpu();
  retry:
	acquire_glock();
	p_my_pcb = get_my_pcb();
	if (p_selftsk != p_my_pcb->p_schedtsk) {
		release_glock();
		dispatch();
		goto retry;
	}
	if (intpri == TIPM_ENAALL && p_my_pcb->enadsp) {
		/* set_dspflgは，割込み優先度マスクをTIPM_ENAALLにする．*/
		set_dspflg(p_my_pcb);
		if (p_selftsk->raster && p_selftsk->enater) {
			if (task_terminate(p_my_pcb, p_selftsk)) {
				exit_and_migrate(p_my_pcb, p_selftsk);
			}
			else {
				release_glock();
				exit_and_dispatch();
			}
			ercd = E_SYS;
			goto unlock_and_exit;
		}
		else {
			if (p_selftsk != p_my_pcb->p_schedtsk) {
				release_glock();
				dispatch();
				ercd = E_OK;
				goto unlock_and_exit;
			}
		}
		ercd = E_OK;
	}
	else {
		t_set_ipm(intpri);						/*［NGKI3111］*/
		p_my_pcb->dspflg = false;
		ercd = E_OK;
	}
	release_glock();
  unlock_and_exit:
	unlock_cpu();

  error_exit:
	LOG_CHG_IPM_LEAVE(ercd);
	return(ercd);
}

#endif /* TOPPERS_chg_ipm */

/*
 *  割込み優先度マスクの参照［NGKI3115］
 */
#ifdef TOPPERS_get_ipm

ER
get_ipm(PRI *p_intpri)
{
	ER		ercd;

	LOG_GET_IPM_ENTER(p_intpri);
	CHECK_TSKCTX_UNL();							/*［NGKI3116］［NGKI3117］*/

	lock_cpu();
	*p_intpri = t_get_ipm();					/*［NGKI3120］*/
	ercd = E_OK;
	unlock_cpu();

  error_exit:
	LOG_GET_IPM_LEAVE(ercd, p_intpri);
	return(ercd);
}

#endif /* TOPPERS_get_ipm */
