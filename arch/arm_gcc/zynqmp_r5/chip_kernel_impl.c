/*
 *  TOPPERS/FMP Kernel
 *      Toyohashi Open Platform for Embedded Real-Time Systems/
 *      Flexible MultiProcessor Kernel
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
 *		カーネルのチップ依存部（ZynqMP RPU用）
 */

#include "kernel_impl.h"
#include <sil.h>
#include "arm.h"

/*
 *  全リージョンを無効にする
 */
void
mpu_disable_allregion(void)
{
	uint32_t	index, num, tmp;

	CP15_READ_MPUIR(num);
	num = (num >> 8) & 0xffU;			/* DRegionフィールド */
	for (index = 0; index < num; index++) {
		CP15_WRITE_RGNR(index);
		CP15_READ_DRSR(tmp);
		tmp &= ~((uint32_t) RSAE_EN);
		data_sync_barrier();
		CP15_WRITE_DRSR(tmp);
		data_sync_barrier();
		inst_sync_barrier();
	}
}

/*
 *  MPUのリージョンの属性を設定
 */
void
mpu_set_region(uint32_t base, uint32_t size, uint32_t number, uint32_t attr)
{
	uint32_t	tmp = (size << RSAE_RS_OFFSET) | RSAE_EN;

	data_sync_barrier();
	CP15_WRITE_RGNR(number);
	inst_sync_barrier();
	CP15_WRITE_DRBAR(base);
	CP15_WRITE_DRACR(attr);
	CP15_WRITE_DRSR(tmp);
	data_sync_barrier();
}

/*
 *  チップ依存の初期化（マスタプロセッサ，sta_ker前）
 */
void
chip_mprc_initialize(void)
{
	/*
	 *  GICのディストリビュータの初期化
	 */
	gicd_initialize();
}

/*
 *  チップ依存の初期化
 */
void
chip_initialize(PCB *p_my_pcb)
{
	/*
	 *  コア依存の初期化
	 */
	core_initialize(p_my_pcb);

	/*
	 *  GICのCPUインタフェースの初期化
	 */
	gicc_initialize(p_my_pcb);
}

/*
 *  チップ依存の終了処理
 */
void
chip_terminate(void)
{
	/*
	 *  GICのCPUインタフェースの終了処理
	 */
	gicc_terminate();

	/*
	 *  GICのディストリビュータの終了処理
	 */
	gicd_terminate();

	/*
	 *  コア依存の終了処理
	 */
	core_terminate();
}
