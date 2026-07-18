/*
 *		システムサービスのターゲット依存部（KRIA Cortex-A53(AArch64)用）
 *
 *  システムサービスのターゲット依存部のヘッダファイル．システムサービ
 *  スのターゲット依存の設定は，できる限りコンポーネント記述ファイルで
 *  記述し，このファイルに記述するものは最小限とする．
 * 
 *  $Id: target_syssvc.h 306 2022-08-30 15:07:36Z ertl-honda $
 */

#ifndef TOPPERS_TARGET_SYSSVC_H
#define TOPPERS_TARGET_SYSSVC_H

#ifdef TOPPERS_OMIT_TECS

#include "kria.h"
#include "zynqmp.h"

/*
 *  起動メッセージのターゲットシステム名
 *
 *  Kria 共通ターゲット．ボード名は Makefile.target の BOARD 変数から渡される
 *  -DTOPPERS_KRIA_<BOARD> で切替える（KR260/KV260=K26 SOM, KD240=K24 SOM．
 *  いずれも Zynq UltraScale+ quad A53・GIC-400 で PS は同一，差は表示名のみ）．
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

#ifdef TOPPERS_TZ_S
#define TARGET_NAME    TARGET_BOARD_NAME " CA53(AArch64 Secure)"
#else /* TOPPERS_TZ_S */
#define TARGET_NAME    TARGET_BOARD_NAME " CA53(AArch64 Non-Secure)"
#endif /* TOPPERS_TZ_S */

/*
 *  シリアルインタフェースドライバを実行するクラスの定義
 */
#define CLS_SERIAL		CLS_PRC1

/*
 *  システムログの低レベル出力のための文字出力
 *
 *  ターゲット依存の方法で，文字cを表示/出力/保存する．
 */
extern void target_fput_log(char c);

/*
 *  サポートするシリアルポートの数
 */
#define TNUM_PORT		1

/*
 *  SIOドライバで使用するXUartPsに関する設定
 */
#ifdef USE_XUART0
#define SIO_XUARTPS_BASE	ZYNQMP_UART0_BASE
#else /* USE_XUART1 */
#define SIO_XUARTPS_BASE	ZYNQMP_UART1_BASE
#endif /* USE_XUART0 */
#define SIO_XUARTPS_MODE	XUARTPS_MR_CHARLEN_8 \
								| XUARTPS_MR_PARITY_NONE | XUARTPS_MR_STOPBIT_1
#define SIO_XUARTPS_BAUDGEN	XUARTPS_BAUDGEN_115K
#define SIO_XUARTPS_BAUDDIV	XUARTPS_BAUDDIV_115K

/*
 *  SIO割込みを登録するための定義
 */
#ifdef USE_XUART0
#define INTNO_SIO		ZYNQMP_UART0_IRQ	/* SIO割込み番号 */
#else /* USE_XUART1 */
#define INTNO_SIO		ZYNQMP_UART1_IRQ	/* SIO割込み番号 */
#endif /* USE_XUART0 */

#define ISRPRI_SIO		1					/* SIO ISR優先度 */
#define INTPRI_SIO		(-2)				/* SIO割込み優先度 */
#define INTATR_SIO		TA_NULL				/* SIO割込み属性 */

/*
 *  低レベル出力で使用するSIOポート
 */
#define SIOPID_FPUT		1

/*
 *  ログタスクのスタックサイズ
 */
#define LOGTASK_STACK_SIZE	4096

/*
 *  トレースログに関する設定
 */
#ifdef TOPPERS_ENABLE_TRACE
#include "arch/tracelog/trace_log.h"
#endif /* TOPPERS_ENABLE_TRACE */

#endif /* TOPPERS_OMIT_TECS */

/*
 *  コアで共通な定義（チップ依存部は飛ばす）
 */
#include "core_syssvc.h"

#endif /* TOPPERS_TARGET_SYSSVC_H */
