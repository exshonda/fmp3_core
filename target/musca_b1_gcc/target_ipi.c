/*
 *  プロセッサ間割込みのターゲット依存部
 *  （ARM Musca-B1 / SSE-200 dual Cortex-M33 用）
 *
 *  SSE-200 内蔵 MHU0 を用いたクロスコア IPI の受信ハンドラと初期化処理．
 *  詳細は target_ipi.h を参照．
 */

#include "kernel_impl.h"
#include "time_event.h"
#include "target_ipi.h"

#if TNUM_PRCID >= 2

/*
 *  自コア宛ての MHU0 INTR_STAT／INTR_CLR レジスタ番地を返す．
 *
 *    prcidx 0（CPU0）→ CPU0INTR_*
 *    prcidx 1（CPU1）→ CPU1INTR_*
 */
Inline uint32_t
mhu0_my_stat_reg(uint_t prcidx)
{
	return (prcidx == 0)
		? MUSCA_B1_MHU_CPU0INTR_STAT(MUSCA_B1_MHU0_BASE)
		: MUSCA_B1_MHU_CPU1INTR_STAT(MUSCA_B1_MHU0_BASE);
}

Inline uint32_t
mhu0_my_clr_reg(uint_t prcidx)
{
	return (prcidx == 0)
		? MUSCA_B1_MHU_CPU0INTR_CLR(MUSCA_B1_MHU0_BASE)
		: MUSCA_B1_MHU_CPU1INTR_CLR(MUSCA_B1_MHU0_BASE);
}

/*
 *  プロセッサ間割込みの初期化処理
 *
 *  自コア宛ての MHU0 割込み要求が残っていればクリアし，NVIC の保留状態も
 *  クリアする．各コアのクラスに ATT_INI で登録される（target_ipi.cfg）．
 */
void
target_ipi_initialize(intptr_t exinf)
{
	uint_t	prcidx = get_my_prcidx();

	/*
	 *  MHU の STAT に残っている要求をすべてクリアする．
	 */
	sil_wrw_mem((void *) mhu0_my_clr_reg(prcidx), 0xfU);
	data_synchronization_barrier();

	/*
	 *  NVIC 側の保留もクリアする（exinf は prcid）．
	 */
	if (exinf == 1) {
		clear_int(INTNO_IPI_PRC1);
	}
	else {
		clear_int(INTNO_IPI_PRC2);
	}
}

/*
 *  プロセッサ間割込みの受信ハンドラ
 *
 *  自コア宛ての MHU0 STAT を読み，CLR でレベルを落としてから，立っていた
 *  種別ビットに応じて各処理を呼び出す．MHU はレベルトリガのため，STAT を
 *  読んでから一括で CLR することで取りこぼし・多重発火を防ぐ．
 */
void
_kernel_ipi_irq_handler(void)
{
	uint_t		prcidx = get_my_prcidx();
	uint32_t	stat;

	stat = sil_rew_mem((void *) mhu0_my_stat_reg(prcidx)) & 0xfU;

	/*
	 *  受け取った要求のレベルを落とす（先にクリアして再発火を防ぐ）．
	 */
	sil_wrw_mem((void *) mhu0_my_clr_reg(prcidx), stat);
	data_synchronization_barrier();

	if ((stat & IPIBIT_DISPATCH) != 0U) {
		request_dispatch_retint();
	}
	if ((stat & IPIBIT_SET_HRT_EVT) != 0U) {
		set_hrt_event_handler();
	}
	if ((stat & IPIBIT_EXT_KER) != 0U) {
		ext_ker_handler();
	}
}

#endif /* TNUM_PRCID >= 2 */
