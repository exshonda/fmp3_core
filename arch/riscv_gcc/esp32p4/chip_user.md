# ESP32-P4 依存部 ユーザーズマニュアル
- 作成者: 本田晋也
- 最終更新: 2026年07月18日

# ドキュメントの位置づけ

このドキュメントは，TOPPERS/FMP3カーネルの ESP32-P4 チップ依存部を使用するために
必要な事項を説明するものである．

# ESP32-P4 チップ依存部の概要

ESP32-P4 チップ依存部は，ESP32-P4 の高性能（HP）RISC-V コア（2コア）を用いた
ターゲットシステムに共通に使用できる部分である．LP コアは対象としない．

ESP32-P4 の HP コアは RV32IMAFC（32bit，圧縮命令・単精度浮動小数点あり）であり，
割込みコントローラは **CLIC モード固定**（mtvec.MODE=3）で動作する．コア間割込み
（IPI）とタイマには **CLINT**（コアローカル）を用いる．

ESP32-P4 チップ依存部（GNU開発環境向け）は，arch/riscv_gcc/esp32p4 に置かれている．

チップ略称等は次の通り．

	チップ略称：esp32p4
	開発環境略称：gcc

ESP32-P4 チップ依存部は，RISC-Vコア依存部，**CLIC依存部**，Mtimer依存部を用いて
いる．そのため，「RISC-V依存部 ユーザーズマニュアル」において，RISC-Vコア依存部，
CLIC依存部，Mtimer依存部に関して記述されたことは，ESP32-P4 チップ依存部にも
適用される（CLIC依存部の詳細は doc/clic_design.md / clic_memo.md を参照）．

# hartid と プロセッサID（prcid）・コンテキストINDEX の関係

ESP32-P4 は core0=hart0，core1=hart1 である．

	prcid  = mhartid + 1     （マスタ＝prcid 1）
	prcidx = mhartid         （INDEX_PRC，マスタ＝0）

CLIC はコアローカルのため，PLIC のようなコンテキストINDEX は実質不要だが，
I/F互換のため get_my_prcidx() は mhartid を返す．

# ターゲット定義事項の規定

## 割込み処理に関する規定

ESP32-P4 の割込みコントローラ（CLIC）は NLBITS=3 で，割込み優先度の最小値（最高値）
は **-7**，割込み優先度の最大値（最低値）は **-1** である．

割込み線は 0〜47（CLIC_TNUM_INTNO=48 本，有効線番号 0..47）．内部割込みは
msip=線3，mtimer=線7．外部割込み（INTERRUPT MATRIX 由来）は +16 オフセットで
線16〜47に現れ，線16は USB-Serial/JTAG が使用するためアプリ用には線17以降を用いる．

割込み優先度マスク（しきい値）レジスタ CLIC_INT_THRESH は read-back が不安定なため，
IPMの真値はソフトウェアシャドウで管理する（割込みマスク動作は不変）．

カーネル管理外の割込みはサポートしない．

プロセッサ間割込みは CLINT により発生させる MSI（msip）を用いる．

IPI 受信入口フック `irc_begin_ipi()`（`chip_kernel_impl.c`．汎用部
`arch/riscv_gcc/common/msi_ipi.c` の `msi_handler()` から，msip クリア前に呼ばれる）には，
dispatch-IPI storm による livelock を防ぐための **escalating 有界 backoff**（storm
breaker）が入っている．連続する割込み間隔が `CLIC_STORM_PERIOD_CYC`（4000cycle，
360MHz 時 ≈11us）未満なら storm と判定し，連続 storm 数に比例した backoff を挿入する．
backoff は `CLIC_STORM_COUNT_CAP * CLIC_STORM_BACKOFF_UNIT` = 512 ループで頭打ち
（360MHz 時 ≈7us）となる．**利用者が最悪割込み応答時間を見積もる際は，この上限値を
加味すること．** これらの定数は ESP32-P4 @360MHz での実測チューニング値であり，
動作クロックを変更する場合は再調整が必要である．msip は level-pending のまま保持
されるため，backoff を挿入しても lost-wakeup は起きない（詳細は
`mtrans2_lost_wakeup_analysis.md` を参照）．

## タイマに関する規定

ESP32-P4 チップ依存部では，Mtimer依存部を用いて，CLINT の Machine Timer で
高分解能タイマを実現している（MTIMER_FREQ_MHZ=360）．

ESP32-P4 の CLINT はコアローカルで，各コアが自コア基準（CLINT_BASE）の
mtimecmp（0x2000_4000）を読み書きすると自プロセッサ分を指す．また mtime は
mtimectl の MTIME_EN で明示的に有効化する必要がある（chip 初期化で行う）．

# ターゲット依存部（方式）に関する補足

ESP32-P4 では，FMP3 を configure.rb/make で静的ライブラリ（libfmp3.a）にまとめ，
ESP-IDF アプリの main コンポーネントがこれをリンクする方式を採る（ABI は IDF と
一致する ilp32f）．.data/.bss は IDF 起動が初期化するため，start.S の自前初期化は
TOPPERS_OMIT_BSS_INIT / TOPPERS_OMIT_DATA_INIT で抑止する．実機書込みは
ESP-IDF の idf.py flash で行う．ビルド・書込み・テストのツール一式と手順は
ターゲット依存部 target/m5stamp_esp32p4_gcc/tools/（tools/README.md）を参照．

# リファレンス

## ディレクトリ構成・ファイル構成

	riscv_gcc/esp32p4/
		MANIFEST				ESP32-P4 依存部のファイルリスト
		Makefile.chip			Makefileのチップ依存部

		chip_asm.inc			アセンブリ言語の共通定義
		chip_kernel.h			kernel.hのチップ依存部
		chip_kernel.trb			kernel.trbのチップ依存部
		chip_kernel_impl.c		カーネル実装のチップ依存部
		chip_kernel_impl.h		カーネル実装のチップ依存部関連の定義
		chip_rename.def			チップ依存部の内部識別名のリネーム定義
		chip_rename.h			チップ依存部の内部識別名のリネーム
		chip_serial.c			簡易シリアルドライバのチップ依存部
		chip_serial.cfg			簡易シリアルドライバのコンフィギュレーションファイル
		chip_serial.h			簡易シリアルドライバに関する定義
		chip_sil.h				sil.hのチップ依存部
		chip_stddef.h			t_stddef.hのチップ依存部
		chip_support.S			カーネル実装のチップ依存部（アセンブリ言語部）
		chip_timer.h			タイマドライバを使用するための定義
		chip_unrename.h			チップ依存部の内部識別名のリネーム解除

		clint_ipi.h				MSIを用いたコア間割込みドライバの定義
		esp32p4.h				チップのハードウェア資源の定義（CLIC/CLINT等）
		chip_user.md			ESP32-P4 チップ依存部 ユーザーズマニュアル
		chip_design.md			チップ依存部の設計メモ・落とし穴集
		esp32p4_hw_reference.md	ESP32-P4 ハードウェアリファレンス
		fmp3_port_mapping.md	FMP3 移植のファイル対応表
		idf_riscv_intr_impl.md	ESP-IDF の RISC-V 割込み実装の調査記録
		mtrans2_lost_wakeup_analysis.md	test_mtrans2 ハングの解析記録
		pie_hwlp_design.md		PIE/HWLP コプロセッサ文脈管理の設計記録

## バージョン履歴

- 2026/06/29
  - clic_kernel_impl.c
    - clic_context_initialize() の全線クリアループを `<=` から `<` に修正
      （範囲外 CLIC_INT_CTRL(CLIC_TNUM_INTNO) への OOB MMIO 書込みを除去．
      TMAX_INTNO の off-by-one と同種．TTSP3 適合性テストが契機）．
  - chip_user.md / MANIFEST
    - PolarFire からの流用記述を ESP32-P4 の実態に合わせて全面改訂．

- 2026/07/18
  - chip_user.md
    - ディレクトリ構成・ファイル構成の一覧に，MANIFEST には記載済みだが漏れていた
      chip_design.md／esp32p4_hw_reference.md／fmp3_port_mapping.md／
      idf_riscv_intr_impl.md／mtrans2_lost_wakeup_analysis.md／pie_hwlp_design.md
      の6件を追加．
    - 「割込み処理に関する規定」に，irc_begin_ipi() の dispatch-IPI storm breaker
      （escalating 有界 backoff）に関する利用者向け記述（最悪割込み応答時間の
      見積りに必要な上限値）を追加．

以上
