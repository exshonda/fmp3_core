/*
 *  TOPPERS Software
 *      Toyohashi Open Platform for Embedded Real-Time Systems
 * 
 *  Copyright (C) 2022 by Embedded and Real-Time Systems Laboratory
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
 *  $Id: test_cyclic1.c 1035 2022-12-16 06:51:23Z ertl-hiro $
 */

/*
 *		周期通知のテスト(1)
 *
 * 【使用リソース】
 *
 *	CLS_ALL_PRC1: 両プロセッサで実行，初期割付けはプロセッサ1
 *	CLS_ALL_PRC2: 両プロセッサで実行，初期割付けはプロセッサ2
 *
 *	TASK1: CLS_ALL_PRC1，中優先度タスク，最初から起動
 *	TASK2: CLS_ALL_PRC2，中優先度タスク
 *	CYC1: CLS_ALL_PRC1，周期通知，最初から起動
 *
 * 【テストシーケンス】
 *
 *	== TASK1（優先度：中）==
 *	1:	tslp_tsk(10 * TEST_TIME_CP)
 *	// CYC1が動作
 *	2:	tslp_tsk(10 * TEST_TIME_CP)
 *	// CYC1が動作
 *	3:	stp_cyc(CYC1)
 * 		tslp_tsk(10 * TEST_TIME_CP) -> E_TMOUT
 *	4:	sta_cyc(CYC1)
 *		tslp_tsk(10 * TEST_TIME_CP)
 *	// CYC1が動作
 *	5:	tslp_tsk(10 * TEST_TIME_CP)
 *	// CYC1が動作
 *	6:	END
 */

#include <kernel.h>
#include <t_syslog.h>
#include "syssvc/test_svc.h"
#include "kernel_cfg.h"
#include "test_common.h"

void
task2(EXINF exinf)
{
}

/* DO NOT DELETE THIS LINE -- gentest depends on it. */

void
task1(EXINF exinf)
{
	ER_UINT	ercd;

	test_start(__FILE__);

	check_point(1);
	ercd = tslp_tsk(10 * TEST_TIME_CP);
	check_ercd(ercd, E_OK);

	check_point(2);
	ercd = tslp_tsk(10 * TEST_TIME_CP);
	check_ercd(ercd, E_OK);

	check_point(3);
	ercd = stp_cyc(CYC1);
	check_ercd(ercd, E_OK);

	ercd = tslp_tsk(10 * TEST_TIME_CP);
	check_ercd(ercd, E_TMOUT);

	check_point(4);
	ercd = sta_cyc(CYC1);
	check_ercd(ercd, E_OK);

	ercd = tslp_tsk(10 * TEST_TIME_CP);
	check_ercd(ercd, E_OK);

	check_point(5);
	ercd = tslp_tsk(10 * TEST_TIME_CP);
	check_ercd(ercd, E_OK);

	check_finish(6);
	check_assert(false);
}
