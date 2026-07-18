/*
 *  ターゲット依存部モジュール（ARM Musca-B1 / FMP3 用）
 *
 *  カーネルのターゲット依存部のインクルードファイル．kernel_impl.h のター
 *  ゲット依存部の位置付けとなる．
 */

#ifndef TOPPERS_TARGET_KERNEL_IMPL_H
#define TOPPERS_TARGET_KERNEL_IMPL_H

/*
 *  ボードのハードウェア資源定義
 */
#include "musca_b1.h"

/*
 *  TBITW_IPRI の定義のため読み込み
 */
#include <sil.h>

/*
 *  デフォルトの非タスクコンテキスト用のスタック領域の定義
 */
#define DEFAULT_ISTKSZ	(0x1000)	/* 4KByte */

/*
 *  デフォルトのアイドル処理用のスタック領域の定義
 */
#define DEFAULT_IDSTKSZ	(0x0100U)

#ifndef TOPPERS_MACRO_ONLY

/*
 *  マスタプロセッサ依存の初期化
 */
extern void target_mprc_initialize(void);

/*
 *  ターゲットシステム依存の初期化
 */
extern void target_initialize(PCB *p_my_pcb);

/*
 *  ターゲットシステムの終了
 */
extern void target_exit(void) NoReturn;

/*
 *  エラー発生時の処理
 */
extern void Error_Handler(void);

#endif /* TOPPERS_MACRO_ONLY */

/*
 *  チップ依存モジュール（ARM-M 用）
 */
#include <chip_kernel_impl.h>

#endif /* TOPPERS_TARGET_KERNEL_IMPL_H */
