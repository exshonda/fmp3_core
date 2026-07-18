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
 *		システムサービスのターゲット依存部（ZynqMP R5用）
 *
 *  システムサービスのターゲット依存部のヘッダファイル．
 */

#ifndef TOPPERS_TARGET_SYSSVC_H
#define TOPPERS_TARGET_SYSSVC_H

#include "zynqmp_r5.h"

/*
 *  起動メッセージのターゲットシステム名
 *
 *  Kria 共通ターゲット（R5/RPU）．ボード名は Makefile.target の BOARD 変数から渡される
 *  -DTOPPERS_KRIA_<BOARD> で切替える（KR260/KV260=K26 SOM, KD240=K24 SOM。RPU は同一）．
 */
#if defined(TOPPERS_KRIA_KR260)
#define TARGET_BOARD_NAME  "KR260"
#elif defined(TOPPERS_KRIA_KV260)
#define TARGET_BOARD_NAME  "KV260"
#elif defined(TOPPERS_KRIA_KD240)
#define TARGET_BOARD_NAME  "KD240"
#else /* ボード未指定時 */
#define TARGET_BOARD_NAME  "Kria"
#endif

#define TARGET_NAME    TARGET_BOARD_NAME " <Cortex-R5F>"

/*
 *  シリアルインタフェースドライバを実行するクラスの定義
 */
#define CLS_SERIAL		CLS_PRC1

/*
 *  システムログの低レベル出力のための文字出力
 *
 *  ターゲット依存の方法で，文字cを表示/出力/保存する．
 */
extern void	target_fput_log(char c);

/*
 *  サポートするシリアルポートの数
 */
#define TNUM_PORT		1

/*
 *  SIOドライバで使用するXUartPsに関する設定
 *
 *  コンソールにはUART1を使用する（QEMUでは2番目の-serialオプション）．
 *  ボーレートの設定値は，UARTの入力クロックを100MHzとして115.2Kbpsと
 *  なる値（100MHz / (62 * (13 + 1)) ≒ 115207bps）．QEMUではボーレー
 *  ト設定は動作に影響しない．
 */
#define SIO_XUARTPS_BASE	ZYNQMP_UART1_BASE
#define SIO_XUARTPS_MODE	(XUARTPS_MR_CHARLEN_8 \
								| XUARTPS_MR_PARITY_NONE | XUARTPS_MR_STOPBIT_1)
#define SIO_XUARTPS_BAUDGEN	62U
#define SIO_XUARTPS_BAUDDIV	13U

/*
 *  SIO割込みを登録するための定義
 */
#define INTNO_SIO		ZYNQMP_UART1_IRQ	/* SIO割込み番号 */
#define ISRPRI_SIO		1					/* SIO ISR優先度 */
#define INTPRI_SIO		(-4)				/* SIO割込み優先度 */
#define INTATR_SIO		TA_NULL				/* SIO割込み属性 */

/*
 *  低レベル出力で使用するSIOポート
 */
#define SIOPID_FPUT		1

/*
 *  トレースログに関する設定
 */
#ifdef TOPPERS_ENABLE_TRACE
#include "arch/tracelog/trace_log.h"
#endif /* TOPPERS_ENABLE_TRACE */

/*
 *  コアで共通な定義（チップ依存部は飛ばす）
 */
#include "core_syssvc.h"

#endif /* TOPPERS_TARGET_SYSSVC_H */
