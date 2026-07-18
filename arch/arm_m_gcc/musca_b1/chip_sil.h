/*
 *  sil.h のチップ依存部（Musca-B1 用）
 */

#ifndef TOPPERS_CHIP_SIL_H
#define TOPPERS_CHIP_SIL_H

#define SIL_ENDIAN_LITTLE

/*
 *  プロセッサで共通な定義
 */
#include <core_sil.h>
#ifndef TOPPERS_MACRO_ONLY
#include <core_insn.h>
#endif /* TOPPERS_MACRO_ONLY */

/*
 *  一般共通レジスタに対するビット操作関数
 *
 *  SSE-200 にはアトミックなビット操作機構が無いため，読出し・修正・書込み
 *  で実現する．
 */
#ifndef TOPPERS_MACRO_ONLY
#define sil_orb(mem, val)   sil_wrb_mem((mem), (uint8_t)(sil_reb_mem(mem) | (val)))
#define sil_andb(mem, val)  sil_wrb_mem((mem), (uint8_t)(sil_reb_mem(mem) & (val)))
#define sil_clrb(mem, val)  sil_wrb_mem((mem), (uint8_t)(sil_reb_mem(mem) & ~(val)))
#define sil_mskb(mem, val, msk) \
				sil_wrb_mem((mem), (uint8_t)((sil_reb_mem(mem) & ~(msk)) | ((val) & (msk))))

#define sil_orh(mem, val)   sil_wrh_mem((mem), (uint16_t)(sil_reh_mem(mem) | (val)))
#define sil_andh(mem, val)  sil_wrh_mem((mem), (uint16_t)(sil_reh_mem(mem) & (val)))
#define sil_clrh(mem, val)  sil_wrh_mem((mem), (uint16_t)(sil_reh_mem(mem) & ~(val)))
#define sil_mskh(mem, val, msk) \
				sil_wrh_mem((mem), (uint16_t)((sil_reh_mem(mem) & ~(msk)) | ((val) & (msk))))

#define sil_orw(mem, val)   sil_wrw_mem((mem), (uint32_t)(sil_rew_mem(mem) | (val)))
#define sil_andw(mem, val)  sil_wrw_mem((mem), (uint32_t)(sil_rew_mem(mem) & (val)))
#define sil_clrw(mem, val)  sil_wrw_mem((mem), (uint32_t)(sil_rew_mem(mem) & ~(val)))
#define sil_mskw(mem, val, msk) \
				sil_wrw_mem((mem), (uint32_t)((sil_rew_mem(mem) & ~(msk)) | ((val) & (msk))))

/*
 *  プロセッサIDの取得
 *
 *  SSE-200 には自コア番号を読めるレジスタが無いため，現在のスタックポインタ
 *  （MSP）が，どのコアの割込みスタック（_kernel_istack_prcN）の範囲に入って
 *  いるかで自コア番号を判定する（get_my_prcidx() と同一手法）．プロセッサID
 *  は 1 オリジンなので TMIN_PRCID（=1）を加える．
 *
 *  単一プロセッサ（TNUM_PRCID==1）では常に TMIN_PRCID（=1）を返す．
 */
Inline void
sil_get_pid(ID *p_prcid)
{
#if defined(TNUM_PRCID) && (TNUM_PRCID >= 2)
	/*
	 *  自コア番号の判定は istack 範囲を参照するため，STK_T 等のカーネル型に
	 *  依存する．sil.h は kernel_impl.h を介さずに取り込まれる場合があるので，
	 *  実体はチップ依存部（chip_kernel_impl.c）の関数に委ね，ここでは関数を
	 *  プロトタイプ宣言して呼ぶだけにする．プロセッサIDは 1 オリジン．
	 */
	extern uint_t _kernel_chip_get_my_prcidx(void);
	*p_prcid = (ID)(_kernel_chip_get_my_prcidx() + 1);
#else /* defined(TNUM_PRCID) && (TNUM_PRCID >= 2) */
	*p_prcid = 1;
#endif /* defined(TNUM_PRCID) && (TNUM_PRCID >= 2) */
}

/*
 *  SIL スピンロックの強制解放（単一プロセッサでは何もしない）
 */
Inline void
TOPPERS_sil_force_unl_spn(void)
{
}
#endif /* TOPPERS_MACRO_ONLY */

#endif /* TOPPERS_CHIP_SIL_H */
