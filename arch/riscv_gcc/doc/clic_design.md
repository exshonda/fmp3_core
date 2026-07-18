# CLIC（Core-Local Interrupt Controller）依存部 設計メモ
- 作成者: 本田晋也
- 最終更新: 2026年06月29日

# CLIC依存部の位置づけ

CLIC依存部は，カーネルのターゲット依存部の中で，CLIC（Core-Local Interrupt
Controller）に準拠した割込みコントローラを持つチップに共通に使用できる部分である．
PLIC依存部（plic_kernel_impl）と対をなし，PLICを持たずCLICモードで動作するチップ
（ESP32-P4 等）で，PLIC依存部の代わりに用いる．両者は割込み管理機能のI/F
（disable_int / enable_int / clear_int / probe_int / raise_int 等）を同一に提供する．

# CLIC依存部を構成するファイル

CLIC依存部は，次のファイルで構成される．

- arch/riscv_gcc/common/
	- clic_kernel_impl.h	カーネルの割込みCLIC依存部のヘッダファイル
	- clic_kernel_impl.c	カーネルの割込みCLIC依存部
	- clic_kernel.trb 		カーネルの割込みCLIC依存部のパス2の生成スクリプト

CLICはハードウェアのトラップ入口で割込みレベル（mil = mintstatus[31:24]）を
更新し，mret で mcause.mpil から復元する．しかしコア依存部 core_support.S の
トラップ復帰機構は CLIC 非依存（中立）に保ち，CLIC 固有の mil 操作は
**チップ依存部の割込み出口フック irc_end_int（chip_support.S）に局所化**する
（ARM の gic_support.S で割込み優先度を復元するのと同じ位置付け）．これにより
PLIC 系チップと CLIC 系チップで共通部のトラップ復帰コードを完全に共有できる
（CLIC 専用のトラップ機構ファイルやフックは持たない）．詳細は後述「トラップ機構」．

また，しきい値レジスタ（割込み優先度マスク）の操作を伴う以下のアセンブリ言語の
関数は，ローカル割込みコントローラの操作も必要となるため，PLICと同様にチップ
依存部（chip_support.S）で定義する．

- 割込み入口処理での割込みコントローラ操作（irc_begin_int）
- 割込み出口処理での割込みコントローラ操作（irc_end_int）
- CPU例外発生前の割込み優先度の取得（irc_get_intpri）
- CPU例外ハンドラ呼出し前後の割込みコントローラ操作（irc_begin_exc / irc_end_exc）

# チップ依存のパラメータ

## アプリケーションから参照できるパラメータ

target_kernel.h（または，そこからインクルードされるファイル）で，以下の定数を
マクロ定義しておく．

(1) TMIN_INTPRI			割込み優先度の最小値（最高値）
(2) TMAX_INTPRI			割込み優先度の最大値（最低値）

TMAX_INTPRIは，-1に定義する．TMIN_INTPRIは，CLICのレベルビット数 NLBITS で
表現できる優先度の段数に応じて定義する．

## カーネル内部で使用するパラメータ

target_kernel_impl.h（または，そこからインクルードされるファイル）から
clic_kernel_impl.h をインクルードする前に，以下の定数をマクロ定義しておく
（実体はチップ依存ヘッダ esp32p4.h 等で定義する）．

- CLIC_TNUM_INTNO		割込み線の「本数」（有効線番号は 0 .. CLIC_TNUM_INTNO-1）
- CLIC_BASE				CLICのベースアドレス
- CLIC_INT_CTRL(i)		線 i の制御レジスタ（IE/IP/TRIG/優先度を格納）
- CLIC_INT_THRESH		割込み優先度マスク（しきい値）レジスタ（自コア）
- CLIC_INT_*_BIT/SHIFT	CLIC_INT_CTRL 各フィールドのビット位置

注: TMAX_INTNO は本数ではなく最大線番号（CLIC_TNUM_INTNO-1）に定義すること．
本数に等値すると VALID_INTNO が範囲外の線番号を有効と誤判定する
（chip_kernel_impl.h 参照）．

# 割込み優先度マスク（IPM）のソフトウェアシャドウ

CLIC_INT_THRESH の read-back が不安定な実装（ESP32-P4）では，ソフトが書いた
しきい値をそのまま読み戻せない．このため，IPMの真値をコア別のソフトウェア
シャドウ clic_ipm_shadow[] に保持し，t_get_ipm はシャドウを返す．HWしきい値
レジスタへの書込み（＝実マスク）は従来通り行うため，割込みマスク動作は不変．
シャドウは t_set_ipm 経路（clic_set_context_priority）でのみ更新し，irc_begin/
end_int の一時的なしきい値変更（タスクの論理IPMを変えない）では更新しない．

# トラップ機構（mil の管理）

CLICでは，割込み入口でハードウェアが in-service level（mil = mintstatus[31:24]）を
上昇させ，mret で mcause.mpil（mcause[23:16]）から復元する．割込みハンドラから
タスクへ復帰する／割込み文脈でディスパッチする／アイドルへ戻る際に mil を 0 へ
落とさないと，以降の割込み（他コアからの dispatch IPI=msip やタイマ）がマスクされた
まま起床不能（lost wakeup / livelock）になる（実機 mtrans2/raster2 が露呈．根治の
経緯は 50a1a88 / 24d63fa）．

## mil の復帰は irc_end_int に局所化する

mil の復帰は，**チップ依存部の割込み出口フック irc_end_int（chip_support.S）**で
行う．irc_end_int は全ての割込み出口で（通常復帰・割込み文脈ディスパッチ・アイドル
復帰・ネスト復帰のいずれよりも前に）必ず呼ばれる．ここで mcause.mpil=0 を強制して
おけば，これに続く mret（経路を問わず）が一律に mil=0 へ戻す．これにより：

- 共通部 core_support.S のトラップ復帰は **CLIC 非依存（中立）の素の mret** で済み，
  PLIC 系と CLIC 系で完全に共通化できる（CLIC 専用のトラップ機構ファイルやフック，
  USE_RISCV_CLIC の #ifdef を一切持たない）．idle復帰は dispatcher_0 へ，割込み文脈
  ディスパッチは dispatcher へ，いずれも素の mret でリダイレクトする（arm_m_gcc の
  return_to_idle と同設計）．
- mil 操作（mcause.mpil マスク 0xFF00FFFF．RV32 専用）は CLIC チップの irc_end_int
  にのみ存在し，CLIC 固有の HW 概念を担う適切な層に局所化される（ARM の gic_support.S
  が割込み優先度を復元するのと同じ位置付け）．

## なぜ mil=0 をネスト復帰でも強制してよいか

irc_end_int は全出口で走るため，ネスト割込み B の出口でも mcause.mpil=0 を強制し，
B の mret は外側ハンドラ A を mil=0 で再開させる．これが安全なのは，**本移植の
preemption の門が mil ではなく threshold（CLIC_INT_THRESH）で実装されている**ため
である．irc_end_int は同時に threshold を「この割込みの前」の値（= A の threshold）へ
復元するので，A は mil=0 でも threshold により保護され，「A より高優先の割込みのみが
A を preempt する」意味論は不変となる．mil は本移植では実質常に 0 に保つ不変量で
（gate は threshold が担う），固着だけを断てばよい．

注: CPU例外は割込みではなく mil を上げないため，例外経路は mil 操作を要しない．
例外番号（exccode）の取得のみ，チップ依存マクロ core_get_exccode_asm（chip_asm.inc）
で core_support.S の例外入口にインライン展開する（CLIC は mcause 下位12bit を抽出，
非CLIC は mcause をそのまま）．mret を伴わない純粋計算のためフックにはしない．

## アイドル遷移

アイドルは CLIC 固有処理を要さない．アイドル到達時 mil は（タスク文脈は元から 0，
割込み文脈は上記 irc_end_int により）0 であり，アイドルは MIE=1 にするだけの共通の
標準インライン idle（dispatcher_2 内の unlock_cpu_asm; j dispatcher_2）を CLIC/非CLIC
共通で用いる（フック化しない）．

以上により CLIC 固有のトラップ処理は「irc_end_int の mil クリア」と「例外番号取得
マクロ」の2点に収束し，共通部スケジューラ（core_support.S）は完全に CLIC 非依存と
なる．実機（ESP32-P4 の mtrans2/raster2/int1/cpuexc）と PolarFire QEMU（sample1
4コアSMP）で回帰がないことを確認済み．

# プロセッサ間割込み（IPI）と IPI 入口フック irc_begin_ipi

CLICはコアローカルであり，IPI（ディスパッチ要求）はCLINTのMSI（msip）で発生
させる．汎用 msi_handler（common/msi_ipi.c）は入口（clear_msip の前）で関数フック
irc_begin_ipi を呼ぶ．その実体は割込みコントローラ依存部（チップ）が提供し，
共通部 msi_ipi.c には割込みコントローラ依存を持ち込まない（PLIC 系チップは空の
irc_begin_ipi を提供）．

割込み1回のコストが高いチップ（ESP32-P4 等）では，高レートのディスパッチIPIにより
割込み入口/出口処理がコアを飽和させ，割込まれたタスクが前進できない位相共振
livelock が起こり得る（実機 test_mtrans2 で実証）．ESP32-P4 はこの dispatch-IPI
storm livelock breaker を irc_begin_ipi の実装として持つ（chip_kernel_impl.c）：
連続割込み間隔から storm を検出し，連続数に比例した有界 backoff で受信位相を
ずらして割込み対象タスクの前進を保証する（チューニング定数はチップ依存のため
チップ依存部に置く．詳細は clic_memo.md / 当該チップのソース）．

# パス2の生成スクリプトのチップ依存部

target_kernel.trb（または，そこからインクルードされるファイル）から
clic_kernel.trb を用い，割込み先コンテキストINDEXのテーブル
clic_target_cidx_table を kernel_cfg.c に生成する．CLICはコアローカルのため，
線番号はコアに固定的に対応する（どのコンテキストにも割り付けない場合は 0xff）．

# 割込み要求のクリア・生成

CLICではソフトウェアからのペンディング操作（IP ビットのセット/クリア）が可能で
あり，エッジトリガに設定した線（clic_set_edge）に対しては ras_int で割込み要求を
生成できる（PLICでは不可だった点が異なり，割込み管理機能テスト int1 が本ターゲット
では実行可能）．レベルトリガ線のペンディングクリアは irc_end_int で行う．

以上
