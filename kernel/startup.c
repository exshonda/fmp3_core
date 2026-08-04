/*
 *  TOPPERS/FMP Kernel
 *      Toyohashi Open Platform for Embedded Real-Time Systems/
 *      Flexible MultiProcessor Kernel
 * 
 *  Copyright (C) 2000-2003 by Embedded and Real-Time Systems Laboratory
 *                              Toyohashi Univ. of Technology, JAPAN
 *  Copyright (C) 2005-2025 by Embedded and Real-Time Systems Laboratory
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
 *  $Id: startup.c 464 2026-05-27 11:42:34Z ertl-honda $
 */

/*
 *		カーネルの初期化と終了処理
 */

#include "kernel_impl.h"
#include "time_event.h"
#include "spin_lock.h"
#include "target_ipi.h"
#include <sil.h>

/*
 *  トレースログマクロのデフォルト定義
 */
#ifndef LOG_KER_ENTER
#define LOG_KER_ENTER()
#endif /* LOG_KER_ENTER */

#ifndef LOG_KER_LEAVE
#define LOG_KER_LEAVE()
#endif /* LOG_KER_LEAVE */

#ifndef LOG_EXT_KER_ENTER
#define LOG_EXT_KER_ENTER()
#endif /* LOG_EXT_KER_ENTER */

#ifndef LOG_EXT_KER_LEAVE
#define LOG_EXT_KER_LEAVE(ercd)
#endif /* LOG_EXT_KER_LEAVE */

#ifdef TOPPERS_barsync
#ifndef OMIT_BARRIER_SYNC

/*
 *  バリア同期用変数
 */
static volatile uint_t	prc_phase[TNUM_PRCID];
static volatile uint_t	sys_phase;

/*
 *  カーネルの起動／終了に用いるバリア同期
 */
void
barrier_sync(uint_t phase)
{
	uint_t	prcidx;
	uint_t	count;
	uint_t	i;

	prcidx = get_my_prcidx();
	prc_phase[prcidx] = phase;

	if (ID_PRC(prcidx) == TOPPERS_MASTER_PRCID) {
		/* マスタプロセッサの処理 */
		do {
			sil_dly_nse(100);
			count = 0;
			for (i = 0; i < TNUM_PRCID; i++) {
				if (prc_phase[i] == phase) {
					count++;
				}
			}
		} while (count < TNUM_PRCID);
		sys_phase = phase;
	}
	else {
		/* 他のプロセッサの処理 */
		while (sys_phase != phase) {
			sil_dly_nse(100);
		}
	}
}

#endif /* OMIT_BARRIER_SYNC */
#endif /* TOPPERS_barsync */

#ifdef TOPPERS_sta_ker

/*
 *  カーネルメモリプール領域有効フラグ
 */
bool_t	mpk_valid;

/*
 *  初期化ルーチンの呼び出し
 */
static void
call_inirtn(const INIRTNBB *p_inirtnbb)
{
	uint_t			i;
	const INIRTNB	*inirtnb_table;

	inirtnb_table = p_inirtnbb->inirtnb_table;
	for (i = 0; i < p_inirtnbb->tnum_inirtn; i++) {
		(*(inirtnb_table[i].inirtn))(inirtnb_table[i].exinf);
	}
}

/*
 *  カーネルの起動
 */
void
sta_ker(void)
{
	uint_t	my_prcidx;
	PCB		*p_my_pcb;

	/*
	 *  TECSの初期化
	 */
#ifndef TOPPERS_OMIT_TECS
	initialize_tecs();
#endif /* TOPPERS_OMIT_TECS */

	/*
	 *  p_my_pcbとプロセッサIDの初期化
	 */
	my_prcidx = get_my_prcidx();
	p_my_pcb = p_pcb_table[my_prcidx];
	p_my_pcb->prcid = ID_PRC(my_prcidx);

	/*
	 *  取得しているスピンロックの初期化
	 */
	p_my_pcb->p_locspn = NULL;

	/*
	 *  ターゲット依存の初期開始のバリア同期
	 */    
	barrier_sync(1);

	/*
	 *  ターゲット依存の初期化
	 */
	target_initialize(p_my_pcb);

	/*
	 *  ターゲット依存の初期化待ちのバリア同期
	 */
	barrier_sync(2);

	/*
	 *  カーネルメモリプール領域の初期化（マスタプロセッサのみ）
	 */
	if (p_my_pcb->prcid == TOPPERS_MASTER_PRCID) {
		if (mpksz > 0U && mpk != NULL) {
			mpk_valid = initialize_mempool(mpk, mpksz);
		}
		else {
			mpk_valid = false;
		}
	}

	/*
	 *  各モジュールの初期化
	 *
	 *  タイムイベント管理モジュールは他のモジュールより先に初期化
	 *  する必要がある．
	 */
	initialize_tmevt(p_my_pcb);								/*［ASPD1061］*/
	initialize_object(p_my_pcb);

	/*
	 *  ジャイアントロックの初期化（マスタプロセッサのみ）
	 */
	if (p_my_pcb->prcid == TOPPERS_MASTER_PRCID) {
		initialize_glock();
	}

	/*
	 *  初期化ルーチンの実行
	 */
	barrier_sync(3);

	if (p_my_pcb->prcid == TOPPERS_MASTER_PRCID) {
		call_inirtn(&(inirtnbb_table[0]));
	}

	barrier_sync(4);

	call_inirtn(&(inirtnbb_table[p_my_pcb->prcid]));

	/*
	 *  オブジェクトの初期化待ちのバリア同期
	 */
	barrier_sync(5);

	/*
	 *  高分解能タイマの設定
	 */
	if (p_my_pcb->prcid == TOPPERS_TMASTER_PRCID) {
		current_hrtcnt = target_hrt_get_current();		/*［ASPD1063］*/
	}

	barrier_sync(6);

	if (p_my_pcb->p_tevtcb != NULL) {
		/*
		 *  acquire_glockは，CPUロック状態を解除する場合があるが，ここ
		 *  でCPUロック状態を解除してはならないため，try_glockを用いて
		 *  ジャイアントロックを取得する．
		 */
		while (try_glock());
		set_hrt_event(p_my_pcb);						/*［ASPD1064］*/
		release_glock();
	}

	/*
	 *  カーネル動作の開始
	 */
	kerflg_table[my_prcidx] = true;
	LOG_KER_ENTER();
	start_dispatch();
	assert(0);
}

#endif /* TOPPERS_sta_ker */

/*
 *  カーネルの終了
 */
#ifdef TOPPERS_ext_ker

ER
ext_ker(void)
{
	PCB		*p_my_pcb;
	ID		prcid;
	ER		ercd;
	SIL_PRE_LOC;

	LOG_EXT_KER_ENTER();

	/*
	 *  割込みロック状態に移行
	 */
	SIL_LOC_INT();

	p_my_pcb = get_my_pcb();

	/*
	 *  スピンロックを取得している場合は，スピンロックを解放する
	 */
	force_unlock_spin(p_my_pcb);

	/*
	 *  カーネル動作の終了
	 */
	LOG_KER_LEAVE();
	kerflg_table[INDEX_PRC(p_my_pcb->prcid)] = false;

	/*
	 *  他のプロセッサへ終了処理を要求する
	 */
	for (prcid = TMIN_PRCID; prcid <= TMAX_PRCID; prcid++) {
		if (prcid != p_my_pcb->prcid) {
			request_ext_ker(prcid);
		}
	}

	/*
	 *  カーネルの終了処理の呼出し
	 *
	 *  非タスクコンテキストに切り換えて，exit_kernelを呼び出す．
	 */
	call_exit_kernel(p_my_pcb);

	/*
	 *  コンパイラの警告対策（ここへ来ることはないはず）
	 */
	SIL_UNL_INT();
	ercd = E_SYS;

	LOG_EXT_KER_LEAVE(ercd);
	return(ercd);
}

/*
 *  終了処理ルーチンの呼び出し
 */
static void
call_terrtn(const TERRTNBB *p_terrtnbb)
{
	uint_t			i;
	const TERRTNB	*terrtnb_table;

	terrtnb_table = p_terrtnbb->terrtnb_table;
	for (i = 0; i < p_terrtnbb->tnum_terrtn; i++) {
		(*(terrtnb_table[i].terrtn))(terrtnb_table[i].exinf);
	}
}

/*
 *  カーネルの終了処理
 */
void
exit_kernel(PCB *p_my_pcb)
{
	/*
	 *  SILスピンロックの強制解放
	 */
	TOPPERS_sil_force_unl_spn();

	barrier_sync(5);

	/*
	 *  終了処理ルーチンの実行
	 */
	call_terrtn(&(terrtnbb_table[p_my_pcb->prcid]));

	barrier_sync(6);

	if (p_my_pcb->prcid == TOPPERS_MASTER_PRCID) {
		call_terrtn(&(terrtnbb_table[0]));
	}

	barrier_sync(7);

	/*
	 *  ターゲット依存の終了処理
	 */
	target_exit();
	assert(0);
}

#endif /* TOPPERS_ext_ker */

/*
 *  ディスパッチハンドラ
 *
 *  非タスクコンテキストで実行されるため，他のプロセッサへマイグレート
 *  することはなく，CPUロック状態にせずに自プロセッサのPCBにアクセスし
 *  てよい．
 */
#ifdef TOPPERS_dsphdr
#ifndef OMIT_DISPATCH_HANDLER

void
dispatch_handler(void)
{
	assert(sense_context(get_my_pcb()));
	assert(!sense_lock());
}

#endif /* OMIT_DISPATCH_HANDLER */
#endif /* TOPPERS_dsphdr */

/*
 *  カーネル終了ハンドラ
 *
 *  非タスクコンテキストで実行されるため，他のプロセッサへマイグレート
 *  することはなく，CPUロック状態にせずに自プロセッサのPCBにアクセスし
 *  てよい．
 */
#ifdef TOPPERS_extkerhdr

void
ext_ker_handler(void)
{
	PCB		*p_my_pcb = get_my_pcb();
	SIL_PRE_LOC;

	assert(sense_context(p_my_pcb));
	assert(!sense_lock());

	/*
	 *  割込みロック状態に移行
	 */
	SIL_LOC_INT();

	/*
	 *  スピンロックを取得している場合は，スピンロックを解放する
	 */
	force_unlock_spin(p_my_pcb);

	/*
	 *  カーネル動作の終了
	 */
	LOG_KER_LEAVE();
	kerflg_table[INDEX_PRC(p_my_pcb->prcid)] = false;

	/*
	 *  カーネルの終了処理の呼出し
	 *
	 *  非タスクコンテキストに切り換えて，exit_kernelを呼び出す．
	 */
	call_exit_kernel(p_my_pcb);

	/*
	 *  コンパイラの警告対策（ここへ来ることはないはず）
	 */
	SIL_UNL_INT();
}

#endif /* TOPPERS_extkerhdr */

/*
 *  デフォルトのメモリプール管理機能
 *
 *  メモリプール領域の先頭から順に割り当てを行い，すべてのメモリ領域が
 *  解放されるまで解放されたメモリ領域を再利用しないメモリプール管理機
 *  能．
 */
#ifdef TOPPERS_kermem
#ifndef OMIT_MEMPOOL_DEFAULT

typedef struct {
	void	*brk;		/* メモリプール領域の未使用領域の先頭番地 */
	void	*limit;		/* メモリプール領域の上限 */
	uint_t	count;		/* 割り当てたメモリ領域の数 */
} MEMPOOLCB;

bool_t
initialize_mempool(MB_T *mempool, size_t size)
{
	MEMPOOLCB	*p_mempoolcb = ((MEMPOOLCB *) mempool);

	if (size >= sizeof(MEMPOOLCB)) {
		p_mempoolcb->brk = ((char *) mempool) + sizeof(MEMPOOLCB);
		p_mempoolcb->limit = ((char *) mempool) + size;
		p_mempoolcb->count = 0;
		return(true);
	}
	else {
		return(false);
	}
}

/*
 *  メモリプール領域からの割当て（malloc_mempool / aligned_alloc_mempool の共通部）
 *
 *  ★dcreからの意図的な逸脱：符号混在比較とあふれの安全化（上流報告候補c）
 *
 *  dcre（extension/dcre/kernel/startup.c）の原形は
 *
 *      brk = align_pointer(p_mempoolcb->brk, alignment);
 *      if (((char *)(p_mempoolcb->limit)) - ((char *) brk) >= size) { … }
 *
 *  であり，2つの欠陥がある．
 *
 *  (1) 左辺のポインタ差はptrdiff_t（符号つき），右辺のsizeはsize_t（符号
 *      なし）である．通常の算術変換により左辺が符号なしへ変換されるので，
 *      アラインメント調整でbrkがlimitを越えた場合（差が負）に，その負の値が
 *      巨大な符号なし値になり，比較が真＝「入る」と誤判定する．その結果，
 *      メモリプール領域の外側の番地を成功として返す．
 *  (2) align_pointerの加算 ((uintptr_t) ptr + alignment - 1) 自体があふれ
 *      うる．あふれるとbrkが小さな値へ巻き戻り，(1)と同じ誤判定を招く．
 *
 *  そこで本実装は，すべての算術を符号なし（uintptr_t）で行い，
 *
 *      ・調整の加算があふれないこと
 *      ・調整後のbrkがlimitを越えていないこと
 *
 *  をこの順に確かめてから初めて残量（limit - aligned）を計算する．負の差が
 *  生じうる箇所が式の上に存在しないため，(1)の誤判定は構造的に起こらない．
 *
 *  ★この修正だけでは穴は塞がらない．呼出し側（acre_dtq/acre_pdq/acre_mpf/
 *  acre_tsk）が渡すsize自体が乗算・丸めであふれていると，「小さくなったsize」
 *  は正当に「入る」と判定されて成功してしまう．両者は同じ穴の両端であり，
 *  一体で修正している（各acre_*のCHECK_PARを参照）．
 *
 *  alignmentは2の巾乗であること（呼出し側の責任．alignof(MB_T)ないし
 *  aligned_alloc_mpkの引数）．
 */
Inline void *
alloc_mempool(MEMPOOLCB *p_mempoolcb, size_t alignment, size_t size)
{
	uintptr_t	brk = ((uintptr_t)(p_mempoolcb->brk));
	uintptr_t	limit = ((uintptr_t)(p_mempoolcb->limit));
	uintptr_t	adjust = (((uintptr_t) alignment) - 1U);
	uintptr_t	aligned;

	/*
	 *  アラインメント調整の加算があふれる場合は割り当てられない．
	 */
	if (brk > ((~((uintptr_t) 0U)) - adjust)) {
		return(NULL);
	}
	aligned = ((brk + adjust) & ~adjust);

	/*
	 *  調整の結果が上限を越えた場合は割り当てられない．
	 *  ★この検査を済ませるまで limit - aligned を計算してはならない．
	 */
	if (aligned > limit) {
		return(NULL);
	}
	if (((uintptr_t) size) > (limit - aligned)) {
		return(NULL);
	}

	p_mempoolcb->brk = ((void *)(aligned + ((uintptr_t) size)));
	p_mempoolcb->count += 1;
	return(((void *) aligned));
}

void *
malloc_mempool(MB_T *mempool, size_t size)
{
	return(alloc_mempool(((MEMPOOLCB *) mempool), alignof(MB_T), size));
}

void *
aligned_alloc_mempool(MB_T *mempool, size_t alignment, size_t size)
{
	return(alloc_mempool(((MEMPOOLCB *) mempool), alignment, size));
}

void
free_mempool(MB_T *mempool, void *ptr)
{
	MEMPOOLCB	*p_mempoolcb = ((MEMPOOLCB *) mempool);

	p_mempoolcb->count -= 1;
	if (p_mempoolcb->count == 0) {
		p_mempoolcb->brk = ((char *) mempool) + sizeof(MEMPOOLCB);
	}
}

#endif /* OMIT_MEMPOOL_DEFAULT */
#endif /* TOPPERS_kermem */
