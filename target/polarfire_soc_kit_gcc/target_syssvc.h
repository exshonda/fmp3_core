/*
 *    システムサービスのターゲット依存部（Plarfire SoC Kit用）
 *
 *  システムサービスのターゲット依存部のヘッダファイル．システムサービ
 *  スのターゲット依存の設定は，できる限りコンポーネント記述ファイルで
 *  記述し，このファイルに記述するものは最小限とする．
 * 
 *  $Id: target_syssvc.h 334 2023-04-14 07:39:43Z ertl-honda $
 */

#ifndef TOPPERS_TARGET_SYSSVC_H
#define TOPPERS_TARGET_SYSSVC_H

#ifdef TOPPERS_OMIT_TECS

#include "polarfire_soc_kit.h"
#include "polarfire_soc.h"

/*
 *  シリアルインタフェースドライバを実行するクラスの定義
 */
#define CLS_SERIAL  CLS_PRC1

/*
 *  システムログの低レベル出力のための文字出力
 *
 *  ターゲット依存の方法で，文字cを表示/出力/保存する．
 */
extern void target_fput_log(char c);

/*
 *  サポートするシリアルポートの数
 */
#define TNUM_PORT  1

#ifdef USE_UART0
#define INTNO_SIO   90                /* SIO割込み番号 */
#define SIO0_BASE      MMUART0_BASE
#endif /* USE_UART0 */

#ifdef USE_UART1
#define INTNO_SIO   91                /* SIO割込み番号 */
#define SIO0_BASE      MMUART1_BASE
#endif /* USE_UART1 */

/*
 *  SIO割込みを登録するための定義
 */
#define ISRPRI_SIO  1                 /* SIO ISR優先度 */
#define INTPRI_SIO  (-4)              /* SIO割込み優先度 */
#define INTATR_SIO  TA_NULL           /* SIO割込み属性 */

/*
 *  SIOの設定値
 */
#define SIO0_LCONFIG   (MMUART_LCR_WLS_8BIT | MMUART_LCR_PEN_NOP | MMUART_LCR_STB_ONE)
#define SIO0_BAUD      UINT_C(81)
#define SIO0_FBAUDDIV  UINT_C(24)

/*
 *  低レベル出力で使用するSIOポート
 */
#define SIOPID_FPUT  1

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
