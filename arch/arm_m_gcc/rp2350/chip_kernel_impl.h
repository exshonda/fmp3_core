/*
 *  TOPPERS/FMP Kernel
 *      Toyohashi Open Platform for Embedded Real-Time Systems/
 *      Flexible MultiProcessor Kernel
 *
 *  Copyright (C) 2005-2020 by Embedded and Real-Time Systems Laboratory
 *              Graduate School of Information Science, Nagoya Univ., JAPAN
 *
 *  上記著作権者は，以下の(1)〜(4)の条件を満たす場合に限り，本ソフトウェ
 *  ア（本ソフトウェアを改変したものを含む．以下同じ）を使用・複製・改
 *  変・再配布（以下，利用と呼ぶ）することを無償で許諾する．（条件略）
 *
 *  本ソフトウェアは，無保証で提供されているものである．
 */

/*
 * ターゲット依存部モジュール（RP2350用）
 */

#ifndef TOPPERS_CHIP_KERNEL_IMPL_H
#define TOPPERS_CHIP_KERNEL_IMPL_H

#include "RP2350.h"

#ifndef TOPPERS_MACRO_ONLY

/*
 * 自プロセッサのインデックス（0オリジン）の取得
 *
 * RP2350 は SIO の CPUID レジスタで自コア番号（0/1）を直接読める．
 */
Inline uint_t
get_my_prcidx(void)
{
#if TNUM_PRCID >= 2
	return (uint_t)(*RP2350_SIO_CPUID);
#else /* TNUM_PRCID >= 2 */
	return 0U;
#endif /* TNUM_PRCID >= 2 */
}

#endif /* TOPPERS_MACRO_ONLY */

/*
 *  コア依存モジュール（ARM-M用）
 *  core_kernel_impl.h が get_my_prcidx() に依存しているので，ここで読み込む．
 */
#include <core_kernel_impl.h>

#ifndef TOPPERS_MACRO_ONLY

#if TNUM_PRCID >= 2

/*
 * ロックに関する定義と操作（RP2350 / SIO ハードウェアスピンロック）
 *
 * RP2350 の SIO は 32 個のハードウェアスピンロックを備える．LOCK 型は
 * 対応する SIO_SPINLOCKn レジスタのアドレスである．
 *   ・読み出し：0 でない値が返れば取得成功（同時に占有），0 なら他コアが
 *     占有中（取得失敗）．
 *   ・書き込み：占有を解放する．
 *
 * try 系の返り値の意味は trunk 共通契約に合わせ「既に取得されていた
 * （＝取得失敗）」が true である．
 */
typedef volatile uint32_t LOCK;

Inline void
initialize_lock(LOCK *p_lock)
{
	*p_lock = 0U; /* 解放状態にする */
	ARM_MEMORY_CHANGED;
}

Inline void
acquire_lock(LOCK *p_lock)
{
	while (*p_lock == 0U) {
		/* 取得できるまでスピン */
	}
	ARM_MEMORY_CHANGED;
}

Inline bool_t
try_lock(LOCK *p_lock)
{
	if (*p_lock == 0U) {
		/* 他コアが占有中＝取得失敗 */
		return true;
	}
	ARM_MEMORY_CHANGED;
	return false;
}

Inline void
release_lock(LOCK *p_lock)
{
	ARM_MEMORY_CHANGED;
	*p_lock = 0U;
}

Inline bool_t
refer_lock(LOCK *p_lock)
{
	const int index = (int)(((uintptr_t) p_lock - (uintptr_t) RP2350_SIO_SPINLOCKn(0)) >> 2);
	return (sil_rew_mem(RP2350_SIO_SPINLOCK_ST) & (1U << index)) != 0U;
}

/*
 * ジャイアントロックに関する定義と操作
 */
extern LOCK *giant_lock;

#define initialize_glock()	(giant_lock = (LOCK *)RP2350_SIO_SPINLOCKn(SPINLOCK_NO_FOR_GIANT_LOCK))
#define acquire_glock()		acquire_lock(giant_lock)
#define try_glock()			try_lock(giant_lock)
#define release_glock()		release_lock(giant_lock)

/*
 * ネイティブスピンロックに関する定義と操作
 *
 * spninib の lock メンバ（SIO_SPINLOCKn のアドレス）を用いる．
 */
#ifndef TMAX_NATIVE_SPN
#define TMAX_NATIVE_SPN 14
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
 * エミュレーションされたスピンロックに関する定義
 */
#define delay_for_emulate_spn()

#else /* TNUM_PRCID >= 2 */

/*
 * ロックに関する定義と操作（ユニプロセッサ用ダミー）
 */
typedef uint32_t LOCK;

Inline void initialize_lock(LOCK *p_lock) { }
Inline void acquire_lock(LOCK *p_lock) { }
Inline bool_t try_lock(LOCK *p_lock) { return false; }
Inline void release_lock(LOCK *p_lock) { }
Inline bool_t refer_lock(LOCK *p_lock) { return false; }

extern LOCK giant_lock;

#define initialize_glock()	((void) 0)
#define acquire_glock()		((void) 0)
#define try_glock()			(false)
#define release_glock()		((void) 0)

#ifndef TMAX_NATIVE_SPN
#define TMAX_NATIVE_SPN 14
#endif /* TMAX_NATIVE_SPN */

#define initialize_native_spn(p_spninib)	((void) 0)
#define lock_native_spn(p_spninib)			((void) 0)
#define try_native_spn(p_spninib)			(false)
#define unlock_native_spn(p_spninib)		((void) 0)
#define refer_native_spn(p_spninib)			(false)

#define delay_for_emulate_spn()

#endif /* TNUM_PRCID >= 2 */

#endif /* TOPPERS_MACRO_ONLY */

/*
 * トレースログに関する設定
 */
#ifdef TOPPERS_ENABLE_TRACE
#include "logtrace/trace_config.h"
#endif /* TOPPERS_ENABLE_TRACE */

#endif /* TOPPERS_CHIP_KERNEL_IMPL_H */
