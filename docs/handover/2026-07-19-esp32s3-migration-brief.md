# ESP32-S3 / 無印 ESP32 の FMP3 ポートを fmp3_core 上へ移す — 引き継ぎ資料

**宛先**: `/home/honda/TOPPERS/ESP32/esp32_s3`（`/home/honda/TOPPERS/esp32_s3` はこれへの symlink）で
作業するエージェント
**差出**: `fmp3_core` 側（2026-07-19 時点）
**前提**: あなたは自分のリポジトリを知っている。`fmp3_core` の契約と、我々が6ターゲット移植する
過程で踏んだ罠は知らない。**この文書はその差分だけを書く。** ツリーを読めば分かることは書かない。

> **パスの訂正**: 依頼時に `/home/honda/esp32_s3` と伝えられたが、そのパスは存在しない。
> 実体は `/home/honda/TOPPERS/ESP32/esp32_s3`（`/home/honda/TOPPERS/esp32_s3` が symlink）。

---

## 1. 受け入れ契約 — これが中心的な発見

### 汎用層が外から include するのは 1 本だけ

`fmp3_core` の汎用 `CMakeLists.txt` が外部から取り込むのは **`${FMP3_TARGET_DIR}/target.cmake`
ただ一つ**（`CMakeLists.txt:100`）。そこから先は完全に **target 側が駆動**する：

| 段 | 実例 | 何を include するか |
|---|---|---|
| target | `target/kria_r5_gcc/target.cmake:97` | `include(${CHIPDIR}/chip.cmake)` |
| chip | `arch/arm_gcc/zynqmp_r5/chip.cmake:59` | `include(${COREDIR}/arch.cmake)` |
| arch | — | 終端 |

**帰結: 外部リポジトリは arch・chip・target の3層すべてを供給できる。`fmp3_core` が名前すら
知らない Xtensa arch を、`fmp3_core` を一行も変えずに差し込める。**

これは6ターゲットを移植した経験からの推論ではなく、include 連鎖を実際に辿って確認した事実。
ただし **実地で外部リポジトリから使われたことは一度もない**（§8 参照）。

### 外部 target.cmake が積むべき変数

汎用層が消費する `FMP3_*` 変数。**`CMakeLists.txt:49-58` のコメントに一覧があるが、それは
現状と食い違っている**ので、以下は実物を読んで作り直したもの：

```
FMP3_CFG_FILES              FMP3_KERNEL_CFG_TRB_FILES   FMP3_CHECK_TRB_FILES
FMP3_OFFSET_TRB_FILES       FMP3_CLASS_TRB_FILES        FMP3_SYMVAL_TABLES
FMP3_INCLUDE_DIRS           FMP3_COMPILE_DEFS           FMP3_COMPILE_OPTIONS
FMP3_LINK_OPTIONS           FMP3_CFG1_OUT_LINK_OPTIONS  FMP3_LINK_LIBS
FMP3_LDSCRIPT               FMP3_ARCH_C_FILES           FMP3_TARGET_C_FILES
FMP3_SYSSVC_TARGET_C_FILES  FMP3_START_FILES
```

さらに target/arch が**宣言できる**トグル類：

| 変数 | 既定 | 宣言する層 | 意味 |
|---|---|---|---|
| `FMP3_LDSCRIPT_VIA_DRIVER_T` | OFF (`CMakeLists.txt:77-78`) | target | §6-A |
| `FMP3_DUMP_FORMAT` | `srec` (`CMakeLists.txt:93-94`) | **arch** | §6-C |
| `FMP3_DUMPOPTS` | `""` (`CMakeLists.txt:96-97`) | target | objdump のオプション |
| `FMP3_RUN_COMMAND` | — | target | `run` ターゲットの実行コマンド |
| `FMP3_PRC_NUM` | `""` (`CMakeLists.txt:43`) | コマンドライン | `TNUM_PRCID` を上書き（`:152-153`） |

> **コメントの陳腐化（ツリーが正）**: `CMakeLists.txt:50-52` は「`FMP3_API_TABLES` は
> CMakeLists.txt 側で、`FMP3_RUN_COMMAND` は target.cmake 側で、ともに将来 Task で追加予定」と
> 書いているが、**両方すでに実在する**。`FMP3_API_TABLES` は `CMakeLists.txt:219` で設定され、
> `FMP3_RUN_COMMAND` は5つの target.cmake が宣言済み。このコメントを信用しないこと。

### `FMP3_LIBRARY_ONLY`

`CMakeLists.txt:29` の `option()`。ON にすると `libfmp3.a` の生成までで止まり、実行ファイル
`fmp` のリンク・`run` ターゲット・pass3 チェックを作らない（`:671`〜`:744` が
`if(NOT FMP3_LIBRARY_ONLY)` で囲まれている）。外部 SDK が `add_subdirectory(fmp3_core)` して
自前のリンクを行う場合に使う。ESP-IDF 側でリンクを握るなら**こちらが本命**の可能性が高い。

### 既知の穴 — 外部利用者だけを刺す2件（文書化済み・未修正）

1. **`fmp3_add_syssvc()`**（`fmp3_core.cmake:79`）は**呼び出し側スコープの `FMP3_ROOT_DIR`** に
   依存し、`FMP3_SYSSVC_TARGET_C_FILES` は親スコープへ伝播しない。
   `fmp3_core.cmake:21-22` が明記：*「本ファイルに PARENT_SCOPE export は無い（asp3_core も同じ）」*、
   `:44`：*「未対応。対応するなら target.cmake 側から PARENT_SCOPE export する設計変更が要る」*。
2. **`fmp3_cfg_check()`**（`CMakeLists.txt:577`）は `FMP3_PASS3_ARGS` / `FMP3_CFG_GEN_DIR` に
   依存する。これらは `FMP3_LIBRARY_ONLY=ON` のときだけ `PARENT_SCOPE` export される
   （`:666-667`）。OFF のまま外部から呼ぶと未定義。

どちらも**失敗が静かではなく FATAL_ERROR で鳴る**ようにはしてある（`:583`、`:618-620`）が、
**直してはいない**。あなたが最初に踏む可能性が高い2箇所。

---

## 2. 統治 — あなたにとっては簡単な話

上流 FMP3 3.4.0 に **Xtensa は一切ない**。上流の ESP 対応は `arch/riscv_gcc/esp32p4` +
`target/m5stamp_esp32p4_gcc`（RISC-V）だけ。

したがって Xtensa は：

- `fmp3_core` に**入らない**
- `upstream` vendor ブランチに**触れない**
- `fmp3_core` 側の `DIVERGENCE_MAP.md` に**行を足す必要がない**

`fmp3_core` は pristine + 派生のまま保たれる。あなたは自分のリポジトリの中だけで完結する。

### 後で効いてくる非対称性

| チップ | arch | chip | target |
|---|---|---|---|
| S3 / 無印 (Xtensa) | 外部 | 外部 | 外部 |
| **P4 (RISC-V)** | `fmp3_core`（pristine） | **`fmp3_core`（pristine）** | 外部 |

P4 を統合するとき、chip 層は既に `fmp3_core` にある（上流が出しているため）。
S3/無印 の「3層まるごと外部」とは**供給の形が違う**。同じ仕組みで書こうとすると噛み合わない。

---

## 3. バージョン

`fmp3_core` は 3.4.0 に pin（`UPSTREAM_VERSION` = 3.4.0、`UPSTREAM_PRISTINE.txt` =
`b59797f14dedcb07020f96895903ca7fcd14a4af`）。

ユーザは「`fmp3_trunk` は 3.4.0 相当」と述べており、**rebase は不要**の見込み。ただし
信用せず確かめること。あなたの arch 層はカーネル内部インタフェース
（PCB レイアウト、`.trb` が参照するカーネル側変数名など）に対して書かれているので、
食い違えば移植ではなく rebase になる。

### ★`KERNEL_FCSRCS` の罠（`AGENTS.md` §4）

`CMakeLists.txt` は `kernel/*.c` の22個を**手書きで列挙**している。上流が
`kernel/Makefile.kernel` の `KERNEL_FCSRCS` にソースを追加・削除しても
**CMake 側はサイレントに古いまま**追従しない。上流追従のたびに突き合わせが要る。
あなたが 3.4.0 相当かを確認するときも、まずここを見るとよい。

---

## 4. cfg テンプレートの移植（`.trb` → `.py`）

あなたの最大の機械的作業。**規模は我々の直近2ターゲットと同程度**で、十分手が届く。

### 対象の棚卸し（実測）

| ファイル | 行数 |
|---|---|
| `arch/xtensa_gcc/common/core_kernel.trb` | 66 |
| `arch/xtensa_gcc/common/core_offset.trb` | 30 |
| `arch/xtensa_gcc/common/core_check.trb` | 26 |
| `arch/xtensa_gcc/esp32/chip_kernel.trb` | 13 |
| `arch/xtensa_gcc/esp32s3/chip_kernel.trb` | 13 |
| **arch 小計** | **148** |
| `target/esp32_devkitc_gcc/{target_kernel,target_class,target_check}.trb` | 78 / 34 / 9 |
| `target/esp32s3_devkitc_gcc/{target_kernel,target_class,target_check}.trb` | 78 / 34 / 9 |
| **target 小計** | **242** |
| **合計（11ファイル）** | **390** |

参考: `kria_arm64_gcc` は 526行、`kria_r5_gcc` は 439行。どちらも 1〜2 タスクで移植できた。
S3 と無印の target 層は行数が同一なので、**ほぼ同一内容**の可能性が高い（差分を取って確認せよ）。

### Ruby → Python の意味論の罠 — 我々が実際に踏んだものだけ

| 罠 | 症状 | 実例 |
|---|---|---|
| **未定義グローバル** | Ruby は未定義グローバルを `nil` として**空文字を出力**する。Python は `NameError` で落ちる | `arch/riscv_gcc/common/core_offset.py:27-30` の `offsetof_T_EXCINF_cpsr` / `offsetof_PCB_exncnt`、`arch/arm64_gcc/common/core_kernel.py:55-56` ほかの `USE_INTCFG_TABLE` |
| **範囲は閉区間** | Ruby `(32..186)` は 186 を**含む**。Python は `range(32, 187)` | `arch/arm_gcc/zynqmp_r5/chip_kernel.py:16`。**このプロジェクトで実際に off-by-one を一度踏んでいる** |
| **整数除算** | Ruby の `/` は Integer 同士なら切り捨て。Python の `/` は float | `//` を使う |
| **ハッシュの反復順序** | 出力順が変わるとバイト比較が落ちる | — |
| **文字列書式** | `%` 書式・`to_s` の差 | — |

未定義グローバルの対処は `if "NAME" not in globals(): NAME = <既定>` の形。既定値が
`False` なのか `""` なのかは**Ruby が nil をどう出力するか**で決まる
（`core_offset.py:16-20` に実測の記録がある — `#define T_EXCINF_cpsr\t` のように空で出る）。

**Xtensa 固有の注意**: `core_kernel.trb` 66行は我々のどの arch より小さい。しかし
window overflow/underflow を持つ Xtensa は、レジスタウィンドウ関連の offset や
例外フレームの構造が我々の6ターゲットのどれとも似ていない。**既存 `.py` の丸写しは効かない。**

---

## 5. 検証の仕掛け — 我々が渡せる一番価値のあるもの

### `tools/cfg_equivalence.sh`

pristine の Ruby コンフィギュレータ（`cfg/cfg.rb`）と Python 実装（`cfg_py/`）を、
**pass1 から完全に独立した2本のパイプライン**として走らせ、生成物を**バイト比較**する。
比較対象は `cfg1_out.c` / `offset.h` / `kernel_cfg.c` / `kernel_cfg.h`。

```
usage: tools/cfg_equivalence.sh <build-preset-dir> [--pass1-only]
```

**終了コード**:

| 値 | 意味 |
|---|---|
| 0 | 一致（MATCH） |
| 1 | 不一致（MISMATCH） |
| **2** | **実行前提が満たされていない — これは PASS ではない** |

★**exit=2 を成功と読まないこと。** ビルドディレクトリが無い（`:26-28`）、必要な中間生成物が
無い、といった場合に返る。我々は実際に「両エンジンが**同一に失敗**していたのを一致と誤読
しかけた」ことがある。

### ★正直な留保: 外部ターゲットで走らせたことは一度もない

`cfg_equivalence.sh` は `<build-dir>` を受け取り、`build.ninja` からコマンドを抽出して再実行する
作りなので、**外部 target でも動くはず**——だがこれは**推論であって検証された事実ではない**。
`FMP3_TARGET_DIR` 経由のビルドで動かした実績はゼロ。**適応作業が要ると見込むこと。**

### ハーネス自身に欠陥があった履歴 — これが一番の警告

このハーネスは**ターゲットの形に依存しており、新しいターゲットで壊れてきた**：

1. **`cfg1_out.srec` 決め打ち**（Task 6 まで）。`FMP3_DUMP_FORMAT=dump` のターゲットでは
   `.srec` が存在せず exit=2。srec/dump を実体で検出するよう修正。
   ★危険だったのは、拡張子を `.srec` に固定リネームすると**両エンジンとも magic number の
   読み取りに失敗し、「失敗が同一」であるために一致と誤読しうる**点。
2. **`start.S.obj` 決め打ち**（`tools/cfg_error_tests/run.sh`、Task 11 まで）。
   `cfg1_out` にアセンブリオブジェクトが**2個**あるターゲット（`kria_r5`: `common/start.S` +
   `zynqmp_r5/chip_support.S`）で、`start.S` だけ絶対化され他方が相対のまま残りリンク失敗。
   ★**旧ガードは「置換が1件でもあれば OK」という発火条件だったため検出できなかった。**
   現在は `run.sh:211` で「相対パスが1つでも残っていれば FATAL」という**完全性**検査になっている。

**Xtensa で警戒すべきこと**: あなたの `cfg1_out` がリンクするアセンブリの数と名前、
ROM イメージの形式、`nm`/`objcopy`/`objdump` の出力書式。いずれもハーネスが暗黙に
仮定している箇所。**exit=2 が出たら、まずハーネス側を疑ってよい。**

---

## 6. 6ターゲット分の罠 — Xtensa/ESP-IDF で再発しうるものだけ

### A. リンカスクリプトの適用形式（`-T` か `-Wl,-T,` か）

`-T` の適用は `CMakeLists.txt:128` の1箇所に集約され、target が宣言する
`FMP3_LDSCRIPT_VIA_DRIVER_T`（既定 OFF）で切り替わる。

ON が要るのは、**C ライブラリの specs が「gcc ドライバの `-T` スイッチが立っているか」だけを見て
既定リンカスクリプトを追加注入する**場合。`picolibc.specs` の `%{!T:-Tpicolibc.ld}` がこれ
（`CMakeLists.txt:112-124` に説明）。`-Wl,-T,` は gcc ドライバから見ると `-T` が立っていないので
検出されず、**自前のスクリプトと specs の既定が二重に入る**。

ESP-IDF は独自の specs / リンカフラグメントを持つので、**ここは確認が要る**。

### B. `-lc` が要る（上流 Makefile の字面に反して）

`kria_arm64_gcc` で実際にリンクエラー（`undefined reference to memcpy`）になった。上流
`Makefile.core` は `-lgcc` しか書いていないが、`sample/Makefile` が `SRCLANG=c` 判定で
**全ターゲット共通に `-lc` を無条件付加**している。`riscv_gcc` / `arm_m_gcc` / `arm64_gcc` の
`arch.cmake` はいずれも `FMP3_LINK_LIBS` に `c gcc` を積む。

一方 `kria_r5_gcc` は `-lgcc` だけで通った。**ターゲット次第なので、字面でなく実際のリンクで判断する。**

### C. `FMP3_DUMP_FORMAT` を宣言する層は arch（target ではない）

実測（`Makefile.core` の `DUMP` 行）：

| arch | `DUMP` |
|---|---|
| `arm_m_gcc` | `DUMP = dump` |
| `arm64_gcc` | `DUMP = dump` |
| `arm_gcc` | **（無し）** → 汎用既定 `srec` |
| `riscv_gcc` | **（無し）** → 汎用既定 `srec` |

**形式（`FMP3_DUMP_FORMAT`）は arch.cmake で宣言**（`arch/arm64_gcc/common/arch.cmake:39`）、
**オプション（`FMP3_DUMPOPTS`）は target.cmake で宣言**（`target/kria_arm64_gcc/target.cmake:18`）。
この2つは別変数で、置く層が違う。推測せず `Makefile.core` の現物を見ること。

> 未解決として記録済み: `arm_m_gcc` は `DUMP = dump` なのに `musca_b1_gcc` / `rp2350_pico2_gcc` の
> target.cmake は `FMP3_DUMP_FORMAT` を宣言しておらず srec のまま
> （`arch/arm64_gcc/common/arch.cmake:35` のコメント）。上流に忠実でない箇所。

### D. `.trb` の `IncludeTrb` 依存閉包（`cmake/trb_depends.cmake`）

`.trb` は `IncludeTrb("...")` で他の `.trb` を再帰的に読む
（`target_kernel.trb` → `chip_kernel.trb` → `core_kernel.trb` → `kernel/kernel.trb` → …）。
`add_custom_command(DEPENDS ...)` にトップレベルしか入っていないと、**間接的に取り込まれる
`.trb` を編集しても Ninja が「変更なし」と誤判定し、古い生成物が無警告で使われ続ける**
（`cmake/trb_depends.cmake:1-12`）。

`fmp3_trb_closure()` が configure 時に連鎖をテキスト走査で辿り、閉包を `DEPENDS` に足す。
**あなたの Xtensa テンプレートも同じ経路に乗せる必要がある。** 乗せ忘れると症状は
「直したはずの変更が反映されない」——最も気づきにくい類の壊れ方。

### E. ツールチェーンの `-dumpmachine` 検査

`cmake/toolchain_check.cmake` が、ツールチェーンファイルの宣言する
`FMP3_EXPECTED_TOOLCHAIN_MACHINE` と `gcc -dumpmachine` の実測値を突き合わせる
（例: `cmake/toolchain-aarch64-none-elf.cmake:33-34` が `aarch64-none-elf`）。
`xtensa-esp32s3-elf` 用に自分のツールチェーンファイルで宣言すること。

### F. magic number 検査

`cmake/check_magic_number.cmake` が `cfg1_out.syms` に `TOPPERS_magic_number` が
含まれることを確認する。cfg の 3 パス構成では pass1 の出力を**リンクするが実行しない**ため、
リンクが静かに壊れていても後段まで気づけない。そのガード。

---

## 7. 検証の作法 — これが一番伝えたいこと

### ★壊れた検証は、成功と同じ顔をする

**失敗しえない検査は、検査が無いより悪い。証拠として読まれてしまうから。**
positive control（差が出るはずの場合に実際に差が出ることの実演）と negative control を
**対で**用意すること。

このセッションで実際に出た型：

| 型 | 具体例 |
|---|---|
| **空 vs 空** | `diff -q` は**2つの空ファイルを「同一」と判定する**。両パイプラインが同一に失敗した状態が「一致」に見えた |
| **死んだ分岐への変異** | `TNUM_PRCID==1` の分岐に変異を植えても、既定 `TNUM_PRCID=4` のビルドでは**何も証明しない**。計画自身がこの罠を警告していたのに、計画の sed がまさにそれを踏んでいた |
| **部分成功を全体成功と誤読するガード** | 「置換が0件なら FATAL」は「2件中1件だけ置換」を見逃す（§5-2） |
| **出力が静かに消える** | QEMU の `-serial` を1個しか渡さないと、UART1 への出力が**エラーも出さず消える**。ハングやクラッシュに見える（`hw/arm/xlnx-zynqmp.c` は n 番目の `-serial` を UART*n* に割り当てる） |
| **ビルドの成功判定** | `.o` の個数で成否を判定していたスクリプト、`EXTRA_OFLAGS` を黙殺してバイト同一のバイナリを吐いていたスクリプトの実例が出た |

### 誰の報告も、成果物と対照実験より下

このセッションで、**制御側（我々）の検証が10回間違い、そのたびにサブエージェントが正しかった**。
内訳の例: QEMU のバージョンとオプションを間違えて「起動しない」と誤認、パイプで
`$?` が別コマンドの終了コードになっていた、POST_BUILD の実行をビルドログの grep で判定
（POST_BUILD はログに出ない）、など。

**この文書の記述も含めて、報告より成果物と対照実験を信じること。**
我々の brief は6タスク連続で誤りを含んでおり、実行者が現物で訂正したのが毎回正しかった。

---

## 8. 我々が知らないこと（境界を明示する）

**以下はすべて未経験。この文書がそれ以上の保証を与えているように読まないこと。**

- **Xtensa をビルドしたことがない。** 6ターゲットは RISC-V / ARM Cortex-M / AArch64 / ARM32。
  あなたの `arch/xtensa_gcc`（window overflow/underflow、IPI、FPU の eager save/restore）に
  **対応物が我々の側に一つも無い**。
- **seam boot を動かしたことがない。** 我々の6ターゲットはすべて QEMU 直起動か素の ELF。
  ESP-IDF の 2nd-stage bootloader から FMP3 のエントリへ跳ぶ方式は完全に未知。
- **外部リポジトリから `fmp3_core` を消費した実績がゼロ。** §1 の契約は include 連鎖を
  読んで確認した事実だが、**実際に外から使われたことはない。** `FMP3_TARGET_DIR` と
  `FMP3_LIBRARY_ONLY` は「そのつもりで作った受け口」であって、実地試験は**あなたが最初**。
- **等価性ハーネスを out-of-tree で走らせたことがない**（§5）。
- **ESP-IDF のビルドと CMake の噛み合わせ**について何も知らない。

加えて、我々の側に**未解決事項が7件**記録されている（`DIVERGENCE_MAP.md` の「未解決事項」）。
外部利用者に効きうるのは §1 の2件（`fmp3_add_syssvc()` / `fmp3_cfg_check()` のスコープ依存）と
コンパイルオプションの順序が上流と逆で **target が `-O2` を上書きできない**件。

---

## 9. 我々なら最初に踏む3歩

1. **契約が成立することを、最小の面積で確かめる。**
   S3 の target/chip/arch の `.cmake` 三点セットを書き、`cmake -DFMP3_TARGET_DIR=<外部パス>` で
   **`libfmp3.a` が出るところまで**（`FMP3_LIBRARY_ONLY=ON` でリンクを避ける）。
   cfg テンプレートの移植より先にこれをやる。**ここが通らなければ他は全部無駄になる**し、
   通れば「arch ごと外部供給」という前例のない形が成立する証拠が最初に手に入る。

2. **`cfg_equivalence.sh` を外部ターゲットで走らせてみる。**
   ほぼ確実に何か壊れる（§5）。**壊れ方を早く知るほど安い。** exit=2 が出たらハーネス側を疑う。
   ここが動くようになれば、以降のテンプレート移植390行は「Ruby とバイト一致」という
   **決定的な判定基準**の上で進められる。これは「ビルドが通る」「起動する」より遥かに強い。

3. **テンプレート移植は S3 と無印を続けてやる。**
   両者の target 層は行数が完全に一致しており、共通の `arch/xtensa_gcc/common` を共有する。
   **2つ目が chip 層の切り方の誤りを即座に炙り出す。** 我々は polarfire の次に musca_b1 を
   足した時点で、汎用層に残っていた `POLARFIRE_QEMU` 決め打ちの漏れを発見した——
   1ターゲットだけでは「層が正しく切れている」ことを確かめられない。

---

## 参照

- `AGENTS.md` — `fmp3_core` の規約の正本（§4 に上流追従手順と `KERNEL_FCSRCS` の罠）
- `DIVERGENCE_MAP.md` — pristine への乖離台帳と未解決事項
- `docs/superpowers/specs/2026-07-18-fmp3-cmake-design.md` — 設計書。§9.8 に
  ESP 系統合リポジトリの起動方式3種（Direct Boot / seam boot / ローダ殻）の整理がある
- `target/kria_arm64_gcc/` と `target/kria_r5_gcc/` — 最も新しい2つの実例。
  ZynqMP という同一 SoC を APU/RPU で分けており、**chip 層の分け方の参考になる**
