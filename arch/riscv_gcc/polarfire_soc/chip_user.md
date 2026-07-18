# PolarFire SoC 依存部 ユーザーズマニュアル
- 作成者: 本田晋也
- 最終更新: 2024年04月23日

# メモの位置づけ

このドキュメントは，TOPPERS/FMP3カーネルのPolaFire SoC チップ依存部を使用するために必要な事項を説明するものである．

# hartid と PLICのコンテキストINDEXの関係

(hartid - 1) * 2 + 1


# Polarfire SoC チップ依存部の概要

PolarFire SoC チップ依存部は，PolarFire SoCの4コアのU54を用いたターゲットシステムに共通に使用できる部分である．

PolarFire SoC チップ依存部は，U54コアのみ対応する．

PolarFire SoC チップ依存部には，PolarFire SoCに内蔵されるUARTの操作などが含まれる．

PolarFire SoC チップ依存部（GNU開発環境向け）は，arch/riscv_gcc/polarfireに置かれている．

チップ略称等は次の通り．

	チップ略称：PolarFireSoC
	開発環境略称：gcc

PolarFire SoC チップ依存部は，RISC-Vコア依存部，PLIC依存部，Mtimer依存部を用いている．

そのため，「RISC-V依存部 ユーザーズマニュアル」において，RISC-Vコア依存部，PLIC依存部，Mtimer依存部に関して記述されたことは，PolarFire SoCチップ依存部にも適用される．


# ターゲット定義事項の規定

## 割込み処理に関する規定

PolarFire SoC の割込みコントローラ（PLIC）は，7レベルの割込み優先度をサポートしている．そのため，割込み優先度の最小値（最高値）は-6，割込み優先度の最大値（最低値）は-1である．

カーネル管理外の割込みはサポートしない．

ローカル割込みはサポートしない．

プロセッサ間割込みはCLINTにより発生させるMSIを用いる．

## タイマに関する規定

PolarFire SoC チップ依存部では，Mtiemr依存部を用いて，CLINTの Machine Timerで高分解能タイマを実現している．

# リファレンス

## ディレクトリ構成・ファイル構成

	riscv_gcc/polarfire/
		MANIFEST				Polarfire依存部のファイルリスト
		Makefile.chip			Makefileのチップ依存部

		chip_kernel.h			kernel.hのチップ依存部
		chip_kernel.trb			kernel.trbのチップ依存部
		chip_kernel_impl.c		カーネル実装のチップ依存部
		chip_kernel_impl.h		カーネル実装のチップ依存部関連の定義
		chip_rename.def			チップ依存部の内部識別名のリネーム定義
		chip_rename.h			チップ依存部の内部識別名のリネーム
		chip_serial.c			簡易シリアルドライバのチップ依存部
		chip_serial.cfg			簡易シリアルドライバのコンフィギュレーションファイルのチップ依存部
		chip_sil.h				sil.hのチップ依存部
		chip_stddef.h			t_stddef.hのチップ依存部
		chip_support.S			カーネル実装のチップ依存部（アセンブリ言語で記述した部分）
		chip_timer.h			タイマドライバを使用するための定義
		chip_unrename.h			チップ依存部の内部識別名のリネーム解除

		clint_ipi.h				MSIを用いたコア間割込みドライバの定義
		mmuart.c				簡易SIOドライバ
		mmuart.h				簡易SIOドライバに関する定義
		polarfire_soc.h			チップのハードウェア資源の定義
		chip_user.txt			Polarfireチップ依存部 ユーザーズマニュアル

## バージョン履歴

- 2024/09/09
  - chip_kernel_impl.c
    - target_hrt_initialize() をchip_initialize()で呼び出しているので，同様に，chip_terminate()でtarget_hrt_terminate()を呼び出すよう変更．

- 2026/06/14
  - chip_support.S
    - irc_get_intpri における PLIC の割込み優先度しきい値（threshold）レジスタの
      読出しを，`ld`（64ビットロード）から `lw`（32ビットロード）に修正．
      threshold レジスタは32ビット幅であり，同レジスタを操作する他の箇所
      （irc_begin_int / irc_end_int / irc_begin_exc / irc_end_exc）はいずれも
      `lw` / `sw`（32ビットアクセス）を用いている．irc_get_intpri のみ `ld` で
      8バイトアクセスしていたため，CPU例外ハンドラの呼出し経路でしきい値を
      取得する際に，8バイトアクセスを許可しないPLIC実装（QEMU の
      microchip-icicle-kit など）でロードアクセスフォルトを誘発していた．
      この経路はCPU例外発生時にのみ通るため，通常の割込み処理は影響を
      受けていなかった（cpuexc系テストのみで顕在化）．

以上
