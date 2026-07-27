/*
 *  TOPPERS Software
 *      Toyohashi Open Platform for Embedded Real-Time Systems
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
 *		割込み管理機能のテスト(2)
 *
 * 【テストの目的】
 *
 *  同一の割込み番号に対して複数の割込みサービスルーチンを登録した場合
 *  に，コンフィギュレータが生成する割込みハンドラ（doc/configurator.txt
 *  の4.7.1.2）が，正しくコンパイルでき，かつ仕様どおりに動作することを
 *  テストする．
 *
 *  ★このテストの第一の目的は「生成されたkernel_cfg.cがコンパイルでき
 *  ること」である．同一の割込み番号に2本目以降の割込みサービスルーチン
 *  があるときにだけ，コンフィギュレータは
 *
 *      PCB		*p_my_pcb = get_my_pcb();
 *      ...
 *      if (_kernel_sense_lock()) {
 *          _kernel_force_unlock_spin(p_my_pcb);
 *          _kernel_unlock_cpu();
 *      }
 *
 *  というコードを生成する．このコードはkernel_int.h末尾のリネーム解除
 *  後の文脈に置かれるため，そのアーキテクチャのcore_rename.defが
 *  sense_lock／unlock_cpuをリネームしているかどうかに依存する．この経
 *  路を踏むテストが1つも無かったため，一部のアーキテクチャでコンパイル
 *  できない状態が長期間検出されなかった．したがって，本テストは実行結
 *  果を見る前に「ビルドが通ること」自体に価値がある．
 *
 * 【制限事項】
 *
 *  このテストを実施するには，ras_intとclr_intがサポートされており，そ
 *  れらを行える割込み要求ラインがあることが必要である．また，同一の割
 *  込み番号に対して複数の割込みサービスルーチンを登録できることが必要
 *  である．
 *
 * 【テスト項目】
 *
 *	(A) 生成される割込みハンドラのコンパイル
 *		(A-1) 同一の割込み番号に割込みサービスルーチンを3本登録した構成
 *		      がビルドできる
 *	(B) 割込みサービスルーチンの呼出し
 *		(B-1) isrpriの昇順に呼び出される
 *		(B-2) isrpriが同じ場合は，システムコンフィギュレーションファイル
 *		      中の静的APIの順序で呼び出される
 *		(B-3) それぞれの割込みサービスルーチンに，登録時のexinfが渡される
 *	(C) 割込みサービスルーチンの呼出し前の状態への復帰
 *		(C-1) 割込みサービスルーチンがCPUロック状態のままリターンしても，
 *		      次の割込みサービスルーチンはCPUロック解除状態で呼び出される
 *		(C-2) 最後の割込みサービスルーチンからリターンした後，割り込まれ
 *		      たタスクはCPUロック解除状態に戻る
 *
 * 【使用リソース】
 *
 *	TASK1: 中優先度タスク，TA_ACT属性
 *	INTNO1_ISR_LAST:   INTNO1，isr3，exinf=3，isrpri=2（記述順1番目）
 *	INTNO1_ISR_FIRST:  INTNO1，isr1，exinf=1，isrpri=1（記述順2番目）
 *	INTNO1_ISR_SECOND: INTNO1，isr2，exinf=2，isrpri=1（記述順3番目）
 *
 * 【テストシーケンス】
 *
 *	== TASK1（優先度：中）==
 *	1:	ras_int(INTNO1)
 *	== ISR1 ==
 *	2:	assert(exinf == 1)					... (B-1)(B-3)
 *		DO(intno1_clear())
 *		iloc_cpu()							... (C-1)の前提を作る
 *		RETURN
 *	== ISR2 ==
 *	3:	assert(exinf == 2)					... (B-2)(B-3)
 *		assert(!isns_loc())					... (C-1)
 *		RETURN
 *	== ISR3 ==
 *	4:	assert(exinf == 3)					... (B-1)(B-3)
 *		assert(!isns_loc())					... (C-1)
 *		RETURN
 *	== TASK1（続き）==
 *	5:	assert(!sns_loc())					... (C-2)
 *	6:	END
 */

#include <kernel.h>
#include <t_syslog.h>
#include "syssvc/test_svc.h"
#include "kernel_cfg.h"
#include "test_int2.h"

/* DO NOT DELETE THIS LINE -- gentest depends on it. */

void
isr1(EXINF exinf)
{
	ER_UINT	ercd;

	check_point(2);
	check_assert(exinf == 1);

	intno1_clear();

	ercd = iloc_cpu();
	check_ercd(ercd, E_OK);

	return;

	check_assert(false);
}

void
isr2(EXINF exinf)
{

	check_point(3);
	check_assert(exinf == 2);

	check_assert(!isns_loc());

	return;

	check_assert(false);
}

void
isr3(EXINF exinf)
{

	check_point(4);
	check_assert(exinf == 3);

	check_assert(!isns_loc());

	return;

	check_assert(false);
}

void
task1(EXINF exinf)
{
	ER_UINT	ercd;

	test_start(__FILE__);

	check_point(1);
	ercd = ras_int(INTNO1);
	check_ercd(ercd, E_OK);

	check_point(5);
	check_assert(!sns_loc());

	check_finish(6);
	check_assert(false);
}
