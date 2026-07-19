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
 *  SIL スピンロックは chip_sil.h で定義する
 *
 *  以前はここで無条件に SIL_LOC_INT()（＝PRIMASK による割込みロック）と等価に
 *  定義していたが，これは単一プロセッサ前提の記述であり，2 コア構成では
 *  プロセッサ間排他にならなかった．その結果，syslog の低レベル出力が両コアで
 *  混ざり，テストの合否マーカー文字列が分断されて自動判定が偽陰性を出していた．
 *  chip_sil.h が TNUM_PRCID に応じて実装を切り替える．
 */

#endif /* TOPPERS_TARGET_SIL_H */
