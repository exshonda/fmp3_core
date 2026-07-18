# fmp3_core CMake 化 計画A（polarfire / Ruby cfg 足場）Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** pristine の Ruby cfg を使って `polarfire_soc_kit_gcc` の `sample1` を CMake でビルドし、`qemu-system-riscv64` 上で起動させる。

**Architecture:** asp3_core と同型の層構造（`CMakePresets.json` → `target/<t>/presets.json` → `cmake/presets-base.json`、`CMakeLists.txt` → `fmp3_core.cmake` → `target.cmake` → `chip.cmake` → `arch.cmake`）を敷き、cfg の3パス（pass1 → `cfg1_out` リンク → `nm`/`objcopy` → pass2×2 → pass3）を CMake に載せる。**CMake が呼ぶ cfg は常に `cfg_py/cfg.py` のみ**とする。本計画（計画A）の間、`cfg_py/cfg.py` は pristine の `cfg/cfg.rb` へそのまま委譲する薄いシムであり、これにより CMake パイプラインの正しさを、テンプレート Python 移植のバグと**切り離して**検証できる。

**Tech Stack:** CMake 3.23+ / Ninja / `riscv64-unknown-elf-gcc` 13.2.0 / picolibc 1.8.6-2 / ruby 3.2.3 / `qemu-system-riscv64` 8.2.2

## Global Constraints

- 設計書は `docs/superpowers/specs/2026-07-18-fmp3-cmake-design.md`。本計画は §8 前半のマイルストーン 1〜3 のみを対象とする。4 以降は計画B。
- **pristine を編集したら必ず `DIVERGENCE_MAP.md` に1行足す**（AGENTS.md §2 規則2）。本計画で pristine ディレクトリに追加する `.cmake` ファイルはすべて記録対象。
- **`upstream` ブランチに派生ファイルを載せない**（AGENTS.md §2 規則1）。本計画の作業はすべて `main` 上で行う。
- **`cfg_py/cfg.py` は本計画（計画A）限りの Ruby 委譲シムである**。CMake が呼ぶのは常に `cfg_py/cfg.py` のみなので、AGENTS.md §2 規則3「pristine の `cfg/` は使わない。cfg 相当は `cfg_py/`（Python）で提供し、CMake から呼ぶ」の**文言は満たす**。ただし**精神には抵触する**（シムは結局 pristine の `cfg/cfg.rb` を実行する）。計画Bでこのシムを asp3_core 1.7.1 の本物のエンジンへ差し替える。**この逸脱は Task 9 で `DIVERGENCE_MAP.md` に期限付きで記録する。**
- ライブラリ名 `fmp3`（`libfmp3.a`）、実行ファイル名 `fmp`（拡張子なし。上流 `sample/Makefile` の `OBJNAME = fmp` に合わせる）。
- プリセット名は `polarfire_soc_kit`（実機）/ `polarfire_soc_kit-qemu`（QEMU）。
- 変数接頭辞は `FMP3_`。asp3_core の `ASP3_*` と同名・同義で揃える。
- `-march` の既定は **`rv64imafdc`**。上流 `arch/riscv_gcc/polarfire_soc/Makefile.chip:25` の `rv64gc` と **ISA としては同一**だが、綴りで multilib の解決先が変わる。`rv64gc` は `-print-multi-directory` が `rv64imafdc/lp64d` を返し、Ubuntu の `picolibc-riscv64-unknown-elf` 1.8.6-2 にはそのディレクトリが実在しないため `crt0.o` が見つからずリンクできない。`rv64imafdc` と綴ると既定ディレクトリ `.` に解決され、picolibc はそこに `crt0.o`/`libc.a` を置いているのでリンクできる（ツールチェーンの `-march` 既定が `rv64imafdc_zicsr` であるため）。ABI は `lp64d` のまま、**圧縮命令(C)も落とさない**（コードサイズが小さくなる。FMP3 は RAM 実行前提でサイズが効く）。Task 1 で実測確認する。

---

## File Structure

| ファイル | 責務 | Task |
|---|---|---|
| `cmake/toolchain-riscv64.cmake` | RISC-V ベアメタル用ツールチェーン定義（コンパイラ・`nm`・`objcopy` の解決） | 1 |
| `cmake/presets-base.json` | 全ターゲット共通の hidden preset `_base`（Ninja・binaryDir・compile_commands） | 1 |
| `CMakePresets.json` | 各 target の `presets.json` を include するだけ | 1 |
| `target/polarfire_soc_kit_gcc/presets.json` | `polarfire_soc_kit` / `-qemu` の2プリセット | 1 |
| `fmp3_core.cmake` | `FMP3_ROOT_DIR` / `FMP3_TARGET` / `FMP3_TARGET_DIR` の解決と `fmp3_add_syssvc()` | 1 |
| `cfg_py/cfg.py` | cfg 実行シム（計画A限り）。`cfg/cfg.rb` へ引数をそのまま委譲し終了コードを透過する | 3 |
| `CMakeLists.txt` | cfg 3パス・`libfmp3.a`・`fmp` 実行ファイル・`run` ターゲット。層の最上位 | 1,4,5,6,7,8 |
| `target/polarfire_soc_kit_gcc/target.cmake` | ボード選択・SDK ソース・リンカスクリプト・`FMP3_RUN_COMMAND` | 1,2,7,8 |
| `arch/riscv_gcc/polarfire_soc/chip.cmake` | チップ依存（`-march`/`-mabi`・PLIC/mtimer/IPI・MMUART） | 2 |
| `arch/riscv_gcc/common/arch.cmake` | コア依存（`core_sym.def`・`core_offset.trb`・`start.S`） | 2 |

`CMakeLists.txt` は cfg パイプラインと2つの成果物を持つため単一ファイルでは大きくなるが、asp3_core（510行）と同型であり、分割すると変数の積み上げ順序が追いにくくなる。**分割しない。**

---

### Task 1: ツールチェーン・プリセット・エントリの骨格

**Files:**
- Create: `cmake/toolchain-riscv64.cmake`
- Create: `cmake/toolchain_check.cmake`
- Create: `cmake/presets-base.json`
- Create: `CMakePresets.json`
- Create: `target/polarfire_soc_kit_gcc/presets.json`
- Create: `fmp3_core.cmake`
- Modify: `CMakeLists.txt`（既存の6行の雛形を置き換える）
- Create: `target/polarfire_soc_kit_gcc/target.cmake`（この Task では最小の骨組みのみ）

**Interfaces:**
- Produces:
  - `FMP3_ROOT_DIR`（文字列、リポジトリルートの絶対パス）
  - `FMP3_TARGET`（文字列、キャッシュ変数。例 `polarfire_soc_kit_gcc`）
  - `FMP3_TARGET_DIR`（文字列、既定 `${FMP3_ROOT_DIR}/target/${FMP3_TARGET}`）
  - `fmp3_add_syssvc(TARGET)`（関数。非TECS版システムサービスと library の `.c` を `TARGET` に追加する）
  - `FMP3_SYSSVC_TARGET_C_FILES`（リスト。`fmp3_add_syssvc` が読む。chip.cmake が積む）
  - ツールチェーン同定の検査（`cmake/toolchain_check.cmake`）：`${CMAKE_C_COMPILER} -dumpmachine`
    の出力を，ツールチェーンファイルが宣言する `FMP3_EXPECTED_TOOLCHAIN_MACHINE`（例
    `cmake/toolchain-riscv64.cmake` は `riscv64`）と MATCHES で照合する。一致しなければ
    configure 時に FATAL_ERROR で止める（ホスト gcc へのフォールバックを検出して事故を防ぐ）。
    期待値が未宣言のときは，`CMAKE_TOOLCHAIN_FILE` も未指定なら FATAL_ERROR（ホスト gcc への
    フォールバック濃厚），`CMAKE_TOOLCHAIN_FILE` は指定されているが期待値だけ未宣言なら
    `message(STATUS ...)` を出して検査をスキップする（将来 ARM 系のツールチェーンファイルが
    宣言を忘れても無関係な FATAL で止めないため）。`-DFMP3_EXPECTED_TOOLCHAIN_MACHINE=<pattern>`
    で上書き可能。`fmp3_core.cmake` の先頭で `project()` 後に include する。

- [ ] **Step 1: 環境の前提を実測で確認する（これが崩れると以降が全部崩れる）**

Run:
```bash
riscv64-unknown-elf-gcc -march=rv64gc     -mabi=lp64d -print-multi-directory
riscv64-unknown-elf-gcc -march=rv64imafdc -mabi=lp64d -print-multi-directory
ls -d /usr/lib/picolibc/riscv64-unknown-elf/lib/rv64imafdc 2>&1
ls    /usr/lib/picolibc/riscv64-unknown-elf/lib/crt0.o
```
Expected:
```
rv64imafdc/lp64d
.
ls: cannot access '/usr/lib/picolibc/riscv64-unknown-elf/lib/rv64imafdc': No such file or directory
/usr/lib/picolibc/riscv64-unknown-elf/lib/crt0.o
```
→ **同じ ISA でも綴りで multilib の解決先が変わる**ことの確認。`rv64gc` は実在しない
`rv64imafdc/lp64d` を指し、`rv64imafdc` は既定ディレクトリ `.` を指す。picolibc は `.` に
`crt0.o`/`libc.a` を置いている。**問題は圧縮命令(C)ではなく綴りである。**

Run（negative control と positive control を対にする）:
```bash
printf 'int main(void){ return 0; }\n' > /tmp/lk.c
for m in rv64gc rv64imafdc; do
  echo "=== $m ==="
  riscv64-unknown-elf-gcc -march=$m -mabi=lp64d -mcmodel=medany \
    --specs=picolibc.specs /tmp/lk.c -o /tmp/lk_$m.elf 2>&1 | tail -2
  echo "rc=${PIPESTATUS[0]}"
done
```
Expected:
```
=== rv64gc ===
...ld: cannot find /usr/lib/picolibc/riscv64-unknown-elf/lib/rv64imafdc/lp64d/crt0.o: No such file or directory
collect2: error: ld returned 1 exit status
rc=1
=== rv64imafdc ===
rc=0
```
→ **`rv64gc` は落ち、`rv64imafdc` は通る**ことの確認。落ちる理由が `crt0.o` の不在
（＝multilib ディレクトリの不在）であって命令セット非対応ではないことも、このエラー文で分かる。
これが Global Constraints の `-march` 既定の根拠。

- [ ] **Step 2: 設定が通らないことを確認する（失敗の確認）**

Run: `cmake --preset polarfire_soc_kit-qemu`
Expected: FAIL — `No such preset in ...: "polarfire_soc_kit-qemu"`（`CMakePresets.json` に該当プリセットが無い）

- [ ] **Step 3: ツールチェーンファイルを書く**

`cmake/toolchain-riscv64.cmake`:
```cmake
#
#		ツールチェーンファイル（RISC-V ベアメタル：RV64 / lp64d）
#
#  既定は riscv64-unknown-elf．別のプレフィックスを使う場合は
#  -DRISCV64_TOOLCHAIN_PREFIX=riscv64-elf- 等で上書きできる．
#
#  実行ファイルに拡張子を付けない（上流 sample/Makefile の OBJNAME = fmp と
#  同じ名前にして，Makefile ビルドとの突き合わせを容易にするため）．
#
set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR riscv64)

#
#  cmake/toolchain_check.cmake が照合する「このツールチェーンファイルが
#  期待する -dumpmachine パターン」．«未定義のときだけ» 既定を与える（素の
#  set() だとコマンドラインの -D を黙って上書きしてしまい，-D による上書き
#  手段が「効かない案内＝嘘」になる。asp3_esp_idf の C5 で実際に踏んだ罠：
#  asp3/target/esp32c6_espidf/target.cmake:44-47）。
#
#  申し送り（将来 ARM 系 = Cortex-M/A/R のツールチェーンファイルを足す人へ）：
#  同じ要領で自分のツールチェーンファイルにも FMP3_EXPECTED_TOOLCHAIN_MACHINE を
#  宣言すること（例 arm-none-eabi）。宣言を忘れても FATAL_ERROR にはならない
#  （toolchain_check.cmake は「期待値が未定義なら検査を行わない」設計）が，
#  検査が効かなくなる。STATUS ログに出るので見落としに注意。
#
if(NOT DEFINED FMP3_EXPECTED_TOOLCHAIN_MACHINE)
    set(FMP3_EXPECTED_TOOLCHAIN_MACHINE riscv64)
endif()

if(NOT DEFINED RISCV64_TOOLCHAIN_PREFIX)
    set(RISCV64_TOOLCHAIN_PREFIX riscv64-unknown-elf-)
endif()

set(CMAKE_C_COMPILER   ${RISCV64_TOOLCHAIN_PREFIX}gcc)
set(CMAKE_ASM_COMPILER ${RISCV64_TOOLCHAIN_PREFIX}gcc)
set(CMAKE_OBJCOPY      ${RISCV64_TOOLCHAIN_PREFIX}objcopy CACHE FILEPATH "objcopy")
set(CMAKE_NM           ${RISCV64_TOOLCHAIN_PREFIX}nm      CACHE FILEPATH "nm")
set(CMAKE_OBJDUMP      ${RISCV64_TOOLCHAIN_PREFIX}objdump CACHE FILEPATH "objdump")

#  ベアメタルではリンクできる完全な実行ファイルを作れないため，
#  try_compile はスタティックライブラリで行う
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
```

- [ ] **Step 3b: ツールチェーン同定の検査を書く**

兄弟プロジェクト `asp3_esp_idf` で実測された事故（`-DRISCV64_TOOLCHAIN_PREFIX` の渡し忘れが
「ビルドは通るのに間違ったコンパイラ」を生み，build/ 配下 320 構成のうち 164 構成が
Ubuntu 汎用 GCC でビルドされていた。`asp3/target/esp32c6_espidf/target.cmake:18-33` に記録）
を防ぐ。`toolchain-riscv64.cmake` は `CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY` を
設定しているため，ホストの gcc でも `try_compile` が通ってしまい，同じ穴がある。

**当初「`-dumpmachine` に `riscv64` を含まなければ FATAL」という固定判定で実装したが，
これはレビューで2つの欠陥が指摘された：**
1. **RV32 を弾く**（ESP32-P4 の HP コアは RV32IMAFC，ESP32-C6 も RV32。将来
   `fmp3_esp_idf` が本 repo を submodule として `riscv32-esp-elf-gcc` で configure すると，
   正当な構成が FATAL_ERROR で止まり，回避手段も無い）。
2. **本 repo は RISC-V 専用ではない**（取り込み済み `target/` のうち `musca_b1_gcc` /
   `rp2350_pico2_gcc` は Cortex-M，`kria_arm64_gcc` は AArch64，`kria_r5_gcc` は
   Cortex-R。これらを CMake 化した時点で全部弾かれる）。

**そこで判定基準はツールチェーンファイル自身に宣言させる**設計に改めた：
`cmake/toolchain-riscv64.cmake` が `FMP3_EXPECTED_TOOLCHAIN_MACHINE` に自分の期待パターン
（RV64 なので `riscv64`）を設定し，`toolchain_check.cmake` はその変数と `-dumpmachine` の
出力を照合するだけにする。期待値が未定義のときは検査を行わない（将来 ARM 系のツールチェーン
ファイルを足す人が宣言し忘れても，無関係な FATAL で止まらないように）。ただし黙って素通り
させると「診断可能な失敗」が「謎の失敗」に劣化する（asp3_esp_idf の C5/C6 比較で実測済み）
ので，その場合は `message(STATUS ...)` で検査を行わなかった旨を出す。さらに，期待値が未定義
かつ `CMAKE_TOOLCHAIN_FILE` も未指定（＝ホスト gcc へのフォールバックが濃厚）なときは，
これまで通り FATAL_ERROR で止める。`-DFMP3_EXPECTED_TOOLCHAIN_MACHINE=<pattern>` で上書き
可能とし，ツールチェーンファイル側は素の `set()` ではなく `if(NOT DEFINED ...)` で既定を
与える（素の `set()` だとコマンドラインの `-D` を黙って上書きし，エラーメッセージが案内する
退避先が「効かない案内＝嘘」になる。asp3_esp_idf の C5 で実際に踏んだ罠：
`asp3/target/esp32c6_espidf/target.cmake:44-47`）。

まず `-dumpmachine` の出力形式を実測する:
```bash
riscv64-unknown-elf-gcc -dumpmachine
gcc -dumpmachine
```
Expected:
```
riscv64-unknown-elf
x86_64-linux-gnu
```
→ クロスは `riscv64` を含み，ホストは含まない。この差で判定する。

`cmake/toolchain-riscv64.cmake`（抜粋。期待値の宣言）:
```cmake
if(NOT DEFINED FMP3_EXPECTED_TOOLCHAIN_MACHINE)
    set(FMP3_EXPECTED_TOOLCHAIN_MACHINE riscv64)
endif()
```

`cmake/toolchain_check.cmake`:
```cmake
#
#		ツールチェーン同定の検査
#
#  cmake/toolchain-riscv64.cmake は CMAKE_TRY_COMPILE_TARGET_TYPE を
#  STATIC_LIBRARY にしている（ベアメタルは完全なリンクができないため）．
#  この設定のせいで，-DCMAKE_TOOLCHAIN_FILE を渡し忘れてもホストの gcc で
#  try_compile が通ってしまい，「ビルドは通るのに間違ったコンパイラ」という
#  事故が起きる（実測：兄弟プロジェクト asp3_esp_idf/asp3/target/esp32c6_espidf/
#  target.cmake:18-33，build/ 配下 320 構成のうち 164 構成がホストの
#  Ubuntu 汎用 GCC でビルドされていた）．
#
#  fmp3_core は RISC-V 専用ではないので「riscv64 でなければ弾く」という固定
#  判定は書けない．判定基準はツールチェーンファイル自身に FMP3_EXPECTED_
#  TOOLCHAIN_MACHINE として宣言させ，本ファイルはそれと MATCHES で照合する
#  だけにする．期待値が未宣言で CMAKE_TOOLCHAIN_FILE も未指定なら FATAL_ERROR，
#  期待値だけ未宣言（CMAKE_TOOLCHAIN_FILE はある）なら STATUS で検査省略を
#  告知してスキップする．-DFMP3_EXPECTED_TOOLCHAIN_MACHINE=<pattern> で上書き
#  可能（ツールチェーンファイル側は if(NOT DEFINED ...) で既定を与えるので，
#  この -D は黙って上書きされない）．
#
#  configure 時に `${CMAKE_C_COMPILER} -dumpmachine` の出力を見る。
#  コンパイラは project() が実行されるまで確定しないため，本ファイルは
#  project() の後（fmp3_core.cmake の先頭）から include すること．
#
execute_process(
    COMMAND ${CMAKE_C_COMPILER} -dumpmachine
    OUTPUT_VARIABLE FMP3_C_COMPILER_MACHINE
    OUTPUT_STRIP_TRAILING_WHITESPACE
    RESULT_VARIABLE FMP3_DUMPMACHINE_RESULT
)

if(NOT FMP3_DUMPMACHINE_RESULT EQUAL 0)
    message(FATAL_ERROR
        "Failed to run '${CMAKE_C_COMPILER} -dumpmachine' (exit ${FMP3_DUMPMACHINE_RESULT}). "
        "CMAKE_C_COMPILER='${CMAKE_C_COMPILER}' does not look like a working compiler. "
        "Did you forget to pass a toolchain file "
        "(-DCMAKE_TOOLCHAIN_FILE=${FMP3_ROOT_DIR}/cmake/toolchain-riscv64.cmake), "
        "or use a CMake preset that sets it (e.g. --preset polarfire_soc_kit-qemu)?")
endif()

if(DEFINED FMP3_EXPECTED_TOOLCHAIN_MACHINE)
    if(NOT FMP3_C_COMPILER_MACHINE MATCHES "${FMP3_EXPECTED_TOOLCHAIN_MACHINE}")
        message(FATAL_ERROR
            "CMAKE_C_COMPILER ('${CMAKE_C_COMPILER}') reports target machine "
            "'${FMP3_C_COMPILER_MACHINE}' (via -dumpmachine), which does not match "
            "the expected pattern '${FMP3_EXPECTED_TOOLCHAIN_MACHINE}' "
            "(FMP3_EXPECTED_TOOLCHAIN_MACHINE, declared by the toolchain file and/or "
            "overridden with -DFMP3_EXPECTED_TOOLCHAIN_MACHINE=<pattern>). Configuring "
            "with a mismatched compiler silently produces a binary for the WRONG "
            "target, because CMAKE_TRY_COMPILE_TARGET_TYPE is STATIC_LIBRARY here and "
            "try_compile does not fail against a mismatched (even host) gcc "
            "(this exact mistake caused 164/320 misbuilt configurations in the sibling "
            "asp3_esp_idf project: asp3/target/esp32c6_espidf/target.cmake:18-33). "
            "Fix: pass the matching -DCMAKE_TOOLCHAIN_FILE (or use a CMake preset that "
            "sets it, e.g. --preset polarfire_soc_kit-qemu), check that the toolchain "
            "prefix variable (e.g. RISCV64_TOOLCHAIN_PREFIX) matches an installed cross "
            "toolchain, or override -DFMP3_EXPECTED_TOOLCHAIN_MACHINE=<pattern> if this "
            "toolchain file's default expectation genuinely does not apply here.")
    endif()
elseif(CMAKE_TOOLCHAIN_FILE)
    message(STATUS
        "fmp3_core: toolchain identity check skipped -- CMAKE_TOOLCHAIN_FILE="
        "'${CMAKE_TOOLCHAIN_FILE}' does not declare FMP3_EXPECTED_TOOLCHAIN_MACHINE "
        "(compiler '${CMAKE_C_COMPILER}' reports '${FMP3_C_COMPILER_MACHINE}' via "
        "-dumpmachine, unverified). This is expected for toolchain files that have "
        "not yet adopted the check; it is NOT verified against FMP3_TARGET. Add "
        "'set(FMP3_EXPECTED_TOOLCHAIN_MACHINE <pattern>)' to the toolchain file "
        "(see cmake/toolchain-riscv64.cmake) or pass "
        "-DFMP3_EXPECTED_TOOLCHAIN_MACHINE=<pattern> to enable it.")
else()
    message(FATAL_ERROR
        "No CMAKE_TOOLCHAIN_FILE was given, so CMAKE_C_COMPILER='${CMAKE_C_COMPILER}' "
        "is presumed to be the HOST compiler (it reports target machine "
        "'${FMP3_C_COMPILER_MACHINE}' via -dumpmachine, and no "
        "FMP3_EXPECTED_TOOLCHAIN_MACHINE was declared to check it against). fmp3_core "
        "is a bare-metal cross-build; configuring with the host compiler silently "
        "produces a binary for the HOST, not the target firmware, because "
        "CMAKE_TRY_COMPILE_TARGET_TYPE is STATIC_LIBRARY here and try_compile does "
        "not fail against a host gcc "
        "(this exact mistake caused 164/320 misbuilt configurations in the sibling "
        "asp3_esp_idf project: asp3/target/esp32c6_espidf/target.cmake:18-33). "
        "Fix: pass -DCMAKE_TOOLCHAIN_FILE=${FMP3_ROOT_DIR}/cmake/toolchain-riscv64.cmake "
        "(or use a CMake preset that sets it, e.g. --preset polarfire_soc_kit-qemu).")
endif()
```

- [ ] **Step 4: presets のベースを書く**

`cmake/presets-base.json`:
```json
{
  "version": 4,
  "configurePresets": [
    {
      "name": "_base",
      "hidden": true,
      "generator": "Ninja",
      "binaryDir": "${sourceDir}/build/${presetName}",
      "cacheVariables": {
        "CMAKE_EXPORT_COMPILE_COMMANDS": "ON"
      }
    }
  ]
}
```

- [ ] **Step 5: ターゲットの presets を書く**

`target/polarfire_soc_kit_gcc/presets.json`:
```json
{
  "version": 4,
  "include": [
    "../../cmake/presets-base.json"
  ],
  "configurePresets": [
    {
      "name": "polarfire_soc_kit",
      "inherits": "_base",
      "displayName": "PolarFire SoC Kit (U54/RV64, 実機)",
      "description": "実機向け。SoftConsole 同梱ツールチェーン（nano.specs）が前提",
      "toolchainFile": "${sourceDir}/cmake/toolchain-riscv64.cmake",
      "cacheVariables": {
        "FMP3_TARGET": "polarfire_soc_kit_gcc",
        "POLARFIRE_QEMU": "OFF"
      }
    },
    {
      "name": "polarfire_soc_kit-qemu",
      "inherits": "polarfire_soc_kit",
      "displayName": "PolarFire SoC Kit / QEMU (microchip-icicle-kit)",
      "description": "QEMU 実行用。picolibc を使用",
      "cacheVariables": {
        "POLARFIRE_QEMU": "ON"
      }
    }
  ],
  "buildPresets": [
    { "name": "polarfire_soc_kit",      "configurePreset": "polarfire_soc_kit" },
    { "name": "polarfire_soc_kit-qemu", "configurePreset": "polarfire_soc_kit-qemu" },
    {
      "name": "run-polarfire_soc_kit-qemu",
      "configurePreset": "polarfire_soc_kit-qemu",
      "targets": [ "run" ]
    }
  ]
}
```

- [ ] **Step 6: ルートの presets を書く**

`CMakePresets.json`（既存の雛形を丸ごと置き換える）:
```json
{
  "version": 4,
  "cmakeMinimumRequired": {
    "major": 3,
    "minor": 23,
    "patch": 0
  },
  "include": [
    "target/polarfire_soc_kit_gcc/presets.json"
  ]
}
```

- [ ] **Step 7: エントリファイルを書く**

`fmp3_core.cmake`:
```cmake
#
#		TOPPERS/FMP3 Core CMake エントリ
#
#  アプリケーション（または本リポジトリのルート CMakeLists.txt）から
#  include して使用する．
#
#  - FMP3_ROOT_DIR：FMP3 カーネルソースのルート（本ファイルの場所）
#  - FMP3_TARGET：ターゲット名（target/ 配下のディレクトリ名）
#  - FMP3_TARGET_DIR：ターゲット依存部（target.cmake）のディレクトリ．
#    未指定なら ${FMP3_ROOT_DIR}/target/${FMP3_TARGET}．
#    外部 SDK が target/ をリポジトリ外に置く場合は
#    -DFMP3_TARGET_DIR=<絶対パス> で供給する．
#
set(FMP3_ROOT_DIR ${CMAKE_CURRENT_LIST_DIR})

#
#  コンパイラが本当に RISC-V ベアメタル向けかを検査する（project() の後で
#  ないと CMAKE_C_COMPILER が確定しないため，project() の後で include される
#  本ファイルの先頭で行う）．
#
include(${FMP3_ROOT_DIR}/cmake/toolchain_check.cmake)

if(NOT DEFINED FMP3_TARGET)
    message(FATAL_ERROR
        "FMP3_TARGET is not defined. "
        "Use a preset (e.g. --preset polarfire_soc_kit-qemu) or -DFMP3_TARGET=<target>.")
endif()

if(NOT DEFINED FMP3_TARGET_DIR)
    set(FMP3_TARGET_DIR ${FMP3_ROOT_DIR}/target/${FMP3_TARGET})
endif()

if(NOT EXISTS ${FMP3_TARGET_DIR}/target.cmake)
    message(FATAL_ERROR
        "${FMP3_TARGET_DIR}/target.cmake not found. "
        "FMP3_TARGET='${FMP3_TARGET}' is not supported by the CMake build "
        "(set -DFMP3_TARGET_DIR=<dir> for an external/SDK target).")
endif()

#
#  非TECS版システムサービスと library のソースを TARGET に追加するヘルパ．
#  上流の configure.rb 引数
#    -S "syslog.o banner.o serial.o serial_cfg.o logtask.o mmuart.o chip_serial.o"
#  に対応する（mmuart.o / chip_serial.o は chip.cmake が
#  FMP3_SYSSVC_TARGET_C_FILES に積む）．
#
function(fmp3_add_syssvc TARGET)
    target_sources(${TARGET} PRIVATE
        ${FMP3_ROOT_DIR}/syssvc/syslog.c
        ${FMP3_ROOT_DIR}/syssvc/banner.c
        ${FMP3_ROOT_DIR}/syssvc/serial.c
        ${FMP3_ROOT_DIR}/syssvc/serial_cfg.c
        ${FMP3_ROOT_DIR}/syssvc/logtask.c
        ${FMP3_SYSSVC_TARGET_C_FILES}
    )
    target_sources(${TARGET} PRIVATE
        ${FMP3_ROOT_DIR}/library/log_output.c
        ${FMP3_ROOT_DIR}/library/vasyslog.c
        ${FMP3_ROOT_DIR}/library/t_perror.c
        ${FMP3_ROOT_DIR}/library/strerror.c
    )
endfunction()
```

- [ ] **Step 8: 最小の target.cmake を置く（Task 2 で中身を入れる）**

`target/polarfire_soc_kit_gcc/target.cmake`:
```cmake
#
#		ターゲット依存部の CMake 定義（PolarFire SoC Kit 用）
#
#  上流 target/polarfire_soc_kit_gcc/Makefile.target の CMake 版．
#  Task 2 で中身を入れる．
#
set(TARGETDIR ${FMP3_TARGET_DIR})

list(APPEND FMP3_INCLUDE_DIRS ${TARGETDIR})
```

- [ ] **Step 9: 最小の CMakeLists.txt を書く**

`CMakeLists.txt`（既存の6行の雛形を丸ごと置き換える）:
```cmake
#
#		TOPPERS/FMP3 Core CMake ビルド
#
#  使い方（例）：
#    cmake --preset polarfire_soc_kit-qemu
#    cmake --build build/polarfire_soc_kit-qemu
#    cmake --build build/polarfire_soc_kit-qemu --target run
#
cmake_minimum_required(VERSION 3.23)

project(fmp3_core C ASM)

include(${CMAKE_CURRENT_LIST_DIR}/fmp3_core.cmake)

#
#  ライブラリ専用モード（外部 SDK アプリ向け．esp32p4 が要求する）
#
#  ON のとき libfmp3.a までを作り，fmp 実行ファイル・run ターゲット・
#  pass3 の POST_BUILD を作らない．
#
option(FMP3_LIBRARY_ONLY
    "Build only the fmp3 kernel library (no fmp executable)" OFF)

#
#  ターゲット依存部の情報を読み込み
#
#  target.cmake は以下の変数を積み上げ，チップ依存部の chip.cmake を
#  include する：
#    FMP3_CFG_FILES / FMP3_KERNEL_CFG_TRB_FILES / FMP3_CHECK_TRB_FILES /
#    FMP3_OFFSET_TRB_FILES / FMP3_CLASS_TRB_FILES / FMP3_SYMVAL_TABLES /
#    FMP3_API_TABLES / FMP3_INCLUDE_DIRS / FMP3_COMPILE_DEFS /
#    FMP3_COMPILE_OPTIONS / FMP3_LINK_OPTIONS / FMP3_CFG1_OUT_LINK_OPTIONS /
#    FMP3_LINK_LIBS / FMP3_LDSCRIPT / FMP3_ARCH_C_FILES / FMP3_TARGET_C_FILES /
#    FMP3_SYSSVC_TARGET_C_FILES / FMP3_START_FILES / FMP3_RUN_COMMAND
#
include(${FMP3_TARGET_DIR}/target.cmake)

message(STATUS "fmp3_core: FMP3_ROOT_DIR   = ${FMP3_ROOT_DIR}")
message(STATUS "fmp3_core: FMP3_TARGET     = ${FMP3_TARGET}")
message(STATUS "fmp3_core: FMP3_TARGET_DIR = ${FMP3_TARGET_DIR}")
```

- [ ] **Step 10: 設定が通ることを確認する**

Run: `cmake --preset polarfire_soc_kit-qemu 2>&1 | grep -E 'fmp3_core:|Configuring done'`
Expected:
```
-- fmp3_core: FMP3_ROOT_DIR   = /home/honda/TOPPERS/FMP3/fmp3_core
-- fmp3_core: FMP3_TARGET     = polarfire_soc_kit_gcc
-- fmp3_core: FMP3_TARGET_DIR = /home/honda/TOPPERS/FMP3/fmp3_core/target/polarfire_soc_kit_gcc
-- Configuring done
```

- [ ] **Step 11: 未対応ターゲットで正しく落ちることを確認する（エラー経路の確認）**

Run: `cmake -G Ninja -B /tmp/fmp3-badtarget -S . -DFMP3_TARGET=musca_b1_gcc -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-riscv64.cmake 2>&1 | grep -A3 "CMake Error"`
Expected: `target/musca_b1_gcc/target.cmake not found.` を含む `CMake Error at fmp3_core.cmake (message):`
（`message(FATAL_ERROR ...)` の出力に文字列 "FATAL" 自体は含まれないため `grep FATAL` ではなく
`grep "CMake Error"` で拾う。`musca_b1_gcc` はまだ `target.cmake` を持たないため FAIL は正しい振る舞い）
exit code は 1 であること（`echo $?` あるいは `${PIPESTATUS[0]}` で確認）。

- [ ] **Step 12: `FMP3_TARGET` 未指定で正しく落ちることを確認する（未検証だった分岐のテスト）**

`fmp3_core.cmake` の `FMP3_TARGET is not defined` FATAL_ERROR 分岐はどの Step でも踏まれて
いなかった（Step 10/11 はどちらも `FMP3_TARGET` を指定している）。これを検証する。

Run:
```bash
cmake -G Ninja -B /tmp/fmp3-notarget -S . \
    -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-riscv64.cmake
echo "exit=$?"
```
Expected: `CMake Error at fmp3_core.cmake (message):` に続けて
`FMP3_TARGET is not defined. Use a preset (e.g. --preset polarfire_soc_kit-qemu) or -DFMP3_TARGET=<target>.`
を含む FATAL_ERROR、`exit=1`。

- [ ] **Step 13: ツールチェーン同定の検査を4パターンで確認する**

**(a) positive control**（正しいツールチェーンでは通る）:
```bash
cmake --preset polarfire_soc_kit-qemu 2>&1 | grep -E 'fmp3_core:|Configuring done'
```
Expected: Step 10 と同じく `Configuring done` まで到達する（`toolchain_check.cmake` が
`riscv64-unknown-elf-gcc` の `-dumpmachine`＝`riscv64-unknown-elf` を，
`toolchain-riscv64.cmake` が宣言する `FMP3_EXPECTED_TOOLCHAIN_MACHINE=riscv64` と正しく
MATCHES し，通過することの確認）。

**(b) negative control**（ツールチェーンファイルを渡し忘れると，configure 時にこの検査で止まる）:
```bash
cmake -G Ninja -B /tmp/fmp3-hostgcc -S . -DFMP3_TARGET=polarfire_soc_kit_gcc
echo "exit=$?"
```
Expected: `CMake Error at cmake/toolchain_check.cmake (message):` に続けて
`No CMAKE_TOOLCHAIN_FILE was given, so CMAKE_C_COMPILER='/usr/bin/cc' is presumed to be
the HOST compiler ...` を含む FATAL_ERROR、`exit=1`。**ビルド途中の分かりにくいエラーでは
なく，configure 時点で止まること**を確認する（`ninja` や `make` まで進んでからのリンク
エラー等ではない）。

**(c) RV32 を誤って弾かないことの実演**（実機の `riscv32-esp-elf-gcc` が無いため，
`-DFMP3_EXPECTED_TOOLCHAIN_MACHINE` の上書きで「弾かれる／上書きで通る」の対を示す）:
```bash
# 弾かれる：わざと食い違う期待値（RV32 相当）を与える
cmake -G Ninja -B /tmp/fmp3-rv32-mismatch -S . -DFMP3_TARGET=polarfire_soc_kit_gcc \
    -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-riscv64.cmake \
    -DFMP3_EXPECTED_TOOLCHAIN_MACHINE=riscv32
echo "exit=$?"

# 上書きで通る：実コンパイラの -dumpmachine と一致する期待値を与える
cmake -G Ninja -B /tmp/fmp3-rv32-override-ok -S . -DFMP3_TARGET=polarfire_soc_kit_gcc \
    -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-riscv64.cmake \
    -DFMP3_EXPECTED_TOOLCHAIN_MACHINE=riscv64-unknown-elf
echo "exit=$?"
```
Expected: 前者は `does not match the expected pattern 'riscv32'` を含む FATAL_ERROR，`exit=1`。
後者は `Configuring done`，`exit=0`。**`-D` による上書きが実際に効いている**ことの確認
（効いていなければ `toolchain-riscv64.cmake` の既定 `riscv64` が両方とも通してしまう）。
これにより，将来 `riscv32-esp-elf-gcc` で configure する際，対応するツールチェーンファイルが
`FMP3_EXPECTED_TOOLCHAIN_MACHINE` に `riscv32` 系パターンを宣言しさえすれば，本検査が
RV32 を誤って弾かないことが示される。

**(d) 期待値未定義時にスキップされることの確認**（将来 ARM 系ツールチェーンファイルが
宣言を忘れるケースを，宣言を持たない使い捨てツールチェーンファイルで模す）:
```bash
cmake -G Ninja -B /tmp/fmp3-skip-test -S . -DFMP3_TARGET=polarfire_soc_kit_gcc \
    -DCMAKE_TOOLCHAIN_FILE=<FMP3_EXPECTED_TOOLCHAIN_MACHINE を宣言しない使い捨てツールチェーンファイル>
```
Expected: `-- fmp3_core: toolchain identity check skipped -- CMAKE_TOOLCHAIN_FILE='...' does
not declare FMP3_EXPECTED_TOOLCHAIN_MACHINE ...` が STATUS で出た上で `Configuring done`，
`exit=0`。**黙って素通りせず，検査を行わなかったことが分かる**ことを確認する。

- [ ] **Step 14: コミット**

```bash
git add cmake/toolchain-riscv64.cmake cmake/toolchain_check.cmake cmake/presets-base.json \
        CMakePresets.json CMakeLists.txt fmp3_core.cmake \
        target/polarfire_soc_kit_gcc/presets.json target/polarfire_soc_kit_gcc/target.cmake
git commit -m "build: CMake 骨格（ツールチェーン・presets・エントリ）を追加

polarfire_soc_kit / -qemu の2プリセットで configure が通るところまで。
-march は上流の rv64gc ではなく rv64imafdc を既定とする（ISA は同一だが，
rv64gc は実在しない multilib ディレクトリ rv64imafdc/lp64d に解決され
crt0.o が見つからない。rv64imafdc なら既定ディレクトリ . に解決される）。
ABI は lp64d のまま。"
```

---

### Task 2: arch / chip / target の変数積み上げ

**Files:**
- Create: `arch/riscv_gcc/common/arch.cmake`
- Create: `arch/riscv_gcc/polarfire_soc/chip.cmake`
- Modify: `target/polarfire_soc_kit_gcc/target.cmake`（Task 1 の骨組みを本体で置き換える）
- Modify: `CMakeLists.txt`（`FMP3_PRC_NUM` の適用と変数ダンプを追加。2026-07-19 追記：
  `-T` の1箇所集約も Step 4b でここに追加）

**Interfaces:**
- Consumes: `FMP3_ROOT_DIR` / `FMP3_TARGET_DIR`（Task 1）
- Produces（すべてリスト変数。`CMakeLists.txt` が読む）:
  - `FMP3_SYMVAL_TABLES` — cfg の `--symval-table` に渡す `.def`
  - `FMP3_OFFSET_TRB_FILES` — `offset.h` 生成用テンプレート（計画Aでは `.trb`）
  - `FMP3_KERNEL_CFG_TRB_FILES` — `kernel_cfg.c/h` 生成用テンプレート
  - `FMP3_CHECK_TRB_FILES` — pass3 用テンプレート
  - `FMP3_CLASS_TRB_FILES` — cfg の `-C` に渡すクラス定義（FMP3 固有）
  - `FMP3_CFG_FILES` — cfg に渡す `.cfg`（target 分。アプリ分は `CMakeLists.txt` が足す）
  - `FMP3_INCLUDE_DIRS` / `FMP3_COMPILE_DEFS` / `FMP3_COMPILE_OPTIONS`
  - `FMP3_LINK_OPTIONS` / `FMP3_CFG1_OUT_LINK_OPTIONS` / `FMP3_LINK_LIBS` / `FMP3_LDSCRIPT`
  - `FMP3_ARCH_C_FILES` / `FMP3_TARGET_C_FILES` / `FMP3_SYSSVC_TARGET_C_FILES` / `FMP3_START_FILES`
  - `FMP3_SDK_C_FILES` / `FMP3_SDK_ASM_FILES` — polarfire の Microchip SDK ソース（Task 7 で使う）

- [ ] **Step 1: 変数が空であることを確認する（失敗の確認）**

`CMakeLists.txt` の末尾（Step 5 で入れるダンプの直前の状態）で確認する。
Run: `cmake --preset polarfire_soc_kit-qemu 2>&1 | grep -c 'FMP3_ARCH_C_FILES'`
Expected: `0`（まだダンプも変数も無い）

- [ ] **Step 2: コア依存部を書く**

`arch/riscv_gcc/common/arch.cmake`（上流 `arch/riscv_gcc/common/Makefile.core` の CMake 版）:
```cmake
#
#		アーキテクチャ依存部の CMake 定義（RISC-V コア共通）
#
#  chip.cmake から include される（上流 Makefile.core に相当）．
#  start.S はライブラリ外で先頭にリンクする（FMP3_START_FILES）．
#
set(COREDIR ${FMP3_ROOT_DIR}/arch/riscv_gcc/common)
set(TOOLDIR ${FMP3_ROOT_DIR}/arch/gcc)

#  Makefile.core:33
list(APPEND FMP3_SYMVAL_TABLES
    ${COREDIR}/core_sym.def
)

#  Makefile.core:38  TARGET_OFFSET_TRB
list(APPEND FMP3_OFFSET_TRB_FILES
    ${COREDIR}/core_offset.trb
)

#  Makefile.core:20
list(APPEND FMP3_INCLUDE_DIRS
    ${COREDIR}
    ${TOOLDIR}
)

#  Makefile.core:27-28
list(APPEND FMP3_ARCH_C_FILES
    ${COREDIR}/core_kernel_impl.c
    ${COREDIR}/core_support.S
)

#  Makefile.core:45  START_OBJS
list(APPEND FMP3_START_FILES
    ${COREDIR}/start.S
)

#  Makefile.core:51
list(APPEND FMP3_LINK_OPTIONS
    -nostdlib
)

#  Makefile.core:21（-lgcc）と sample/Makefile:63（-lc）
list(APPEND FMP3_LINK_LIBS c gcc)
```

- [ ] **Step 3: チップ依存部を書く**

`arch/riscv_gcc/polarfire_soc/chip.cmake`（上流 `arch/riscv_gcc/polarfire_soc/Makefile.chip` の CMake 版）:
```cmake
#
#		チップ依存部の CMake 定義（PolarFire SoC 用）
#
#  target.cmake から include される（上流 Makefile.chip に相当）．
#
set(CHIPDIR ${FMP3_ROOT_DIR}/arch/riscv_gcc/polarfire_soc)
set(COREDIR ${FMP3_ROOT_DIR}/arch/riscv_gcc/common)

#
#  ISA と ABI
#
#  上流 Makefile.chip:25 は -march=rv64gc．ISA としては rv64imafdc と同一
#  だが，綴りで multilib の解決先が変わる．rv64gc は
#  rv64imafdc/lp64d に解決され，Ubuntu の picolibc-riscv64-unknown-elf
#  1.8.6-2 にはそのディレクトリが実在しないため crt0.o が見つからない．
#  rv64imafdc と綴ると既定ディレクトリ . に解決され，picolibc はそこに
#  crt0.o/libc.a を置いているのでリンクできる（ツールチェーンの -march
#  既定が rv64imafdc_zicsr であるため）．よって rv64imafdc を既定とする．
#  ABI は lp64d のまま変わらず，圧縮命令(C)も維持されるのでコードサイズが
#  小さくなる．
#
set(FMP3_RISCV_MARCH "rv64imafdc" CACHE STRING
    "RISC-V ISA string passed to -march (same ISA as upstream rv64gc; this spelling resolves to picolibc's default multilib)")

#
#  C ライブラリの specs
#
#  上流 Makefile.chip:24 は nano.specs（SoftConsole 同梱の newlib-nano）
#  を既定とし，QEMU ビルドでは Makefile.target:20 が picolibc.specs へ
#  差し替える．同じ切り分けを POLARFIRE_QEMU で行う（target.cmake が設定）．
#
if(NOT DEFINED FMP3_RISCV_SPECS)
    set(FMP3_RISCV_SPECS "--specs=nano.specs")
endif()

list(APPEND FMP3_INCLUDE_DIRS
    ${CHIPDIR}
)

#  Makefile.chip:25-27
list(APPEND FMP3_COMPILE_OPTIONS
    -march=${FMP3_RISCV_MARCH}
    -mabi=lp64d
    -mcmodel=medany
    -msmall-data-limit=8
    -mstrict-align
    -mno-save-restore
    -fsigned-char
    -ffunction-sections
    -fdata-sections
    ${FMP3_RISCV_SPECS}
)

#  リンク時にも ISA/ABI/specs を渡す（gcc をリンカドライバとして使うため）
list(APPEND FMP3_LINK_OPTIONS
    -march=${FMP3_RISCV_MARCH}
    -mabi=lp64d
    -mcmodel=medany
    ${FMP3_RISCV_SPECS}
    -nostartfiles          #  Makefile.chip:28
)

#
#  カーネルに含めるチップ依存ソース（Makefile.chip:36,42,44）
#
#  plic_kernel_impl.c / msi_ipi.c / mtimer.c は COREDIR にあるが，
#  「PLIC と Machine Timer と MSI-IPI を使う」というのはチップの決定なので，
#  上流 Makefile.chip と同じくここで選ぶ．
#
list(APPEND FMP3_ARCH_C_FILES
    ${CHIPDIR}/chip_kernel_impl.c
    ${CHIPDIR}/chip_support.S
    ${COREDIR}/plic_kernel_impl.c
    ${COREDIR}/msi_ipi.c
    ${COREDIR}/mtimer.c
)

#
#  非TECS版 SIO ドライバ（MMUART）
#  上流の configure.rb -S "... mmuart.o chip_serial.o" に対応
#
list(APPEND FMP3_SYSSVC_TARGET_C_FILES
    ${CHIPDIR}/chip_serial.c
    ${CHIPDIR}/mmuart.c
)

#
#  コア依存部（Makefile.chip:54）
#
include(${FMP3_ROOT_DIR}/arch/riscv_gcc/common/arch.cmake)
```

- [ ] **Step 4: ターゲット依存部を書く**

`target/polarfire_soc_kit_gcc/target.cmake`（Task 1 の骨組みを丸ごと置き換える。上流 `Makefile.target` の CMake 版）:
```cmake
#
#		ターゲット依存部の CMake 定義（PolarFire SoC Kit 用）
#
#  上流 target/polarfire_soc_kit_gcc/Makefile.target の CMake 版．
#
set(TARGETDIR ${FMP3_TARGET_DIR})

#
#  QEMU（microchip-icicle-kit）向けビルド設定
#
#  上流 Makefile.target:17-22 の QEMU=1 に相当．ボードを Icicle Kit に，
#  C ライブラリを picolibc に切り替える．
#
option(POLARFIRE_QEMU "Build for QEMU microchip-icicle-kit (OFF: real board)" ON)

if(POLARFIRE_QEMU)
    set(FMP3_BOARD MPFS_ICICLE_KIT)
    set(FMP3_RISCV_SPECS "--specs=picolibc.specs")   #  Makefile.target:20
    list(APPEND FMP3_COMPILE_DEFS TOPPERS_USE_QEMU)  #  Makefile.target:21
else()
    set(FMP3_BOARD MPFS_DISCOVERY_KIT)               #  Makefile.target:28
endif()

#  Makefile.target:83
list(APPEND FMP3_COMPILE_DEFS ${FMP3_BOARD})

#  Makefile.target:46
list(APPEND FMP3_COMPILE_DEFS
    TOPPERS_OMIT_BSS_INIT
    TOPPERS_OMIT_DATA_INIT
)

#  非TECS版システムサービスを使う（syssvc/syslog.h 等が参照する）
list(APPEND FMP3_COMPILE_DEFS TOPPERS_OMIT_TECS)

#  Makefile.target:43, :75
list(APPEND FMP3_INCLUDE_DIRS
    ${TARGETDIR}
    ${TARGETDIR}/sdk/platform
)

#
#  ボード毎の include とリンカスクリプト（Makefile.target:85-95）
#
if(FMP3_BOARD STREQUAL "MPFS_ICICLE_KIT")
    list(APPEND FMP3_INCLUDE_DIRS
        ${TARGETDIR}/sdk/boards/icicle-kit-es
        ${TARGETDIR}/sdk/boards/icicle-kit-es/platform_config/lim-debug
    )
    set(FMP3_LDSCRIPT
        ${TARGETDIR}/sdk/boards/icicle-kit-es/platform_config/lim-debug/linker/mpfs-lim.ld)
else()
    list(APPEND FMP3_INCLUDE_DIRS
        ${TARGETDIR}/sdk/boards/mpfs-discovery-kit
        ${TARGETDIR}/sdk/boards/mpfs-discovery-kit/platform_config/lim-debug
    )
    set(FMP3_LDSCRIPT
        ${TARGETDIR}/sdk/boards/mpfs-discovery-kit/platform_config/lim-debug/linker/mpfs-lim.ld)
endif()

#
#  カーネルに含めるターゲット依存ソース（Makefile.target:117）
#
list(APPEND FMP3_TARGET_C_FILES
    ${TARGETDIR}/target_kernel_impl.c
)

#
#  Microchip SDK のソース（Makefile.target:52-65）
#
#  上流は SYSSVC_COBJS / SYSSVC_ASMOBJS として最終リンクに加えている．
#  カーネルライブラリには入れない（asp3_core の polarfire にはこの層が
#  無く，流用できない部分）．
#
set(SDKDIR ${TARGETDIR}/sdk)
list(APPEND FMP3_SDK_C_FILES
    ${TARGETDIR}/sdk_entry.c
    ${SDKDIR}/platform/mpfs_hal/startup_gcc/system_startup.c
    ${SDKDIR}/platform/mpfs_hal/common/nwc/mss_io.c
    ${SDKDIR}/platform/mpfs_hal/common/nwc/mss_nwc_init.c
    ${SDKDIR}/platform/mpfs_hal/common/nwc/mss_pll.c
    ${SDKDIR}/platform/mpfs_hal/common/nwc/mss_sgmii.c
    ${SDKDIR}/platform/mpfs_hal/common/mss_beu.c
    ${SDKDIR}/platform/mpfs_hal/common/mss_irq_handler_stubs.c
    ${SDKDIR}/platform/mpfs_hal/common/mss_l2_cache.c
    ${SDKDIR}/platform/mpfs_hal/common/mss_mpu.c
    ${SDKDIR}/platform/mpfs_hal/common/mss_peripherals.c
    ${SDKDIR}/platform/mpfs_hal/common/mss_plic.c
    ${SDKDIR}/platform/mpfs_hal/common/mss_pmp.c
    ${SDKDIR}/platform/mpfs_hal/common/mss_util.c
)
list(APPEND FMP3_SDK_ASM_FILES
    ${SDKDIR}/platform/mpfs_hal/startup_gcc/mss_entry.S
    ${SDKDIR}/platform/mpfs_hal/startup_gcc/mss_utils.S
)

#
#  cfg に渡すファイル（sample/Makefile:309-319 の TARGET_*_TRB / _CFG）
#
list(APPEND FMP3_CFG_FILES            ${TARGETDIR}/target_kernel.cfg)
list(APPEND FMP3_CLASS_TRB_FILES      ${TARGETDIR}/target_class.trb)
list(APPEND FMP3_KERNEL_CFG_TRB_FILES ${TARGETDIR}/target_kernel.trb)
list(APPEND FMP3_CHECK_TRB_FILES      ${TARGETDIR}/target_check.trb)

#
#  チップ依存部
#
include(${FMP3_ROOT_DIR}/arch/riscv_gcc/polarfire_soc/chip.cmake)

#
#  最終リンクのオプション（Makefile.target:47, :106）
#
list(APPEND FMP3_LINK_OPTIONS -Wl,--gc-sections)

#
#  ★-T（リンカスクリプト指定）はここでは積まない．FMP3_LDSCRIPT の値
#    （LINK_DEPENDS 追跡用）を確定させるだけに留める．-T の適用は
#    CMakeLists.txt の include(target.cmake) 直後の1箇所に集約する
#    （下記 Step 5 参照）。ここで積むと，Task 7 が fmp 実行ファイルを
#    組むときに asp3_core と同じパターンで -Wl,-T, を足した場合，
#    -T と -Wl,-T, が両方入って ld が
#      "linker script file '...' appears multiple times" で fatal error
#    になる（実リンカで再現確認済み。2026-07-19 の地雷潰しタスクで発覚）。
#    上流 Makefile.target:106-107 も QEMU=1 のとき COPTS に -T を混ぜた
#    直後に `LDSCRIPT =` で空にしているのと同じ理由．
#
if(POLARFIRE_QEMU)
    #  Makefile.target:106
    #  --undefined=_kernel_mpfinib_table は --gc-sections で消えるカーネル
    #  構成テーブルを保持するためのもの．-T 自体（picolibc.specs の
    #  %{!T:-Tpicolibc.ld} を抑止する側）は CMakeLists.txt 側で積む．
    list(APPEND FMP3_LINK_OPTIONS -Wl,--undefined=_kernel_mpfinib_table)

    #
    #  Makefile.target:108-110
    #  cfg1_out のリンクは _start を参照しないため --gc-sections で
    #  TOPPERS_magic_number が除去される．これを抑止する．
    #
    list(APPEND FMP3_CFG1_OUT_LINK_OPTIONS -Wl,--no-gc-sections)
endif()
```

- [ ] **Step 4b（2026-07-19 追記）: `CMakeLists.txt` 側で `-T` を1箇所に集約する**

`CMakeLists.txt` の `include(${FMP3_TARGET_DIR}/target.cmake)` の**直後**に挿入:
```cmake
#
#  リンカスクリプトの適用（-T）はここ1箇所に集約する．
#
#  ★QEMU（picolibc）と実機（newlib-nano）で書式を変える必要がある：
#    picolibc.specs は `%{!T:-Tpicolibc.ld}` を持ち，gcc ドライバの -T
#    スイッチが立っているかどうかだけで picolibc 既定のリンカスクリプトを
#    追加するか判定する．-Wl,-T,<file> は gcc の -T スイッチを立てない
#    ため，QEMU 側で asp3_core と同じ -Wl,-T, を使うと picolibc.ld が
#    「も」-T されてしまう．実測：mpfs-lim.ld のみ意図しているのに
#    -Wl,-T, で統一すると _start が picolibc.ld 既定の 0x80000000 に
#    化ける（ld はエラーにせず後着の picolibc.ld が勝つ）．ビルドは
#    通るのに実機で起動しない，という壊れ方をするので asp3_core の
#    書式をそのまま流用しないこと．
#
if(DEFINED FMP3_LDSCRIPT)
    if(POLARFIRE_QEMU)
        list(APPEND FMP3_LINK_OPTIONS -T ${FMP3_LDSCRIPT})
    else()
        list(APPEND FMP3_LINK_OPTIONS -Wl,-T,${FMP3_LDSCRIPT})
    endif()
endif()
```

- [ ] **Step 5: `CMakeLists.txt` に `FMP3_PRC_NUM` と変数ダンプを足す**

`CMakeLists.txt` の `include(${FMP3_TARGET_DIR}/target.cmake)` の**直前**に挿入:
```cmake
#
#  プロセッサ数（TNUM_PRCID）
#
#  上流 sample/Makefile:193-194 の PRC_NUM に相当する．指定時のみ
#  -DTNUM_PRCID=<N> をコンパイル定義に足す．未指定ならターゲットの
#  target_kernel.h の既定値（polarfire は 4）が使われる．
#
#  ★これは CMake の分岐材料ではない．ファイルリストやターゲット構成を
#    この値で分岐させてはならない（TNUM_PRCID は cfg1_out をリンクして
#    初めて cfg が知る値であり，CMake から見える保証がない）．
#
set(FMP3_PRC_NUM "" CACHE STRING
    "Number of processors (TNUM_PRCID). Empty = target default")
```

`include(${FMP3_TARGET_DIR}/target.cmake)` の**直後**に挿入:
```cmake
if(NOT FMP3_PRC_NUM STREQUAL "")
    list(APPEND FMP3_COMPILE_DEFS TNUM_PRCID=${FMP3_PRC_NUM})
endif()
```

`CMakeLists.txt` の末尾の `message(STATUS ...)` 3行を、以下に置き換える:
```cmake
message(STATUS "fmp3_core: FMP3_ROOT_DIR   = ${FMP3_ROOT_DIR}")
message(STATUS "fmp3_core: FMP3_TARGET     = ${FMP3_TARGET}")
message(STATUS "fmp3_core: FMP3_TARGET_DIR = ${FMP3_TARGET_DIR}")
message(STATUS "fmp3_core: FMP3_BOARD      = ${FMP3_BOARD}")
message(STATUS "fmp3_core: FMP3_RISCV_MARCH= ${FMP3_RISCV_MARCH}")
message(STATUS "fmp3_core: FMP3_RISCV_SPECS= ${FMP3_RISCV_SPECS}")
message(STATUS "fmp3_core: FMP3_PRC_NUM    = '${FMP3_PRC_NUM}'")
message(STATUS "fmp3_core: FMP3_COMPILE_DEFS= ${FMP3_COMPILE_DEFS}")
message(STATUS "fmp3_core: FMP3_ARCH_C_FILES count = ")
foreach(f IN LISTS FMP3_ARCH_C_FILES)
    message(STATUS "    arch: ${f}")
endforeach()
message(STATUS "fmp3_core: FMP3_LDSCRIPT   = ${FMP3_LDSCRIPT}")
message(STATUS "fmp3_core: FMP3_CFG1_OUT_LINK_OPTIONS = ${FMP3_CFG1_OUT_LINK_OPTIONS}")
message(STATUS "fmp3_core: FMP3_LINK_OPTIONS = ${FMP3_LINK_OPTIONS}")
```
（2026-07-19 追記：`FMP3_LINK_OPTIONS` の行は Step 4b の -T 集約が「1回だけ」効いていることを
目視確認するために追加した。）

- [ ] **Step 6: 積み上がった変数を確認する**

Run: `cmake --preset polarfire_soc_kit-qemu 2>&1 | grep 'fmp3_core:\|    arch:'`
Expected（順不同ではなくこの順で出る）:
```
-- fmp3_core: FMP3_ROOT_DIR   = /home/honda/TOPPERS/FMP3/fmp3_core
-- fmp3_core: FMP3_TARGET     = polarfire_soc_kit_gcc
-- fmp3_core: FMP3_TARGET_DIR = /home/honda/TOPPERS/FMP3/fmp3_core/target/polarfire_soc_kit_gcc
-- fmp3_core: FMP3_BOARD      = MPFS_ICICLE_KIT
-- fmp3_core: FMP3_RISCV_MARCH= rv64imafdc
-- fmp3_core: FMP3_RISCV_SPECS= --specs=picolibc.specs
-- fmp3_core: FMP3_PRC_NUM    = ''
-- fmp3_core: FMP3_COMPILE_DEFS= TOPPERS_USE_QEMU;MPFS_ICICLE_KIT;TOPPERS_OMIT_BSS_INIT;TOPPERS_OMIT_DATA_INIT;TOPPERS_OMIT_TECS
--     arch: .../arch/riscv_gcc/polarfire_soc/chip_kernel_impl.c
--     arch: .../arch/riscv_gcc/polarfire_soc/chip_support.S
--     arch: .../arch/riscv_gcc/common/plic_kernel_impl.c
--     arch: .../arch/riscv_gcc/common/msi_ipi.c
--     arch: .../arch/riscv_gcc/common/mtimer.c
--     arch: .../arch/riscv_gcc/common/core_kernel_impl.c
--     arch: .../arch/riscv_gcc/common/core_support.S
-- fmp3_core: FMP3_LDSCRIPT   = .../sdk/boards/icicle-kit-es/platform_config/lim-debug/linker/mpfs-lim.ld
-- fmp3_core: FMP3_CFG1_OUT_LINK_OPTIONS = -Wl,--no-gc-sections
-- fmp3_core: FMP3_LINK_OPTIONS = ...;-Wl,--gc-sections;-Wl,--undefined=_kernel_mpfinib_table;-T;.../mpfs-lim.ld
```
（`FMP3_LINK_OPTIONS` は `-T` が1回だけ現れることを確認する。`grep -c` で
`-T`／`-Wl,-T` の出現数を数えて `1` であることを確認するとよい。）

- [ ] **Step 7: `FMP3_PRC_NUM` が効くことを確認する**

Run: `cmake --preset polarfire_soc_kit-qemu -DFMP3_PRC_NUM=2 2>&1 | grep 'FMP3_COMPILE_DEFS\|FMP3_PRC_NUM'`
Expected:
```
-- fmp3_core: FMP3_PRC_NUM    = '2'
-- fmp3_core: FMP3_COMPILE_DEFS= TOPPERS_USE_QEMU;MPFS_ICICLE_KIT;TOPPERS_OMIT_BSS_INIT;TOPPERS_OMIT_DATA_INIT;TOPPERS_OMIT_TECS;TNUM_PRCID=2
```

- [ ] **Step 8: 実機プリセットで specs とボードが切り替わることを確認する**

Run: `cmake --preset polarfire_soc_kit 2>&1 | grep 'FMP3_BOARD\|FMP3_RISCV_SPECS\|CFG1_OUT_LINK'`
Expected:
```
-- fmp3_core: FMP3_BOARD      = MPFS_DISCOVERY_KIT
-- fmp3_core: FMP3_RISCV_SPECS= --specs=nano.specs
-- fmp3_core: FMP3_CFG1_OUT_LINK_OPTIONS = 
```

- [ ] **Step 9: キャッシュを消して QEMU プリセットに戻す**

Run: `rm -rf build/polarfire_soc_kit-qemu && cmake --preset polarfire_soc_kit-qemu > /dev/null && echo OK`
Expected: `OK`
（Step 7 で `FMP3_PRC_NUM=2` がキャッシュに残っているため、以降の Task に影響しないよう消す）

- [ ] **Step 10: コミット**

```bash
git add arch/riscv_gcc/common/arch.cmake arch/riscv_gcc/polarfire_soc/chip.cmake \
        target/polarfire_soc_kit_gcc/target.cmake CMakeLists.txt
git commit -m "build: polarfire の arch/chip/target 層を追加

上流 Makefile.core / Makefile.chip / Makefile.target を CMake に写した。
FMP3_PRC_NUM（-DTNUM_PRCID）と FMP3_CFG1_OUT_LINK_OPTIONS
（cfg1_out 専用の --no-gc-sections）を最初から入れてある。"
```

---

### Task 3: `cfg_py/cfg.py` シム（Ruby への委譲）

**Files:**
- Create: `cfg_py/cfg.py`

**Interfaces:**
- Consumes: pristine `cfg/cfg.rb`（コマンドライン引数の形式をそのまま踏襲する）
- Produces: `cfg_py/cfg.py`（実行可能な Python スクリプト。以降 Task 4〜9 の `CFG_COMMAND` はこれだけを呼ぶ）

- [ ] **Step 1: シムがまだ無いことを確認する（失敗の確認）**

Run: `ls cfg_py/cfg.py 2>&1`
Expected: `ls: cannot access 'cfg_py/cfg.py': No such file or directory`
（`cfg_py/` には README.md しか無い）

- [ ] **Step 2: Ruby 版の `--version` 出力を確認する（シムの判定基準を先に固定する）**

Run: `ruby cfg/cfg.rb --version`
Expected: `cfg 1.7.1`
（このリポジトリの `cfg/cfg.rb` の実測値。以降の positive control はこの文字列と比較する）

- [ ] **Step 3: シム本体を書く**

`cfg_py/cfg.py`:
```python
#!/usr/bin/env python3
#
#               cfg_py/cfg.py -- 計画A限りの Ruby 委譲シム
#
#  ★これは計画A（本計画）の間だけ存在するシムである。計画Bで
#    asp3_core 1.7.1 の本物の Python cfg エンジンにこのファイルごと
#    差し替える。
#
#  pristine の cfg/cfg.rb をそのまま呼び出す薄いラッパ。cfg.rb と
#  同じコマンドライン引数を受け取り、解釈せずに ruby へそのまま渡し、
#  終了コードを透過する。
#
#  ★引数を解釈しない。解釈すると Ruby 版との差異が生まれる。
#
#  作業ディレクトリと環境変数は呼び出し元から引き継ぐ（cd しない、
#  env を作り直さない）。cfg は cfg1_out.db / cfg1_out.syms /
#  cfg1_out.srec / cfg2_out.db を裸の相対名で読み書きするため、
#  cwd が load-bearing である。
#
import shutil
import subprocess
import sys
from pathlib import Path


def main() -> int:
    ruby = shutil.which("ruby")
    if ruby is None:
        print(
            "cfg_py/cfg.py: 'ruby' not found on PATH. "
            "cfg_py/cfg.py is a Plan-A shim that delegates to the pristine "
            "cfg/cfg.rb and requires a ruby interpreter.",
            file=sys.stderr,
        )
        return 127

    cfg_rb = Path(__file__).resolve().parent.parent / "cfg" / "cfg.rb"
    if not cfg_rb.is_file():
        print(f"cfg_py/cfg.py: {cfg_rb} not found.", file=sys.stderr)
        return 1

    #  sys.argv[1:] をそのまま渡す。解釈しない。
    result = subprocess.run([ruby, str(cfg_rb), *sys.argv[1:]])
    return result.returncode


if __name__ == "__main__":
    sys.exit(main())
```

- [ ] **Step 4: positive control — シム経由で本当に Ruby が動いていることを確認する**

Run: `python3 -B cfg_py/cfg.py --version`
Expected: `cfg 1.7.1`
（Step 2 で確認した `cfg/cfg.rb` 直接実行時の出力と一致する。一致しなければシムが `cfg.rb` を呼んでいないか、別の `cfg.rb` を拾っている）

- [ ] **Step 5: negative control — `ruby` が無い環境では明確に失敗する**

Run:
```bash
tmpbin=$(mktemp -d)
ln -s "$(command -v python3)" "$tmpbin/python3"
PATH="$tmpbin" "$tmpbin/python3" -B cfg_py/cfg.py --version
echo "exit=$?"
```
Expected:
```
cfg_py/cfg.py: 'ruby' not found on PATH. cfg_py/cfg.py is a Plan-A shim that delegates to the pristine cfg/cfg.rb and requires a ruby interpreter.
exit=127
```
→ **`ruby` が無いと確実に落ちる**ことの実証（黙って何もしない、あるいは別の cfg 実装へ
フォールバックするといった曖昧な失敗をしないことの確認）。

- [ ] **Step 6: コミット**

```bash
git add cfg_py/cfg.py
git commit -m "build: cfg_py/cfg.py に Ruby 委譲シムを追加（計画A限り）

CMake が呼ぶ cfg を常に cfg_py/cfg.py だけにするための土台。
中身は pristine の cfg/cfg.rb へ引数をそのまま渡す薄いラッパで、
計画Bで asp3_core 1.7.1 の本物のエンジンに差し替える。
--version がシム経由でも直接実行と同じ 'cfg 1.7.1' を返すこと、
ruby が無い環境では明確に失敗することを確認した。"
```

---

### Task 4: cfg pass1 と `cfg1_out` のリンク

**Files:**
- Modify: `CMakeLists.txt`

**Interfaces:**
- Consumes: Task 2 が積んだ全変数、`cfg_py/cfg.py`（Task 3）
- Produces:
  - `CFG_COMMAND`（リスト。cfg の起動コマンド。常に `${Python3_EXECUTABLE} -B ${FMP3_ROOT_DIR}/cfg_py/cfg.py` 固定）
  - `CFG_GEN_DIR` = `${CMAKE_BINARY_DIR}/generated`（cfg の作業ディレクトリ。全パス共通）
  - `${CFG_GEN_DIR}/cfg1_out.c` / `cfg1_out.timestamp` / `cfg1_out.db`
  - `${CFG_GEN_DIR}/cfg1_out.syms` / `cfg1_out.srec`
  - CMake ターゲット `cfg1_out`（実行ファイル。**実行はしない**）

- [ ] **Step 1: 生成物がまだ無いことを確認する（失敗の確認）**

Run: `cmake --build build/polarfire_soc_kit-qemu 2>&1 | tail -3; ls build/polarfire_soc_kit-qemu/generated 2>&1`
Expected: `ls: cannot access 'build/polarfire_soc_kit-qemu/generated': No such file or directory`

- [ ] **Step 2: cfg 起動コマンドと引数組み立てを書く**

`CMakeLists.txt` の `include(${FMP3_TARGET_DIR}/target.cmake)` と `FMP3_PRC_NUM` 適用の**後**に追加:
```cmake
#
#  コンフィギュレータの起動コマンド
#
#  CMake が呼ぶのは常に cfg_py/cfg.py だけ（AGENTS.md §2 規則3）。
#  ★計画Aの間，cfg_py/cfg.py は pristine の cfg/cfg.rb へ委譲する薄い
#    シムである（Task 3）。CMake から見た呼び先は計画Bでも変わらない
#    ため，このパイプラインは計画Bでそのまま残る。
#    計画Bで cfg_py/ の中身を asp3_core 1.7.1 の本物のエンジンに
#    差し替える。DIVERGENCE_MAP.md の「期限付きの逸脱」参照。
#
find_package(Python3 REQUIRED COMPONENTS Interpreter)
set(CFG_COMMAND ${Python3_EXECUTABLE} -B ${FMP3_ROOT_DIR}/cfg_py/cfg.py)
set(CFG_SCRIPT_DEPS
    ${FMP3_ROOT_DIR}/cfg_py/cfg.py
    ${FMP3_ROOT_DIR}/cfg/cfg.rb
    ${FMP3_ROOT_DIR}/cfg/pass1.rb
    ${FMP3_ROOT_DIR}/cfg/pass2.rb
    ${FMP3_ROOT_DIR}/cfg/GenFile.rb
    ${FMP3_ROOT_DIR}/cfg/SRecord.rb
)
message(STATUS "fmp3_core: cfg = cfg_py/cfg.py (Plan-A shim -> pristine cfg/cfg.rb)")

#
#  cfg 共通の定義
#
#  sample/Makefile:263-264 の CFG_TABS に相当（core_sym.def は arch.cmake が積む）
#
list(APPEND FMP3_API_TABLES    ${FMP3_ROOT_DIR}/kernel/kernel_api.def)
list(APPEND FMP3_SYMVAL_TABLES ${FMP3_ROOT_DIR}/kernel/kernel_sym.def)

#
#  アプリケーションの選択
#
if(NOT DEFINED FMP3_APPLNAME)
    set(FMP3_APPLNAME sample1)
endif()
if(NOT DEFINED FMP3_APPLDIR)
    set(FMP3_APPLDIR ${FMP3_ROOT_DIR}/sample)
endif()
if(NOT IS_ABSOLUTE ${FMP3_APPLDIR})
    set(FMP3_APPLDIR ${FMP3_ROOT_DIR}/${FMP3_APPLDIR})
endif()
set(FMP3_APP_CFG_FILE ${FMP3_APPLDIR}/${FMP3_APPLNAME}.cfg)

#  アプリの .cfg は最後に置く（sample/Makefile:393 の $< に相当）
list(APPEND FMP3_CFG_FILES ${FMP3_APP_CFG_FILE})

#  sample/Makefile:197 の INCLUDES := -I. -I$(SRCDIR)/include ... -I$(SRCDIR)
list(APPEND FMP3_INCLUDE_DIRS
    ${FMP3_ROOT_DIR}/include
    ${FMP3_ROOT_DIR}
    ${FMP3_APPLDIR}
)

#
#  生成物の置き場所
#
#  cfg は cfg1_out.db / cfg1_out.syms / cfg1_out.srec / cfg2_out.db を
#  ★裸の相対名で読み書きする★ため，全パスで WORKING_DIRECTORY を
#  ここに固定することが必須である．
#
set(CFG_GEN_DIR ${CMAKE_BINARY_DIR}/generated)
file(MAKE_DIRECTORY ${CFG_GEN_DIR})

set(CFG1_OUT_FILE       ${CFG_GEN_DIR}/cfg1_out.c)
set(CFG1_OUT_TIMESTAMP  ${CFG_GEN_DIR}/cfg1_out.timestamp)
set(CFG1_OUT_DEPFILE    ${CFG_GEN_DIR}/cfg1_out_c.d)
set(CFG1_OUT_SYMS_FILE  ${CFG_GEN_DIR}/cfg1_out.syms)
set(CFG1_OUT_SREC_FILE  ${CFG_GEN_DIR}/cfg1_out.srec)
set(OFFSET_H_FILE       ${CFG_GEN_DIR}/offset.h)
set(OFFSET_TIMESTAMP    ${CFG_GEN_DIR}/offset.timestamp)
set(KERNEL_CFG_TIMESTAMP ${CFG_GEN_DIR}/kernel_cfg.timestamp)
set(KERNEL_CFG_C_FILE   ${CFG_GEN_DIR}/kernel_cfg.c)
set(KERNEL_CFG_H_FILE   ${CFG_GEN_DIR}/kernel_cfg.h)

#  生成した kernel_cfg.h / offset.h を #include できるようにする
list(APPEND FMP3_INCLUDE_DIRS ${CFG_GEN_DIR})

#
#  cfg コマンドライン引数の組み立て
#
foreach(path IN LISTS FMP3_SYMVAL_TABLES)
    list(APPEND CFG_SYMVAL_TABLES "--symval-table" ${path})
endforeach()
foreach(path IN LISTS FMP3_API_TABLES)
    list(APPEND CFG_API_TABLES "--api-table" ${path})
endforeach()
foreach(path IN LISTS FMP3_CLASS_TRB_FILES)
    list(APPEND CFG_CLASS_TRB_FILES "-C" ${path})
endforeach()
foreach(path IN LISTS FMP3_OFFSET_TRB_FILES)
    list(APPEND CFG_OFFSET_TRB_FILES "-T" ${path})
endforeach()
foreach(path IN LISTS FMP3_KERNEL_CFG_TRB_FILES)
    list(APPEND CFG_KERNEL_CFG_TRB_FILES "-T" ${path})
endforeach()
foreach(path IN LISTS FMP3_CHECK_TRB_FILES)
    list(APPEND CFG_CHECK_TRB_FILES "-T" ${path})
endforeach()
foreach(path IN LISTS FMP3_INCLUDE_DIRS)
    list(APPEND CFG_INCLUDE_DIRS "-I${path}")
endforeach()
```

- [ ] **Step 3: pass1 と `cfg1_out` を書く**

`CMakeLists.txt` の Step 2 の続きに追加:
```cmake
#
#  cfg パス1：.cfg ＋ api-table → cfg1_out.c
#
#  ★OUTPUT に宣言するのは cfg1_out.timestamp であって cfg1_out.c ではない．
#    cfg の GenFile は内容が変わらないときファイルを書き直さない
#    （タイムスタンプを更新しない）ため，.c を OUTPUT に宣言すると
#    毎ビルド pass1 が再実行される．上流 Makefile も timestamp 方式である．
#
#  ★DEPFILE で .cfg が #include するヘッダを追跡する．cfg が -M で書く
#    depfile のターゲットは裸の "cfg1_out.timestamp" だが，CMake 3.23+ の
#    Ninja ジェネレータが変換するため OUTPUT と一致しなくてよい（Task 5 の
#    positive control で実証する）．
#
add_custom_command(
    OUTPUT ${CFG1_OUT_TIMESTAMP}
    BYPRODUCTS ${CFG1_OUT_FILE} ${CFG_GEN_DIR}/cfg1_out.db
    WORKING_DIRECTORY ${CFG_GEN_DIR}
    COMMAND ${CFG_COMMAND} --pass 1 --kernel fmp
            ${CFG_INCLUDE_DIRS} ${CFG_API_TABLES} ${CFG_SYMVAL_TABLES}
            -M ${CFG1_OUT_DEPFILE} ${FMP3_CFG_FILES}
    DEPENDS ${CFG_SCRIPT_DEPS} ${FMP3_API_TABLES} ${FMP3_SYMVAL_TABLES}
            ${FMP3_CFG_FILES}
    DEPFILE ${CFG1_OUT_DEPFILE}
    COMMENT "Running cfg pass 1 to generate cfg1_out.c"
)

#
#  ★cfg1_out.c を「別の add_custom_command の OUTPUT」にしてはならない．
#    同じファイルを生成する規則が2つできて Ninja が
#    "multiple rules generate ..." で失敗する．
#    BYPRODUCTS で GENERATED 扱いにし，順序は add_dependencies で付ける．
#
add_custom_target(cfg_pass1 DEPENDS ${CFG1_OUT_TIMESTAMP})
set_source_files_properties(${CFG1_OUT_FILE} PROPERTIES GENERATED TRUE)

#
#  cfg1_out のビルド（静的 API パラメータの値の取り出し用）
#
#  ★この実行ファイルは絶対に実行しない．nm と objcopy でシンボルの値を
#    静的に読み出すだけである．これがクロスコンパイルで成立する理由．
#
add_executable(cfg1_out ${FMP3_START_FILES} ${CFG1_OUT_FILE})
add_dependencies(cfg1_out cfg_pass1)
target_include_directories(cfg1_out
    PRIVATE ${FMP3_INCLUDE_DIRS}
    PRIVATE ${FMP3_ROOT_DIR}/kernel
)
target_compile_definitions(cfg1_out PRIVATE ${FMP3_COMPILE_DEFS})
target_compile_options(cfg1_out PRIVATE ${FMP3_COMPILE_OPTIONS})
#  ★最終 ELF 用の --gc-sections が cfg1_out に効くと TOPPERS_magic_number が
#    消え，pass2 が cfg1_out.syms から見つけられずに停止する．
#    上流 Makefile.target:110 と同じく --no-gc-sections を明示的に足す．
target_link_options(cfg1_out PRIVATE
    ${FMP3_LINK_OPTIONS} ${FMP3_CFG1_OUT_LINK_OPTIONS})
target_link_libraries(cfg1_out PRIVATE ${FMP3_LINK_LIBS})

if(DEFINED FMP3_LDSCRIPT)
    set_property(TARGET cfg1_out APPEND PROPERTY LINK_DEPENDS ${FMP3_LDSCRIPT})
endif()

#
#  シンボルテーブルとイメージの取り出し
#
#  ★">" のシェルリダイレクトは使わない（ジェネレータ依存になるため）．
#    cmake -E env の代わりに nm の出力を CMake 経由でファイルへ書く．
#
add_custom_command(
    OUTPUT ${CFG1_OUT_SYMS_FILE}
    COMMAND ${CMAKE_COMMAND}
            -DNM=${CMAKE_NM}
            -DELF=$<TARGET_FILE:cfg1_out>
            -DOUT=${CFG1_OUT_SYMS_FILE}
            -P ${FMP3_ROOT_DIR}/cmake/nm_to_file.cmake
    DEPENDS cfg1_out ${FMP3_ROOT_DIR}/cmake/nm_to_file.cmake
    COMMENT "Generating cfg1_out.syms"
)

add_custom_command(
    OUTPUT ${CFG1_OUT_SREC_FILE}
    COMMAND ${CMAKE_OBJCOPY} -O srec -S $<TARGET_FILE:cfg1_out> ${CFG1_OUT_SREC_FILE}
    DEPENDS cfg1_out
    COMMENT "Generating cfg1_out.srec"
)
```

- [ ] **Step 4: `nm` の出力をファイルに書くヘルパを書く**

`cmake/nm_to_file.cmake` を新規作成:
```cmake
#
#  nm の出力をファイルに書く（シェルの ">" を使わないための小道具）
#
#  使い方:
#    cmake -DNM=<nm> -DELF=<elf> -DOUT=<file> -P nm_to_file.cmake
#
#  add_custom_command で "COMMAND nm ... > out" と書くと，リダイレクトを
#  解釈できるジェネレータ（Ninja/Makefile）でしか動かない．
#
execute_process(
    COMMAND ${NM} -n ${ELF}
    OUTPUT_VARIABLE _syms
    RESULT_VARIABLE _rc
    ERROR_VARIABLE  _err
)
if(NOT _rc EQUAL 0)
    message(FATAL_ERROR "nm failed (${_rc}): ${_err}")
endif()
file(WRITE ${OUT} "${_syms}")
```

- [ ] **Step 5: `cfg1_out` が生成・リンクされることを確認する**

Run:
```bash
cmake --preset polarfire_soc_kit-qemu > /dev/null
cmake --build build/polarfire_soc_kit-qemu --target cfg1_out 2>&1 | tail -5
ls -la build/polarfire_soc_kit-qemu/generated/cfg1_out.c build/polarfire_soc_kit-qemu/cfg1_out
```
Expected: ビルドが成功し、`cfg1_out.c`（数百KB規模）と実行ファイル `cfg1_out` が存在する。

- [ ] **Step 6: `.syms` と `.srec` を作り、マジックナンバーが残っていることを確認する**

Run:
```bash
cmake --build build/polarfire_soc_kit-qemu --target cfg1_out
cd build/polarfire_soc_kit-qemu && ninja generated/cfg1_out.syms generated/cfg1_out.srec
grep -c TOPPERS_magic_number generated/cfg1_out.syms
```
Expected: `1`
（0 なら `--no-gc-sections` が効いていない。pass2 が `error_exit` で止まる）

- [ ] **Step 7: positive control — `--no-gc-sections` を外すと本当にマジックナンバーが消えることを実演する**

Run:
```bash
cd build/polarfire_soc_kit-qemu
riscv64-unknown-elf-gcc -march=rv64imafdc -mabi=lp64d -mcmodel=medany \
  --specs=picolibc.specs -nostdlib -nostartfiles -Wl,--gc-sections \
  -T ../../target/polarfire_soc_kit_gcc/sdk/boards/icicle-kit-es/platform_config/lim-debug/linker/mpfs-lim.ld \
  CMakeFiles/cfg1_out.dir/generated/cfg1_out.c.o \
  CMakeFiles/cfg1_out.dir/arch/riscv_gcc/common/start.S.o \
  -lc -lgcc -o /tmp/cfg1_out_gc 2>/dev/null
riscv64-unknown-elf-nm -n /tmp/cfg1_out_gc | grep -c TOPPERS_magic_number
```
Expected: `0`
→ **`--gc-sections` を効かせるとマジックナンバーが実際に消える**ことの実証。Step 6 の `1` が偶然でないことがこれで裏付けられる。

- [ ] **Step 8: コミット**

```bash
git add CMakeLists.txt cmake/nm_to_file.cmake
git commit -m "build: cfg pass1 と cfg1_out のリンクを追加

CFG_COMMAND は cfg_py/cfg.py 固定（中身は計画AではRuby委譲シム。Task 3）。
OUTPUT は cfg1_out.timestamp（GenFile が内容不変時にファイルを書き直さない
ため .c を OUTPUT にすると毎ビルド再実行される）。
cfg1_out には --no-gc-sections を明示的に足す（TOPPERS_magic_number が
消えると pass2 が停止するため）。"
```

---

### Task 5: cfg pass2（`offset.h` と `kernel_cfg.c/h`）

**Files:**
- Modify: `CMakeLists.txt`

**Interfaces:**
- Consumes: `CFG_COMMAND` / `CFG_GEN_DIR` / `CFG1_OUT_SYMS_FILE` / `CFG1_OUT_SREC_FILE`（Task 4）
- Produces:
  - `${CFG_GEN_DIR}/offset.h` / `offset.timestamp`
  - `${CFG_GEN_DIR}/kernel_cfg.c` / `kernel_cfg.h` / `kernel_cfg.timestamp` / `cfg2_out.db`
  - CMake ターゲット `generate_cfg_gen_files`（上記すべてを束ねる）

- [ ] **Step 1: `kernel_cfg.c` がまだ無いことを確認する（失敗の確認）**

Run: `ls build/polarfire_soc_kit-qemu/generated/kernel_cfg.c 2>&1`
Expected: `ls: cannot access '...': No such file or directory`

- [ ] **Step 2: pass2 の2回の呼び出しを書く**

`CMakeLists.txt` の Task 4 の末尾に追加:
```cmake
#
#  cfg パス2（-O）：offset.h の生成
#
#  上流 sample/Makefile:417-421 に相当．-O は cfg2_out.db を出力しない指定で，
#  次の kernel_cfg 生成側が書く db を潰さないためにある．
#
add_custom_command(
    OUTPUT ${OFFSET_TIMESTAMP}
    BYPRODUCTS ${OFFSET_H_FILE}
    WORKING_DIRECTORY ${CFG_GEN_DIR}
    COMMAND ${CFG_COMMAND} --pass 2 -O --kernel fmp
            ${CFG_INCLUDE_DIRS} ${CFG_CLASS_TRB_FILES} ${CFG_OFFSET_TRB_FILES}
            --rom-symbol ${CFG1_OUT_SYMS_FILE} --rom-image ${CFG1_OUT_SREC_FILE}
    DEPENDS ${CFG_SCRIPT_DEPS} ${FMP3_CLASS_TRB_FILES} ${FMP3_OFFSET_TRB_FILES}
            ${CFG1_OUT_SYMS_FILE} ${CFG1_OUT_SREC_FILE} ${CFG1_OUT_TIMESTAMP}
    COMMENT "Running cfg pass 2 to generate offset.h"
)

#
#  cfg パス2：kernel_cfg.c / kernel_cfg.h の生成
#
#  上流 sample/Makefile:408-411 に相当．こちらは --rom-symbol / --rom-image を
#  渡さない（cfg が cfg1_out.syms / cfg1_out.srec を裸の相対名で開くため，
#  WORKING_DIRECTORY が効いている必要がある）．
#
#  offset.h の生成と順序を付けるため OFFSET_TIMESTAMP に依存させる
#  （上流も同じ理由で cfg1_out.db を共有する2つの規則を分けている）．
#
add_custom_command(
    OUTPUT ${KERNEL_CFG_TIMESTAMP}
    BYPRODUCTS ${KERNEL_CFG_C_FILE} ${KERNEL_CFG_H_FILE} ${CFG_GEN_DIR}/cfg2_out.db
    WORKING_DIRECTORY ${CFG_GEN_DIR}
    COMMAND ${CFG_COMMAND} --pass 2 --kernel fmp
            ${CFG_INCLUDE_DIRS} ${CFG_CLASS_TRB_FILES} ${CFG_KERNEL_CFG_TRB_FILES}
    DEPENDS ${CFG_SCRIPT_DEPS} ${FMP3_CLASS_TRB_FILES} ${FMP3_KERNEL_CFG_TRB_FILES}
            ${CFG1_OUT_SYMS_FILE} ${CFG1_OUT_SREC_FILE} ${OFFSET_TIMESTAMP}
    COMMENT "Running cfg pass 2 to generate kernel_cfg.c/h"
)

#
#  ★cfg1_out.c と同じ理由で，kernel_cfg.c を別規則の OUTPUT にしない．
#    BYPRODUCTS で GENERATED 扱いにし，順序は add_dependencies で付ける
#    （Task 6 の add_library が ${KERNEL_CFG_C_FILE} をソースに取る）．
#
add_custom_target(generate_cfg_gen_files
    DEPENDS ${OFFSET_TIMESTAMP} ${KERNEL_CFG_TIMESTAMP}
)
set_source_files_properties(${KERNEL_CFG_C_FILE} PROPERTIES GENERATED TRUE)
```

- [ ] **Step 3: 生成されることを確認する**

Run:
```bash
cmake --build build/polarfire_soc_kit-qemu --target generate_cfg_gen_files 2>&1 | tail -4
ls build/polarfire_soc_kit-qemu/generated/
```
Expected: `cfg1_out.c cfg1_out.db cfg1_out.srec cfg1_out.syms cfg1_out.timestamp cfg1_out_c.d kernel_cfg.c kernel_cfg.h kernel_cfg.timestamp cfg2_out.db offset.h offset.timestamp` が揃う。

- [ ] **Step 4: マルチプロセッサ構成が生成物に出ていることを確認する**

Run: `grep -c '_kernel_pcb_prc[1-4]' build/polarfire_soc_kit-qemu/generated/kernel_cfg.c`
Expected: `4` 以上
（polarfire の `TNUM_PRCID` 既定は 4。プロセッサ別 PCB が4つ出る。設計書 §3 の「マルチプロセッサ性は生成物の中身に現れる」の実測確認）

Run: `grep -c 'CLS_' build/polarfire_soc_kit-qemu/generated/kernel_cfg.c`
Expected: `1` 以上（クラス別セクション名が出ている）

- [ ] **Step 5: positive control（その1）— 無変更の再ビルドで cfg が走らないこと**

Run:
```bash
cd build/polarfire_soc_kit-qemu
ninja generate_cfg_gen_files > /dev/null
ninja generate_cfg_gen_files 2>&1
```
Expected: `ninja: no work to do.`
→ **設計書 §6-2（毎ビルド cfg 再実行）を踏んでいない**ことの実証。
（もし毎回 pass1〜2 が走るなら、`OUTPUT` に `.c` を宣言してしまっている）

- [ ] **Step 6: positive control（その2）— `.cfg` が include するヘッダを触ると pass1 が再実行されること**

`sample/sample1.cfg` は `syssvc/syslog.cfg` 等を `INCLUDE` している（`sample/sample1.cfg:6-9`）。
Run:
```bash
cd build/polarfire_soc_kit-qemu
touch ../../syssvc/syslog.cfg
ninja generate_cfg_gen_files -d explain 2>&1 | grep -E 'explain.*syslog|Running cfg pass 1'
```
Expected: `syslog.cfg` を理由として `cfg1_out.timestamp` が dirty になり、`Running cfg pass 1` が実行される。
→ **設計書 §6-1（depfile 未接続）を踏んでいない**ことの実証。

Run（対照）:
```bash
cd build/polarfire_soc_kit-qemu
ninja generate_cfg_gen_files 2>&1
```
Expected: `ninja: no work to do.`
→ Step 6 の再実行が「毎回走っているだけ」ではないことの確認。**この対照を省略してはならない。**

- [ ] **Step 7: コミット**

```bash
git add CMakeLists.txt
git commit -m "build: cfg pass2（offset.h と kernel_cfg.c/h）を追加

OUTPUT は timestamp、生成される .c/.h は BYPRODUCTS。
無変更再ビルドで cfg が走らないこと、.cfg の include ヘッダを触ると
pass1 が再実行されることを positive control で確認した。"
```

---

### Task 6: カーネルライブラリ `libfmp3.a`

**Files:**
- Modify: `CMakeLists.txt`

**Interfaces:**
- Consumes: `FMP3_ARCH_C_FILES` / `FMP3_TARGET_C_FILES`（Task 2）、`KERNEL_CFG_C_FILE`（Task 5）
- Produces: CMake ターゲット `fmp3`（`libfmp3.a`）

- [ ] **Step 1: ライブラリがまだ無いことを確認する（失敗の確認）**

Run: `ls build/polarfire_soc_kit-qemu/libfmp3.a 2>&1`
Expected: `ls: cannot access 'build/polarfire_soc_kit-qemu/libfmp3.a': No such file or directory`

- [ ] **Step 2: ライブラリを書く**

`CMakeLists.txt` の Task 5 の末尾に追加:
```cmake
#
#  カーネルライブラリ（libfmp3.a）
#
#  カーネルの .c は上流 kernel/Makefile.kernel の KERNEL_FCSRCS（22個）に
#  対応する．上流は1つの .c から複数の .o を作る（ALLFUNC を定義しない）
#  ビルドもできるが，CMake 側は asp3_core と同じく .c を1回ずつコンパイルし，
#  ALLFUNC を定義して全関数を1つの .o に入れる．
#
add_library(fmp3 STATIC
    ${FMP3_ARCH_C_FILES}
    ${FMP3_TARGET_C_FILES}
    ${FMP3_ROOT_DIR}/kernel/startup.c
    ${FMP3_ROOT_DIR}/kernel/task.c
    ${FMP3_ROOT_DIR}/kernel/taskhook.c
    ${FMP3_ROOT_DIR}/kernel/wait.c
    ${FMP3_ROOT_DIR}/kernel/time_event.c
    ${FMP3_ROOT_DIR}/kernel/task_manage.c
    ${FMP3_ROOT_DIR}/kernel/task_refer.c
    ${FMP3_ROOT_DIR}/kernel/task_sync.c
    ${FMP3_ROOT_DIR}/kernel/task_term.c
    ${FMP3_ROOT_DIR}/kernel/semaphore.c
    ${FMP3_ROOT_DIR}/kernel/eventflag.c
    ${FMP3_ROOT_DIR}/kernel/dataqueue.c
    ${FMP3_ROOT_DIR}/kernel/pridataq.c
    ${FMP3_ROOT_DIR}/kernel/mutex.c
    ${FMP3_ROOT_DIR}/kernel/mempfix.c
    ${FMP3_ROOT_DIR}/kernel/time_manage.c
    ${FMP3_ROOT_DIR}/kernel/cyclic.c
    ${FMP3_ROOT_DIR}/kernel/alarm.c
    ${FMP3_ROOT_DIR}/kernel/spin_lock.c
    ${FMP3_ROOT_DIR}/kernel/sys_manage.c
    ${FMP3_ROOT_DIR}/kernel/interrupt.c
    ${FMP3_ROOT_DIR}/kernel/exception.c
    ${KERNEL_CFG_C_FILE}
)
add_dependencies(fmp3 generate_cfg_gen_files)

target_include_directories(fmp3
    PUBLIC  ${FMP3_INCLUDE_DIRS}
    PRIVATE ${FMP3_ROOT_DIR}/kernel
)
#  ALLFUNC: 上流 sample/Makefile:302 の KERNEL_CFLAGS := -DALLFUNC ...
target_compile_definitions(fmp3
    PRIVATE ALLFUNC
    PUBLIC  ${FMP3_COMPILE_DEFS}
)
target_compile_options(fmp3 PUBLIC ${FMP3_COMPILE_OPTIONS})
```

- [ ] **Step 3: ライブラリがビルドできることを確認する**

Run: `cmake --build build/polarfire_soc_kit-qemu --target fmp3 2>&1 | tail -3; ls -la build/polarfire_soc_kit-qemu/libfmp3.a`
Expected: ビルド成功、`libfmp3.a` が存在する（数百KB規模）

- [ ] **Step 4: 中身が期待どおりであることを確認する**

Run: `riscv64-unknown-elf-nm build/polarfire_soc_kit-qemu/libfmp3.a | grep -c '_kernel_pcb_prc'`
Expected: `4` 以上（プロセッサ別 PCB がライブラリに入っている）

Run: `riscv64-unknown-elf-ar t build/polarfire_soc_kit-qemu/libfmp3.a | grep -E 'spin_lock|msi_ipi|plic_kernel_impl|kernel_cfg' | sort`
Expected:
```
kernel_cfg.c.o
msi_ipi.c.o
plic_kernel_impl.c.o
spin_lock.c.o
```
（`spin_lock.c` は FMP3 固有で ASP3 には無い。`msi_ipi.c` は IPI。両方が入っていることが FMP3 として正しい証拠）

- [ ] **Step 5: コミット**

```bash
git add CMakeLists.txt
git commit -m "build: カーネルライブラリ libfmp3.a を追加

kernel/Makefile.kernel の KERNEL_FCSRCS 22個 + arch/chip/target のソース
+ 生成された kernel_cfg.c。FMP3 固有の spin_lock.c / msi_ipi.c を含む。"
```

---

### Task 7: `fmp` 実行ファイル（SDK 込み）と pass3

**Files:**
- Modify: `CMakeLists.txt`

**Interfaces:**
- Consumes: `fmp3` ライブラリ（Task 6）、`FMP3_SDK_C_FILES` / `FMP3_SDK_ASM_FILES`（Task 2）、`fmp3_add_syssvc()`（Task 1）
- Produces:
  - CMake ターゲット `fmp`（実行ファイル `fmp`）
  - `fmp3_cfg_check(TARGET)`（関数。pass3 を `TARGET` の POST_BUILD に付ける）

- [ ] **Step 1: 実行ファイルがまだ無いことを確認する（失敗の確認）**

Run: `ls build/polarfire_soc_kit-qemu/fmp 2>&1`
Expected: `ls: cannot access 'build/polarfire_soc_kit-qemu/fmp': No such file or directory`

- [ ] **Step 2: pass3 のヘルパと実行ファイルを書く**

`CMakeLists.txt` の Task 6 の末尾に追加:
```cmake
#
#  cfg パス3：構成チェック（最終 ELF に対する POST_BUILD）
#
#  上流 sample/Makefile:483-487 に相当．
#  ライブラリ専用モードでは最終 ELF が無いため呼ばない．引数一式は
#  親スコープへ export し，外部 SDK 側が自前の ELF に対して呼べるようにする．
#
set(PASS3_ARGS ${CFG_COMMAND} --pass 3 --kernel fmp -O
    ${CFG_INCLUDE_DIRS} ${CFG_CHECK_TRB_FILES})

function(fmp3_cfg_check TARGET)
    add_custom_command(TARGET ${TARGET} POST_BUILD
        WORKING_DIRECTORY ${CFG_GEN_DIR}
        COMMAND ${CMAKE_COMMAND}
                -DNM=${CMAKE_NM}
                -DELF=$<TARGET_FILE:${TARGET}>
                -DOUT=${CFG_GEN_DIR}/${TARGET}.syms
                -P ${FMP3_ROOT_DIR}/cmake/nm_to_file.cmake
        COMMAND ${CMAKE_OBJCOPY} -O srec -S $<TARGET_FILE:${TARGET}>
                ${CFG_GEN_DIR}/${TARGET}.srec
        COMMAND ${PASS3_ARGS}
                --rom-symbol ${CFG_GEN_DIR}/${TARGET}.syms
                --rom-image  ${CFG_GEN_DIR}/${TARGET}.srec
        COMMENT "Running cfg pass 3 to check configuration"
    )
endfunction()

if(FMP3_LIBRARY_ONLY)
    #  外部 SDK 側が最終 ELF を作るため，pass3 の材料を渡す
    set(FMP3_PASS3_ARGS   ${PASS3_ARGS}  PARENT_SCOPE)
    set(FMP3_CFG_GEN_DIR  ${CFG_GEN_DIR} PARENT_SCOPE)
endif()

if(NOT FMP3_LIBRARY_ONLY)

#
#  アプリケーション実行ファイル（fmp）
#
#  上流 sample/Makefile:453-456 の $(OBJFILE) に相当．
#  START_OBJS（start.S）はライブラリ外で先頭にリンクする．
#  Microchip SDK のソースは上流が SYSSVC_COBJS/SYSSVC_ASMOBJS として
#  最終リンクに加えているのと同じ扱いにする．
#
add_executable(fmp
    ${FMP3_START_FILES}
    ${FMP3_APPLDIR}/${FMP3_APPLNAME}.c
    ${FMP3_SDK_C_FILES}
    ${FMP3_SDK_ASM_FILES}
)
fmp3_add_syssvc(fmp)

target_include_directories(fmp
    PRIVATE ${FMP3_ROOT_DIR}/kernel
)
target_link_libraries(fmp PRIVATE fmp3 ${FMP3_LINK_LIBS})
target_link_options(fmp PRIVATE ${FMP3_LINK_OPTIONS})

#  リンカスクリプトを変更したら再リンクされるようにする
#  （-T / -Wl,-T は自動では依存にならない）
if(DEFINED FMP3_LDSCRIPT)
    set_property(TARGET fmp APPEND PROPERTY LINK_DEPENDS ${FMP3_LDSCRIPT})
endif()

fmp3_cfg_check(fmp)

endif()  # NOT FMP3_LIBRARY_ONLY
```

- [ ] **Step 3: リンクが通り、pass3 が通ることを確認する**

Run: `cmake --build build/polarfire_soc_kit-qemu 2>&1 | tail -8`
Expected: 全ターゲットがビルドされ、末尾に pass3 の実行が現れる。エラーなし。
（`configuration check passed` は上流 Makefile が `echo` しているメッセージであり、cfg 自体は成功時に何も出さない。**pass3 が非ゼロ終了しないこと**が合格条件）

- [ ] **Step 4: 実行ファイルの中身を確認する**

Run: `riscv64-unknown-elf-nm build/polarfire_soc_kit-qemu/fmp | grep -E ' (T|t) (_start|main_task|sta_ker)$' | sort`
Expected: `_start` / `main_task` / `sta_ker` の3つが現れる。

Run: `riscv64-unknown-elf-nm build/polarfire_soc_kit-qemu/fmp | grep -c '_kernel_mpfinib_table'`
Expected: `1` 以上
（`--gc-sections` が効く構成なので、`-Wl,--undefined=_kernel_mpfinib_table` が無いとここで消える）

- [ ] **Step 5: コミット**

```bash
git add CMakeLists.txt
git commit -m "build: fmp 実行ファイル（Microchip SDK 込み）と cfg pass3 を追加

SDK ソース16個は上流 Makefile.target:52-65 の SYSSVC_COBJS/ASMOBJS に対応。
asp3_core の polarfire にはこの層が無く流用できないため新規に書いた。
FMP3_LIBRARY_ONLY=ON では実行ファイルを作らず、pass3 の引数を親スコープへ
export する（esp32p4 が使う）。"
```

---

### Task 8: `run` ターゲットと QEMU 起動

**Files:**
- Modify: `target/polarfire_soc_kit_gcc/target.cmake`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Consumes: `fmp` 実行ファイル（Task 7）
- Produces: CMake ターゲット `run`

- [ ] **Step 1: `run` ターゲットがまだ無いことを確認する（失敗の確認）**

Run: `cmake --build build/polarfire_soc_kit-qemu --target run 2>&1 | tail -2`
Expected: `ninja: error: unknown target 'run'`

- [ ] **Step 2: `FMP3_RUN_COMMAND` を書く**

`target/polarfire_soc_kit_gcc/target.cmake` の末尾（`if(POLARFIRE_QEMU)` ブロックの中、`FMP3_CFG1_OUT_LINK_OPTIONS` の行の後）に追加:
```cmake
    #
    #  QEMU による実行（cmake --build <dir> --target run）
    #
    #  ★asp3_core の polarfire の RUN_COMMAND は流用できない．
    #    icicle-kit マシンは既定で envm にリセットして HSS の起動を待つため，
    #    全ハートのリセット PC をカーネルのエントリ（_start）に向ける必要が
    #    ある．そのため5ハートすべてに -device loader を与え，-bios none で
    #    既定の OpenSBI を載せない．
    #    出典: target/polarfire_soc_kit_gcc/target_user.md:177-186
    #
    #    E51（hart0）は MPFS HAL により待機し，U54（hart1〜4＝PRC1〜4）で
    #    FMP3 が動作する．
    #
    set(QEMU_SYSTEM_RISCV64 qemu-system-riscv64
        CACHE STRING "Path to qemu-system-riscv64")
    set(FMP3_RUN_COMMAND
        ${QEMU_SYSTEM_RISCV64} -M microchip-icicle-kit -smp 5 -m 2G -nographic
        -serial mon:stdio -bios none
        -kernel $<TARGET_FILE:fmp>
        -device loader,file=$<TARGET_FILE:fmp>,cpu-num=0
        -device loader,file=$<TARGET_FILE:fmp>,cpu-num=1
        -device loader,file=$<TARGET_FILE:fmp>,cpu-num=2
        -device loader,file=$<TARGET_FILE:fmp>,cpu-num=3
        -device loader,file=$<TARGET_FILE:fmp>,cpu-num=4
    )
```

- [ ] **Step 3: `run` ターゲットを書く**

`CMakeLists.txt` の `endif()  # NOT FMP3_LIBRARY_ONLY` の**直前**に追加:
```cmake
#
#  実行ターゲット（target.cmake が FMP3_RUN_COMMAND を定義した場合）
#
if(DEFINED FMP3_RUN_COMMAND)
    add_custom_target(run
        COMMAND ${FMP3_RUN_COMMAND}
        DEPENDS fmp
        USES_TERMINAL
        COMMENT "Running on QEMU (Ctrl-A X to quit)"
    )
endif()
```

- [ ] **Step 4: QEMU で起動することを確認する**

Run:
```bash
cmake --preset polarfire_soc_kit-qemu > /dev/null
cmake --build build/polarfire_soc_kit-qemu > /dev/null 2>&1
timeout 20 qemu-system-riscv64 -M microchip-icicle-kit -smp 5 -m 2G -nographic \
  -serial mon:stdio -bios none -kernel build/polarfire_soc_kit-qemu/fmp \
  -device loader,file=build/polarfire_soc_kit-qemu/fmp,cpu-num=0 \
  -device loader,file=build/polarfire_soc_kit-qemu/fmp,cpu-num=1 \
  -device loader,file=build/polarfire_soc_kit-qemu/fmp,cpu-num=2 \
  -device loader,file=build/polarfire_soc_kit-qemu/fmp,cpu-num=3 \
  -device loader,file=build/polarfire_soc_kit-qemu/fmp,cpu-num=4 \
  < /dev/null > /tmp/fmp3-qemu.log 2>&1
grep -E 'TOPPERS/FMP3 Kernel Release|Processor [1-4] start\.|Sample program starts' /tmp/fmp3-qemu.log
```
Expected: 次の行がすべて現れる（`syssvc/banner.c:64,72` と `sample/sample1.c:735` の書式に対応）:
```
TOPPERS/FMP3 Kernel Release 3.4.0 for PolarFire SoC FPGA Icicle Kit <U54, RISC-V> (<日付>, <時刻>)
Processor 1 start.
Processor 2 start.
Processor 3 start.
Processor 4 start.
Sample program starts (exinf = 1).
```
（`Processor N start.` が4つ出ることが、4プロセッサ SMP が実際に動いた証拠。1つしか出ないなら二次ハートが起動していない）

- [ ] **Step 5: `--target run` でも同じ結果になることを確認する**

Run: `timeout 20 cmake --build build/polarfire_soc_kit-qemu --target run < /dev/null > /tmp/fmp3-run.log 2>&1; grep -c 'Processor [1-4] start\.' /tmp/fmp3-run.log`
Expected: `4`

- [ ] **Step 6: コミット**

```bash
git add target/polarfire_soc_kit_gcc/target.cmake CMakeLists.txt
git commit -m "build: QEMU 実行ターゲット run を追加

icicle-kit は既定で envm にリセットして HSS を待つため、5ハート全てに
-device loader を与え -bios none を指定する（target_user.md:177-186）。
asp3_core の RUN_COMMAND は流用できない。
sample1 が4プロセッサで起動することを確認済み。"
```

---

### Task 9: ライブラリ専用モードの確認と記録

**Files:**
- Modify: `DIVERGENCE_MAP.md`
- Modify: `CLAUDE.md`

**Interfaces:**
- Consumes: すべて
- Produces: なし（記録のみ）

- [ ] **Step 1: ライブラリ専用モードがまだ検証されていないことを確認する**

Run: `ls build/polarfire_soc_kit-libonly 2>&1`
Expected: `ls: cannot access 'build/polarfire_soc_kit-libonly': No such file or directory`

- [ ] **Step 2: ライブラリ専用モードで設定・ビルドできることを確認する**

Run:
```bash
cmake -G Ninja -B build/polarfire_soc_kit-libonly -S . \
  -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-riscv64.cmake \
  -DFMP3_TARGET=polarfire_soc_kit_gcc -DPOLARFIRE_QEMU=ON \
  -DFMP3_LIBRARY_ONLY=ON > /dev/null
cmake --build build/polarfire_soc_kit-libonly 2>&1 | tail -3
ls build/polarfire_soc_kit-libonly/libfmp3.a
ls build/polarfire_soc_kit-libonly/fmp 2>&1
```
Expected:
- ビルド成功、`libfmp3.a` が存在する
- `ls: cannot access 'build/polarfire_soc_kit-libonly/fmp': No such file or directory`
  （ライブラリ専用モードでは実行ファイルを作らない）

- [ ] **Step 3: `run` ターゲットが作られないことを確認する**

Run: `cmake --build build/polarfire_soc_kit-libonly --target run 2>&1 | tail -2`
Expected: `ninja: error: unknown target 'run'`

- [ ] **Step 4: `DIVERGENCE_MAP.md` に記録する**

`DIVERGENCE_MAP.md` の表に以下の3行を追加する（既存の `cfg/` と `target/` の行の後）:
```markdown
| arch/riscv_gcc/common/arch.cmake | add | Makefile.core の CMake 版。上流の Makefile は残すが CMake ビルドからは参照しない | - |
| arch/riscv_gcc/polarfire_soc/chip.cmake | add | Makefile.chip の CMake 版。`-march` は上流の `rv64gc` ではなく `rv64imafdc`（ISA は同一。`rv64gc` は実在しない multilib ディレクトリ `rv64imafdc/lp64d` に解決され `crt0.o` が見つからないが、`rv64imafdc` は既定ディレクトリ `.` に解決される。ABI は `lp64d` のまま） | 未 |
| target/polarfire_soc_kit_gcc/{target.cmake,presets.json} | add | Makefile.target の CMake 版。Microchip SDK のソース16個を最終リンクに加える | - |
```

さらに表の下に、cfg_py シム足場の期限付き記録を追加する:
```markdown
## 期限付きの逸脱

| 対象 | 内容 | 解消条件 |
|---|---|---|
| `cfg_py/cfg.py`（計画Aの中身。Task 3） | AGENTS.md §2 規則3「pristine の `cfg/` は使わない。cfg 相当は `cfg_py/`（Python）で提供し、CMake から呼ぶ」の**文言は満たす**（CMake が呼ぶのは常に `cfg_py/cfg.py`）が、**精神には抵触する**（`cfg_py/cfg.py` は pristine の `cfg/cfg.rb` へ委譲する薄いシムであり、実行されるのは結局 Ruby 版である）。CMake パイプラインの正しさを、テンプレート Python 移植のバグと切り離して検証するための足場。 | 計画B（`cfg_py/` への asp3_core 1.7.1 エンジン移植とテンプレート移植）の完了時に、シムを本物のエンジンへ差し替える。以降 Ruby は `tools/cfg_equivalence.sh`（CMake 外）からのみ呼ぶオラクルとして残す。**この行が残っている間は AGENTS.md §6 の完了条件を満たさない。** |
```

- [ ] **Step 5: `CLAUDE.md` の現況を更新する**

`CLAUDE.md` の「## 現況（2026-07-18）」節の以下の行:
```markdown
- **`CMakeLists.txt` は雛形のままで、ビルドは通らない**（`add_subdirectory` 無し）。
  `cfg_py/` も README のみで実装が無い。したがって「ビルドして確認」はまだ成立しない。
```
を、次に置き換える:
```markdown
- **polarfire_soc_kit_gcc は CMake でビルドでき、QEMU で動く**（計画A完了）。
  `cmake --preset polarfire_soc_kit-qemu && cmake --build build/polarfire_soc_kit-qemu --target run`
- ただし **cfg は `cfg_py/cfg.py`（pristine の Ruby `cfg/cfg.rb` へ委譲するシム）を使っている
  足場の状態**である。計画Bでシムを asp3_core 1.7.1 の本物のエンジンへ差し替える
  （DIVERGENCE_MAP.md の「期限付きの逸脱」参照）。
- 他の5ターゲットは未対応（`target.cmake` が無い）。
```

- [ ] **Step 6: 記録が正しいことを確認する**

Run: `grep -c 'arch.cmake\|chip.cmake\|target.cmake\|cfg_py/cfg.py' DIVERGENCE_MAP.md`
Expected: `4` 以上

Run: `grep -c 'polarfire_soc_kit-qemu' CLAUDE.md`
Expected: `1` 以上

- [ ] **Step 7: 最終確認 — クリーンビルドが通ること**

Run:
```bash
rm -rf build/polarfire_soc_kit-qemu
cmake --preset polarfire_soc_kit-qemu > /dev/null
cmake --build build/polarfire_soc_kit-qemu 2>&1 | tail -3
timeout 20 cmake --build build/polarfire_soc_kit-qemu --target run < /dev/null 2>&1 | grep -c 'Processor [1-4] start\.'
```
Expected: ビルド成功、最後の出力が `4`

- [ ] **Step 8: コミット**

```bash
git add DIVERGENCE_MAP.md CLAUDE.md
git commit -m "docs: 計画A完了 — pristine への追加と cfg_py シム足場を記録

DIVERGENCE_MAP.md に arch/chip/target の .cmake 追加を記録し、
cfg_py/cfg.py の Ruby 委譲シムを『期限付きの逸脱』として解消条件つきで残した。
CLAUDE.md の現況を polarfire がビルド・実行できる状態に更新。"
```

---

## 完了条件

- [ ] `cmake --preset polarfire_soc_kit-qemu && cmake --build build/polarfire_soc_kit-qemu` が通る
- [ ] `--target run` で `Processor 1 start.` 〜 `Processor 4 start.` と `Sample program starts` が出る
- [ ] 無変更の再ビルドで cfg が走らない（Task 5 Step 5）
- [ ] `.cfg` の include ヘッダを触ると pass1 が再実行される（Task 5 Step 6、対照込み）
- [ ] `cfg1_out.syms` に `TOPPERS_magic_number` があり、`--gc-sections` を効かせると消えることを実演済み（Task 4 Step 6-7）
- [ ] `FMP3_LIBRARY_ONLY=ON` で `libfmp3.a` だけが作られる（Task 9 Step 2-3）
- [ ] `FMP3_PRC_NUM=2` が `-DTNUM_PRCID=2` になる（Task 2 Step 7）
- [ ] pristine への追加が `DIVERGENCE_MAP.md` に記録されている
- [ ] cfg_py シム足場が解消条件つきで記録されている

## 計画Bへの引き継ぎ

計画Aは **CMake パイプラインが正しいこと**だけを保証する。cfg の生成物の正しさは `cfg_py/cfg.py` が委譲する Ruby cfg（＝上流と同じ実装）に依存しているため、この時点では自明に正しい。計画Bで `cfg_py/` の中身を Python エンジンへ差し替えるとき、この Ruby 経路が**そのままオラクル**になる（設計書 §7.1）。CMake 側が呼ぶのは計画A・計画Bを通じて常に `cfg_py/cfg.py` のままなので、A で検証した CMake パイプラインはそのまま残る。

計画Bの入口:
1. `cfg_py/` へ asp3_core 1.7.1 のエンジンを移植し、`cfg_py/cfg.py` の Ruby 委譲シムを本物のエンジンへ差し替える（設計書 §8-4）
2. テンプレート移植 — `kernel/` 15個、`arch/riscv_gcc/` 5個・449行、`target/polarfire_soc_kit_gcc/` 3個（設計書 §8-5）
3. `tools/cfg_equivalence.sh` で Ruby と Python の生成物を比較（`cfg1_out.c` を比較対象に含めること）
4. エラー経路の回帰スイート（設計書 §7.2）
5. `cfg_py/cfg.py` からシム時代の Ruby 委譲コード（`shutil.which("ruby")` 呼び出し等）を削除し、`CFG_SCRIPT_DEPS` から `cfg/cfg.rb` 系ファイルへの依存を外す（CMake 側の `CFG_COMMAND` 自体は変更不要）
