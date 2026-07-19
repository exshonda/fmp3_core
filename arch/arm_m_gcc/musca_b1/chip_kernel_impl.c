/*
 *  チップ依存モジュール（Musca-B1 / SSE-200 dual Cortex-M33 用）
 */

#include "kernel_impl.h"

/*
 *  ジャイアントロック用のロック変数
 *
 *  TNUM_PRCID == 1 のときは未使用のダミー．
 */
LOCK giant_lock;

#if TNUM_PRCID >= 2

/*
 *  マルチプロセッサ起動ハンドシェイク用フラグ
 *
 *  .noinit セクションに置く（BSS ゼロクリア・DATA 初期化のいずれの対象にも
 *  しない）．QEMU では SRAM がマシン起動時に 0 で確保されるため，マシンリ
 *  セット直後の値は 0 とみなせる．
 *
 *    0 : まだどのコアも起動していない（CPU0 が最初に観測する）
 *    1 : CPU0 が二次コア（CPU1）を解放する直前にセットする
 *
 *  _kernel_start 冒頭（スタック設定前）の my_prcidx_boot マクロからのみ参照
 *  され，自コアがマスタコア（CPU0）か二次コア（CPU1）かを判定する．
 */
__attribute__((section(".noinit")))
volatile uint32_t _kernel_mp_boot_flag;

/*
 *  自コア番号（0オリジン）の取得（実体）
 *
 *  現在の MSP（メインスタックポインタ）が，どのコアの割込みスタック
 *  （_kernel_istack_prcN）の範囲に入っているかで判定する．MSP は各コアの
 *  起動時（start.S・core_support.S）に istkpt_table[idx] へ設定され，タスク
 *  実行中（PSP 使用時）も保持されるため，例外・タスクのいずれの文脈から呼ん
 *  でも正しい値を返す（冪等）．
 */
uint_t
_kernel_chip_get_my_prcidx(void)
{
	uint32_t	msp;
	uint_t		i;

	Asm("mrs %0, msp" : "=r"(msp));
	for (i = 0; i < TNUM_PRCID; i++) {
		/*
		 *  各コアの割込みスタックは隣接して配置され，istkpt_table[i]（上限）
		 *  が次のコアの istk_table[i+1]（下限）と一致する．MSP は空スタック
		 *  時に上限（istkpt）を指すため，下限は排他・上限は包含で判定し，
		 *  共有境界を上位コア側に帰属させる（アセンブリ版 my_prcidx と一致）．
		 *  両端を包含で判定すると，あるコアが空スタックでタスク実行中（MSP が
		 *  istkpt = 隣接コアの istk と一致）のとき，下位コア番号に誤判定する．
		 */
		if ((uint32_t)(uintptr_t) istk_table[i] < msp
				&& msp <= (uint32_t)(uintptr_t) istkpt_table[i]) {
			return i;
		}
	}
	/*
	 *  どの istack 範囲にも入らない場合（通常は起こらない）はマスタコア
	 *  （0）とみなす．
	 */
	return 0U;
}

/*
 *  SIL スピンロック（chip_sil.h 参照）
 *
 *  syslog の低レベル出力をプロセッサ間で排他するために用いる．
 *  TOPPERS_sil_spn_var には取得中のプロセッサID（1オリジン）を保持し，
 *  同一プロセッサからの再帰取得を識別する．
 */
LOCK		TOPPERS_sil_spn_lock;
uint32_t	TOPPERS_sil_spn_var;

/*
 *  SIL スピンロックの取得
 *
 *  PRIMASK の元の値を返す．自プロセッサが既に取得していた場合は bit1 を
 *  セットして返す（bit1 は PRIMASK では未使用のため識別に利用できる）．
 *
 *  取得できない間は，PRIMASK を元に戻して割込みを受け付けられるようにしてから
 *  再度禁止して試行する（他プロセッサの解放を待つ間に自プロセッサの割込みを
 *  塞ぎ続けないため）．
 */
uint32_t
TOPPERS_sil_loc_spn(void)
{
	uint32_t	primask = get_primask();
	ID			prcid;

	set_primask();
	sil_get_pid(&prcid);

	if (TOPPERS_sil_spn_var == (uint32_t) prcid) {
		/* すでに取得済み（再帰取得） */
		return (1U << 1) | primask;
	}
	while (try_lock(&TOPPERS_sil_spn_lock)) {
		/* 取得できなかったので，一旦割込みを許可してから再試行する */
		assign_primask(primask);
		set_primask();
	}
	TOPPERS_sil_spn_var = (uint32_t) prcid;
	return primask;
}

/*
 *  SIL スピンロックの返却
 */
void
TOPPERS_sil_unl_spn(uint32_t primask)
{
	if ((primask & (1U << 1)) != 0U) {
		/* 再帰取得だったので解放しない */
		primask &= ~(1U << 1);
	}
	else {
		TOPPERS_sil_spn_var = 0U;
		release_lock(&TOPPERS_sil_spn_lock);
	}
	assign_primask(primask);
}

/*
 *  SIL スピンロックの強制解放（自プロセッサが取得していれば解放する）
 */
void
TOPPERS_sil_force_unl_spn(void)
{
	ID	prcid;

	sil_get_pid(&prcid);
	if (TOPPERS_sil_spn_var == (uint32_t) prcid) {
		TOPPERS_sil_spn_var = 0U;
		release_lock(&TOPPERS_sil_spn_lock);
	}
}

#endif /* TNUM_PRCID >= 2 */
