/*
 *  TOPPERS Software
 *      Toyohashi Open Platform for Embedded Real-Time Systems
 *
 *  Copyright (C) 2006-2023 by Embedded and Real-Time Systems Laboratory
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
 *  $Id: gic_kernel_impl.c 483 2026-06-13 15:00:34Z ertl-honda $
 */

/*
 *		カーネルの割込みコントローラ依存部（GIC用）
 */

#include "kernel_impl.h"
#include "interrupt.h"
#include <sil.h>
#include "arm.h"
#include "gic_ipi.h"			/* IPINO_EXT_KER / IPINO_DISPATCH（残留IPIクリア用）*/

/*
 *  CPUインタフェースの操作
 */

/*
 *  CPUインタフェースの初期化
 */
void
gicc_initialize(PCB *p_my_pcb)
{
	/*
	 *  CPUインタフェースをディスエーブル
	 */
	sil_wrw_mem(GICC_CTLR, GICC_CTLR_DISABLE);

	/*
	 *  割込み優先度マスクを最低優先度に設定
	 */
	gicc_set_priority((GIC_PRI_LEVEL - 1) << GIC_PRI_SHIFT);

	/*
	 *  割込み優先度の全ビット有効に
	 */
	sil_wrw_mem(GICC_BPR, 0U);

	/*
	 *  CPUインタフェースをイネーブル
	 *
	 *  以降のペンディング/アクティブ割込みクリアのため，先にイネーブルする．
	 */
#ifdef TOPPERS_SAFEG_SECURE
	sil_wrw_mem(GICC_CTLR, (GICC_CTLR_FIQEN|GICC_CTLR_ENABLEGRP1
												|GICC_CTLR_ENABLEGRP0));
#else /* TOPPERS_SAFEG_SECURE */
	sil_wrw_mem(GICC_CTLR, GICC_CTLR_ENABLE);
#endif /* TOPPERS_SAFEG_SECURE */

	/*
	 *  残留するペンディング/アクティブ割込みの正規化
	 *
	 *  リセットを介さずにカーネルを再起動した場合（JTAGによる再ロード等）に，
	 *  前回実行時のペンディング割込みや，完了せずに残ったアクティブ割込みが
	 *  CPUインタフェースに残存することがある．特にカーネル終了IPI（ext_ker）
	 *  は irc_end_int を実行せずに停止するためEOIされず，アクティブのまま残
	 *  る．アクティブ割込みが残るとその実行優先度により以降のIPI（他プロセッ
	 *  サからのディスパッチ要求等）がマスクされ，マルチプロセッサ動作がデッ
	 *  ドロックする．これを正常な状態へ正規化する．
	 *
	 *  通常のリセット起動時はペンディング/アクティブ割込みは存在せず（GICC_RPR
	 *  はアイドル0xffを示す），本処理は実質的な副作用を持たない．
	 */
	{
		uint_t		i;
		uint32_t	iar;

		/* ペンディング割込みのドレイン（IARでack→EOIで完了） */
		for (i = 0; i < 64U; i++) {
			iar = sil_rew_mem(GICC_IAR);
			if ((iar & 0x03ffU) == 0x03ffU) {
				break;					/* spurious(1023)＝ペンディング無し */
			}
			sil_wrw_mem(GICC_EOIR, iar);
		}

		/*
		 *  残留アクティブ割込みのクリア．GICC_RPRがアイドル(0xff)以外なら，
		 *  前回実行で完了しなかったIPIハンドラのアクティブ状態が残っている．
		 *  カーネルが用いるIPI(SGI)をEOIして解除する（SGIのEOIRは送信元CPU
		 *  を含むため，両プロセッサ分を発行する）．
		 */
		for (i = 0; i < 16U; i++) {
			if ((sil_rew_mem(GICC_RPR) & 0xffU) == 0xffU) {
				break;					/* アクティブ割込み無し */
			}
			sil_wrw_mem(GICC_EOIR, IPINO_EXT_KER);
			sil_wrw_mem(GICC_EOIR, (1U << 10) | IPINO_EXT_KER);
			sil_wrw_mem(GICC_EOIR, IPINO_DISPATCH);
			sil_wrw_mem(GICC_EOIR, (1U << 10) | IPINO_DISPATCH);
		}
	}
}

/*
 *  CPUインタフェースの終了処理
 */
void
gicc_terminate(void)
{
	sil_wrw_mem(GICC_CTLR, GICC_CTLR_DISABLE);
}

/*
 *  ディストリビュータの操作
 */

/*
 *  ディストリビュータの初期化
 * 
 *  この関数は，他のプロセッサが実行を開始する前に，マスタプロセッサの
 *  みから呼び出されるため，プロセッサ間排他制御は必要ない．
 */
void
gicd_initialize(void)
{
	int		i;

	/*
	 *  ディストリビュータをディスエーブル
	 */
	sil_wrw_mem(GICD_CTLR, GICD_CTLR_DISABLE);

#ifdef TOPPERS_SAFEG_SECURE
	/*
	 *  すべての割込みをグループ1（IRQ）に設定
	 */
	for (i = 0; i < (GIC_TNUM_INTNO + 31) / 32; i++) {
		sil_wrw_mem(GICD_IGROUPR(i), 0xffffffffU);
	}
#endif /* TOPPERS_SAFEG_SECURE */

	/*
	 *  すべての割込みを禁止
	 */
	for (i = 0; i < (GIC_TNUM_INTNO + 31) / 32; i++) {
		sil_wrw_mem(GICD_ICENABLER(i), 0xffffffffU);
	}

	/*
	 *  すべての割込みペンディングをクリア
	 */
	for (i = 0; i < (GIC_TNUM_INTNO + 31) / 32; i++) {
		sil_wrw_mem(GICD_ICPENDR(i), 0xffffffffU);
	}

	/*
	 *  すべての割込みを最低優先度に設定
	 */
	for (i = 0; i < (GIC_TNUM_INTNO + 3) / 4; i++){
		sil_wrw_mem(GICD_IPRIORITYR(i), 0xffffffffU);
	}

	/*
	 *  すべての共有ペリフェラル割込みのターゲットをプロセッサ0に設定
	 */
	for (i = GIC_INTNO_SPI0 / 4; i < (GIC_TNUM_INTNO + 3) / 4; i++) {
		sil_wrw_mem(GICD_ITARGETSR(i), 0x01010101U);
	}

	/*
	 *  すべてのペリフェラル割込みをレベルトリガに設定
	 */
	for (i = GIC_INTNO_PPI0 / 16; i < (GIC_TNUM_INTNO + 15) / 16; i++) {
#ifdef GIC_ARM11MPCORE
		sil_wrw_mem(GICD_ICFGR(i), 0x55555555U);
#else /* GIC_ARM11MPCORE */
		sil_wrw_mem(GICD_ICFGR(i), 0x00000000U);
#endif /* GIC_ARM11MPCORE */
	}

	/*
	 *  ディストリビュータをイネーブル
	 */
	sil_wrw_mem(GICD_CTLR, GICD_CTLR_ENABLE);
}

/*
 *  ディストリビュータの終了処理
 * 
 *  この関数は，他のプロセッサが実行を終了した後に，マスタプロセッサの
 *  みから呼び出されるため，プロセッサ間排他制御は必要ない．
 */
void
gicd_terminate(void)
{
	sil_wrw_mem(GICD_CTLR, GICD_CTLR_DISABLE);
}

#ifndef OMIT_GIC_INITIALIZE_INTERRUPT

/*
 *  割込み要求ラインの属性の設定
 *
 *  FMP3カーネルでの利用を想定して，パラメータエラーはアサーションで
 *  チェックしている．
 */
Inline void
config_int(PCB *p_my_pcb, INTNO intno, ATR intatr, PRI intpri, uint_t affinity)
{
	SIL_PRE_LOC;

	assert(VALID_INTNO(p_my_pcb->prcid, intno));
	assert(TMIN_INTPRI <= intpri && intpri <= TMAX_INTPRI);
    
	/*
	 *  SILスピンロックを取得
	 */
	SIL_LOC_SPN();
    
	/*
	 *  割込みを禁止
	 *
	 *  割込みを受け付けたまま，レベルトリガ／エッジトリガの設定や，割
	 *  込み優先度の設定を行うのは危険なため，割込み属性にかかわらず，
	 *  一旦マスクする．
	 */
	disable_int(intno);

	/*
	 *  割込みをコンフィギュレーション
	 */
#ifdef TOPPERS_SAFEG_SECURE
	gicd_config_group(INTNO_MASK(intno), 1U);
#endif /* TOPPERS_SAFEG_SECURE */

	if ((intatr & TA_EDGE) != 0U) {
#ifdef GIC_ARM11MPCORE
		gicd_config(INTNO_MASK(intno), GICD_ICFGRn_EDGE|GICD_ICFGRn_1_N);
#else /* GIC_ARM11MPCORE */
		gicd_config(INTNO_MASK(intno), GICD_ICFGRn_EDGE);
#endif /* GIC_ARM11MPCORE */
		clear_int(intno);
	}
	else {
#ifdef GIC_ARM11MPCORE
		gicd_config(INTNO_MASK(intno), GICD_ICFGRn_LEVEL|GICD_ICFGRn_1_N);
#else /* GIC_ARM11MPCORE */
		gicd_config(INTNO_MASK(intno), GICD_ICFGRn_LEVEL);
#endif /* GIC_ARM11MPCORE */
	}

	/*
	 *  割込み優先度とターゲットプロセッサを設定
	 */
	gicd_set_priority(INTNO_MASK(intno), INT_IPM(intpri));
	gicd_set_target(INTNO_MASK(intno), affinity);

	/*
	 * 割込みを許可
	 */
	if ((intatr & TA_ENAINT) != 0U) {
		enable_int(intno);
	}

	/*
	 *  SILスピンロックを解放
	 */
	SIL_UNL_SPN();
}

/*
 *  割込み管理機能の初期化
 */
void
initialize_interrupt(PCB *p_my_pcb)
{
	uint_t			i;
	const INTINIB	*p_intinib;

	for (i = 0; i < tnum_cfg_intno; i++) {
		p_intinib = &(intinib_table[i]);
		if (p_intinib->iprcid == p_my_pcb->prcid) {
			config_int(p_my_pcb, p_intinib->intno, p_intinib->intatr,
								p_intinib->intpri, p_intinib->affinity);
		}
	}
}

#endif /* OMIT_GIC_INITIALIZE_INTERRUPT */
