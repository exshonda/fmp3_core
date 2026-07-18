/*
 *  プロセッサ間割込みに関する定義のターゲット依存部
 *  （ARM Musca-B1 / SSE-200 dual Cortex-M33 用）
 *
 *  クロスコアの IPI は SSE-200 内蔵の MHU（Message Handling Unit）を用いて
 *  実現する．本ターゲットでは MHU0 のみを使用し，その割込みレジスタ下位
 *  4bit に IPI の種別を割り当てる（複数種別の同時要求も 1 本の IRQ で運べる）．
 *
 *    bit0 : ディスパッチ要求      (IPIBIT_DISPATCH)
 *    bit1 : カーネル終了要求      (IPIBIT_EXT_KER)
 *    bit2 : 高分解能タイマ設定要求 (IPIBIT_SET_HRT_EVT)
 *
 *  相手コア n（prcid）に対する要求は，MHU0 の CPU(n)INTR_SET に該当ビットを
 *  書き込むことで，相手コアの NVIC IRQ6（例外番号 22）を発火させる．prcid と
 *  コア番号の対応は prcid 1 → CPU0，prcid 2 → CPU1 である．
 *
 *  MHU の STAT はレベルトリガであり，STAT が非ゼロの間 IRQ がアサートされ
 *  続けるため，受信側ハンドラ（_kernel_ipi_irq_handler）は先頭で必ず CLR
 *  する（target_ipi.c）．
 *
 *  TNUM_PRCID == 1 のときは真のプロセッサ間割込みは存在しないため，ディス
 *  パッチ要求は PendSV をローカルに発行し，それ以外は何もしない．
 */

#ifndef TOPPERS_TARGET_IPI_H
#define TOPPERS_TARGET_IPI_H

#include "musca_b1.h"

/*
 *  IPI 種別ビット（MHU 割込みレジスタ下位 4bit のうち 3 本を使用）
 */
#define IPIBIT_DISPATCH      0x1U   /* ディスパッチ要求 */
#define IPIBIT_EXT_KER       0x2U   /* カーネル終了要求 */
#define IPIBIT_SET_HRT_EVT   0x4U   /* 高分解能タイマ設定要求 */

/*
 *  IPI 割込み（MHU0）の割込み番号・割込みハンドラ番号（PRC 符号化）
 *
 *  各コアごとに自コア宛ての MHU0 割込みを，自コアのクラスに登録する．
 */
#define INTNO_IPI_PRC1   ((1 << 16) | MUSCA_B1_MHU0_INTNO)
#define INTNO_IPI_PRC2   ((2 << 16) | MUSCA_B1_MHU0_INTNO)
#define INHNO_IPI_PRC1   ((1 << 16) | MUSCA_B1_MHU0_INTNO)
#define INHNO_IPI_PRC2   ((2 << 16) | MUSCA_B1_MHU0_INTNO)

/*
 *  IPI 割込みの割込み優先度
 *
 *  ディスパッチ要求は最も低いカーネル管理割込み優先度でよいが，本ターゲット
 *  では全 IPI 種別を 1 本の IRQ に集約しているため，単一の優先度を用いる．
 *  最高優先（TMIN_INTPRI = -3）とし，カーネル終了要求の即応性も確保する．
 */
#define INTPRI_IPI       (TMIN_INTPRI)

#ifndef TOPPERS_MACRO_ONLY

/*
 *  相手コア宛ての MHU0 INTR_SET レジスタ番地を返す．
 *
 *    prcid 1（CPU0）→ CPU0INTR_SET
 *    prcid 2（CPU1）→ CPU1INTR_SET
 */
Inline uint32_t
musca_b1_mhu0_set_reg(ID prcid)
{
	if (prcid == 1) {
		return MUSCA_B1_MHU_CPU0INTR_SET(MUSCA_B1_MHU0_BASE);
	}
	else {
		return MUSCA_B1_MHU_CPU1INTR_SET(MUSCA_B1_MHU0_BASE);
	}
}

/*
 *  ディスパッチ要求プロセッサ間割込みの発行
 */
Inline void
request_dispatch_prc(ID prcid)
{
#if TNUM_PRCID >= 2
	sil_wrw_mem((void *) musca_b1_mhu0_set_reg(prcid), IPIBIT_DISPATCH);
#else /* TNUM_PRCID >= 2 */
	request_dispatch_retint();
#endif /* TNUM_PRCID >= 2 */
}

/*
 *  カーネル終了要求プロセッサ間割込みの発行
 */
Inline void
request_ext_ker(ID prcid)
{
#if TNUM_PRCID >= 2
	sil_wrw_mem((void *) musca_b1_mhu0_set_reg(prcid), IPIBIT_EXT_KER);
#endif /* TNUM_PRCID >= 2 */
}

/*
 *  高分解能タイマ設定要求プロセッサ間割込みの発行
 */
Inline void
request_set_hrt_event(ID prcid)
{
#if TNUM_PRCID >= 2
	sil_wrw_mem((void *) musca_b1_mhu0_set_reg(prcid), IPIBIT_SET_HRT_EVT);
#endif /* TNUM_PRCID >= 2 */
}

#if TNUM_PRCID >= 2
extern void target_ipi_initialize(intptr_t exinf);
extern void _kernel_ipi_irq_handler(void);
#endif /* TNUM_PRCID >= 2 */

#endif /* TOPPERS_MACRO_ONLY */
#endif /* TOPPERS_TARGET_IPI_H */
