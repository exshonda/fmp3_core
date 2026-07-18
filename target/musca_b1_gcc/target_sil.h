/*
 *  sil.h のターゲット依存部（ARM Musca-B1 / FMP3 用）
 */

#ifndef TOPPERS_TARGET_SIL_H
#define TOPPERS_TARGET_SIL_H

/*
 *  プロセッサのインディアン定義（Cortex-M33，リトルエンディアン）
 */
#define SIL_ENDIAN_LITTLE

/*
 *  割込み優先度のビット幅
 *
 *  実機の SSE-200 は 3 ビット（8 レベル）を実装する．
 */
#define TBITW_IPRI     3

/*
 *  チップで共通な定義
 */
#include <chip_sil.h>

/*
 *  ボード（SSE-200 / Musca-B1）のハードウェア資源定義
 */
#include "musca_b1.h"

/*
 *  SIL スピンロック（単一プロセッサ版）
 *
 *  単一プロセッサであり，スピンロックは割込みロックと等価である．
 */
#define SIL_LOC_SPN()  SIL_LOC_INT()
#define SIL_UNL_SPN()  SIL_UNL_INT()

#endif /* TOPPERS_TARGET_SIL_H */
