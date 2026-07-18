# fmp3_core CMake 化 設計案 v3（2026-07-18）

対象: `/home/honda/TOPPERS/FMP3/fmp3_core`

v1 → v2 は Fable と codex の2者レビューを現物で裏取りして反映（§10）。
v2 → v3 は **esp32_p4 を第1波に追加**したことによる（§11）。

**第1波の対象は 2 ターゲット**:

| | `polarfire_soc_kit_gcc` | `m5stamp_esp32p4_gcc` |
|---|---|---|
| 割込み制御器 | PLIC | **CLIC** |
| プロセッサ数 | 4（既定） | **2**（`PRC_NUM=2` 必須） |
| 成果物 | 独立 ELF | **`libfmp3.a`＋ESP-IDF ローダ殻** |
| 実行検証 | `qemu-system-riscv64` | **`esp-emu`**（マージ済フラッシュ像） |

**この2つは意図的に性質が異なる。** 片方だけでは「たまたま動く設計」に気づけないため、
層の切り方の妥当性を早期に検出する目的で組み合わせている。

## 0. 現状

- FMP3 3.4.0 pristine を vendor ブランチ `upstream` 経由で取り込み済み（archive commit `f3d29a4`）。
- target は 6 個: `m5stamp_esp32p4_gcc` `musca_b1_gcc` `rp2350_pico2_gcc`
  `polarfire_soc_kit_gcc` `kria_arm64_gcc` `kria_r5_gcc`。
- `CMakeLists.txt` は 6 行の雛形、`cfg_py/` は README のみ。ビルドは通らない。

## 1. 参考実装（実測）

### 1.1 `asp3_core`（`/home/honda/TOPPERS/ASP3CORE/asp3_core`）

ASP3 の CMake 化。層構造・presets・run ターゲットが成熟している。cfg は Python 1.7.1。
`.trb` は `.py` 化済みで `IncludeTrb()` が Python を `exec` する。`test_cfg/` に golden 方式の回帰スイート。

**ただし「写せば安全」ではない**: `CMakeLists.txt` に `DEPFILE` は 0 件、timestamp 処理も無い。
すなわち §6-1（depfile 未接続）と §6-2（毎ビルド再実行）は **asp3_core にも存在する**。

### 1.2 `fmp3_pico_sdk`

FMP3 3.3.0 の Pico 専用 CMake 化。**FMP3 用 Python cfg 一式がある**（`kernel/*.py` 全部、
FMP3 固有の `spin_lock.py`、`target_class.py`）。ただし `arch/` は `arm_m_gcc` のみ。
CMake には実害のある不具合が複数（§6）。

### 1.3 cfg エンジンの選定 — asp3_core 1.7.1

| ファイル | asp3_core | fmp3_pico_sdk | 差分 |
|---|---|---|---|
| `cfg.py` | 787 | 779 | 12 |
| `pass1.py` | 945 | 963 | 20 |
| `pass2.py` | 575 | 560 | 53 |
| `gen_file.py` / `srecord.py` | — | — | 0（バイト同一） |

差分の実体は 2 つだけ: (a) `E_OBJ`（オブジェクト識別名重複）チェックの pass1→pass2 移設、
(b) `error_flag` が `__main__`/`cfg` の二重モジュールで握り潰される実バグの修正。
**FMP 経路（`--kernel fmp` / `CLASS` 解析 / `_class_proc` / `affinityPrcBitmap`）は両者バイト同一。**

決定打: **pristine FMP3 3.4.0 の Ruby cfg 自身が VERSION 1.7.1**（`cfg/cfg.rb:58`）で、
`E_OBJ` も `pass2.rb` 側にある。つまり asp3_core 1.7.1 は上流 Ruby 1.7.1 への追従であり、
**§7 のオラクルと版が揃う**。1.7.0 を採るとオラクルと挙動がずれる。

### 1.4 移植対象の実量（v1 の見積もりは約2.5倍過小だった）

**kernel/**: 3.3.0 → 3.4.0 で変わったのは 15 個中 3 個のみ。

| ファイル | 変更行数 | 性質 |
|---|---|---|
| `interrupt.trb` | 106 | 意味変更あり（`OMIT_MULTIPRC_INTERRUPT` 新設、`:affinity`→`:affinityPrcBitmap`、`TargetCheckCfgInt2` と2巡目ループの削除）。約6箇所の外科的編集 |
| `kernel_check.trb` | 80 | `SYMBOL(sym, true)` で包む機械的変更。エンジンは `cont_flag` 対応済み |
| `kernel.trb` | 20 | 機械的 |
| 他 12 個 | 0 | fmp3_pico_sdk の `.py` をそのまま流用 |

**arch/riscv_gcc/**: ★**すべて 3.4.0 で新規**（`git cat-file -e v3.3.0:<path>` で 1 個ずつ確認）。
fmp3_pico_sdk に流用元は無く（arch は `arm_m_gcc` のみ）、asp3_core の RISC-V `.py` は
単一プロセッサ版で別物（`core_kernel` は ASP3 73 行 vs FMP3 209 行）。

| ファイル | 行数 |
|---|---|
| `arch/riscv_gcc/common/core_kernel.trb` | 209 |
| `arch/riscv_gcc/common/core_check.trb` | 92 |
| `arch/riscv_gcc/common/plic_kernel.trb` | 79 |
| `arch/riscv_gcc/common/core_offset.trb` | 33 |
| `arch/riscv_gcc/polarfire_soc/chip_kernel.trb` | 36 |
| **polarfire 経路 計** | **449** |
| `arch/riscv_gcc/common/clic_kernel.trb`（esp32p4） | 47 |
| `arch/riscv_gcc/esp32p4/chip_kernel.trb`（esp32p4） | 38 |
| **第1波 合計** | **534** |

**この 534 行はすべて Python 前例ゼロの FMP3 固有生成コード**（プロセッサ別 inh/intcfg/exc テーブル、
クラス別セクション関数、`GenerateNativeSpn`）であり、最もリスクが高い。

呼び出し連鎖はターゲットごとに異なる:

- polarfire: `target_kernel.py` → `chip_kernel.py` → `core_kernel.py` + `plic_kernel.py`
- esp32p4: `target_kernel.py` → `chip_kernel.py` → `core_kernel.py` + **`clic_kernel.py`**
- 両者共通の末端: `core_kernel.py` → `kernel/kernel.py`

（`IncludeTrb` の実測。`chip_kernel.trb:33,38`(esp32p4) / `:31,36`(polarfire) / `core_kernel.trb:115`）

**割込み制御器が PLIC / CLIC で分かれるため、前半の移植が正しくても後半の保証にはならない**
（§8-9 で esp32p4 の `.cfg` に対しても差分等価性検査を回す理由）。

## 2. 決定事項

| 項目 | 決定 | 根拠 |
|---|---|---|
| 第1波の対象 | `polarfire_soc_kit_gcc` → `m5stamp_esp32p4_gcc` の順 | 性質の異なる2つで層の切り方を検証する。順序は、外部 SDK 依存の無い polarfire で cfg パイプラインを確立してから、esp32p4 の IDF 統合を載せる（cfg 移植のバグと IDF 統合のバグを同時に出さないため） |
| 最初のターゲット | `polarfire_soc_kit_gcc` | QEMU 検証可能。`TNUM_PRCID` 既定 4 で FMP3 固有部分を最初から踏める。TTSP3 実績あり |
| ライブラリモード | **最初から設計に入れる**（後付けしない） | esp32p4 が `libfmp3.a` を要求するため（§3.1）。後から入れると層の切り方をやり直すことになる |
| cfg エンジン | asp3_core 1.7.1 を `cfg_py/` へ | §1.3。オラクルと版が揃う |
| テンプレート配置 | pristine 並置 | 参考 repo 同型。`IncludeTrb` の探索パスが揃う |
| 検証 | Ruby cfg との差分等価性検査（§7.1） | golden 不要、positive control 可能 |
| 進め方 | Ruby cfg で骨格を先に通す → Python へ | 切り分け可能性。Ruby がオラクルになる |

## 3. 層構造と変数プロトコル

```
CMakePresets.json ─include▶ target/<t>/presets.json ─inherits▶ cmake/presets-base.json
CMakeLists.txt ─include▶ fmp3_core.cmake ─include▶ target/<t>/target.cmake
                                                     └include▶ arch/riscv_gcc/polarfire_soc/chip.cmake
                                                                  └include▶ arch/riscv_gcc/common/arch.cmake
```

asp3_core と同名（接頭辞のみ `FMP3_`）:
`FMP3_CFG_FILES` / `FMP3_KERNEL_CFG_TRB_FILES` / `FMP3_CHECK_TRB_FILES` /
`FMP3_OFFSET_TRB_FILES` / `FMP3_SYMVAL_TABLES` / `FMP3_API_TABLES` / `FMP3_INCLUDE_DIRS` /
`FMP3_COMPILE_DEFS` / `FMP3_COMPILE_OPTIONS` / `FMP3_LINK_OPTIONS` / `FMP3_LINK_LIBS` /
`FMP3_LDSCRIPT` / `FMP3_ARCH_C_FILES` / `FMP3_TARGET_C_FILES` / `FMP3_SYSSVC_TARGET_C_FILES` /
`FMP3_START_FILES` / `FMP3_RUN_COMMAND`

**FMP3 のために増えるもの（v1 では「1つだけ」と書いたが誤り。3つ）**:

1. **`FMP3_CLASS_TRB_FILES`** — cfg の `-C`（クラス定義）に渡す。
2. **`FMP3_PRC_NUM`**（キャッシュ変数、既定はターゲットの `target_kernel.h` に委ねる） —
   指定時のみ `-DTNUM_PRCID=${FMP3_PRC_NUM}` を `FMP3_COMPILE_DEFS` に足す。
   上流の `sample/Makefile:193-194` の `PRC_NUM` に対応する。
   **これは CMake の分岐材料ではなく、コンパイル定義に落とすだけの入口**である。

   省略不可である根拠は 2 つ:
   - **esp32p4 では必須**。ESP32-P4 は HP コア 2 基だが `target/m5stamp_esp32p4_gcc/target_kernel.h:18`
     の `TNUM_PRCID` 既定値は **4** であり、上流の `tools/fmp_loader/build_fmp3_lib.sh` は
     `PRC_NUM=2` を明示的に渡している（`CFG_ARGS+=(PRC_NUM="${PRC_NUM:-2}")`）。
     手段が無いとこのターゲットは正しくビルドできない。
   - polarfire では TTSP3 の `mtskman1`〜`3` / `mmutex1` が `PRC_NUM=2` でないと完了しない。
3. **`FMP3_CFG1_OUT_LINK_OPTIONS`** — `cfg1_out` 専用のリンクオプション（§5 参照）。

**`TNUM_PRCID` の値そのもので CMake を分岐させてはならない。** これは C マクロで、
`cfg1_out` をリンクして初めて cfg が知る値である（`target_class.trb` が 1〜4 で分岐する）。
ファイルリストやターゲット構成の分岐材料には使えない。

マルチプロセッサ性は**生成物の中身**に現れる（`_kernel_pcb_prc{N}`、`_kernel_istack_prc{N}`、
プロセッサ別時間イベントヒープ、`INIRTNBB` の `TNUM_PRCID + 1` バケツ）。
単一 `kernel_cfg.c`・単一 ELF のまま（`sample/Makefile:275` `:453` で確認）。

**ただしリンカスクリプトには現れる。** `arch/riscv_gcc/common/core_kernel.trb` の
`SecnameKernelData` / `SecnameStack` が `.kernel_data_<clsid>` / `.stack_<clsid>` を生成する。
`target/kria_arm64_gcc/kria.ld:81-82` は `*(.kernel_data_*)` `*(.stack*)` を明示回収しているが、
polarfire の `.ld` は列挙しておらず orphan section のまま動いている（TTSP3 実績あり）。
**ターゲット追加時の確認項目**とする（§8 の横展開チェックリスト）。
esp32p4 では「orphan で放置」が成立せず、能動的なリネームで解決している（§3.1）。

### 3.1 ライブラリモード（esp32p4 が要求。後付けしない）

`FMP3_LIBRARY_ONLY` オプションを設ける（asp3_core の `ASP3_LIBRARY_ONLY`、
`CMakeLists.txt:28` / `:358` / `:509` が前例）。ON のとき `libfmp3.a` までを作り、
**実行ファイル・run ターゲット・pass3 の POST_BUILD を作らない**。

esp32p4 はこのモードで ESP-IDF の CMake プロジェクトから取り込まれる。
上流では `target/m5stamp_esp32p4_gcc/tools/fmp_loader/main/CMakeLists.txt` が
`add_custom_command` で `build_fmp3_lib.sh` を呼び、それが `configure.rb`/`make` で
`libfmp3.a` を作っている。**この内側の `configure.rb`/`make` を我々の CMake が置き換える。**

#### なぜライブラリなのか（方式(b) が棄却されている）

`target/m5stamp_esp32p4_gcc/idf_image_integration.md:9`:

> 方式(b)「完全独立 ELF + elf2image」は棄却．主理由: `bootloader_utility.c:842` の
> `assert(rom_index==2)`（flash セグメントちょうど 2 本前提）を RAM-only イメージが踏む．

したがって「FMP3 単体の ELF を焼く」経路は選べない。**ライブラリモードは
esp32p4 にとって選択肢ではなく前提**である。

#### esp32p4 固有のビルド要件

| 要件 | 内容 | 出典 |
|---|---|---|
| 起動時初期化の抑止 | `-DTOPPERS_OMIT_BSS_INIT -DTOPPERS_OMIT_DATA_INIT`。`.data`/`.bss` は IDF 起動が初期化するため `start.S` の自前初期化を抑止する | `build_fmp3_lib.sh`（`CFG_ARGS+=(-O …)`） |
| ABI | `-march=rv32imafc_zicsr_zifencei_xesppie -mabi=ilp32f -mcmodel=medany`。IDF が ilp32f でビルドされており、静的リンクするには一致が必要 | `arch/riscv_gcc/esp32p4/Makefile.chip:27-28,45` |
| セクション分割の抑止 | `-fno-function-sections`（`SECTION_OPTS` を上書き） | `build_fmp3_lib.sh` |
| `.text` の IRAM 移送 | 全 `.text` → `.iram1.fmptext` にリネーム。理由は (1) FMP3 の巨大な `.text` を IDF の `.flash.text`(XIP) に足すと flash MMU 窓 `.flash_rodata_dummy` と overlap する、(2) FMP3 は元々 RAM 実行前提 | `build_fmp3_lib.sh` |
| **クラス別セクションの回収** | `.kernel_data_CLS_*` / `.stack_CLS_*` → `.data.fmp<sanitized>` にリネーム。IDF の `sections.ld` は `*(.data .data.*)` で拾うため、リネームすれば IDF の配置・初期化に自動的に乗る。**しないと orphan となり overlap する** | `build_fmp3_lib.sh` |
| `cfg1_out*.o` の除外 | archive に含めると `sta_ker` / `_kernel_istkpt_table` 等のスタブが `kernel_cfg.o`(pass2) と衝突して multiple definition になる | `build_fmp3_lib.sh` |

**セクションリネームはアーカイブ作成前の各 `.o` への後処理**であり、CMake の実要件である。
しかも**セクション名は `PRC_NUM`（クラス数）に依存する**ため固定リストにできない。
上流は `objdump -h` で各 `.o` の実セクションを走査し、見つかったものだけを動的にリネームしている:

```sh
for o in objs/*.o; do
  args="--rename-section .text=.iram1.fmptext"
  secs=$(riscv32-esp-elf-objdump -h "$o" | awk '/\.kernel_data_CLS_|\.stack_CLS_/{print $2}')
  for s in $secs; do
    san=$(echo "$s" | sed 's/[^A-Za-z0-9]/_/g')
    args="$args --rename-section $s=.data.fmp$san"
  done
  riscv32-esp-elf-objcopy $args "$o" "$o"
done
```

この「走査してから改名する」性質は CMake の宣言的な記述と相性が悪い。
**スクリプトを1本用意して `add_custom_command` から呼ぶ**のが素直だと考える（推測。
実装時に、CMake の `TARGET_OBJECTS` ジェネレータ式で足りるかを確かめる）。

#### pass3 の扱い

ライブラリモードでは最終 ELF が無いため、pass3（`fmp3_cfg_check`）はこちらでは走らせられない。
**IDF 側の最終 ELF に対して呼べるよう、pass3 の引数一式を親スコープへ export する。**
fmp3_pico_sdk の `CMakeLists.txt:248-249` が前例:

```cmake
set(FMP3_PASS3_ARGS ${PASS3_ARGS} PARENT_SCOPE)
set(FMP3_KERNEL_CFG_DIR ${KERNEL_CFG_DIR} PARENT_SCOPE)
```

なお fmp3_pico_sdk では `--gc-sections` により pass3 が必要とするシンボルが消えるため
実際には無効化されている。esp32p4 で同じ問題が起きるかは未確認。

## 4. 配置

```
CMakeLists.txt  fmp3_core.cmake                          ← 派生（新規）
cmake/presets-base.json  cmake/toolchain-riscv64.cmake
cfg_py/ cfg.py pass1.py pass2.py gen_file.py srecord.py   ← 派生（asp3_core 1.7.1 ベース）
tools/cfg_equivalence.sh                                 ← 派生（§7.1 のオラクル比較。CMake 外）
kernel/ kernel.py task.py … spin_lock.py                 ← 派生（pristine 並置・15個）
arch/riscv_gcc/
  common/  arch.cmake  core_kernel.py  core_check.py  plic_kernel.py  core_offset.py
  polarfire_soc/  chip.cmake  chip_kernel.py           ← 派生（★449行の新規移植）
target/polarfire_soc_kit_gcc/
  target.cmake  presets.json
  target_kernel.py  target_class.py  target_check.py

（第1波 後半・esp32p4）
arch/riscv_gcc/
  common/  clic_kernel.py                              ← 派生（47行）
  esp32p4/  chip.cmake  chip_kernel.py                 ← 派生（38行）
target/m5stamp_esp32p4_gcc/
  target.cmake                                         ← 派生（presets.json の要否は未決。§9）
  target_kernel.py  target_class.py  target_check.py
  tools/fmp_loader/                                    ← pristine（IDF ローダ殻。CMake から改変しない）
```

ライブラリ名 `fmp3`、実行ファイル名 `fmp`。プリセット名 `polarfire_soc_kit` /
`polarfire_soc_kit-qemu`。pristine へ追加する `.py` は DIVERGENCE_MAP.md に `add` で 1 行ずつ記録。

## 5. cfg パイプライン

`cfg1_out` は**実行しない**（クロスコンパイルが成立する理由）。

1. **pass1**: `--pass 1 --kernel fmp -I… --api-table… --symval-table… -M cfg1_out_c.d <cfg files>`
   → `generated/cfg1_out.c`
2. **`add_executable(cfg1_out …)`** をクロスツールチェーンでリンク
3. `nm -n` → `.syms`、`objcopy -O srec -S` → `.srec`
4. **pass2 (offset)**: `--pass 2 -O --kernel fmp -C target_class.py -T core_offset.py --rom-symbol … --rom-image …`
5. **pass2 (kernel_cfg)**: `--pass 2 --kernel fmp -C target_class.py -T target_kernel.py`
6. **pass3**: 最終 ELF に POST_BUILD で `nm`/`objcopy` → `--pass 3 -T target_check.py`

作業ディレクトリは全パス `${CMAKE_BINARY_DIR}/generated` に固定する
（`cfg1_out.db` `cfg1_out.syms` `cfg1_out.srec` `cfg2_out.db` を裸の相対名で読み書きするため、
cwd が load-bearing）。

### 5.1 `cfg1_out` 専用リンク（★見落としやすい）

`cfg1_out` は `_start` を参照しないため、`--gc-sections` が効くと `TOPPERS_magic_number` が
除去され、pass2 が `cfg1_out.syms` から見つけられずに `error_exit` で停止する。
上流 polarfire は明示的に抑止している:

```make
# target/polarfire_soc_kit_gcc/Makefile.target:108-110
#  cfg1_out のリンクは _start を参照しないため --gc-sections で
#  TOPPERS_magic_number が除去される．これを抑止する．
CFG1_OUT_LDFLAGS := $(CFG1_OUT_LDFLAGS) -Wl,--no-gc-sections
```

asp3_core は `CMakeLists.txt:240` の `list(REMOVE_ITEM CFG1_LINK_OPTIONS "-Wl,--gc-sections")`
で対処しているが、これは**文字列完全一致での除去**であり、別綴りやツールチェーン既定で
入った場合に効かない。**本設計は polarfire 方式（`-Wl,--no-gc-sections` を明示的に足す）を採る**。
`FMP3_CFG1_OUT_LINK_OPTIONS` に既定でこれを入れ、target がさらに追加できるようにする。

### 5.2 依存関係の正しい接続

- `-M` の depfile を **CMake の `DEPFILE` に接続する**（`.cfg` が `#include` するヘッダを追跡）。
- `gen_file.py` は内容不変ならファイルを書き直さない。したがって `add_custom_command` の
  `OUTPUT` に `kernel_cfg.c` を直接宣言すると**毎ビルド cfg が再実行される**。
  上流 Makefile と同じく **`*.timestamp` を `OUTPUT` に宣言**し、`.c`/`.h` は
  `BYPRODUCTS` 扱いにする。
- 依存に `FMP3_API_TABLES` / `FMP3_SYMVAL_TABLES` を確実に含める。

## 6. 参考実装の不具合（持ち込まない）

fmp3_pico_sdk に実在（`CMakeLists.txt` 現物で確認）:

1. `-M` の depfile が `DEPFILE` に未接続 → `.cfg` の include ヘッダを編集しても pass1 が再実行されない
2. `gen_file.py` のタイムスタンプ非更新と `OUTPUT` 宣言の不整合 → 毎ビルド cfg 再実行
3. `DEPENDS ${CFG_API_TABLE}`（単数・未定義変数）→ `.def` が依存に入っていない
4. `cmake_minimum_required(3.13)` と `ZIP_LISTS`(3.17 必要) の不整合
5. `FMP3_COMMON_LANG_FLAGS` 等が未定義で当該ブロックが no-op
6. `cfg1_out.bin` のデッドルール
7. `>` リダイレクトのシェル依存

**1 と 2 は asp3_core にも存在する**（§1.1）。「asp3_core を写せば再発しない」は成り立たない。
修正後は positive control を必ず実演する（ヘッダ編集 → pass1 が実際に再実行されること、
無変更再ビルド → cfg が実際に走らないこと）。

## 7. 検証

### 7.1 Ruby cfg との差分等価性検査（主検証）

pristine の `cfg/cfg.rb`（VERSION 1.7.1）と ruby 3.2.3 が使える。

**Ruby と Python を完全に独立したパイプラインとして pass1 から走らせ**、生成物を比較する。
pass2 は各実装が自前形式の中間ファイル（Ruby は PStore、Python は pickle）を要求するため、
「同じ `cfg1_out.syms`/`.srec` を両者に与える」ことはできない。

比較対象:

| 対象 | 理由 |
|---|---|
| `kernel_cfg.c` / `kernel_cfg.h` | 本体 |
| `offset.h` | 本体 |
| **`cfg1_out.c`** | ★これを外すと **Python pass1 の生成ミスを検出できない**。Python が `TOPPERS_cfg_static_api_N` や `TOPPERS_cfg_clsid_N` を欠落させても、Ruby 由来の syms/srec を使う比較なら通ってしまう。製品ビルドでは Python 自身の `cfg1_out` がリンクされるため、そこで初めて壊れる |

不成立条件は探した上で、単独では成立を壊さないと判断した:
生成物にバージョン・タイムスタンプの刻印は無い / Ruby(≥1.9)・Python(≥3.7) とも挿入順保存 /
上流自身が unstable sort を認識して安定化済み（`kernel/interrupt.trb:457` `i = 0 # stable sortを行うための変数`）/
浮動小数点はテンプレート未使用 / 負数 `%x` は非負の `affinityPrcBitmap` のみ / 行末は両者 LF。
**ただしこれらは「実測で一致するはず」の根拠であって、一致の保証ではない。**

**positive control が必須**: テンプレートを 1 箇所わざと壊したときに実際に差分が出ることを
実演してから、「一致」を根拠として採用する。壊れた比較も「全部一致」を返しうる。

**規約との整合**: AGENTS.md §2-3 / §5 は「pristine の `cfg/` を CMake から参照しない」と定める。
したがって **Ruby オラクルは CMake に一切入れず、`tools/cfg_equivalence.sh`（CMake 外のスクリプト）に置く**。
製品ビルドの CMake は `cfg_py/` の Python のみを呼ぶ。

### 7.2 エラー検出経路の回帰スイート（★v1 では優先度低と誤判断）

差分等価性検査は**正常系しか見ない**。正しい `.cfg` ではエラーも警告も生成物に現れないためである。
そして 3.3.0→3.4.0 の `interrupt.trb` 差分の過半は**新規エラーチェック・警告の追加**である。
よって「§7.1 があるので回帰スイートは不要」は成り立たない。

エラーを起こす `.cfg` に対する最小限のスイートを持つ（asp3_core の `test_cfg/` 方式）。
規模は最小で良いが、`interrupt.trb` の新規チェックは必ず1件ずつ覆う。

### 7.3 実行検証（従検証）

第1波の 2 ターゲットは**実行経路が全く異なる**。共通化できるのは
「`FMP3_RUN_COMMAND` を target.cmake が定める」という枠だけである。

#### 7.3.1 polarfire / QEMU

**asp3_core の `ASP3_RUN_COMMAND` は流用できない。** icicle-kit は既定で envm にリセットして
HSS を待つため、5 ハート全てへイメージを配る必要がある
（`target/polarfire_soc_kit_gcc/target_user.md:182-186`）:

```
qemu-system-riscv64 -M microchip-icicle-kit -smp 5 -m 2G -nographic \
  -serial mon:stdio -bios none -kernel fmp \
  -device loader,file=fmp,cpu-num=0 -device loader,file=fmp,cpu-num=1 \
  -device loader,file=fmp,cpu-num=2 -device loader,file=fmp,cpu-num=3 \
  -device loader,file=fmp,cpu-num=4
```

E51（hart0）は MPFS HAL により待機し、U54（hart1〜4 = PRC1〜4）で FMP3 が動く。
上流には `QEMU=1` によるビルド切替もある（`Makefile.target:17` 以降でリンカスクリプト・
picolibc・`-DTOPPERS_USE_QEMU` を選択）。asp3_core の `POLARFIRE_QEMU` オプション相当を設ける。

#### 7.3.2 esp32p4 / esp-emu

```
esp-emu --chip esp32p4 --firmware <merged.bin>
```

**ELF ではなくマージ済みフラッシュ像を要求する**（`idf.py merge-bin` の出力）。
polarfire の `-kernel fmp` とは全く別経路であり、`FMP3_RUN_COMMAND` の中身どころか
**入力成果物の種類が違う**。ライブラリモードでは我々の CMake は ELF を作らないため、
実行像を作るのは IDF 側（`idf.py build` → `idf.py merge-bin`）になる。

確認済みの事実:

- `esp-emu` は `/home/honda/.local/bin/esp-emu` にインストール済み。
- `--chip` が受け付ける値に `esp32p4` が含まれる（`esp-emu --help` の
  `Target chip (esp32c3, esp32c6, esp32h2, esp32p4, esp32s31)`）。
- `--firmware` の説明が `Path to firmware binary (merged flash image)` である。

未確認（重要）:

- **このマシンで esp32p4 が実際に動作するかは未確認。** 動作確認済みなのは C3 である。
- esp-emu は QEMU ベースではなく Rust 実装で、P4 のデュアルコア SMP をサポートすると
  公称しているが、**FMP3 の SMP 動作がその上で成立するかは踏んでみるまで分からない**。
  マイルストーン 12（§8）はここが失敗しうる前提で立てる。

## 8. マイルストーン

**前半（polarfire）を縦に通してから、後半（esp32p4）を載せる。**
cfg 移植のバグと IDF 統合のバグを同時に出さないため（§2）。

### 前半: polarfire を縦に通す

1. `cmake/`（presets-base, toolchain-riscv64）+ `fmp3_core.cmake` + `CMakeLists.txt` 骨格
   — **`FMP3_LIBRARY_ONLY` の分岐をこの時点で入れる**（後付けしない。§3.1）
2. `target/polarfire_soc_kit_gcc/{target.cmake, presets.json}` +
   `arch/riscv_gcc/{common/arch.cmake, polarfire_soc/chip.cmake}`
   — `FMP3_CFG1_OUT_LINK_OPTIONS`（§5.1）と `FMP3_PRC_NUM`（§3）を含める
3. **Ruby cfg 経路**（CMake 外のスクリプトから駆動）で `sample1` が QEMU で動く
   → CMake パイプラインの検証完了。§7.3.1 のコマンドで判定
4. `cfg_py/` へ asp3_core 1.7.1 エンジンを移植
5. テンプレート移植（polarfire 経路）:
   - 5a. `kernel/` 15 個（fmp3_pico_sdk 流用 12 ＋ 3.4.0 差分 3 = 約 206 行の編集）
   - 5b. ★**`arch/riscv_gcc/` 5 個・449 行の新規移植**（前例ゼロ・最リスク）
   - 5c. `target/polarfire_soc_kit_gcc/` 3 個
6. **差分等価性検査**（§7.1、positive control 込み）＋ **エラー経路スイート**（§7.2）
7. 製品ビルドを Python 経路へ切替。Ruby はオラクル用スクリプトとしてのみ残す
8. 依存関係の正しさを positive control で実演（§6）

### 後半: esp32p4 を載せる

9. テンプレート移植（esp32p4 経路・**85 行**）:
   - `arch/riscv_gcc/common/clic_kernel.py`（47 行）
   - `arch/riscv_gcc/esp32p4/chip_kernel.py`（38 行）
   - `target/m5stamp_esp32p4_gcc/` の 3 個
   → ここで**差分等価性検査を esp32p4 の `.cfg` に対しても回す**（§7.1）。
     CLIC 経路は polarfire の PLIC 経路と別物なので、前半の一致は根拠にならない。
10. **`FMP3_LIBRARY_ONLY=ON` で `libfmp3.a` がビルドできる**
    — ABI（ilp32f）、`TOPPERS_OMIT_*_INIT`、`-fno-function-sections`、
      セクションリネーム、`cfg1_out*.o` 除外（§3.1）
    → 判定は `libfmp3.a` の中身を上流 `build_fmp3_lib.sh` の生成物と突き合わせる
      （obj 一覧・各 obj のセクション名を `objdump -h` で比較）。**ビルドが通っただけで
      成功としない**（セクションリネームの漏れはリンクするまで表面化しない）。
11. **IDF ローダ殻との統合** — `tools/fmp_loader` の `main/CMakeLists.txt` が
    `build_fmp3_lib.sh` を呼んでいる箇所を、我々の CMake を呼ぶ形に差し替える。
    `idf.py build` が通り、最終 ELF がリンクできること。
    pass3 の引数 export（§3.1）がここで効く。
12. **esp-emu での実行** — `idf.py merge-bin` → `esp-emu --chip esp32p4 --firmware …`。
    §7.3.2 のとおり**この経路は未検証**なので、失敗しうる前提で立てる。
    失敗した場合は実機（M5Stamp ESP32P4）へフォールバックする。

13. 残り 4 ターゲットへ横展開

### 横展開チェックリスト（ターゲット追加時）

- [ ] **ライブラリモードが要るか**（外部 SDK にリンクされるターゲットか）。
      要るならセクションリネームの要否と、pass3 を誰が呼ぶかを決める
- [ ] リンカスクリプトが `.kernel_data_*` / `.stack_*` を回収するか、orphan で問題ないか、
      それとも esp32p4 のようにリネームで解決するか
- [ ] `cfg1_out` のリンクで `--gc-sections` が効いていないか
- [ ] `arch/` 側テンプレートの有無（割込み制御器が PLIC か CLIC か等でチップ層が変わる）
- [ ] 実行手段（QEMU / エミュレータ / 実機）と、それが要求する成果物の種類（ELF / フラッシュ像）
- [ ] `FMP3_PRC_NUM` の既定値がそのチップの実コア数と合っているか
      （esp32p4 は `target_kernel.h` 既定 4 に対し実コア 2）

## 9. 未決事項

- syssvc / library の取り込み方（asp3_core の `asp3_add_syssvc()` 関数方式を採る想定）。
- FMP3 の syssvc がプロセッサごとの `logtask` / `serial` 構成を要求するかは未確認。
- polarfire の SDK ソース群（`mss_entry.o` 等）のビルド方法。上流は
  `Makefile.target:52-65` で `SYSSVC_ASMOBJS`/`SYSSVC_COBJS` としてリンクしており、
  asp3_core の polarfire target.cmake にはこの SDK コンパイル群が無いため流用では済まない。

esp32p4 に伴うもの:

- **ESP-IDF のバージョン固定。** 上流ドキュメントは v5.5 系（`idf_image_integration.md`
  冒頭が `ESP-IDF: v5.5 / esptool 4.12.dev2`）。ローカルの `~/.espressif` には v5.5 と v6.1 の
  constraints があり、どちらを正とするか未決。
- **`IDF_PATH` の扱い。** `tools/fmp_loader/CMakeLists.txt` は `include($ENV{IDF_PATH}/tools/cmake/project.cmake)`
  で環境変数に依存する。CMake のキャッシュ変数へ移すか、`export.sh` 前提のままにするか。
- **`idf.py build` を CMake から呼ぶのか、外から呼ぶのか。** 上流は外から呼ぶ
  （`idf.py build` が内側で `build_fmp3_lib.sh` を呼ぶ、という入れ子）。
  我々が逆向き（CMake から `idf.py` を呼ぶ）にすると、依存関係の管理が二重になる。
  **上流の向き（外から `idf.py`）を保つのが素直**だと考えるが未決。
- **esp32p4 に `presets.json` を置くか。** ライブラリモードでは `cmake --preset` が
  主経路にならないため。ただし差分等価性検査（§7.1）を esp32p4 の `.cfg` に対して回すには
  スタンドアロンで configure できると都合が良い。
- 上流の `main/CMakeLists.txt` のコメントは「sample1, PRC_NUM=1」と書いてあるが、
  実際の `build_fmp3_lib.sh` の既定は `tools/fmp_app` / `PRC_NUM=2` である
  （コメントが陳腐化している）。移植時にどちらを既定とするか要確認。

## 10. v1 からの変更点（レビュー反映）

すべて現物で裏取り済み。

| # | 指摘元 | 内容 | 反映先 |
|---|---|---|---|
| 1 | Fable + codex | `cfg1_out.c` を比較対象に入れないと Python pass1 の生成ミスを検出できない | §7.1 |
| 2 | Fable + codex | Ruby 経路を CMake に入れると AGENTS.md HARD RULE 違反 | §7.1・§8-3 |
| 3 | codex | `cfg1_out` 専用の `--no-gc-sections` が要る | §3・§5.1 |
| 4 | codex | リンカのクラス別セクション回収はターゲットごとに要確認 | §3・§8 |
| 5 | Fable | `arch/riscv_gcc/` 449 行が 3.4.0 新規で流用元が無い（v1 の見積もりが約2.5倍過小） | §1.4・§4・§8-5b |
| 6 | Fable | `PRC_NUM`→`TNUM_PRCID` の設定手段が要る | §3 |
| 7 | Fable | QEMU 起動コマンドは asp3_core から流用不可 | §7.3 |
| 8 | Fable | エラー検出経路は差分等価性検査で見えない | §7.2 |
| 9 | Fable | depfile 未接続・毎ビルド再実行は asp3_core にも存在する | §1.1・§6 |
| 10 | 自分 | pristine Ruby cfg が 1.7.1 でありオラクルと版が揃う（エンジン選定の根拠が強化された） | §1.3 |

**codex が誤った点**: 主張4「移植量は直接移せる範囲」は誤り。codex は自ら `git` 使用を避け
`work/fmp3_3.3` と `work/fmp3_3.4` の通常 diff で代替した結果 `kernel/` しか見ておらず、
`arch/riscv_gcc/` が丸ごと新規である事実に届いていない。#5 のとおり Fable が正しい。

## 11. v2 からの変更点（esp32_p4 の第1波追加）

すべて現物で確認済み。

| # | 内容 | 根拠 | 反映先 |
|---|---|---|---|
| 1 | **ライブラリモードが必須要件になった。** esp32p4 は `libfmp3.a` を ESP-IDF アプリへ静的リンクする方式で、FMP3 単体 ELF の経路は上流が明示的に棄却している | `target/m5stamp_esp32p4_gcc/idf_image_integration.md:9`（`bootloader_utility.c:842` の `assert(rom_index==2)`）。前例は asp3_core `CMakeLists.txt:28,358,509` の `ASP3_LIBRARY_ONLY` | 冒頭表・§2・**§3.1（新設）**・§8-1 |
| 2 | `FMP3_PRC_NUM` の必須性が確定した。ESP32-P4 は HP コア 2 基だが `TNUM_PRCID` 既定値は 4 | `target/m5stamp_esp32p4_gcc/target_kernel.h:18` が `#define TNUM_PRCID 4`、`tools/fmp_loader/build_fmp3_lib.sh` が `CFG_ARGS+=(PRC_NUM="${PRC_NUM:-2}")` | §3-2 |
| 3 | **クラス別セクションの扱いに第3の型があった。** polarfire は orphan 放置、kria は `.ld` で明示回収、**esp32p4 は objcopy でリネーム**して IDF の `*(.data .data.*)` に乗せる | `build_fmp3_lib.sh`（`.kernel_data_CLS_*` / `.stack_CLS_*` → `.data.fmp*`）。回収しないと overlap する旨がコメントに明記 | §3・§3.1・§8 チェックリスト |
| 4 | `.text` → `.iram1.fmptext` のリネームが要る（flash MMU 窓 `.flash_rodata_dummy` との overlap 回避、FMP3 は RAM 実行前提） | `build_fmp3_lib.sh` | §3.1 |
| 5 | セクションリネームは **`PRC_NUM` 依存で固定リストにできない**。上流は `objdump -h` で走査して動的にリネームしている | `build_fmp3_lib.sh` の `for o in objs/*.o` ループ | §3.1 |
| 6 | `cfg1_out*.o` を archive から除外しないと `sta_ker` 等で multiple definition になる | `build_fmp3_lib.sh` | §3.1 |
| 7 | ABI が ilp32f 固定（IDF と一致必須）。`-march=rv32imafc_zicsr_zifencei_xesppie -mabi=ilp32f -mcmodel=medany` | `arch/riscv_gcc/esp32p4/Makefile.chip:27-28,45` | §3.1 |
| 8 | `TOPPERS_OMIT_BSS_INIT` / `TOPPERS_OMIT_DATA_INIT` が要る（`.data`/`.bss` は IDF 起動が初期化） | `build_fmp3_lib.sh` | §3.1 |
| 9 | pass3 はライブラリモードでは走らせられず、引数を親スコープへ export する必要がある | fmp3_pico_sdk `CMakeLists.txt:248-249` の `FMP3_PASS3_ARGS` / `FMP3_KERNEL_CFG_DIR` | §3.1 |
| 10 | 実行検証が 2 系統になった。**入力成果物の種類が違う**（ELF ⇔ マージ済フラッシュ像） | `esp-emu --help` の `--firmware <FIRMWARE> Path to firmware binary (merged flash image)` | §7.3 を 7.3.1 / 7.3.2 に分割 |
| 11 | テンプレート移植量が第1波で 449 → **534 行**（`clic_kernel.trb` 47 + `esp32p4/chip_kernel.trb` 38） | 現物の `wc -l` | §1.4・§8-9 |

**未確認のまま残したこと**: esp-emu 上で ESP32-P4 が実際に動くか。
`esp-emu` の存在と `--chip esp32p4` の受理は確認したが、動作確認済みなのは C3 である。
§8-12 はここが失敗しうる前提で立てている。
