/*
 *  TOPPERS Software
 *      Toyohashi Open Platform for Embedded Real-Time Systems
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
 *		動的生成API（acre_isr/del_isr）のテスト
 *
 * 【テストの目的】
 *
 *	(A) ENA_DYNISR された intno のディスパッチがキュー方式になり，静的 ISR
 *	    だけのときの呼出し順序（isrpri 昇順・同一 isrpri は記述順）が
 *	    インライン連鎖のときと変わらないこと（ISR段階の訂正I の invariant）．
 *	(B) 静的 ISR と動的 ISR が isrpri 順に混在して呼ばれること．
 *	(C) 同一 isrpri の動的 ISR が acre した順（isrseq 昇順）に呼ばれること．
 *	(D) ★走査中に別コアが del_isr / acre_isr を行っても，取りこぼしも
 *	    二重実行も起きないこと（Codex 指摘 #1 のシナリオそのもの）．
 *	(E) ★del_isr が，他コアで実行中の ISR 本体の完了を待って戻ること
 *	    （quiesce．Codex 指摘 #2）．
 *	(F) エラー：E_OBJ（未 ENA_DYNISR の intno／範囲外の intno／静的 ISR の削除）・
 *	    E_PAR（isrpri 範囲外）・E_RSATR・E_ID・E_NOID・E_NOEXS・E_CTX．
 *	(G) 削除後は当該 ISR が呼ばれなくなること．
 *
 * 【この構成でしか実証できないこと／できないこと】
 *
 *	musca_b1 は割込み番号にプロセッサIDを符号化する（(prcid << 16) | irq）
 *	ため，同一の割込み番号を複数コアが受け付ける構成が作れない
 *	（affinity が2コアのクラスに CFG_INT を書くと E_RSATR）．したがって
 *	「同一キューを2コアが同時に走査する」ことは本テストでは実証できない．
 *	実証できるのは「PRC2 で走っている ISR を PRC1 のタスクが del する」
 *	（＝quiesce の本質）までである．PLIC/GIC のグローバル割込みを持つ
 *	ターゲット（polarfire_soc・zynq 系）では前者も到達可能である
 *	（ISR段階の訂正H）．
 *
 * 【ISR の中で syslog を伴う API を呼ばない理由】
 *
 *	本テストの ISR は別コアのタスクと同時に走る．ISR の中で check_point を
 *	呼ぶと，出力の順序がコア間で非決定的になり，行数の期待値が立たない．
 *	そのため ISR は volatile なグローバルへ記録するだけにし，判定はすべて
 *	タスクで行う（test_int2 は単一コアなので ISR 内 check_point でよい）．
 *
 * 【使用リソース】
 *
 *	TASK1: 中優先度タスク，TA_ACT属性（静的・PRC1．主体）
 *	TASK3: 高優先度タスク，TA_ACT属性（静的・PRC2．手先）
 *	INTNO1（PRC1）: 静的 ISR_S4(isrpri 4)・ISR_S2(isrpri 2) ＋ ENA_DYNISR
 *	INTNO2（PRC2）: 静的 ISR なし ＋ ENA_DYNISR
 *	AID_ISR(4): 動的スロット4個
 *
 * 【チェックポイント】
 *
 *	PRC1（check_count[0]，TASK1）: 1..9 + check_finish(10)
 *	PRC2（check_count[1]，TASK3）: 1,2（出力は "Check point 2-1/2-2 passed."）
 *	  ＝ログ中の "Check point" 行は合計 12 本（check_finish 自身の1本を含む）
 *	  ★この本数は実測で確かめ，違っていたら実測値を正とする．
 *
 * 【実装メモ】
 *
 *	EXINF は include/t_stddef.h で `typedef intptr_t EXINF;` であるため，
 *	既に intptr_t である値を intptr_t へ再キャストするのは冗長．
 *	`(char) exinf` の単一キャストで警告なく縮小変換できる（-Wall で確認済み，
 *	-Wconversion は本ビルドで使っていない）．
 */

#include <kernel.h>
#include <t_syslog.h>
#include "syssvc/test_svc.h"
#include "kernel_cfg.h"
#include "test_dcre5.h"

/* DO NOT DELETE THIS LINE -- gentest depends on it. */

/*
 *  ISR の呼出し順序のログ
 */
static volatile uint_t	isr_log_cnt;
static volatile char	isr_log[ISR_LOG_SIZE];

/*
 *  走査中の del/acre（手順4）のハンドシェイク
 */
static volatile bool_t	hs_enable;		/*  ISR が待ち合わせるか  */
static volatile bool_t	hs_in_isr;		/*  ISR が走査の途中にいる  */
static volatile bool_t	hs_done;		/*  PRC2 側の del/acre が完了した  */
static volatile bool_t	hs_isr_timeout;	/*  ISR の待ちが上限に達した  */

/*
 *  quiesce の実証（手順5）
 */
static volatile bool_t	long_started;
static volatile bool_t	long_finished;
static volatile uint32_t spin_sink;

/*
 *  ISR 文脈からのサービスコール（手順7）
 */
static volatile ER		ctx_acre_ercd;
static volatile ER		ctx_del_ercd;

/*
 *  PRC2 のタスクへの指令
 */
static volatile uint_t	prc2_cmd;
static volatile ID		hs_del_isrid;	/*  走査中に削除する ISRID  */
static volatile ER_ID	hs_acre_erid;	/*  走査中に生成した ISRID（結果）  */
static volatile bool_t	prc2_quit;

/*
 *  TASK1 の test_start() 完了合図（実測で判明した起動レースの回避）
 *
 *  test_start() は check_count[] を全プロセッサ分ゼロクリアする．
 *  TASK3（PRC2）が独立カウンタ check_count[1] へ check_point_prc(1, 2) を
 *  打つタイミングが，TASK1（PRC1）の test_start() 呼び出しより先行すると，
 *  その直後に test_start() がカウンタを黙って 0 に巻き戻し，のちの
 *  check_point_prc(2, 2) が「Unexpected check point 2-2」で失敗する
 *  （実測で発生を確認．2コアが同時に起動レースするため非決定的）．
 *  TASK1 が test_start() を終えたことを合図する volatile フラグを設け，
 *  TASK3 はそれを待ってから最初の check_point_prc を打つ．
 */
static volatile bool_t	task1_ready;

/*
 *  ログの記録（ISR からのみ呼ばれる）
 */
static void
isr_log_put(char c)
{
	if (isr_log_cnt < ISR_LOG_SIZE) {
		isr_log[isr_log_cnt] = c;
		isr_log_cnt += 1U;
	}
}

/*
 *  ログの比較（タスクからのみ呼ばれる）
 */
static bool_t
isr_log_is(const char *expected)
{
	uint_t	i;

	for (i = 0U; expected[i] != '\0'; i++) {
		if (i >= isr_log_cnt || isr_log[i] != expected[i]) {
			return(false);
		}
	}
	return(isr_log_cnt == i);
}

/*
 *  静的 ISR（exinf がログに記録する文字）
 */
void
static_isr(EXINF exinf)
{
	intno1_clear();
	isr_log_put((char) exinf);
}

/*
 *  動的 ISR（exinf がログに記録する文字）
 *
 *  hs_enable が真のとき，'A' の ISR だけが「走査の途中」で待ち合わせる．
 */
void
dyn_isr(EXINF exinf)
{
	uint32_t	i;
	char		c = (char) exinf;

	intno1_clear();
	isr_log_put(c);

	if (hs_enable && c == 'A') {
		hs_in_isr = true;
		for (i = 0U; i < SPIN_LIMIT; i++) {
			if (hs_done) {
				break;
			}
		}
		if (!hs_done) {
			hs_isr_timeout = true;
		}
	}
}

/*
 *  quiesce 実証用の長い ISR（PRC2 で走る）
 */
void
long_isr(EXINF exinf)
{
	uint32_t	i;

	intno2_clear();
	long_started = true;
	for (i = 0U; i < LONG_ISR_SPIN; i++) {
		spin_sink = spin_sink + 1U;
	}
	long_finished = true;
	isr_log_put('L');
}

/*
 *  ISR 文脈からサービスコールを呼ぶ ISR（E_CTX の検査）
 */
void
ctx_isr(EXINF exinf)
{
	T_CISR	cisr;

	intno1_clear();
	isr_log_put('X');

	cisr.isratr = TA_NULL;
	cisr.exinf = (EXINF) 'Z';
	cisr.intno = INTNO1;
	cisr.isr = dyn_isr;
	cisr.isrpri = 8;
	ctx_acre_ercd = (ER) acre_isr(&cisr);
	ctx_del_ercd = del_isr(ISR_S2);
}

/*
 *  PRC2 側の手先
 */
void
task3(EXINF exinf)
{
	uint32_t	i;
	T_CISR		cisr;

	/*
	 *  TASK1 の test_start() が check_count[] をゼロクリアし終えるのを
	 *  待ってから，PRC2 側の最初の check_point_prc を打つ（起動レース回避）．
	 */
	for (i = 0U; i < SPIN_LIMIT && !task1_ready; i++) {
	}
	check_assert(task1_ready);

	check_point_prc(1, 2);

	while (!prc2_quit) {
		if (prc2_cmd == CMD_HANDSHAKE) {
			/*
			 *  PRC1 の走査が 'A' の ISR に入るのを待ってから，
			 *  同一 isrpri の 'B' を削除し，'D' を生成する．
			 */
			for (i = 0U; i < SPIN_LIMIT && !hs_in_isr; i++) {
			}
			(void) del_isr(hs_del_isrid);
			cisr.isratr = TA_NULL;
			cisr.exinf = (EXINF) 'D';
			cisr.intno = INTNO1;
			cisr.isr = dyn_isr;
			cisr.isrpri = 3;
			hs_acre_erid = acre_isr(&cisr);
			hs_done = true;
			prc2_cmd = CMD_NONE;
		}
		else if (prc2_cmd == CMD_FIRE_LONG) {
			(void) ras_int(INTNO2);
			prc2_cmd = CMD_NONE;
		}
		else {
			(void) dly_tsk(1U);
		}
	}

	check_point_prc(2, 2);
	ext_tsk();
}

void
task1(EXINF exinf)
{
	T_CISR		cisr;
	ER_ID		erid;
	ID			id_a, id_b, id_c, id_l, id_x;
	uint32_t	i;

	test_start(__FILE__);
	task1_ready = true;	/*  TASK3 の起動レース回避（実測で発覚，上のコメント参照）  */
	check_point(1);

	cisr.isratr = TA_NULL;
	cisr.intno = INTNO1;
	cisr.isr = dyn_isr;

	/*
	 *  1) 静的 ISR だけの呼出し順序（訂正I の invariant）
	 *
	 *  ENA_DYNISR された intno でも，静的 ISR は isrpri 昇順に呼ばれる．
	 *  記述順は S4→S2 だが，呼出しは S2（isrpri 2）→S4（isrpri 4）である．
	 */
	isr_log_cnt = 0U;
	check_ercd(ras_int(INTNO1), E_OK);
	check_assert(isr_log_is("24"));
	check_point(2);

	/*
	 *  2) 静的 ISR と動的 ISR の isrpri 順の混在
	 */
	cisr.exinf = (EXINF) 'a';	cisr.isrpri = 1;
	erid = acre_isr(&cisr);
	check_assert(erid > ISR_S2);	/*  2レンジ ISRID の直接検証  */
	check_assert(erid > ISR_S4);
	check_assert(erid > ISR_SIO);
	id_a = (ID) erid;

	cisr.exinf = (EXINF) 'b';	cisr.isrpri = 3;
	erid = acre_isr(&cisr);
	check_assert(erid > ISR_S4);
	id_b = (ID) erid;

	cisr.exinf = (EXINF) 'c';	cisr.isrpri = 5;
	erid = acre_isr(&cisr);
	check_assert(erid > ISR_S4);
	id_c = (ID) erid;

	isr_log_cnt = 0U;
	check_ercd(ras_int(INTNO1), E_OK);
	check_assert(isr_log_is("a2b4c"));

	check_ercd(del_isr(id_a), E_OK);
	check_ercd(del_isr(id_b), E_OK);
	check_ercd(del_isr(id_c), E_OK);
	check_point(3);

	/*
	 *  3) 同一 isrpri の動的 ISR は acre した順（isrseq 昇順）に呼ばれる
	 *
	 *  ★走査キーが isrpri だけだと，'A' を呼んだ後に「isrpri > 3」の
	 *    要素（'4'）へ飛んでしまい 'B'/'C' を取りこぼす．本手順は
	 *    ISR_KEY_GT の第2キー（isrseq）が生きていることの直接検証である．
	 */
	cisr.isrpri = 3;
	cisr.exinf = (EXINF) 'A';	erid = acre_isr(&cisr);	check_assert(erid > ISR_S4);
	id_a = (ID) erid;
	cisr.exinf = (EXINF) 'B';	erid = acre_isr(&cisr);	check_assert(erid > ISR_S4);
	id_b = (ID) erid;
	cisr.exinf = (EXINF) 'C';	erid = acre_isr(&cisr);	check_assert(erid > ISR_S4);
	id_c = (ID) erid;

	isr_log_cnt = 0U;
	check_ercd(ras_int(INTNO1), E_OK);
	check_assert(isr_log_is("2ABC4"));
	check_point(4);

	/*
	 *  4) ★走査中の del/acre（Codex 指摘 #1 のシナリオ）
	 *
	 *  PRC1 の走査が 'A'（isrpri 3）の中で待ち合わせている間に，PRC2 の
	 *  TASK3 が 'B' を削除し 'D'（isrpri 3・新しい isrseq）を生成する．
	 *  走査は 'A' の (3, seqA) より大きい最小の要素から再開するので，
	 *    ・'B' は削除済みなので呼ばれない
	 *    ・'C'（3, seqC > seqA）は取りこぼされない
	 *    ・'D'（3, seqD > seqC）は呼ばれる（spec §5 の例と同じ）
	 *    ・'A' は二重に呼ばれない
	 *  結果のログは "2ACD4" になる．
	 */
	hs_in_isr = false;
	hs_done = false;
	hs_isr_timeout = false;
	hs_del_isrid = id_b;
	hs_acre_erid = 0;
	hs_enable = true;
	prc2_cmd = CMD_HANDSHAKE;

	isr_log_cnt = 0U;
	check_ercd(ras_int(INTNO1), E_OK);

	/*  TASK3 の acre_isr の結果が確定するのを待つ  */
	for (i = 0U; i < SPIN_LIMIT && !hs_done; i++) {
	}
	hs_enable = false;
	check_assert(!hs_isr_timeout);
	check_assert(hs_done);
	check_assert(hs_acre_erid > ISR_S4);
	check_assert(isr_log_is("2ACD4"));

	check_ercd(del_isr(id_a), E_OK);
	check_ercd(del_isr(id_c), E_OK);
	check_ercd(del_isr((ID) hs_acre_erid), E_OK);
	check_ercd(del_isr(id_b), E_NOEXS);	/*  TASK3 が削除済み  */
	check_point(5);

	/*
	 *  5) ★quiesce の実証（Codex 指摘 #2）
	 *
	 *  PRC2 で long_isr が空回ししている最中に PRC1 から del_isr を呼ぶ．
	 *  del_isr は running が 0 になるまで戻らないので，戻った時点で
	 *  long_finished は必ず真である．quiesce が無ければ偽になる．
	 */
	cisr.intno = INTNO2;
	cisr.isr = long_isr;
	cisr.exinf = (EXINF) 'L';
	cisr.isrpri = 1;
	erid = acre_isr(&cisr);
	check_assert(erid > ISR_S4);
	id_l = (ID) erid;

	isr_log_cnt = 0U;
	long_started = false;
	long_finished = false;
	prc2_cmd = CMD_FIRE_LONG;

	for (i = 0U; i < SPIN_LIMIT && !long_started; i++) {
	}
	check_assert(long_started);
	check_ercd(del_isr(id_l), E_OK);
	check_assert(long_finished);		/*  ★quiesce の証拠  */
	check_assert(isr_log_is("L"));

	cisr.intno = INTNO1;
	cisr.isr = dyn_isr;
	check_point(6);

	/*
	 *  6) エラー
	 */
	cisr.exinf = (EXINF) 'z';
	cisr.isrpri = 1;

	cisr.intno = INTNO_UNOPTED;
	check_assert(acre_isr(&cisr) == E_OBJ);		/*  ENA_DYNISR されていない  */
	cisr.intno = INTNO_BAD;
	check_assert(acre_isr(&cisr) == E_OBJ);		/*  範囲外も E_OBJ（訂正A）  */
	cisr.intno = INTNO1;

	cisr.isrpri = TMIN_ISRPRI - 1;
	check_assert(acre_isr(&cisr) == E_PAR);
	cisr.isrpri = TMAX_ISRPRI + 1;
	check_assert(acre_isr(&cisr) == E_PAR);
	cisr.isrpri = 1;

	cisr.isratr = TA_NULL | 0x01U;
	check_assert(acre_isr(&cisr) == E_RSATR);
	cisr.isratr = TA_NULL;

	check_ercd(del_isr(0), E_ID);
	check_ercd(del_isr(TNUM_ISRID + 1), E_ID);
	check_ercd(del_isr(ISR_S2), E_OBJ);			/*  静的生成オブジェクト  */
	check_ercd(del_isr(ISR_SIO), E_OBJ);

	/*  スロット4個を使い切る → E_NOID  */
	cisr.exinf = (EXINF) 'p';	erid = acre_isr(&cisr);	check_assert(erid > ISR_S4);
	id_a = (ID) erid;
	cisr.exinf = (EXINF) 'q';	erid = acre_isr(&cisr);	check_assert(erid > ISR_S4);
	id_b = (ID) erid;
	cisr.exinf = (EXINF) 'r';	erid = acre_isr(&cisr);	check_assert(erid > ISR_S4);
	id_c = (ID) erid;
	cisr.exinf = (EXINF) 's';	erid = acre_isr(&cisr);	check_assert(erid > ISR_S4);
	id_x = (ID) erid;
	check_assert(acre_isr(&cisr) == E_NOID);

	/*  空きが1個だけの状態で del → 再 acre（FIFO/LIFO 不問で決定的）  */
	check_ercd(del_isr(id_x), E_OK);
	cisr.exinf = (EXINF) 't';
	erid = acre_isr(&cisr);
	check_assert(erid == id_x);
	check_ercd(del_isr(id_x), E_OK);
	check_ercd(del_isr(id_x), E_NOEXS);
	check_ercd(del_isr(id_a), E_OK);
	check_ercd(del_isr(id_b), E_OK);
	check_ercd(del_isr(id_c), E_OK);
	check_point(7);

	/*
	 *  7) ISR 文脈からのサービスコールは E_CTX
	 */
	cisr.exinf = (EXINF) 'X';
	cisr.isr = ctx_isr;
	cisr.isrpri = 1;
	erid = acre_isr(&cisr);
	check_assert(erid > ISR_S4);
	id_x = (ID) erid;

	ctx_acre_ercd = E_OK;
	ctx_del_ercd = E_OK;
	isr_log_cnt = 0U;
	check_ercd(ras_int(INTNO1), E_OK);
	check_assert(isr_log_is("X24"));
	check_ercd(ctx_acre_ercd, E_CTX);
	check_ercd(ctx_del_ercd, E_CTX);
	check_ercd(del_isr(id_x), E_OK);
	cisr.isr = dyn_isr;
	check_point(8);

	/*
	 *  8) 削除後は呼ばれない（静的 ISR だけに戻る）
	 */
	isr_log_cnt = 0U;
	check_ercd(ras_int(INTNO1), E_OK);
	check_assert(isr_log_is("24"));
	check_point(9);

	prc2_quit = true;
	check_finish(10);
}
