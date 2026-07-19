/*
 *  ターゲット依存部モジュール（Musca-B1 / SSE-200 dual Cortex-M33 用）
 *
 *  カーネルのチップ依存部のインクルードファイル．本チップ（QEMU musca-b1）
 *  は SSE-200 を中核とする 2 コア（dual Cortex-M33）構成である．
 *
 *  TNUM_PRCID == 1 のときは単一プロセッサとして扱い，マルチプロセッサ機構
 *  （IPI・スピンロック・ジャイアントロック）はユニプロセッサ用ダミーで構成
 *  する．TNUM_PRCID >= 2 のときは二次コアを起動し，ソフトウェア（LDREX/
 *  STREX 排他モニタ）によるスピンロックを用いる．
 *
 *  SSE-200 には自コア番号を読めるレジスタ（M-profile に MPIDR 相当は無い）も，
 *  ハードウェアスピンロック機構も存在しないため，自コア番号はブートハンド
 *  シェイク（_kernel_mp_boot_flag）で確定し，確定後は割込みスタック（istack）
 *  の範囲から導出する．
 */

#ifndef TOPPERS_CHIP_KERNEL_IMPL_H
#define TOPPERS_CHIP_KERNEL_IMPL_H

#ifndef TOPPERS_MACRO_ONLY

/*
 * 自プロセッサのインデックス（0オリジン）の取得
 *
 * TNUM_PRCID == 1 のときは常に 0．
 *
 * TNUM_PRCID >= 2 のときは，現在のスタックポインタ（MSP）が，どのコアの割込
 * みスタック（_kernel_istack_prcN）の範囲に入っているかで自コア番号を判定す
 * る．_kernel_start でブートハンドシェイクにより各コアが自分用の istack を
 * 設定済みであり，例外・割込みハンドラも MSP（=istack）上で動作するため，
 * この方法は呼出し回数に依らず（冪等に）正しい値を返す．
 */
#if TNUM_PRCID >= 2

/*
 *  自コア番号判定の実体（chip_kernel_impl.c で定義）
 *
 *  現在の MSP（メインスタックポインタ）が，どのコアの割込みスタック
 *  （_kernel_istack_prcN）の範囲に入っているかで判定する．MSP は各コアの
 *  起動時に istkpt_table[idx] へ設定され，タスク実行中（PSP 使用時）も保持
 *  されるため，例外・タスクのいずれの文脈から呼んでも正しい値を返す．
 */
extern uint_t _kernel_chip_get_my_prcidx(void);

Inline uint_t
get_my_prcidx(void)
{
	return _kernel_chip_get_my_prcidx();
}

#else /* TNUM_PRCID >= 2 */

Inline uint_t
get_my_prcidx(void)
{
	return 0U;
}

#endif /* TNUM_PRCID >= 2 */

#endif /* TOPPERS_MACRO_ONLY */

/*
 *  コア依存モジュール（ARM-M用）
 *  core_kernel_impl.h が get_my_prcidx() に依存しているので，ここで読み込む．
 */
#include <core_kernel_impl.h>

#ifndef TOPPERS_MACRO_ONLY

#if TNUM_PRCID >= 2

/*
 *  ロックに関する定義と操作（SSE-200 / ソフトウェアスピンロック）
 *
 *  SSE-200 にはハードウェアスピンロック機構が無いため，LDREX/STREX による
 *  排他モニタを用いてソフトウェアスピンロックを実装する．LOCK 変数は
 *  0:未取得，1:取得済 を表す．
 */
typedef volatile uint32_t LOCK;

Inline void
initialize_lock(LOCK *p_lock)
{
	*p_lock = 0U;
	Asm("dmb":::"memory");
}

Inline void
acquire_lock(LOCK *p_lock)
{
	uint32_t	status;
	uint32_t	locked;

	do {
		/* 0（未取得）になるまでスピン */
		do {
			Asm("ldrex %0, [%1]" : "=r"(locked) : "r"(p_lock));
		} while (locked != 0U);
		/* 1 を書き込んで取得を試みる（status==0 で成功） */
		Asm("strex %0, %2, [%1]"
			: "=&r"(status) : "r"(p_lock), "r"(1U) : "memory");
	} while (status != 0U);
	Asm("dmb":::"memory");
}

Inline bool_t
try_lock(LOCK *p_lock)
{
	uint32_t	status;
	uint32_t	locked;

	/*
	 *  返り値の意味は「既に取得されていた（失敗）」が true．
	 */
	Asm("ldrex %0, [%1]" : "=r"(locked) : "r"(p_lock));
	if (locked != 0U) {
		Asm("clrex":::"memory");
		return true;
	}
	Asm("strex %0, %2, [%1]"
		: "=&r"(status) : "r"(p_lock), "r"(1U) : "memory");
	if (status != 0U) {
		/* STREX 失敗＝他コアが取得した */
		return true;
	}
	Asm("dmb":::"memory");
	return false;
}

Inline void
release_lock(LOCK *p_lock)
{
	Asm("dmb":::"memory");
	*p_lock = 0U;
	Asm("dsb":::"memory");
}

Inline bool_t
refer_lock(LOCK *p_lock)
{
	return (*p_lock != 0U);
}

/*
 *  ジャイアントロックに関する定義と操作
 */
extern LOCK giant_lock;

#define initialize_glock()	initialize_lock(&giant_lock)
#define acquire_glock()		acquire_lock(&giant_lock)
#define try_glock()			try_lock(&giant_lock)
#define release_glock()		release_lock(&giant_lock)

/*
 *  ネイティブスピンロックに関する定義と操作
 *
 *  spninib の lock メンバ（LOCK*）が指す領域をソフトウェアスピンロックとして
 *  用いる．
 */
#ifndef TMAX_NATIVE_SPN
#define TMAX_NATIVE_SPN 16
#endif /* TMAX_NATIVE_SPN */

#define initialize_native_spn(p_spninib)	initialize_lock((LOCK *)((p_spninib)->lock))
/*
 *  ネイティブスピンロックの取得
 *
 *  doc/porting.txt (6-21-3-2) の要求:
 *    「CPUロック状態で呼び出される．ハードウェアスピンロックが取得を試み，
 *      取得できればリターンする．取得できない場合は，一旦割込みを許可した
 *      後，再び割込みを禁止した後にハードウェアスピンロックが取得を試みる．」
 *
 *  従来は acquire_lock() をそのまま呼んでおり，取得できるまで CPU ロック状態の
 *  ままスピンしていた（＝割込みを一切許可しない）．このため，待っている側の
 *  プロセッサでタイムイベント（アラーム等）が発火できず，test_spinlock1 が
 *  デッドロックしていた（相手プロセッサのアラームハンドラが解放の契機を作る
 *  シーケンスのため）．
 *
 *  なお acquire_lock() はジャイアントロック（acquire_glock）と共用のため，
 *  acquire_lock() 自体は変更してはならない（ジャイアントロック取得中に割込みを
 *  許可することになる）．ここでは try_lock() を用いた再試行ループとして実装する．
 */
#define lock_native_spn(p_spninib)									\
	do {															\
		while (try_lock((LOCK *)((p_spninib)->lock))) {				\
			unlock_cpu();											\
			delay_for_interrupt();									\
			lock_cpu();												\
		}															\
	} while (false)
#define try_native_spn(p_spninib)			try_lock((LOCK *)((p_spninib)->lock))
#define unlock_native_spn(p_spninib)		release_lock((LOCK *)((p_spninib)->lock))
#define refer_native_spn(p_spninib)			refer_lock((LOCK *)((p_spninib)->lock))

/*
 *  エミュレーションされたスピンロックに関する定義
 */
#define delay_for_emulate_spn()

#else /* TNUM_PRCID >= 2 */

/*
 *  ロックに関する定義と操作（ユニプロセッサ用ダミー）
 */
typedef uint32_t LOCK;

Inline void
initialize_lock(LOCK *p_lock)
{
}

Inline void
acquire_lock(LOCK *p_lock)
{
}

Inline bool_t
try_lock(LOCK *p_lock)
{
	return false;
}

Inline void
release_lock(LOCK *p_lock)
{
}

Inline bool_t
refer_lock(LOCK *p_lock)
{
	return false;
}

/*
 *  ジャイアントロックに関する定義と操作（ユニプロセッサ用ダミー）
 */
extern LOCK giant_lock;

#define initialize_glock()	((void) 0)
#define acquire_glock()		((void) 0)
#define try_glock()			(false)
#define release_glock()		((void) 0)

/*
 *  ネイティブスピンロックに関する定義と操作（ユニプロセッサ用ダミー）
 */
#ifndef TMAX_NATIVE_SPN
#define TMAX_NATIVE_SPN 16
#endif /* TMAX_NATIVE_SPN */

#define initialize_native_spn(p_spninib)	((void) 0)
#define lock_native_spn(p_spninib)			((void) 0)
#define try_native_spn(p_spninib)			(false)
#define unlock_native_spn(p_spninib)		((void) 0)
#define refer_native_spn(p_spninib)			(false)

/*
 *  エミュレーションされたスピンロックに関する定義
 */
#define delay_for_emulate_spn()

#endif /* TNUM_PRCID >= 2 */

#endif /* TOPPERS_MACRO_ONLY */

/*
 *  トレースログに関する設定
 */
#ifdef TOPPERS_ENABLE_TRACE
#include "arch/tracelog/trace_log.h"
#endif /* TOPPERS_ENABLE_TRACE */

#endif /* TOPPERS_CHIP_KERNEL_IMPL_H */
