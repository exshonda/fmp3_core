# fmp3_core CMake 化 設計案 v4（2026-07-19）

対象: `/home/honda/TOPPERS/FMP3/fmp3_core`

v1 → v2 は Fable と codex の2者レビューを現物で裏取りして反映（§11）。
v2 → v3 は **esp32_p4 を第1波に追加**したことによる（§12）。
v3 → v4 は **esp32p4 が第1波から外れ polarfire 単独になった**こと、
`FMP3_LIBRARY_ONLY` の根拠訂正、ツールチェーン同定検査の追記による（§13）。

**第1波の対象は `polarfire_soc_kit_gcc` 単独**である。

| | `polarfire_soc_kit_gcc` |
|---|---|
| 割込み制御器 | PLIC |
| プロセッサ数 | 4（既定） |
| 成果物 | 独立 ELF |
| 実行検証 | `qemu-system-riscv64` |

**経緯（v3 からの変更）**: ESP32-P4 は chip 依存部・ターゲット依存部とも別リポジトリ
`fmp3_esp_idf` で管理する方針にユーザが決定したため、`target/m5stamp_esp32p4_gcc` は
`tools/upstream_targets.txt` の allowlist から外れた（`git log` の `9dda350` `0d67e33`、
`DIVERGENCE_MAP.md` の該当行）。`arch/riscv_gcc/esp32p4`（chip 依存部）は上流追従の差分を
見る目的で pristine のまま当 repo に残っている（`ls arch/riscv_gcc/` で確認）。target は
現在 5 個（`kria_arm64_gcc` `kria_r5_gcc` `musca_b1_gcc` `polarfire_soc_kit_gcc`
`rp2350_pico2_gcc`）。

**失われた検証観点（正直に書く）**: v3 は「性質の異なる2ターゲット（PLIC/4コア/独立ELF の
polarfire と、CLIC/2コア/`libfmp3.a` の esp32p4）で層の切り方を検証する。片方だけでは
『たまたま動く設計』に気づけない」ことを第1波の狙いにしていた。esp32p4 が外れたことで、
**その検証は fmp3_core 内ではできなくなった。** 代替は次の2点:

(a) 外部ターゲット契約（`FMP3_TARGET_DIR` / `FMP3_LIBRARY_ONLY`、§3.1・§9.9）の妥当性は、
    fmp3_esp_idf と fmp3_pico_sdk が実際にこの契約を使った時点で検証される。
    asp3 系5リポジトリ（§9.2）が同じ契約で動いている実績がある。**★ただし「実績がある」の
    中身は §9.10（最終レビュー Important 1 で追記）を見ること：`FMP3_ROOT_DIR` の
    受け渡しは asp3_pico_sdk で実証済みだが，`FMP3_SYSSVC_TARGET_C_FILES` の
    受け渡しは asp3_pico_sdk では要件自体が発生しておらず（要求値が偶然どちらのスコープでも
    空になる），実績があるとは言えない。**
(b) fmp3_core 内で「polarfire だけで動く設計」になっていないかを見るには、第1波に
    `musca_b1_gcc`（ARM Cortex-M・QEMU）を足すのが自然（RISC-V/PLIC/4コア ⇔
    ARM Cortex-M/NVIC で対照になる）。**これは未決事項**（§10 に追記）。まだ決まっていない。

## 0. 現状

- FMP3 3.4.0 pristine を vendor ブランチ `upstream` 経由で取り込み済み（archive commit `f3d29a4`）。
- target は 5 個: `musca_b1_gcc` `rp2350_pico2_gcc`
  `polarfire_soc_kit_gcc` `kria_arm64_gcc` `kria_r5_gcc`
  （`m5stamp_esp32p4_gcc` は fmp3_esp_idf 側で管理する方針となり allowlist から除外済み。冒頭参照）。
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

（参考・系譜）asp3_core の Python cfg エンジン自体の発祥元は `asp3_fsp` である
（`asp3_core/docs/dev/cfg-python.md:45` 「asp3_fspの実装済みPython版cfgをベースとする」）。
fmp3_pico_sdk の 1.7.0 エンジンも同じ系譜の派生であり、§9.6 で述べる将来の集約先は
系譜としても自然である。

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
| **第1波（polarfire）合計** | **449** |

**esp32p4 は第1波から外れた**（冒頭参照）。esp32p4 経路のテンプレートは削除せず、
「fmp3_esp_idf 側で必要になる分」として以下に残す。`arch/riscv_gcc/esp32p4` は当 repo に
pristine のまま残っているため（`ls arch/riscv_gcc/` で確認）、このテンプレート移植を
fmp3_core 側でやるか fmp3_esp_idf 側でやるかは**未決**である（§9.4 参照）。

| ファイル | 行数 |
|---|---|
| `arch/riscv_gcc/common/clic_kernel.trb`（esp32p4） | 47 |
| `arch/riscv_gcc/esp32p4/chip_kernel.trb`（esp32p4） | 38 |
| **esp32p4 経路 計（fmp3_esp_idf 側で要検討）** | **85** |

**この 449 行はすべて Python 前例ゼロの FMP3 固有生成コード**（プロセッサ別 inh/intcfg/exc テーブル、
クラス別セクション関数、`GenerateNativeSpn`）であり、最もリスクが高い。

呼び出し連鎖:

- polarfire: `target_kernel.py` → `chip_kernel.py` → `core_kernel.py` + `plic_kernel.py`
- 末端: `core_kernel.py` → `kernel/kernel.py`
- （参考・esp32p4 経路）`target_kernel.py` → `chip_kernel.py` → `core_kernel.py` + `clic_kernel.py`。
  割込み制御器が PLIC/CLIC で分かれるため、polarfire 経路の移植が正しくても esp32p4 経路の
  保証にはならない点は、esp32p4 に着手する側（fmp3_core か fmp3_esp_idf か、上記未決）が
  引き続き踏まえること。

（`IncludeTrb` の実測。`chip_kernel.trb:33,38`(esp32p4) / `:31,36`(polarfire) / `core_kernel.trb:115`）

## 2. 決定事項

| 項目 | 決定 | 根拠 |
|---|---|---|
| 第1波の対象 | `polarfire_soc_kit_gcc` 単独 | esp32p4 は chip 依存部・ターゲット依存部とも別リポジトリ `fmp3_esp_idf` が管理する方針にユーザが決定したため、fmp3_core の第1波から外れた（冒頭参照）。外部 SDK 依存の無い polarfire で cfg パイプラインを確立する |
| 最初のターゲット | `polarfire_soc_kit_gcc` | QEMU 検証可能。`TNUM_PRCID` 既定 4 で FMP3 固有部分を最初から踏める。TTSP3 実績あり |
| ライブラリモード | **最初から設計に入れる**（後付けしない） | `fmp3_pico_sdk` が要求するため（§3.1。**訂正**：v3 は esp32p4 を根拠にしていたが誤り）。後から入れると層の切り方をやり直すことになる |
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

   省略不可である根拠:
   - **polarfire では TTSP3 の `mtskman1`〜`3` / `mmutex1` が `PRC_NUM=2` でないと完了しない。**
     fmp3_core の第1波（polarfire 単独。冒頭参照）における必須性の根拠はこれ一本である。
   - （経緯・参考）v3 時点ではもう一つの根拠として esp32p4 を挙げていた：ESP32-P4 は HP コア
     2 基だが `target_kernel.h` の `TNUM_PRCID` 既定値は 4 であり、上流の
     `tools/fmp_loader/build_fmp3_lib.sh` が `PRC_NUM=2` を明示的に渡していた
     （`CFG_ARGS+=(PRC_NUM="${PRC_NUM:-2}")`）。esp32p4 が fmp3_core の第1波から外れた
     （冒頭参照）ため、この根拠は現在の fmp3_core には直接効かない。`target/m5stamp_esp32p4_gcc`
     はこの repo には既に存在しない（fmp3_esp_idf 側の関心事になった）。
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
esp32p4（fmp3_esp_idf 側の関心事、§9 参照）では「orphan で放置」が成立せず、能動的な
リネームで解決している（§3.1 の「esp32p4 固有のビルド要件」に経緯を残している）。

### 3.1 ライブラリモード（`fmp3_pico_sdk` が要求。後付けしない）

**訂正（v3 → v4）**: v3 は本節の根拠を「esp32p4 が `libfmp3.a` を要求するため」としていたが、
esp32p4 は第1波から外れ、かつライブラリモードを実際に要求するのは esp32p4 ではなく
**`fmp3_pico_sdk`** であることが確認できた。結論（最初から設計に入れる）は変わらないが、
根拠が誤っていたので訂正する。

現物確認済みの事実:

- 兄弟プロジェクト `/home/honda/TOPPERS/PICO2/asp3_pico_sdk` は `ASP3_LIBRARY_ONLY=ON` を
  実際に使っている。`README.md:113` に
  `set(ASP3_LIBRARY_ONLY ON CACHE BOOL "build asp3 as library only" FORCE)`、
  `docs/design.md:8` に `add_subdirectory(${ASP3_CORE_DIR} asp3)` ＋ `ASP3_LIBRARY_ONLY=ON` とある。
- `asp3_pico_sdk/docs/README.md:15` はこの2つ（`ASP3_TARGET_DIR` と `ASP3_LIBRARY_ONLY`）を
  「asp3_core 側の**受け入れ口**」と呼んでいる。
- 一方 `asp3_esp_idf` は `ASP3_LIBRARY_ONLY` を**使っていない**（`grep` で確認：`option()` 定義
  自体とドキュメント記述はあるが、既定 OFF のままどこからも ON に上書きされていない。
  core（asp3_core）が最上位で駆動される構成のため）。

したがって**消費のされ方が2種類ある**:

| パターン | 起動 | 前例 |
|---|---|---|
| core が最上位 | `cmake -S <core> -DFMP3_TARGET_DIR=<外部>` | asp3_esp_idf |
| SDK が最上位＋ライブラリ専用 | SDK 側が `add_subdirectory(fmp3_core)` ＋ `FMP3_LIBRARY_ONLY=ON` | asp3_pico_sdk |

fmp3 側でこれに対応するのは `fmp3_esp_idf`（core が最上位型）と `fmp3_pico_sdk`
（SDK が最上位＋ライブラリ専用型）である。**`FMP3_LIBRARY_ONLY` が必須になるのは後者。**
esp32p4 は §9.1 の方針により fmp3_esp_idf 側で管理されるため、この要件判断には直接関与しない
（esp32p4 自身のビルド方式については後述「esp32p4 固有のビルド要件」に調査結果として残す）。

`FMP3_LIBRARY_ONLY` オプションを設ける（asp3_core の `ASP3_LIBRARY_ONLY`、
`CMakeLists.txt:28` / `:358` / `:509` が前例）。ON のとき `libfmp3.a` までを作り、
**実行ファイル・run ターゲット・pass3 の POST_BUILD を作らない**。

#### esp32p4 固有のビルド要件（参考・fmp3_esp_idf 側の関心事）

**この項は v3 時点の調査結果であり、esp32p4 が fmp3_core の第1波から外れた（冒頭参照）ため
fmp3_core 自身の実装対象ではなくなった。削除せず、fmp3_esp_idf 側で再利用できる調査結果として
残す。** 上流では `target/m5stamp_esp32p4_gcc/tools/fmp_loader/main/CMakeLists.txt` が
`add_custom_command` で `build_fmp3_lib.sh` を呼び、それが `configure.rb`/`make` で
`libfmp3.a` を作っている（この経路は当 repo からは `target/m5stamp_esp32p4_gcc` が既に
存在しないため直接確認できないが、v3 時点で現物確認済みだった内容として残す）。

方式(b)「完全独立 ELF + elf2image」が棄却されていた根拠（`idf_image_integration.md:9`、
`bootloader_utility.c:842` の `assert(rom_index==2)`）や、以下のビルド要件は、seam boot 方式
（§9.8）を採らない限り fmp3_esp_idf 側でも踏む可能性が高い。

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

### 3.2 ツールチェーン同定検査（Task 1 で実装済み）

`cmake/toolchain_check.cmake` として実装済み（`git log` の `5788bc7` `fd95b0d`。設計として
本節に反映する）。

**動機**: 兄弟プロジェクト `asp3_esp_idf` の実測記録
（`/home/honda/TOPPERS/ASP3CORE/asp3_esp_idf/asp3/target/esp32c6_espidf/target.cmake:18-33`）:

> asp3_core の toolchain-riscv64.cmake は既定プレフィクス riscv64-unknown-elf- を PATH 経由で
> 解決するため，`-DRISCV64_TOOLCHAIN_PREFIX` の渡し忘れが «ビルドは通るのに間違った
> コンパイラ» を生む（実測：build/ 配下 320 構成のうち 164 構成が Ubuntu汎用GCC で
> ビルドされていた）。

fmp3_core も `cmake/toolchain-riscv64.cmake` で `CMAKE_TRY_COMPILE_TARGET_TYPE` を
`STATIC_LIBRARY` にしている（ベアメタルは完全なリンクができないため）ため、
`-DCMAKE_TOOLCHAIN_FILE` を渡し忘れてもホストの gcc で `try_compile` が通ってしまうという
**同じ穴があった**。

**採った設計**: **ツールチェーンファイルが `FMP3_EXPECTED_TOOLCHAIN_MACHINE` を宣言し、
検査が `-dumpmachine` の出力と `MATCHES` で照合する**（`toolchain_check.cmake` は
project() の後、`fmp3_core.cmake` の先頭から include）。

- 期待値が宣言されていれば、`-dumpmachine` の出力と照合し、一致しなければ `FATAL_ERROR`。
- 期待値が未宣言で、かつ `CMAKE_TOOLCHAIN_FILE` も未指定（ホスト gcc へのフォールバックが
  疑われる）なら `FATAL_ERROR`。
- 期待値が未宣言だが `CMAKE_TOOLCHAIN_FILE` は渡されている（将来の ARM 系などがまだ宣言を
  実装していない）場合は、無関係な `FATAL_ERROR` にはせず、検査を省略した旨を
  `message(STATUS ...)` で必ず告知してスキップする（黙って素通りしない）。
- `-DFMP3_EXPECTED_TOOLCHAIN_MACHINE=<pattern>` で上書き可能。ツールチェーンファイル側は
  `if(NOT DEFINED FMP3_EXPECTED_TOOLCHAIN_MACHINE)` で既定を与える。**素の `set()` だと
  コマンドラインの `-D` を黙って上書きしてしまい、エラーメッセージの「上書きできる」という
  案内が嘘になる**ため、この書き方を守る（asp3_esp_idf C5 で実際に踏んだ罠）。

**この repo は RISC-V 専用ではない**ことに注意（検査を arch 決め打ちにしてはならない理由）:
`musca_b1_gcc`・`rp2350_pico2_gcc` は ARM Cortex-M、`kria_arm64_gcc` は AArch64、
`kria_r5_gcc` は ARM Cortex-R である。したがって「`-dumpmachine` に `riscv64` を含むか」を
固定判定にすると、これらのターゲットを CMake 化した時点で全部弾いてしまう
（実際に旧版の検査（`5788bc7`）はこの固定判定で書かれており、`fd95b0d` で
「ツールチェーンファイルが自分の期待値を宣言する」方式に改めた）。

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

（参考・esp32p4。fmp3_core の第1波からは外れた。fmp3_esp_idf 側の作業になるか、
 arch/riscv_gcc/esp32p4 が pristine で残っている当 repo 側でやるかは未決。§9.4）
arch/riscv_gcc/
  common/  clic_kernel.py                              ← （47行）
  esp32p4/  chip.cmake  chip_kernel.py                 ← （38行）
（target/m5stamp_esp32p4_gcc は当 repo には存在しない。fmp3_esp_idf 側で管理）
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

第1波は polarfire 単独になった（冒頭参照）ため、fmp3_core としての実行検証は §7.3.1 のみを
実施する。§7.3.2（esp32p4 / esp-emu）は fmp3_esp_idf 側の関心事になったが、
v3 時点の調査結果として削除せず残す。**もともとの狙い**（「`FMP3_RUN_COMMAND` を
target.cmake が定める」という枠だけで実行経路が全く異なる2ターゲットを共通化できるかの検証）は
fmp3_core 単独では検証できなくなった点に注意（冒頭「失われた検証観点」参照）。

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

#### 7.3.2 esp32p4 / esp-emu（参考・fmp3_esp_idf 側の関心事）

**esp32p4 が fmp3_core の第1波から外れた（冒頭参照）ため、本節は fmp3_core としての
実行検証計画ではなくなった。** v3 時点の調査結果として削除せず残す。fmp3_esp_idf 側で
実行検証を設計する際の参考にする。

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
  fmp3_esp_idf 側でマイルストーンを立てる際は、ここが失敗しうる前提で立てること
  （fmp3_core の §8 には esp32p4 のマイルストーンはもう無い）。

## 8. マイルストーン

**第1波は polarfire 単独**（冒頭参照）。esp32p4 の統合作業（旧 v3 の「後半」9〜12）は
fmp3_core のマイルストーンからは外れ、**fmp3_esp_idf 側の作業**になる（§9 参照。
特に §9.5 の想定構成・§9.9）。以下は polarfire を縦に通す手順のみを記す。

### polarfire を縦に通す

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
9. 残り 4 ターゲット（`musca_b1_gcc` `rp2350_pico2_gcc` `kria_arm64_gcc` `kria_r5_gcc`）へ横展開。
   **`musca_b1_gcc` を第1波に含めるかどうかは未決**（冒頭「失われた検証観点」(b)、§10）。
   決まればこの番号立ては見直す。

### esp32p4（参考・fmp3_esp_idf 側の作業）

旧 v3 の手順 9〜12（esp32p4 経路のテンプレート移植・`FMP3_LIBRARY_ONLY=ON` での
`libfmp3.a` ビルド・IDF ローダ殻統合・esp-emu 実行）は、fmp3_esp_idf リポジトリが
新設され次第そちらのマイルストーンとして立て直す。内容は §3.1「esp32p4 固有のビルド要件」・
§7.3.2・§9.5 に調査結果として残してある。

### 横展開チェックリスト（ターゲット追加時）

- [ ] **ライブラリモードが要るか**（外部 SDK にリンクされるターゲットか）。
      要るならセクションリネームの要否と、pass3 を誰が呼ぶかを決める
- [ ] リンカスクリプトが `.kernel_data_*` / `.stack_*` を回収するか、orphan で問題ないか、
      それとも（esp32p4 が採った方式のように）リネームで解決するか。
      **クラス別セクションの扱いは現物で確認する限り4種類ある**（現物確認済み）：
      1) polarfire＝orphan のまま（回収しない）。
      2) kria＝`.ld` が `*(.kernel_data_*)` を明示回収。
      3) esp32p4＝`build_fmp3_lib.sh` が objcopy で `.data.fmp*` へリネーム。
      4) **ARM-M＝そもそもセクション名を出力しない**（`arch/arm_m_gcc/common/core_kernel.trb:26-35`
         の `SecnameKernelData`/`SecnameStack` が無条件に `""` を返す。musca_b1 はこの4番目の型）
- [ ] `cfg1_out` のリンクで `--gc-sections` が効いていないか
- [ ] `arch/` 側テンプレートの有無（割込み制御器が PLIC か CLIC か等でチップ層が変わる）
- [ ] 実行手段（QEMU / エミュレータ / 実機）と、それが要求する成果物の種類（ELF / フラッシュ像）
- [ ] `FMP3_PRC_NUM` の既定値がそのチップの実コア数と合っているか
      （参考例：esp32p4 は `target_kernel.h` 既定 4 に対し実コア 2 だった）

## 9. 将来の統合リポジトリ構成（fmp3_esp_idf）

**ユーザから示された、確定した将来方針。** 本節が最初に書かれた時点（v3、コミット `4e92136`）では
「第1波（§2・§8）の実装方針そのものを変更するものではない」としていたが、その後この方針は
実際に実行された：`m5stamp_esp32p4_gcc` が `tools/upstream_targets.txt` の allowlist から
外され（`9dda350`）、DIVERGENCE_MAP.md 等に反映された（`0d67e33`）。**したがって現在は
第1波そのもの（冒頭・§2・§8）を変更している。** 本節はそれでも、Task 2 以降で
「chip 固有コードをどこに置くか」を判断する際の規律として有効なため残す。

### 9.1 方針（ユーザ決定）

1. **ESP32 系（S3 / 無印 ESP32 / P4）は、統合リポジトリ `fmp3_esp_idf` を新設してそこから
   この repo（fmp3_core）を submodule として使う。** 形は asp3 系の `asp3_esp_idf` /
   `asp3_stm32cube` と同じ（§9.2）。
2. **chip 固有の arch コードは統合リポジトリ側に置く**（fmp3_core には入れない）。
3. **P4 は seam 方式（実 ESP-IDF ブートローダ → カーネルへジャンプ）を今後実施する予定。**
   S3 は既に seam 方式で動作実績がある（§9.8）。
4. **`fmp3_pico_sdk`（既存の Pico 用 FMP3 移植）も、この repo が完成し次第
   `asp3_pico_sdk` と同様に fmp3_core を submodule 化する予定**（§9.6）。

### 9.2 前例：asp3 系統合リポジトリ（5件で確認）

asp3 系には同型の統合リポジトリが**独立に5件**あり、同じ構成に収束している。1件の前例と
5件の一致では、設計判断の裏付けとしての重みが違う。

| repo | 所在（このマシン） | SDK | `arch/` |
|---|---|---|---|
| `asp3_fsp` | `ASP3CORE/asp3_fsp` | Renesas FSP | 有（`arm_m_gcc/ra6m5_fsp` 等） |
| `asp3_mcuxsdk` | `ASP3CORE/asp3_mcuxsdk` | NXP MCUXpresso | 有（`arm_m_gcc/mcxn947_mcuxsdk` 等） |
| `asp3_stm32cube` | `ASP3CORE/asp3_stm32cube` | ST STM32Cube | 有（`arm_m_gcc/stm32h5xx_stm32cube` 等） |
| `asp3_esp_idf` | `ASP3CORE/asp3_esp_idf` | Espressif ESP-IDF | **一部**（C5 のみ独自。C3/C6 は無し） |
| `asp3_pico_sdk` | `PICO2/asp3_pico_sdk` | RP2040/RP2350 SDK | **無し** |

（`ASP3CORE/asp3_esp_idf` は `/home/honda/TOPPERS/asp3_esp_idf` からのシンボリックリンク先と同一実体。
`asp3_esp_idf` だけ `asp3/cmake/`（ツールチェーン検証ヘルパ）が追加である。）

**5件に共通する部分**：`asp3/{asp3_core（submodule）, asp3_<sdk>.cmake, target/}` は必ずある。

**5件で分かれる部分（★訂正）**：`arch/` は必須ではない。**chip 依存部が asp3_core 側に
既にあるときは、統合repo側に `arch/` を置かない。** `asp3_pico_sdk` に `arch/` が無いのは、
`arch/riscv_gcc/rp2350` と `arch/arm_m_gcc/rp2350` が asp3_core 側に既にあるためである
（`asp3_pico_sdk/asp3/target/pico2_riscv_sdk_gcc/target.cmake:79` の
`include(${ASP3_ROOT_DIR}/arch/riscv_gcc/rp2350/chip.cmake)` で確認）。
`asp3_esp_idf` の C3/C6 も同様に `${ASP3_ROOT_DIR}/arch/riscv_gcc/esp32c{3,6}/chip.cmake` を
直接 include しており（C3/C6 の chip arch は後に asp3_core 本体へ統合された）、
C5 だけが `${CMAKE_CURRENT_LIST_DIR}/../../arch/riscv_gcc/esp32c5/chip.cmake`
（統合repo側）を使っている。C5 の `target.cmake` 自身のコメントに理由が明記されている：

> チップ依存部はsubmodule外（asp3/arch/riscv_gcc/esp32c5/．CLAUDE.mdの禁則により
> asp3_core submoduleを直接編集しないため。docs/c5-port-design.md §2.2で配置の妥当性を検証済み）

**この C5 のパターンが、本方針の §9.1-2（chip 固有コードは fmp3_core に入れない）の直接の前例である。**
C3/C6 は「後から submodule 側へ統合された」逆方向の経緯であり、混同しないこと。

### 9.3 `ARCHDIR` / `CHIPDIR` の書き分け — 現れるのは「独自 arch を持つ repo」だけ

`asp3_stm32cube/asp3/target/stm32h563_nucleo/target.cmake:9-11,99`:

```cmake
#   - 共通arch（arch/arm_m_gcc/common）は asp3_core サブモジュール側＝ASP3_ROOT_DIR
#     ＝CMAKE_CURRENT_LIST_DIR 相対
set(ARCHDIR ${ASP3_ROOT_DIR}/arch/arm_m_gcc)
get_filename_component(CHIPDIR ${CMAKE_CURRENT_LIST_DIR}/../../arch/arm_m_gcc/stm32h5xx_stm32cube ABSOLUTE)
set(TARGETDIR ${CMAKE_CURRENT_LIST_DIR})
...
include(${CHIPDIR}/arch.cmake)
```

**同一の `ARCHDIR`/`CHIPDIR` idiom を `asp3_fsp`（`ek_ra6m5/target.cmake:7-9,74`）と
`asp3_mcuxsdk`（`frdmmcxn947_mcuxsdk/target.cmake:11-13,115`）でも現物確認した**（変数名まで一致）。
一方 `asp3_esp_idf` と `asp3_pico_sdk` はこの2変数を**使っていない**（`asp3_esp_idf` は
`ASP3_ROOT_DIR` または `CMAKE_CURRENT_LIST_DIR` からの相対パスを `include()` に直接書く。
`asp3_pico_sdk` は `arch/` 自体が無いので `TARGETDIR` しか定義しない）。

**規律のまとめ（5件から抽出。前回「4つとも同じ idiom」と書いたのは誤り）**:

- `target/`・`<name>.cmake`・`asp3_core` submodule は**常に**ある。
- `arch/` は chip 依存部が asp3_core 側に無いときだけ統合repo側に置く。
- `ARCHDIR`/`CHIPDIR` という**名前の変数**は、独自 `arch/` を実際に持つ repo
  （fsp・mcuxsdk・stm32cube・esp_idf の C5）でのみ現れる idiom であり、5件全部の共通構文ではない。
  ただし「chip 依存部は `CMAKE_CURRENT_LIST_DIR` 相対、共通部・pristine は `ASP3_ROOT_DIR` 相対」
  という**規律そのもの**は5件すべてに共通する。fmp3 側で守るべきはこの規律であり、
  変数名の一致ではない。

### 9.4 fmp3 側への適用（現物確認済み）

fmp3_core（この repo）の `arch/` は `arm64_gcc arm_gcc arm_m_gcc gcc riscv_gcc tracelog` の6個、
`arch/arm_m_gcc/` は `common musca_b1 rp2040 rp2350`、`arch/riscv_gcc/` は
`common polarfire_soc esp32p4` である（すべて `ls` で確認）。**`arch/` は
`tools/upstream_targets.txt` の allowlist 対象外**（同ファイルの対象は `target/` 配下のみ）
なので、`target/` を絞り込んでも上流 arch は取り込んだ分だけ全部残る。**現に
`m5stamp_esp32p4_gcc` は allowlist から除外され target 数は5個になったが
（冒頭・`9dda350`・`0d67e33`）、`arch/riscv_gcc/esp32p4` は下記のとおり pristine のまま
残っている。**

| 統合repo | `arch/` の要否 | 根拠 |
|---|---|---|
| `fmp3_pico_sdk`（既存を再構成予定・§9.6） | **不要（推定）** | rp2040/rp2350 の chip arch が既に fmp3_core にある（`arch/arm_m_gcc/{rp2040,rp2350}`） |
| `fmp3_esp_idf`（新設） | **必要（Xtensaについては確定）** | Xtensa（S3/S31）の chip arch は fmp3_core に無い（上記6個に `xtensa_gcc` は無い）。実装は `/home/honda/TOPPERS/esp32_s3/arch/xtensa_gcc/`（`common esp32 esp32s3`）に既存 |

**P4 について訂正**: 前回「esp32p4 chip が fmp3_core に無い」としたのは誤り。
**esp32p4 の基礎 chip arch（`chip_kernel.trb`・`chip_serial.c`・`Makefile.chip` 等、
CLIC 対応を含む）は upstream pristine として `fmp3_core/arch/riscv_gcc/esp32p4/` に
既に存在する。** これは `target/` の allowlist（`tools/upstream_targets.txt`）とは無関係
（同ファイルの対象は `target/` 配下のみ）で、`m5stamp_esp32p4_gcc` が現在この allowlist から
除外されている（冒頭参照）状態でも変わらない。したがって P4 の**pristine chip layer は §9.3 の
規律における「asp3_esp_idf の C3/C6」型**（submodule 側にある）であり、C5 型ではない。

ただし、ESP-IDF 統合固有の拡張（coprocessor context 管理・CLIC 経由 IPI など、upstream FMP3
には無い ESP32 固有拡張）は別枠になる可能性が高い。姉妹プロジェクト `esp32_s3` の
`CLAUDE.md` は将来の `fmp3_esp_idf` の配置ルールとして次の表を示している
（**この表自体は `fmp3_esp_idf`／`esp32_s3` 側の方針であり、fmp3_core 側で確認した事実ではない
点に注意**）:

| arch層 | 場所 | 用途 |
|---|---|---|
| `fmp3/fmp3_core/arch/riscv_gcc/`（＝この repo の `arch/riscv_gcc/`） | pristine（触らない） | P4/S31 共通 |
| `fmp3/arch/xtensa_gcc/` | 統合repo独自 | S3 (LX7) |
| `fmp3/arch/riscv_gcc/esp32p4/` | 統合repo独自 | P4固有拡張（CLIC 依存の IPI、XespV/Xhwlp コプロセッサコンテキスト等） |

### 9.5 想定構成：`fmp3_esp_idf`

§9.2-9.4 の規律を適用すると、想定構成は次のようになる:

```
fmp3_esp_idf/
└── fmp3/
    ├── arch/                ← chip 固有 arch（統合repo側。xtensa_gcc は必須。
    │                            esp32p4 の IDF 固有拡張が要る場合はここにも）
    ├── fmp3_core/            ← submodule（この repo）
    ├── fmp3_esp_idf.cmake    ← パス解決層（asp3_<sdk>.cmake 相当）
    └── target/
```

**確認済みの事実**: `fmp3_esp_idf` という名前の repo は既に存在する
（`/home/honda/TOPPERS/ESP32/fmp3_esp_idf`、remote `git@github.com:exshonda/fmp3_esp_idf.git`）。
ただし現時点では asp3 系の submodule パターンには未移行の**萌芽状態**で、構成は
`fmp3/{apps, arch, fmp3_core, target, docs, hal, scripts, wifi}` であり：

- `fmp3/fmp3_core/` は**空ディレクトリ**（`fmp3/fmp3_core/fmp3_core/` という中身の無い置き場のみ）で、
  `.gitmodules` には `hal`（`esp-hal-3rdparty`）しか登録されていない。**`fmp3_core` はまだ
  submodule 化されていない。**
- `fmp3/arch/` には既に `riscv_gcc/esp32p4/` と `xtensa_gcc/{common,esp32s3}/` の器がある。
- `fmp3/target/` には `esp32p4_evb_gcc/` `esp32s3_devkitc_gcc/` があるが中身は `.gitkeep` のみ。
- 同 repo の `CLAUDE.md` は「`fmp3/fmp3_core/` 配下は編集禁止」「将来 `fmp3_core` リポの
  submodule に切り替える予定」と明記しており、**§9.1 の方針と整合する**（先取りして書かれていた）。

### 9.6 `fmp3_pico_sdk` の将来：submodule化と cfg エンジンの集約

**ユーザ方針**（§9.1-4）: `fmp3_pico_sdk`（`github.com/exshonda/fmp3_pico_sdk`）も、
この repo（fmp3_core）が完成し次第、`asp3_pico_sdk` と同様に fmp3_core を submodule 化する。

**現状（確認済み）**: 現在の `fmp3_pico_sdk`（作業コピーで確認）は `cfg/ kernel/ arch/ target/
library/ syssvc/ include/ utils/` を**すべて自前で抱える一枚岩**であり、`asp3_pico_sdk` が
既に持っている「`asp3_core` submodule + 統合repo」の分離構成には**まだ移行していない**。
`fmp3_pico_sdk/cfg/cfg.py:60` の `VERSION = "1.7.0"` は §1.2-1.3 で参照した通りで、
fmp3_core が今後 `cfg_py/` へ 1.7.1 を持つのと世代が異なる。

**これは計画Bの位置づけを変える。** 現在 `fmp3_pico_sdk` が抱えている Python cfg 一式
（`cfg/` の 1.7.0 エンジンと `kernel/*.py` テンプレート、§1.2 参照）は、submodule 化が
実施されれば fmp3_core 側（`cfg_py/`・`kernel/`）に一本化されることになる。
**つまり計画Bのテンプレート移植は「fmp3_core のために新規に書く」だけでなく、
将来的には `fmp3_pico_sdk` 側の重複（1.7.0 エンジンと `.py` テンプレート）を
解消する作業でもある。** 順序としては、まず本設計（§1.3 決定のとおり 1.7.1 エンジンを
オラクルと版を揃えて fmp3_core に構築し）、その後 `fmp3_pico_sdk` を移行させて
1.7.0 側を廃止する、という流れになる見込み（推測。移行手順自体は未設計）。

### 9.7 Xtensa arch について（事実）

- 上流 FMP3 3.4.0 の pristine に Xtensa の arch は無い（この repo の `arch/` は
  `arm64_gcc arm_gcc arm_m_gcc gcc riscv_gcc tracelog` のみ。§9.4 で `ls` 確認済み）。
- Xtensa LX7（ESP32-S3）の FMP3 移植は `/home/honda/TOPPERS/esp32_s3/arch/xtensa_gcc/`
  （`common esp32 esp32s3`）に既存。
- §9.1-2 のとおり、これは**fmp3_core には入れず統合リポジトリ側に置く**。

### 9.8 起動方式が3種類あること

| 方式 | 前例 | 起動経路 |
|---|---|---|
| Direct Boot | asp3_esp_idf の C3/C6（既定） | フラッシュ先頭マジック → `start.S` → `sta_ker` |
| seam boot | asp3_esp_idf C5 の `ASP3_SEAM_BOOT`、esp32_s3 の seam-S3 | 実 IDF ブートローダ → カーネルへジャンプ |
| ローダ殻 | FMP3 上流の m5stamp_esp32p4（方式(a)、§3.1） | IDF アプリが `libfmp3.a` をリンク |

**S3 は seam 方式で実機動作実績がある**（`/home/honda/TOPPERS/esp32_s3/CLAUDE.md:6`）:

> S3(LX7) seam-S3 bring-up(実ESP-IDF bootloader→FMP3、hello/W1/W2達成)

同ファイル（`:130-134`）はさらに「標準は seam（実ESP-IDF 2nd-stage bootloader →
FMP3自前エントリ直接ジャンプ、FreeRTOS非リンク）に一本化した」「S3・LX6(無印ESP32)とも
hello/W1/W2 が実機parity達成済み」としている。

**§3.1 への重要な訂正**: 設計書 §3.1 は、上流 m5stamp_esp32p4 の方式(a)（ローダ殻＋
`libfmp3.a`）を前提に書かれている。上流が方式(b)（完全独立 ELF + elf2image）を棄却した根拠
（`idf_image_integration.md:9` の `bootloader_utility.c:842` の `assert(rom_index==2)`）は
**elf2image 方式を否定するものであって、seam boot や Direct Boot を否定するものではない**。
将来 P4 を seam 方式でやる場合、§3.1 の「ライブラリモード」の位置づけは見直しが要る
（ライブラリモードは方式(a)専用の要件であり、seam 方式では最終 ELF を fmp3_core 側で
リンクできる可能性がある）。**ただし `FMP3_LIBRARY_ONLY` オプション自体は、他の外部 SDK
統合（asp3_esp_idf 型のライブラリ組込み等）でも使える可能性があり、残す価値がある。**

### 9.9 計画Aへの影響

**計画Aを変更する必要は無い。** Task 1 が作った `fmp3_core.cmake` は既に
`-DFMP3_TARGET_DIR=<絶対パス>` による外部ターゲット供給に対応している（`fmp3_core.cmake:9-30`
で確認済み。`FMP3_TARGET_DIR` が未指定なら `FMP3_ROOT_DIR/target/<t>`、指定されれば
そのディレクトリの `target.cmake` を探す）。唯一の追加制約は §9.3 の「chip 依存部は
`CMAKE_CURRENT_LIST_DIR` 相対、共通部・pristine は `FMP3_ROOT_DIR`（asp3 の `ASP3_ROOT_DIR`
に相当）相対」という書き分けであり、これは Task 2 の実装時に守る。

### 9.10 `fmp3_<sdk>.cmake` パス解決層は必須（ヘルパ関数のスコープ契約）★訂正

**最終レビュー（Important 1）で判明した事実を反映する訂正。** 旧稿の §9.2「5件に共通する部分」は
`asp3/{asp3_core（submodule）, asp3_<sdk>.cmake, target/}` は必ずあると書いていたが，
**この `asp3_<sdk>.cmake` パス解決層が何のために必須なのかを明記していなかった**。
また §0（冒頭の一覧）と §8-2 が「asp3 系5リポジトリが同じ契約で動いている実績がある」と
述べる箇所も，**この節が指す「契約」の中身を具体的に書いていなかった**ため，
`fmp3_add_syssvc()` を `add_subdirectory(fmp3_core)` 経由（SDK-as-top-level，§3.1 の
「SDK が最上位＋ライブラリ専用」パターン）で呼ぶと `FMP3_ROOT_DIR` が空のまま参照され，
`target_sources()` が `/syssvc/syslog.c` という意味不明な絶対パスで失敗する不具合が
実装時（Task 9 以降）まで見つからなかった。

**現物確認済みの契約（`asp3_pico_sdk` / `asp3_core` で確認）**:

1. CMake の `function()` は**定義側スコープではなく呼び出し側スコープ**で変数を解決する。
   `asp3_add_syssvc()`（`asp3_core/asp3_core.cmake:50-65`）が参照する `ASP3_ROOT_DIR` は
   `asp3_core` 自身のディレクトリスコープ変数であり，`add_subdirectory(asp3_core)` の
   **子スコープの外には自動で伝播しない**（`asp3_core` に `PARENT_SCOPE` の使用は無い．
   `grep -c PARENT_SCOPE` で確認済み＝0件）。
2. **これを埋めるのは `asp3_<sdk>.cmake` パス解決層の責務である。**
   `asp3_pico_sdk/asp3/asp3_pico_sdk.cmake:27-30`:
   ```cmake
   #  asp3_add_syssvc() 等のヘルパ関数は呼び出し側スコープの ASP3_ROOT_DIR を参照する
   #  （add_subdirectory(asp3_core) の子スコープで設定される値は親へ伝播しないため，
   #   親スコープ＝本ファイルを include する sample 側でも明示的に設定しておく）．
   set(ASP3_ROOT_DIR ${ASP3_CORE_DIR})
   ```
   これは `add_subdirectory(asp3_core)` より**前に**，呼び出し側（`sample1/CMakeLists.txt`が
   `include()` する `asp3_pico_sdk.cmake`）自身のスコープで行われる。**§9.2 の「5件で
   asp3_<sdk>.cmake は必ずある」の必須性の理由はこれである**：チップ/ターゲット解決の便宜
   だけでなく，ヘルパ関数のスコープ契約を満たすために必須。
3. **`ASP3_SYSSVC_TARGET_C_FILES`（ターゲット依存 SIO ドライバ）には同じ手が使えない。**
   この変数は `set()` で一括供給できる単純な値ではなく，`${ASP3_TARGET_DIR}/target.cmake`
   および `chip.cmake` が `asp3_core` 自身のディレクトリスコープの中で
   `list(APPEND ...)` / `list(REMOVE_ITEM ...)` を重ねて組み立てる値である。
   `add_subdirectory(asp3_core)` 経由（SDK-as-top-level）では，この組み立ては**子スコープの
   中で完結し**，呼び出し側スコープには伝播しない（`asp3_core` に `PARENT_SCOPE` export が
   無い以上，1 と同じ理由で伝わらない）。

   **`asp3_pico_sdk` はこの問題を解決していない。単に踏んでいないだけである。**
   `pico2_riscv_sdk_gcc/target.cmake:109-112` と `pico2_arm_sdk_gcc/target.cmake:99-101` は
   チップ層（`chip.cmake`）が `ASP3_SYSSVC_TARGET_C_FILES` に積んだ `chip_serial.c` /
   `rp2350_uart.c` を，同じ子スコープの中で直後に `list(REMOVE_ITEM ...)` して取り除いている
   （pico-sdk 側の stdio に置き換えるため）。結果，子スコープ内で実質空リストになり，
   一方 `asp3_pico_sdk.cmake` は `ASP3_SYSSVC_TARGET_C_FILES` を一度も `set()` していないため
   呼び出し側スコープでは**未定義＝空展開**になる。両者がたまたま「空」で一致しているため
   問題が表面化しないだけで，target.cmake が非空の `ASP3_SYSSVC_TARGET_C_FILES` を要求する
   外部ターゲットを SDK-as-top-level パターンで使った場合，この経路は今回と同型の不具合
   （呼び出し側で欠落した値を参照）を再現するはずである（未検証・現物ターゲットが無いため）。

**fmp3 側の対応（本レビューで実施）**:

- `fmp3_add_syssvc()` / `fmp3_cfg_check()` に `FMP3_ROOT_DIR` 空チェックのガードを追加し，
  空のときは意味不明なパスエラーではなく，上記契約を説明する `FATAL_ERROR` を出すようにした
  （`fmp3_core.cmake` / `CMakeLists.txt`）。
- `FMP3_SYSSVC_TARGET_C_FILES` 側は `PARENT_SCOPE` export を追加しない（asp3 系5リポジトリの
  作法から外れるため）。将来 `fmp3_pico_sdk` 等が非空の `FMP3_SYSSVC_TARGET_C_FILES` を必要と
  する外部ターゲットを SDK-as-top-level で使う場合は，本節を前提に改めて設計すること。
- `fmp3_core.cmake` の冒頭コメントに，`fmp3_<sdk>.cmake` パス解決層が
  `add_subdirectory(fmp3_core)` より前に `FMP3_ROOT_DIR` を明示的に `set()` すべきという
  契約を明記した（asp3_pico_sdk.cmake:27-30 を前例として引用）。

## 10. 未決事項

- syssvc / library の取り込み方（asp3_core の `asp3_add_syssvc()` 関数方式を採る想定）。
- FMP3 の syssvc がプロセッサごとの `logtask` / `serial` 構成を要求するかは未確認。
- polarfire の SDK ソース群（`mss_entry.o` 等）のビルド方法。上流は
  `Makefile.target:52-65` で `SYSSVC_ASMOBJS`/`SYSSVC_COBJS` としてリンクしており、
  asp3_core の polarfire target.cmake にはこの SDK コンパイル群が無いため流用では済まない。
- **第1波に `musca_b1_gcc`（ARM Cortex-M・QEMU）を足すかどうか。** 冒頭「失われた検証観点」
  (b) の通り、esp32p4 が外れたことで fmp3_core 内では「polarfire だけで動く設計」に
  なっていないかの検証ができなくなった。RISC-V/PLIC/4コア（polarfire）と対照的な
  ARM Cortex-M/NVIC（musca_b1_gcc）を第1波に足すのが対策として自然だが、**未決**。

esp32p4 に伴うもの（**fmp3_core の第1波からは外れたが、fmp3_esp_idf 側でこれから
判断が要る事項として削除せず残す**）:

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

## 11. v1 からの変更点（レビュー反映）

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

## 12. v2 からの変更点（esp32_p4 の第1波追加）

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

（**v4 の注記**: v4 で§8を作り直した際（§13）、polarfire 側の手順1〜9の番号立ては変えて
いない（本表の「§8-1」「§8-3」「§8-5b」はそのまま現行の§8を指す）。ただし esp32p4 経路
だった旧「§8-9」以降（旧「後半」9〜12）は現行の§8には存在しない。本表自体は当時の記録として
書き換えない。）

**未確認のまま残したこと**: esp-emu 上で ESP32-P4 が実際に動くか。
`esp-emu` の存在と `--chip esp32p4` の受理は確認したが、動作確認済みなのは C3 である。
§8-12 はここが失敗しうる前提で立てていた（**v4 の注記**: esp32p4 が第1波から外れたため
§8 の番号立ては§13で述べる通り作り直しており、「§8-12」は現行の§8には存在しない。
この記述は v3 時点のものとして歴史的にそのまま残す）。

## 13. v3 からの変更点（esp32p4 の第1波除外・関連訂正）

すべて現物で確認済み。

| # | 内容 | 根拠 | 反映先 |
|---|---|---|---|
| 1 | **第1波が polarfire 単独になった。** ESP32-P4 は chip 依存部・ターゲット依存部とも別リポジトリ `fmp3_esp_idf` が管理する方針にユーザが決定したため、`target/m5stamp_esp32p4_gcc` を `tools/upstream_targets.txt` の allowlist から除外した。`arch/riscv_gcc/esp32p4`（chip 依存部）は上流追従の差分を見る目的で pristine のまま残している | `git log` の `9dda350`（allowlist から除外）`0d67e33`（台帳・文書反映）。`DIVERGENCE_MAP.md` の `target/m5stamp_esp32p4_gcc` 行。`ls target/` で現在5個であることを確認。`ls arch/riscv_gcc/` で `esp32p4` が pristine のまま残っていることを確認 | 冒頭表・§0・§1.4・§2・§4・§8・§9.1（既存注記の訂正） |
| 2 | **この除外により失われる検証観点がある**（正直に記録）：v3 は性質の異なる2ターゲットで層の切り方を検証することを狙いにしていたが、esp32p4 が外れたため fmp3_core 内ではこの検証ができなくなった。代替は (a) 外部契約の妥当性は fmp3_esp_idf / fmp3_pico_sdk の実使用時に検証される（asp3系5リポジトリの実績あり）、(b) `musca_b1_gcc` を第1波に足すのが対策として自然だが未決 | 設計判断（推測を含む）。asp3系5リポジトリの実績は§9.2で現物確認済み | 冒頭・§10（未決事項に追記） |
| 3 | **`FMP3_LIBRARY_ONLY` が必要な理由を訂正。** v3 は「esp32p4 が `libfmp3.a` を要求するため」としていたが誤り。実際にライブラリ専用モードを要求するのは `fmp3_pico_sdk` 側（前例は `asp3_pico_sdk`）。消費のされ方は「core が最上位」（asp3_esp_idf 型）と「SDK が最上位＋ライブラリ専用」（asp3_pico_sdk 型）の2種類あり、後者で必須になる。結論（最初から設計に入れる）は変わらない | `asp3_pico_sdk/README.md:113` の `set(ASP3_LIBRARY_ONLY ON CACHE BOOL ... FORCE)`。`asp3_pico_sdk/docs/design.md:8` の `add_subdirectory(${ASP3_CORE_DIR} asp3)` ＋ `ASP3_LIBRARY_ONLY=ON`。`asp3_pico_sdk/docs/README.md:15` が両者を「受け入れ口」と呼ぶ記述。`asp3_esp_idf` 側で `ASP3_LIBRARY_ONLY` が定義のみで ON に上書きされていないことを `grep` で確認 | §2・§3.1（訂正・新表） |
| 4 | **ツールチェーン同定検査を設計に反映。** Task 1 で `cmake/toolchain_check.cmake` が実装済み。ツールチェーンファイルが `FMP3_EXPECTED_TOOLCHAIN_MACHINE` を宣言し、検査が `-dumpmachine` の出力と照合する方式。未宣言なら検査をスキップし STATUS で告知。`-D` での上書きは `if(NOT DEFINED ...)` で既定を与えることで機能させる（素の `set()` はコマンドラインの `-D` を黙って上書きするため使わない）。動機は兄弟プロジェクト asp3_esp_idf の実測（320構成中164構成がホストGCCで誤ビルド）。この repo は RISC-V 専用ではない（musca_b1_gcc/rp2350_pico2_gcc は Cortex-M、kria_arm64_gcc は AArch64、kria_r5_gcc は Cortex-R）ため、検査を arch 決め打ちにできない | `git log` の `5788bc7`（初版・riscv64固定判定）`fd95b0d`（期待値宣言方式へ改訂）。`cmake/toolchain_check.cmake` 現物 | §3.2（新設） |

**この改訂で削除したものは無い。** esp32p4 固有の調査結果（ビルド要件・実行検証・IDF統合の
未決事項）はすべて「参考・fmp3_esp_idf 側の関心事」として残してあり、§3.1・§7.3.2・§9・
§10 から参照できる。
