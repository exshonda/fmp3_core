# fmp3_core CMake 化 計画A2（musca_b1 / layering 実地テスト）Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** `musca_b1_gcc`（ARM dual Cortex-M33 / QEMU）を第1波に追加し、汎用層（`CMakeLists.txt`）に手を入れずに target/chip/arch の3ファイル＋ツールチェーン＋プリセットを足すだけで通る設計になっているかを実地で確かめる。手を入れる必要が生じた箇所（`-T` 適用ロジックの target 名決め打ち、`fmp` の cfg 生成物への順序依存漏れ）は汎用層の設計漏れとして修正する。

**Architecture:** 計画A（`polarfire_soc_kit_gcc`）で確立した層構造（`CMakePresets.json` → `target/<t>/presets.json` → `cmake/presets-base.json`、`CMakeLists.txt` → `fmp3_core.cmake` → `target.cmake` → `chip.cmake` → `arch.cmake`）と cfg 3パスパイプラインは**そのまま流用する**（`CMakeLists.txt` は不変のまま動くはずというのが本計画の検証対象）。RISC-V/PLIC/4コア/独立ELF（polarfire）に対し、ARM Cortex-M/NVIC/dual-core/独立ELF（musca_b1）という対照的な構成で層の切り方を検証する。cfg エンジンは計画Aと同じく `cfg_py/cfg.py`（pristine `cfg/cfg.rb` への委譲シム）を使う。

**Tech Stack:** CMake 3.23+ / Ninja / `arm-none-eabi-gcc` 13.2.1（newlib, picolibc 不使用）/ ruby 3.2.3 / `qemu-system-arm`（システム既定 8.2.2、`/home/honda/qemu-build/install/bin/qemu-system-arm` に 11.0.1 がビルド済み）

## Global Constraints

- 設計書は `docs/superpowers/specs/2026-07-18-fmp3-cmake-design.md`。本計画は同文書 §8「横展開」および §10「未決事項（musca_b1 を第1波に足すかどうか）」を実施に移すものである。
- **pristine を編集したら必ず `DIVERGENCE_MAP.md` に1行足す**（AGENTS.md §2 規則2）。本計画で `arch/` `target/` 配下に追加する `.cmake` ファイルはすべて記録対象。
- **`upstream` ブランチに派生ファイルを載せない**（AGENTS.md §2 規則1）。本計画の作業はすべて `main` 上で行う。
- **`cfg_py/cfg.py` は計画Aと同じ Ruby 委譲シムのまま**（計画Bまでの期限付き逸脱。`DIVERGENCE_MAP.md` の「期限付きの逸脱」に既に記録済みで、本計画では変更しない）。
- musca_b1 は **QEMU 専用ターゲット**（実機 Musca-B1 ボードでの動作確認は行っていない。`target/musca_b1_gcc/target_user.txt:42,149-150`）。polarfire のような実機/QEMU 切替オプション（`POLARFIRE_QEMU`）は作らない。
- **実行ファイルに拡張子を付けない**（`fmp`。計画Aの `cmake/toolchain-riscv64.cmake` と作法を揃える。上流 `sample/Makefile` の `OBJNAME = fmp` と同じ名前にする）。
- ライブラリ名 `fmp3`（`libfmp3.a`、変更なし）。
- プリセット名は `musca_b1`（1コア既定）/ `musca_b1-2core`（2コアSMP）。`-qemu` サフィックスは付けない（実機バリアントが無いため polarfire と違い不要）。
- **1コアで通してから2コアを試す。** `FMP3_PRC_NUM=2` の QEMU 実行が仮に失敗しても、原因が QEMU 側（クロスコア IPI の実装度合い）にあるのか実装側にあるのかを切り分けられる形で検証すること（詳細は Task 6）。
- QEMU のパスはキャッシュ変数にする（`QEMU_SYSTEM_ARM_MUSCA_B1`）。システムの `/usr/bin/qemu-system-arm`（8.2.2）を黙って使って2コアが動かない事態を避けるため、`/home/honda/qemu-build/install/bin/qemu-system-arm`（11.0.1、存在すれば）を既定にする。**polarfire 側の QEMU 実行方法（`QEMU_SYSTEM_RISCV64`、システムの 8.2.2 のまま）は変更しない。**
- 各タスクの検証には **positive control と negative control を対で入れる**。「検証したつもりで何も見ていなかった」欠陥が計画Aで10件出ている（`.superpowers/sdd/progress.md`）。特に注意する型：`grep -A2 FATAL` の空振り、パイプで `$?` が壊れる、grep パターン取り違え、POST_BUILD の実行有無をログ grep で判定、`-nostdlib` で判定対象が区別されない。
- QEMU 実行には必ずタイムアウトを付け、プロセスが残らないことを確認する（`sample1` は対話型で終了しない設計であり、`target_exit` が呼ばれない限り QEMU は自発的に終了しない）。

---

## File Structure

| ファイル | 責務 | Task |
|---|---|---|
| `CMakeLists.txt` | **修正のみ**。`-T` 適用ロジックの脱・ターゲット名決め打ち化、`fmp` の cfg 生成物への順序依存追加 | 1 |
| `target/polarfire_soc_kit_gcc/target.cmake` | **修正のみ**。`FMP3_LDSCRIPT_VIA_DRIVER_T` を宣言する側に変わる | 1 |
| `cmake/toolchain-arm-none-eabi.cmake` | ARM Cortex-M ベアメタル用ツールチェーン定義 | 2 |
| `target/musca_b1_gcc/presets.json` | `musca_b1` / `musca_b1-2core` の2プリセット | 2 |
| `CMakePresets.json` | musca_b1 の `presets.json` の include を追加 | 2 |
| `target/musca_b1_gcc/target.cmake` | ボード選択・カーネル/システムサービスソース・リンカスクリプト（Task 3）／`FMP3_RUN_COMMAND`・QEMU 版検査（Task 5） | 2,3,5 |
| `arch/arm_m_gcc/musca_b1/chip.cmake` | チップ依存（Cortex-M33 コンパイルオプション・TrustZone・スタートアップ） | 3 |
| `arch/arm_m_gcc/common/arch.cmake` | コア依存（`core_sym.def`・`core_offset.trb`・`core_kernel_impl.c`/`core_support.S`） | 3 |
| `DIVERGENCE_MAP.md` | pristine ディレクトリへの追加ファイルの記録 | 7 |

---

### Task 1: 汎用層の設計漏れを修正する（-T 決め打ちの解消／fmp の cfg 生成物への順序依存追加）

**Files:**
- Modify: `CMakeLists.txt:89-95`（`-T` 適用ロジック）、`CMakeLists.txt:555-561`（`fmp` の `add_executable` 直後）
- Modify: `target/polarfire_soc_kit_gcc/target.cmake`（`if(POLARFIRE_QEMU)` ブロック内）
- Modify: `DIVERGENCE_MAP.md`（`target/polarfire_soc_kit_gcc/{target.cmake,presets.json}` 行の説明を更新）

**Interfaces:**
- Consumes: なし（既存の `FMP3_LDSCRIPT` / `POLARFIRE_QEMU` / `generate_cfg_gen_files` / `add_executable(fmp ...)`）
- Produces:
  - `FMP3_LDSCRIPT_VIA_DRIVER_T`（bool、既定 OFF。target/chip が ON を宣言できる）
  - `fmp` ターゲットが `generate_cfg_gen_files` に順序依存する（`add_dependencies`）

このタスクは musca_b1 のファイルをまだ一切作らず、**polarfire の回帰確認だけで検証する**（musca_b1 が存在しない時点でも直せる／直すべき漏れであるため）。

- [ ] **Step 1: 現状の -T 適用ロジックを確認する（修正前の記録）**

Run: `sed -n '85,96p' CMakeLists.txt`
Expected（現状。修正対象であることの確認）:
```
if(DEFINED FMP3_LDSCRIPT)
    if(POLARFIRE_QEMU)
        list(APPEND FMP3_LINK_OPTIONS -T ${FMP3_LDSCRIPT})
    else()
        list(APPEND FMP3_LINK_OPTIONS -Wl,-T,${FMP3_LDSCRIPT})
    endif()
endif()
```
`POLARFIRE_QEMU` はターゲット固有の変数であり、汎用層（`CMakeLists.txt`）がこれで直接分岐しているのは設計上の誤り（brief記載の既知の設計漏れ）。

- [ ] **Step 2: `FMP3_LDSCRIPT_VIA_DRIVER_T` の既定を宣言し、`-T` 適用をそれで分岐させる**

`CMakeLists.txt:60`（`include(${FMP3_TARGET_DIR}/target.cmake)` の直前）に追加:
```cmake
#
#  リンカスクリプト指定の書式（-T か -Wl,-T, か）は，ターゲット固有の C
#  ライブラリ specs が「-Wl,-T,<file> だけでは自前のリンカスクリプトを
#  検出できず，specs 側の既定リンカスクリプトを追加注入してしまう」場合に
#  だけ ON にするトグルである．picolibc.specs の %{!T:-Tpicolibc.ld} が
#  これに該当する（gcc ドライバの -T スイッチが立っているかどうかだけを
#  見るため，-Wl 経由の -T では検出できない．詳細は下の -T 適用箇所の
#  コメントと target/polarfire_soc_kit_gcc/target.cmake 参照）．
#
#  ほとんどのツールチェーン（arm-none-eabi の newlib 既定 specs を含む）は
#  この種の自動注入を持たないため，既定は OFF（-Wl,-T, を使う）．
#  «未定義のときだけ» 既定を与える（cmake/toolchain-riscv64.cmake の
#  FMP3_EXPECTED_TOOLCHAIN_MACHINE と同じ理由．素の set() だと
#  target.cmake 側の意図的な ON 宣言があってもここで上書きしてしまう
#  ……というのは誤りで，実際には逆に「target.cmake がこの後 include
#  されて ON を set() する」ので問題は起きないが，将来 -D 経由で
#  上書きしたい場合に備えて同じ書き方に揃えておく）．
#
if(NOT DEFINED FMP3_LDSCRIPT_VIA_DRIVER_T)
    set(FMP3_LDSCRIPT_VIA_DRIVER_T OFF)
endif()

include(${FMP3_TARGET_DIR}/target.cmake)
```

`CMakeLists.txt:89-95` を次のように置き換える:
```cmake
#
#  リンカスクリプトの適用（-T）はここ1箇所に集約する．
#
#  target.cmake は FMP3_LDSCRIPT を「設定するだけ」で -T 自体は積まない
#  （target.cmake 側のコメント参照）．-T をここでしか足さないことで，
#  将来 fmp 実行ファイルを組む Task が asp3_core 由来の
#    list(APPEND FMP3_LINK_OPTIONS -Wl,-T,${FMP3_LDSCRIPT})
#  をそのまま流用しても二重指定（ld の "linker script file '...' appears
#  multiple times" fatal error）にならない．
#
#  ★書式（-T か -Wl,-T, か）はターゲットが宣言する FMP3_LDSCRIPT_VIA_DRIVER_T
#    で決める（既定 OFF＝-Wl,-T,）．汎用層（本ファイル）がターゲット固有の
#    変数名（旧: POLARFIRE_QEMU）で直接分岐していたのは設計上の誤りだった：
#    「picolibc の specs が gcc の -T ドライバスイッチを要求する」という
#    知識は picolibc を使うターゲット（polarfire の QEMU/picolibc ビルド）
#    が持つべきものであり，汎用層はターゲットが宣言した結果だけを読む．
#    実測（riscv64-unknown-elf-gcc 13.2.0 + picolibc 1.8.6-2，mpfs-lim.ld）：
#      -T mpfs-lim.ld          → _start = 0x08000000（l2lim．意図通り）
#      -Wl,-T,mpfs-lim.ld のみ → _start = 0x80000000（picolibc.ld 既定）
#    ld はエラーにせず両方の SECTIONS を受理し，後着の picolibc.ld が
#    勝つ．**ビルドは通るのに実機では起動しない**という，fatal error
#    より発見しにくい壊れ方をする．musca_b1（newlib，picolibc 不使用）は
#    FMP3_LDSCRIPT_VIA_DRIVER_T を一切設定しないため既定の OFF のままとなり
#    -Wl,-T, が使われる（これが正しい）．
#
if(DEFINED FMP3_LDSCRIPT)
    if(FMP3_LDSCRIPT_VIA_DRIVER_T)
        list(APPEND FMP3_LINK_OPTIONS -T ${FMP3_LDSCRIPT})
    else()
        list(APPEND FMP3_LINK_OPTIONS -Wl,-T,${FMP3_LDSCRIPT})
    endif()
endif()
```

- [ ] **Step 3: polarfire の target.cmake に ON 宣言を追加する**

`target/polarfire_soc_kit_gcc/target.cmake` の `if(POLARFIRE_QEMU)` ブロック内（`FMP3_CFG1_OUT_LINK_OPTIONS` に `-Wl,--no-gc-sections` を足している行の直後）に追加:
```cmake
    #
    #  リンカスクリプト指定を「素の -T」にする．picolibc.specs の
    #  %{!T:-Tpicolibc.ld} は gcc ドライバの -T スイッチの有無だけを見て
    #  picolibc 既定のリンカスクリプトを追加注入するため，-Wl,-T,<file>
    #  では防げない（実測。CMakeLists.txt の -T 適用箇所のコメント参照）．
    #  適用そのものは CMakeLists.txt の1箇所に集約されているため，ここでは
    #  ON を宣言するだけに留める．
    #
    set(FMP3_LDSCRIPT_VIA_DRIVER_T ON)
```

- [ ] **Step 4: polarfire の regression（-T 書式が変わっていないこと）を確認する**

Run:
```bash
cmake --build --preset polarfire_soc_kit-qemu --target fmp 2>&1 | tail -5
riscv64-unknown-elf-readelf -h build/polarfire_soc_kit-qemu/fmp | grep Entry
```
Expected: ビルド成功、`Entry point address:  0x8000000`（`_start = 0x08000000`。計画A Task 7 で確認済みの値と一致）。

**negative control**（Step 3 の宣言を外すと元の「たまたま `-T` になっていた」壊れ方が再現することの実演。既に計画A Task2 で実測済みの現象を，今回のリファクタ後でも壊せることを示す）:
```bash
cd /home/honda/TOPPERS/FMP3/fmp3_core
sed -i 's/set(FMP3_LDSCRIPT_VIA_DRIVER_T ON)/set(FMP3_LDSCRIPT_VIA_DRIVER_T OFF)/' target/polarfire_soc_kit_gcc/target.cmake
rm -rf build/polarfire_soc_kit-qemu
cmake --preset polarfire_soc_kit-qemu >/dev/null
cmake --build build/polarfire_soc_kit-qemu --target fmp 2>&1 | tail -5
riscv64-unknown-elf-readelf -h build/polarfire_soc_kit-qemu/fmp | grep Entry
```
Expected: ビルドは成功するが `Entry point address:  0x80000000`（picolibc.ld 既定に化ける。壊れているのにリンクは通る＝「発見しにくい壊れ方」の再現）。確認後、直ちに元に戻す:
```bash
sed -i 's/set(FMP3_LDSCRIPT_VIA_DRIVER_T OFF)/set(FMP3_LDSCRIPT_VIA_DRIVER_T ON)/' target/polarfire_soc_kit_gcc/target.cmake
rm -rf build/polarfire_soc_kit-qemu
cmake --preset polarfire_soc_kit-qemu >/dev/null
cmake --build build/polarfire_soc_kit-qemu --target fmp 2>&1 | tail -5
riscv64-unknown-elf-readelf -h build/polarfire_soc_kit-qemu/fmp | grep Entry
```
Expected: `Entry point address:  0x8000000`（正しい値に戻る）。`git diff target/polarfire_soc_kit_gcc/target.cmake` が空であることを確認する（negative control が作業ツリーに跡を残していないこと）。

- [ ] **Step 5: `fmp` の cfg 生成物への順序依存を追加する**

`CMakeLists.txt:555-561`（`add_executable(fmp ...)` の直後、`fmp3_add_syssvc(fmp)` の前）に追加:
```cmake
add_executable(fmp
    ${FMP3_START_FILES}
    ${FMP3_APPLDIR}/${FMP3_APPLNAME}.c
    ${FMP3_SDK_C_FILES}
    ${FMP3_SDK_ASM_FILES}
)

#
#  fmp 自身が直接コンパイルする C/ASM ソース（sample1.c・非TECS版システム
#  サービス・SDK ソース等）を，offset.h / kernel_cfg.h の生成完了より前に
#  コンパイルしてしまう競合を防ぐ．
#
#  libfmp3.a 側（add_library(fmp3 ...)）は既に
#  add_dependencies(fmp3 generate_cfg_gen_files) を持つが，fmp 自身が直接
#  コンパイルするソース（sample1.c 等，fmp3 ライブラリの外）には同じ順序
#  依存が無かった．target_link_libraries(fmp PRIVATE fmp3 ...) はリンク時
#  の依存しか作らず，fmp 自身のコンパイルステップの順序は保証しない．
#
#  この漏れは，RISC-V（polarfire）では offset.h をアセンブラ（start.S）
#  からしか取り込まないため気付かれなかった（arch/riscv_gcc の start.S は
#  offset.h を含まない）．一方 ARM-M では core_kernel_impl.h が
#  （CFG1_OUT 以外で）offset.h を取り込み（arch/arm_m_gcc/common/
#  Makefile.core:51-64、arch/arm_m_gcc/doc/arm_m_design.md:287-319），
#  これが kernel_impl.h → target_kernel_impl.h → chip_kernel_impl.h →
#  core_kernel_impl.h の連鎖でほぼ全 C オブジェクトに波及するため，並列
#  ビルドでレース不具合（offset.h 不在によるコンパイル失敗）になりうる．
#  また kernel_cfg.h（CRE_TSK 等が払い出すオブジェクトIDの #define）は
#  arch を問わず sample1.c 自身が必要とする．
#
#  cfg1_out には付けない：cfg1_out.c は自身の中で TOPPERS_CFG1_OUT を
#  #define しており（cfg/pass1.rb:785），offset.h の生成自体が cfg1_out の
#  実行結果（.syms/.srec）を必要とするため，ここで依存させると循環になる．
#
add_dependencies(fmp generate_cfg_gen_files)

fmp3_add_syssvc(fmp)
```

- [ ] **Step 6: polarfire の regression（ビルド成功・出力不変）を確認する**

Run:
```bash
rm -rf build/polarfire_soc_kit-qemu
cmake --preset polarfire_soc_kit-qemu
cmake --build build/polarfire_soc_kit-qemu 2>&1 | tail -15
```
Expected: ビルド成功（`add_dependencies` は順序を追加するだけで機能を変えないため，成果物は計画A Task 7/8 で確認済みのものと同一のはず）。

**positive control**（実際に順序が効いていることの確認。offset.h/kernel_cfg.h を一旦消し，`fmp` の再ビルドだけで正しく再生成されることを見る）:
```bash
rm -f build/polarfire_soc_kit-qemu/generated/offset.h build/polarfire_soc_kit-qemu/generated/kernel_cfg.h
cmake --build build/polarfire_soc_kit-qemu --target fmp 2>&1 | grep -c "Running cfg pass 2"
```
Expected: `2`（offset 生成・kernel_cfg 生成の両方が `fmp` のビルド要求だけで走ることの確認。`add_dependencies(fmp generate_cfg_gen_files)` が無ければ，`fmp` 自身の compile ステップは待たずに走り出す可能性があるが，`fmp` は `fmp3` にリンク依存するため `fmp3` 経由でいずれ生成される；この positive control は「順序が保証されている」ことよりも「生成自体が壊れていない」ことの確認に留まる点に注意。真の効果検証は Task 4 で musca_b1 の並列ビルドを使って行う）。

- [ ] **Step 7: `DIVERGENCE_MAP.md` の該当行を更新する**

`target/polarfire_soc_kit_gcc/{target.cmake,presets.json} | add` の行の「内容・理由」を次のように更新する:
```
Makefile.target の CMake 版。Microchip SDK のソース16個を最終リンクに加える。FMP3_LDSCRIPT_VIA_DRIVER_T=ON を宣言する（picolibc.specs の %{!T:-Tpicolibc.ld} が -Wl,-T, では防げないため。汎用層 CMakeLists.txt 側のトグルを読む形に改めた＝計画A2 Task 1）
```

- [ ] **Step 8: コミット**

```bash
git add CMakeLists.txt target/polarfire_soc_kit_gcc/target.cmake DIVERGENCE_MAP.md
git commit -m "fix: 汎用層の設計漏れ2件を修正（-T書式の脱決め打ち化／fmpのcfg生成物への順序依存追加）

polarfire固有の想定（POLARFIRE_QEMU）で汎用層が直接分岐していたのを，
ターゲットが宣言するFMP3_LDSCRIPT_VIA_DRIVER_Tに置き換えた。
また，fmp自身がコンパイルするソースがoffset.h/kernel_cfg.h生成を
待たない順序依存漏れを修正した（ARM-Mではoffset.hがCオブジェクトにも
波及するため必須。RISC-Vでは偶然表面化していなかった）。
polarfireの回帰確認のみで検証（musca_b1のファイルはまだ無い）。"
```

---

### Task 2: ツールチェーンと presets の骨格（musca_b1、1コア既定）

**Files:**
- Create: `cmake/toolchain-arm-none-eabi.cmake`
- Create: `target/musca_b1_gcc/presets.json`
- Create: `target/musca_b1_gcc/target.cmake`（この Task では最小の骨組みのみ。Task 3 で本体を書く）
- Modify: `CMakePresets.json`

**Interfaces:**
- Consumes: `cmake/toolchain_check.cmake`（計画A Task 1 で実装済み。`FMP3_EXPECTED_TOOLCHAIN_MACHINE` の宣言を読む）
- Produces: `musca_b1` プリセットで `cmake --preset musca_b1` が `Configuring done` まで到達すること

- [ ] **Step 1: `-dumpmachine` の出力形式を実測する**

Run:
```bash
arm-none-eabi-gcc -dumpmachine
```
Expected: `arm-none-eabi`

- [ ] **Step 2: ツールチェーンファイルを書く**

`cmake/toolchain-arm-none-eabi.cmake`:
```cmake
#
#		ツールチェーンファイル（ARM Cortex-M ベアメタル：arm-none-eabi）
#
#  既定は arm-none-eabi．別のプレフィックスを使う場合は
#  -DARM_NONE_EABI_TOOLCHAIN_PREFIX=... で上書きできる．
#
#  実行ファイルに拡張子を付けない（cmake/toolchain-riscv64.cmake と同じ
#  理由．上流 sample/Makefile の OBJNAME = fmp と同じ名前にして，Makefile
#  ビルドとの突き合わせを容易にするため）．参考実装 asp3_core の
#  cmake/toolchain-arm-none-eabi.cmake は CMAKE_EXECUTABLE_SUFFIX を .elf に
#  しているが，fmp3_core では riscv64 と作法を揃えて拡張子を付けない．
#
set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR arm)

#
#  cmake/toolchain_check.cmake が照合する「このツールチェーンファイルが
#  期待する -dumpmachine パターン」．arm-none-eabi-gcc -dumpmachine は
#  そのまま "arm-none-eabi" を返す（riscv64-unknown-elf-gcc の
#  "riscv64-unknown-elf" と同型．実測済み）．
#
#  «未定義のときだけ» 既定を与える（cmake/toolchain-riscv64.cmake と同じ
#  理由．素の set() だとコマンドラインの -D を黙って上書きしてしまい，
#  -DFMP3_EXPECTED_TOOLCHAIN_MACHINE=<pattern> という上書き手段の案内が
#  「効かない案内＝嘘」になる）．
#
if(NOT DEFINED FMP3_EXPECTED_TOOLCHAIN_MACHINE)
    set(FMP3_EXPECTED_TOOLCHAIN_MACHINE arm-none-eabi)
endif()

if(NOT DEFINED ARM_NONE_EABI_TOOLCHAIN_PREFIX)
    set(ARM_NONE_EABI_TOOLCHAIN_PREFIX arm-none-eabi-)
endif()

set(CMAKE_C_COMPILER   ${ARM_NONE_EABI_TOOLCHAIN_PREFIX}gcc)
set(CMAKE_ASM_COMPILER ${ARM_NONE_EABI_TOOLCHAIN_PREFIX}gcc)
set(CMAKE_OBJCOPY      ${ARM_NONE_EABI_TOOLCHAIN_PREFIX}objcopy CACHE FILEPATH "objcopy")
set(CMAKE_NM           ${ARM_NONE_EABI_TOOLCHAIN_PREFIX}nm      CACHE FILEPATH "nm")
set(CMAKE_OBJDUMP      ${ARM_NONE_EABI_TOOLCHAIN_PREFIX}objdump CACHE FILEPATH "objdump")

#  ベアメタルではリンクできる完全な実行ファイルを作れないため，
#  try_compile はスタティックライブラリで行う
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
```

- [ ] **Step 3: 最小の target.cmake を置く（Task 3 で中身を入れる）**

`target/musca_b1_gcc/target.cmake`:
```cmake
#
#		ターゲット依存部の CMake 定義（ARM Musca-B1 用）
#
#  上流 target/musca_b1_gcc/Makefile.target の CMake 版．Task 3 で中身を
#  入れる．
#
set(TARGETDIR ${FMP3_TARGET_DIR})

list(APPEND FMP3_INCLUDE_DIRS ${TARGETDIR})
```

- [ ] **Step 4: プリセットを書く**

`target/musca_b1_gcc/presets.json`:
```json
{
  "version": 4,
  "include": [
    "../../cmake/presets-base.json"
  ],
  "configurePresets": [
    {
      "name": "musca_b1",
      "inherits": "_base",
      "displayName": "ARM Musca-B1 (dual Cortex-M33, QEMU, 1 processor)",
      "description": "QEMU musca-b1 マシン向け（実機非対応）。既定は単一プロセッサ（TNUM_PRCID=1）",
      "toolchainFile": "${sourceDir}/cmake/toolchain-arm-none-eabi.cmake",
      "cacheVariables": {
        "FMP3_TARGET": "musca_b1_gcc"
      }
    },
    {
      "name": "musca_b1-2core",
      "inherits": "musca_b1",
      "displayName": "ARM Musca-B1 (dual Cortex-M33, QEMU, 2 processors SMP)",
      "description": "2コアSMP（TNUM_PRCID=2）。クロスコアIPI（MHU）にQEMU>=11.0.1相当が必要",
      "cacheVariables": {
        "FMP3_PRC_NUM": "2"
      }
    }
  ],
  "buildPresets": [
    { "name": "musca_b1",       "configurePreset": "musca_b1" },
    { "name": "musca_b1-2core", "configurePreset": "musca_b1-2core" },
    {
      "name": "run-musca_b1",
      "configurePreset": "musca_b1",
      "targets": [ "run" ]
    },
    {
      "name": "run-musca_b1-2core",
      "configurePreset": "musca_b1-2core",
      "targets": [ "run" ]
    }
  ]
}
```

- [ ] **Step 5: ルートの presets に musca_b1 を追加する**

`CMakePresets.json` を次のように書き換える:
```json
{
  "version": 4,
  "cmakeMinimumRequired": {
    "major": 3,
    "minor": 23,
    "patch": 0
  },
  "include": [
    "target/polarfire_soc_kit_gcc/presets.json",
    "target/musca_b1_gcc/presets.json"
  ]
}
```

- [ ] **Step 6: 設定が通ることを確認する（positive control）**

Run: `cmake --preset musca_b1 2>&1 | grep -E 'fmp3_core:|Configuring done'`
Expected:
```
-- fmp3_core: FMP3_ROOT_DIR   = /home/honda/TOPPERS/FMP3/fmp3_core
-- fmp3_core: FMP3_TARGET     = musca_b1_gcc
-- fmp3_core: FMP3_TARGET_DIR = /home/honda/TOPPERS/FMP3/fmp3_core/target/musca_b1_gcc
-- Configuring done
```

- [ ] **Step 7: ツールチェーン未指定で正しく落ちることを確認する（negative control）**

Run:
```bash
cmake -G Ninja -B /tmp/fmp3-musca-hostgcc -S . -DFMP3_TARGET=musca_b1_gcc
echo "exit=$?"
```
Expected: `CMake Error at cmake/toolchain_check.cmake (message):` に続けて `No CMAKE_TOOLCHAIN_FILE was given, so CMAKE_C_COMPILER='/usr/bin/cc' is presumed to be the HOST compiler ...` を含む FATAL_ERROR、`exit=1`。

- [ ] **Step 8: 期待値の食い違いを検出できることを確認する（negative → override → positive）**

Run:
```bash
cmake -G Ninja -B /tmp/fmp3-musca-mismatch -S . -DFMP3_TARGET=musca_b1_gcc \
    -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-arm-none-eabi.cmake \
    -DFMP3_EXPECTED_TOOLCHAIN_MACHINE=riscv64
echo "exit=$?"

cmake -G Ninja -B /tmp/fmp3-musca-override-ok -S . -DFMP3_TARGET=musca_b1_gcc \
    -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-arm-none-eabi.cmake \
    -DFMP3_EXPECTED_TOOLCHAIN_MACHINE=arm-none-eabi
echo "exit=$?"
```
Expected: 前者は `does not match the expected pattern 'riscv64'` を含む FATAL_ERROR、`exit=1`。後者は `Configuring done`、`exit=0`（`-D` による上書きが実際に効くことの確認。既定と同じ値を明示的に与えているだけだが，上書き経路自体が機能することの確認として十分）。

- [ ] **Step 9: polarfire の regression を確認する**

Run: `cmake --preset polarfire_soc_kit-qemu 2>&1 | grep -E 'fmp3_core:|Configuring done'`
Expected: Task 1 の Step 4 と同じ出力（musca_b1 の追加が polarfire の configure に影響しないことの確認）。

- [ ] **Step 10: 後片付けとコミット**

```bash
rm -rf /tmp/fmp3-musca-hostgcc /tmp/fmp3-musca-mismatch /tmp/fmp3-musca-override-ok
git add cmake/toolchain-arm-none-eabi.cmake target/musca_b1_gcc/presets.json \
        target/musca_b1_gcc/target.cmake CMakePresets.json
git commit -m "build: musca_b1 のツールチェーン・presets骨格を追加

arm-none-eabi 用ツールチェーンファイルと musca_b1 / musca_b1-2core の
2プリセット。configure が Configuring done まで到達するところまで
（target.cmake はまだ最小骨組み）。ツールチェーン同定検査が
arm-none-eabi でも機能することを正負制御で確認した。"
```

---

### Task 3: arch / chip / target の変数積み上げ（1コア）

**Files:**
- Create: `arch/arm_m_gcc/common/arch.cmake`
- Create: `arch/arm_m_gcc/musca_b1/chip.cmake`
- Modify: `target/musca_b1_gcc/target.cmake`（Task 2 の骨組みを本体で置き換える。`FMP3_RUN_COMMAND` はまだ入れない＝Task 5 で追加）

**Interfaces:**
- Consumes: `FMP3_ROOT_DIR` / `FMP3_TARGET_DIR`（`fmp3_core.cmake`）
- Produces（すべてリスト変数。`CMakeLists.txt` が読む。計画A Task 2 と同じ変数名）:
  - `FMP3_SYMVAL_TABLES` / `FMP3_OFFSET_TRB_FILES` / `FMP3_KERNEL_CFG_TRB_FILES` / `FMP3_CHECK_TRB_FILES` / `FMP3_CLASS_TRB_FILES` / `FMP3_CFG_FILES`
  - `FMP3_INCLUDE_DIRS` / `FMP3_COMPILE_DEFS` / `FMP3_COMPILE_OPTIONS`
  - `FMP3_LINK_OPTIONS` / `FMP3_LINK_LIBS` / `FMP3_LDSCRIPT`
  - `FMP3_ARCH_C_FILES` / `FMP3_TARGET_C_FILES` / `FMP3_SYSSVC_TARGET_C_FILES` / `FMP3_START_FILES`
  - （`FMP3_CFG1_OUT_LINK_OPTIONS` は**意図的に設定しない**。Step 5 参照）
  - （`FMP3_LDSCRIPT_VIA_DRIVER_T` も設定しない。既定 OFF のままでよい＝`-Wl,-T,` を使う）

- [ ] **Step 1: 変数が空であることを確認する（失敗の確認）**

Run: `cmake --preset musca_b1 2>&1 | grep -c 'FMP3_ARCH_C_FILES'`
Expected: `0`

- [ ] **Step 2: コア依存部を書く**

`arch/arm_m_gcc/common/arch.cmake`（上流 `arch/arm_m_gcc/common/Makefile.core` の CMake 版）:
```cmake
#
#		アーキテクチャ依存部の CMake 定義（ARM-M コア共通）
#
#  chip.cmake から include される（上流 Makefile.core に相当）．
#
#  本ファイルは常に fmp3_core（このリポジトリ）側にある共通コア層なので，
#  ${FMP3_ROOT_DIR} 基準で自己解決する（arch/riscv_gcc/common/arch.cmake と
#  同じ作法。chip 依存部・target 依存部がリポジトリ外に置かれる場合でも，
#  この層だけは動かない）．
#
set(COREDIR ${FMP3_ROOT_DIR}/arch/arm_m_gcc/common)
set(TOOLDIR ${FMP3_ROOT_DIR}/arch/gcc)

#  Makefile.core:25
list(APPEND FMP3_INCLUDE_DIRS
    ${COREDIR}
    ${TOOLDIR}
)

#  Makefile.core:43  CFG_TABS
list(APPEND FMP3_SYMVAL_TABLES
    ${COREDIR}/core_sym.def
)

#  Makefile.core:48  TARGET_OFFSET_TRB
list(APPEND FMP3_OFFSET_TRB_FILES
    ${COREDIR}/core_offset.trb
)

#  Makefile.core:37-38  KERNEL_ASMOBJS core_support.o / KERNEL_COBJS
#  core_kernel_impl.o（どちらも libfmp3.a に入る．riscv_gcc/common/arch.cmake
#  と同じ扱いで FMP3_ARCH_C_FILES に .c と .S を両方積む）
list(APPEND FMP3_ARCH_C_FILES
    ${COREDIR}/core_kernel_impl.c
    ${COREDIR}/core_support.S
)

#  Makefile.core:26（-lgcc）と sample/Makefile:63（SRCLANG=c のとき -lc）
list(APPEND FMP3_LINK_LIBS c gcc)

#
#  ★offset.h への C ソース依存関係についての注記（Makefile.core:51-64）
#
#  ARM-M コア依存部では core_kernel_impl.h が（CFG1_OUT 以外で）offset.h を
#  取り込み，kernel_impl.h → target_kernel_impl.h → chip_kernel_impl.h →
#  core_kernel_impl.h の連鎖でほぼ全 C オブジェクトに波及する（他アーキ
#  では offset.h をアセンブラからのみ取り込む）．上流 Make ビルドは
#  OFFSET_COBJS を使ってこれに順序専用依存を付けているが，CMake 側では
#  add_dependencies(fmp3 generate_cfg_gen_files)（libfmp3.a 分。既存）と
#  add_dependencies(fmp generate_cfg_gen_files)（fmp 自身が直接コンパイル
#  するソース分。計画A2 Task 1 で追加済み）でカバーする．本ファイルでの
#  対応は不要（汎用層で完結する）．
#
```

- [ ] **Step 3: チップ依存部を書く**

`arch/arm_m_gcc/musca_b1/chip.cmake`（上流 `arch/arm_m_gcc/musca_b1/Makefile.chip` の CMake 版）:
```cmake
#
#		チップ依存部の CMake 定義（ARM Musca-B1 / SSE-200 用）
#
#  target.cmake から include される（上流 Makefile.chip に相当）．
#
#  外部（SDK）ターゲットのパス解決規約（fmp3_esp_idf / fmp3_pico_sdk 等の
#  統合リポジトリから fmp3_core が submodule として使われる場合に備える。
#  arch/riscv_gcc/polarfire_soc/chip.cmake と同じ作法）：
#   - 共通 arch（arch/arm_m_gcc/common）は fmp3_core 側＝ARCHDIR
#   - チップ依存部（musca_b1）・target 依存部は，これを include する
#     target.cmake 側で解決される＝CHIPDIR／TARGETDIR
#  ARCHDIR／CHIPDIR／TARGETDIR は呼び出し元の target.cmake が設定済み
#  （本ファイルではハードコードしない）．
#
set(COREDIR ${ARCHDIR}/common)

#
#  コンパイルオプション（Makefile.chip:16-18）
#
#  Cortex-M33（ARMv8-M Mainline），Thumb 命令のみ．Musca-B1 の M33 は
#  FPU/DSP を搭載するが，本依存部はソフトウェア浮動小数点 ABI（soft-float）
#  でビルドする．
#
list(APPEND FMP3_COMPILE_OPTIONS
    -mcpu=cortex-m33
    -mthumb
    -mfloat-abi=soft
)
list(APPEND FMP3_LINK_OPTIONS
    -mcpu=cortex-m33
    -mthumb
    -mfloat-abi=soft
)
list(APPEND FMP3_COMPILE_DEFS TOPPERS_CORTEX_M33)

#
#  ARM アーキテクチャの世代（ARMv8-M Mainline，Makefile.chip:20-26）
#
#  __TARGET_ARCH_THUMB は旧 ARM 純正コンパイラの組込みマクロであり，
#  GNU 開発環境では定義されないため，明示的に定義する（5 = ARMv8-M）．
#
list(APPEND FMP3_COMPILE_DEFS __TARGET_ARCH_THUMB=5)

#
#  TrustZone 対応コアでのセキュア単独動作（Makefile.chip:28-38）
#
#  QEMU musca-b1（Cortex-M33）はリセット直後セキュア状態で動作する．
#  TrustZone 搭載コアでは，例外リターン値 EXC_RETURN にセキュア状態を
#  表すビットが必要なため，TOPPERS_ENABLE_TRUSTZONE を定義してセキュア用
#  の EXC_RETURN（0xfffffffd）を選択する．これが無いと，セキュア状態から
#  の例外リターン整合性チェックに失敗する．
#
list(APPEND FMP3_COMPILE_DEFS TOPPERS_ENABLE_TRUSTZONE)

list(APPEND FMP3_INCLUDE_DIRS ${CHIPDIR})

#
#  カーネルに含めるチップ依存ソース（Makefile.chip:45-46）
#
list(APPEND FMP3_ARCH_C_FILES
    ${CHIPDIR}/chip_kernel_impl.c
)

#
#  スタートアップモジュール（Makefile.chip:54-64）
#
#  START_OBJS を start.o（COREDIR/start.S）に設定し，LDFLAGS に -nostdlib
#  を追加する．
#
list(APPEND FMP3_START_FILES
    ${COREDIR}/start.S
)
list(APPEND FMP3_LINK_OPTIONS -nostdlib)

#
#  コア依存部（Makefile.chip:69）
#
include(${COREDIR}/arch.cmake)
```

- [ ] **Step 4: ターゲット依存部を書く（1コア。QEMU 実行は Task 5 で追加）**

`target/musca_b1_gcc/target.cmake`（Task 2 の骨組みを丸ごと置き換える。上流 `Makefile.target` の CMake 版）:
```cmake
#
#		ターゲット依存部の CMake 定義（ARM Musca-B1 / QEMU 用）
#
#  上流 target/musca_b1_gcc/Makefile.target の CMake 版．
#
#  外部（SDK）ターゲットのパス解決規約は polarfire_soc_kit_gcc/target.cmake
#  と同じ（ARCHDIR は ${FMP3_ROOT_DIR} 基準，CHIPDIR/TARGETDIR は
#  CMAKE_CURRENT_LIST_DIR 相対）．
#
set(ARCHDIR ${FMP3_ROOT_DIR}/arch/arm_m_gcc)
get_filename_component(CHIPDIR ${CMAKE_CURRENT_LIST_DIR}/../../arch/arm_m_gcc/musca_b1 ABSOLUTE)
set(TARGETDIR ${CMAKE_CURRENT_LIST_DIR})

#
#  QEMU 専用ターゲット（実機 Musca-B1 ボードでの動作確認は行っていない．
#  target_user.txt:42,149-150）．polarfire の POLARFIRE_QEMU のような
#  実機/QEMU 切替オプションは作らない．常に TOPPERS_USE_QEMU を定義する
#  （Makefile.target:19 の CDEFS := $(CDEFS) -DTOPPERS_USE_QEMU に相当。
#  target_kernel_impl.c の target_exit がこのマクロでセミホスティング
#  終了に分岐する）．
#
list(APPEND FMP3_COMPILE_DEFS TOPPERS_USE_QEMU)

#  Makefile.target:24-26
list(APPEND FMP3_INCLUDE_DIRS ${TARGETDIR})
list(APPEND FMP3_COMPILE_OPTIONS -mlittle-endian)
list(APPEND FMP3_LINK_OPTIONS -mlittle-endian)

#  非TECS版システムサービスを使う（polarfire と同じ理由．syssvc/syslog.h
#  等が参照する）
list(APPEND FMP3_COMPILE_DEFS TOPPERS_OMIT_TECS)

#
#  カーネルに含めるターゲット依存ソース（Makefile.target:31-32）
#
list(APPEND FMP3_TARGET_C_FILES
    ${TARGETDIR}/target_kernel_impl.c
    ${TARGETDIR}/target_timer.c
    ${TARGETDIR}/target_ipi.c
)

#
#  非TECS版 SIO ドライバ（Makefile.target:37-38）
#
#  polarfire と異なり，chip.cmake ではなく target.cmake がここを設定する
#  （上流 Makefile.chip に SYSSVC_COBJS の記述が無く，Makefile.target が
#  直接設定しているため。pl011_uart.c は target/musca_b1_gcc/ 直下にあり，
#  chip 層の持ち物ではない）．
#
list(APPEND FMP3_SYSSVC_TARGET_C_FILES
    ${TARGETDIR}/target_serial.c
    ${TARGETDIR}/pl011_uart.c
)

#
#  リンカスクリプトの定義（Makefile.target:43）
#
set(FMP3_LDSCRIPT ${TARGETDIR}/musca_b1.ld)

#
#  cfg に渡すファイル
#
list(APPEND FMP3_CFG_FILES            ${TARGETDIR}/target_kernel.cfg)
list(APPEND FMP3_CLASS_TRB_FILES      ${TARGETDIR}/target_class.trb)
list(APPEND FMP3_KERNEL_CFG_TRB_FILES ${TARGETDIR}/target_kernel.trb)
list(APPEND FMP3_CHECK_TRB_FILES      ${TARGETDIR}/target_check.trb)

#
#  チップ依存部（Makefile.target:48）
#
include(${CHIPDIR}/chip.cmake)

#
#  ★-T（リンカスクリプト指定）はここでは積まない．FMP3_LDSCRIPT の値
#    （cfg1_out / fmp の LINK_DEPENDS 追跡用）を確定させるだけに留める．
#    適用は CMakeLists.txt の1箇所に集約されている（FMP3_LDSCRIPT_VIA_DRIVER_T
#    で分岐。本ターゲットはこの変数を一切設定しないため既定の OFF のまま
#    ＝-Wl,-T, が使われる。picolibc.specs のような自動リンカスクリプト
#    注入は arm-none-eabi 既定の newlib specs には無いため，polarfire の
#    QEMU/picolibc 専用の問題は起きない。詳細は CMakeLists.txt の該当
#    コメントと計画A2 Task 1 参照）．
#
#  ★--gc-sections は使わない：上流 Makefile.chip / Makefile.core /
#    Makefile.target / sample/Makefile のいずれにも --gc-sections /
#    -ffunction-sections / -fdata-sections が無いことを実測で確認済み
#    （polarfire は l2lim 256KiB の制約から --gc-sections を必要としたが，
#    musca_b1 の FLASH 8MB/RAM 512KB にはその制約が無い）．したがって
#    polarfire 固有の cfg1_out --no-gc-sections ワークアラウンド
#    （FMP3_CFG1_OUT_LINK_OPTIONS）も不要（Task 4 で cfg1_out.syms に
#    TOPPERS_magic_number が --no-gc-sections 無しでも残ることを確認する）．
#
```

- [ ] **Step 5: 変数の積み上げを確認する**

`CMakeLists.txt` の変数ダンプ（`FMP3_ARCH_C_FILES` 等を列挙する `message(STATUS ...)` ブロック）は計画A Task 2 で追加済みでターゲット非依存なので，そのまま musca_b1 でも出力される。

Run: `cmake --preset musca_b1 2>&1 | grep -A20 'FMP3_ARCH_C_FILES count'`
Expected（抜粋。パスは絶対パスで出力される）:
```
-- fmp3_core: FMP3_ARCH_C_FILES count =
--     arch: .../arch/arm_m_gcc/musca_b1/chip_kernel_impl.c
--     arch: .../arch/arm_m_gcc/common/core_kernel_impl.c
--     arch: .../arch/arm_m_gcc/common/core_support.S
```
（`FMP3_TARGET_C_FILES`・`FMP3_LDSCRIPT` 等は個別に `grep 'fmp3_core: FMP3_'` で確認する）

- [ ] **Step 6: polarfire の regression を確認する**

Run: `cmake --preset polarfire_soc_kit-qemu 2>&1 | grep -E 'fmp3_core: FMP3_LDSCRIPT|Configuring done'`
Expected: Task 1 の Step 4 と同じ `FMP3_LDSCRIPT` の値（変更なし）、`Configuring done`。

- [ ] **Step 7: コミット**

```bash
git add arch/arm_m_gcc/common/arch.cmake arch/arm_m_gcc/musca_b1/chip.cmake \
        target/musca_b1_gcc/target.cmake
git commit -m "build: musca_b1 の arch/chip/target 層を追加（1コア）

arch/arm_m_gcc/common/arch.cmake（コア共通）・
arch/arm_m_gcc/musca_b1/chip.cmake（Cortex-M33/TrustZone）・
target/musca_b1_gcc/target.cmake（PL011 UART・SSE-200 IPI・
musca_b1.ld）。--gc-sections は上流に無いため使わない
（polarfire の l2lim 制約はこのターゲットには無い）。
QEMU実行（FMP3_RUN_COMMAND）はTask 5で追加する。"
```

---

### Task 4: cfg パイプライン〜libfmp3.a〜fmp 実行ファイル（1コア）をビルドする

**Files:** なし（既存の `CMakeLists.txt` パイプラインをそのまま流用する。これが本計画の主目的＝汎用層に手を入れずに1ターゲット追加できるかの実地テスト）

**Interfaces:**
- Consumes: Task 1〜3 で揃った `FMP3_*` 変数群、既存の cfg 3パスパイプライン（`cfg1_out` → `offset.h` → `kernel_cfg.c/h` → `libfmp3.a` → `fmp`）
- Produces: `build/musca_b1/fmp`（未実行時点での成果物）

- [ ] **Step 1: ビルドする**

Run:
```bash
rm -rf build/musca_b1
cmake --preset musca_b1
cmake --build build/musca_b1 2>&1 | tee /tmp/musca_b1_build.log | tail -30
echo "rc=${PIPESTATUS[1]}"
```
Expected: `rc=0`。ビルドログに `Running cfg pass 1` `Running cfg pass 2`（offset・kernel_cfg の2回）が現れ，最終的に `build/musca_b1/fmp` が生成される。

- [ ] **Step 2: cfg1_out に TOPPERS_magic_number が残っていることを確認する（--gc-sections 不要の実証）**

Run:
```bash
grep TOPPERS_magic_number build/musca_b1/generated/cfg1_out.syms
echo "rc=$?"
```
Expected: `rc=0`（マジックナンバーの行が出力される）。target.cmake が `--gc-sections` を一切使っていないため，polarfire のような `--no-gc-sections` ワークアラウンドが無くても消えないことの確認（`cmake/check_magic_number.cmake` は cfg1_out.syms 生成直後に既にこれを検査しビルドを止める設計なので，Step 1 が成功している時点で実質確認済みだが，ここで明示的に再確認する）。

**negative control**（もし --gc-sections を積んだら本当に消えることの確認。musca_b1 では使わない設計だが，「消える」という前提そのものが正しいことを polarfire 側の既存実測（計画A Task 4）に加えて musca_b1 の実際のツールチェーン・リンカスクリプトでも成立することを確かめる）:
```bash
cd build/musca_b1
arm-none-eabi-gcc -mcpu=cortex-m33 -mthumb -mfloat-abi=soft -mlittle-endian \
    -nostdlib -Wl,-T,../../target/musca_b1_gcc/musca_b1.ld -Wl,--gc-sections \
    $(find CMakeFiles/cfg1_out.dir -name '*.obj') -lc -lgcc \
    -o /tmp/cfg1_out_gc.elf
arm-none-eabi-nm /tmp/cfg1_out_gc.elf | grep TOPPERS_magic_number
echo "rc=$?"
cd /home/honda/TOPPERS/FMP3/fmp3_core
```
Expected: `rc=1`（`--gc-sections` を付けて cfg1_out 相当を手動リンクすると，TOPPERS_magic_number が実際に消えることの確認。musca_b1 のオブジェクトファイル配置は Step 1 のビルドログか `find build/musca_b1/CMakeFiles/cfg1_out.dir -name '*.obj'` で実パスを確認してからコマンドを組み立てること）。

- [ ] **Step 3: offset.h 順序依存の負制御（レースの再現）を行う**

Task 1 で追加した `add_dependencies(fmp generate_cfg_gen_files)` を一時的に外し，高い並列度のクリーンビルドを複数回繰り返して，ARM-M 特有のレース（offset.h/kernel_cfg.h 生成前に `fmp` 側の C オブジェクトがコンパイルされる）が実際に起こりうることを確認する。

Run:
```bash
sed -i '/^add_dependencies(fmp generate_cfg_gen_files)$/d' CMakeLists.txt
grep -n "add_dependencies(fmp " CMakeLists.txt
```
Expected: `add_dependencies(fmp3 generate_cfg_gen_files)` の行だけが残り（`fmp3` 分。musca_b1 でも既存のまま），`add_dependencies(fmp generate_cfg_gen_files)`（`fmp` 分）は消えている。

Run（クリーンビルドを高並列度で5回繰り返す。レースは確率的現象であるため決定的ではない点に注意）:
```bash
NPROC=$(nproc)
FAIL=0
for i in 1 2 3 4 5; do
    rm -rf build/musca_b1
    cmake --preset musca_b1 >/dev/null
    if ! cmake --build build/musca_b1 -- -j"${NPROC}" > /tmp/musca_race_$i.log 2>&1; then
        FAIL=$((FAIL+1))
    fi
done
echo "FAIL=${FAIL} / 5"
grep -l "offset.h: No such file or directory\|kernel_cfg.h: No such file or directory" /tmp/musca_race_*.log
```
Expected: `FAIL` が1以上、かつ少なくとも1つのログに `offset.h: No such file or directory` または `kernel_cfg.h: No such file or directory` を含む行が見つかる（レースが実際に起きたことの確認）。**5回中0回しか失敗しない場合でも，このタスクの修正自体が誤りとは断定しない**（レースはタイミング依存であり，`nproc` が小さい環境や ninja のスケジューリングによっては再現しないことがある。その場合は `-j` をさらに上げる，または試行回数を増やして再試行する。それでも一度も再現しない場合は，その旨と試行条件を正直に記録し，Step 4 の正制御（多数回成功する）で代替の裏付けとする）。

Run（修正を元に戻す）:
```bash
git checkout -- CMakeLists.txt
grep -n "add_dependencies(fmp " CMakeLists.txt
```
Expected: `add_dependencies(fmp generate_cfg_gen_files)` の行が復元されている。

- [ ] **Step 4: 修正込みで高並列度のクリーンビルドを繰り返し，レースが再発しないことを確認する（positive control）**

Run:
```bash
FAIL=0
for i in 1 2 3 4 5; do
    rm -rf build/musca_b1
    cmake --preset musca_b1 >/dev/null
    if ! cmake --build build/musca_b1 -- -j"$(nproc)" > /tmp/musca_norace_$i.log 2>&1; then
        FAIL=$((FAIL+1))
    fi
done
echo "FAIL=${FAIL} / 5"
```
Expected: `FAIL=0 / 5`（`add_dependencies(fmp generate_cfg_gen_files)` がある状態では，同じ試行回数・同じ並列度でレースが再発しないことの確認）。

- [ ] **Step 5: シンボルを確認する（1コア専用の生成物になっていること）**

Run:
```bash
arm-none-eabi-nm build/musca_b1/fmp | grep -E "_kernel_pcb_prc[0-9]|_kernel_istack_prc[0-9]"
```
Expected: `_kernel_pcb_prc1` / `_kernel_istack_prc1` のみが現れ，`_kernel_pcb_prc2` / `_kernel_istack_prc2` は現れない（`FMP3_PRC_NUM` 未指定＝`TNUM_PRCID` 既定1が効いていることの確認。`target_kernel.trb` は `1.upto($TNUM_PRCID)` でテーブルを生成するため，2コア分は生成されないはず）。

**negative control**（2コア指定なら本当に2プロセッサ分のシンボルが増えることの確認。Task 6 の先取りになるが，1コアの結果が「たまたま2コア分も無い」わけではないことをここで裏取りする）:
```bash
rm -rf /tmp/fmp3-musca-2core-check
cmake -G Ninja -B /tmp/fmp3-musca-2core-check -S . \
    --preset musca_b1 -DFMP3_PRC_NUM=2 >/dev/null 2>&1 || \
cmake -G Ninja -B /tmp/fmp3-musca-2core-check -S . \
    -DFMP3_TARGET=musca_b1_gcc -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-arm-none-eabi.cmake \
    -DFMP3_PRC_NUM=2
cmake --build /tmp/fmp3-musca-2core-check --target fmp 2>&1 | tail -5
arm-none-eabi-nm /tmp/fmp3-musca-2core-check/fmp | grep -E "_kernel_pcb_prc[0-9]|_kernel_istack_prc[0-9]"
rm -rf /tmp/fmp3-musca-2core-check
```
Expected: `_kernel_pcb_prc1` と `_kernel_pcb_prc2`（`_kernel_istack_prc1/2` も同様）の両方が現れる。

- [ ] **Step 6: 未定義シンボルが無いことを確認する**

Run: `arm-none-eabi-nm -u build/musca_b1/fmp`
Expected: 出力なし（未定義シンボル無し。`echo "rc=$?"` は `nm -u` が該当行を出さない限り常に成功で終わるため，出力そのものが空であることを直接確認する）。

- [ ] **Step 7: polarfire の regression（別ディレクトリでのビルド干渉が無いこと）を確認する**

Run:
```bash
rm -rf build/polarfire_soc_kit-qemu
cmake --preset polarfire_soc_kit-qemu
cmake --build build/polarfire_soc_kit-qemu 2>&1 | tail -10
riscv64-unknown-elf-readelf -h build/polarfire_soc_kit-qemu/fmp | grep Entry
```
Expected: ビルド成功、`Entry point address:  0x8000000`（変更なし）。

- [ ] **Step 8: コミット**

このタスクはコード変更を伴わない（既存パイプラインの検証のみ）。コミットは不要。作業ディレクトリに変更が残っていないことだけ確認する:
```bash
git status --short
```
Expected: `build/` 配下（`.gitignore` 対象）以外に変更が無いこと。

---

### Task 5: QEMU で1コア起動を確認する

**Files:**
- Modify: `target/musca_b1_gcc/target.cmake`（末尾に QEMU 版検査と `FMP3_RUN_COMMAND` を追加）

**Interfaces:**
- Consumes: `build/musca_b1/fmp`（Task 4）
- Produces: `FMP3_RUN_COMMAND`、`QEMU_SYSTEM_ARM_MUSCA_B1`（キャッシュ変数）

- [ ] **Step 1: 実際に使う QEMU の版を実測する**

Run:
```bash
/home/honda/qemu-build/install/bin/qemu-system-arm --version
qemu-system-arm --version
```
Expected:
```
QEMU emulator version 11.0.1
...
QEMU emulator version 8.2.2 (Debian 1:8.2.2+ds-0ubuntu1.16)
```
（システムの `qemu-system-arm` を黙って使うと 8.2.2 になる恐れがあることの確認。既定は前者を使う設計にする）

- [ ] **Step 2: 期待するバナー文字列を現物で確認する**

Run:
```bash
grep -n "TARGET_NAME" target/musca_b1_gcc/target_syssvc.h
sed -n '60,95p' syssvc/banner.c
```
Expected: `#define TARGET_NAME "ARM Musca-B1"`、banner.c の `print_banner_copyright`/`print_banner` が `"TOPPERS/FMP3 Kernel Release %d.%X.%d for %s"` と `"Processor %d start."` を出力すること（既に Task の研究段階で確認済み。実装時に再確認する）。

- [ ] **Step 3: `target.cmake` に QEMU 検出・版検査・`FMP3_RUN_COMMAND` を追加する**

`target/musca_b1_gcc/target.cmake` の末尾に追加:
```cmake
#
#  QEMU（musca-b1 マシン）
#
#  ★システムの /usr/bin/qemu-system-arm は 8.2.2 であり，上流
#    target_user.txt:47-55 が要求する「MHU（プロセッサ間割込み）と
#    SSE-200 のセカンダリコア起動（CPUWAIT）が正しく実装されたバージョン」
#    （動作確認済みは QEMU 11.0.1 系）を満たさない可能性がある．
#    /home/honda/qemu-build/install/bin/qemu-system-arm に 11.0.1 が
#    ビルド済みであれば既定でそちらを使う．無ければ PATH 上の
#    qemu-system-arm にフォールバックする（1コアなら 8.2.2 でも動く．
#    musca-b1 マシン自体は 8.2.2 にも存在する．実測済み）．
#
set(_fmp3_musca_qemu_builtin /home/honda/qemu-build/install/bin/qemu-system-arm)
if(EXISTS ${_fmp3_musca_qemu_builtin})
    set(_fmp3_musca_qemu_default ${_fmp3_musca_qemu_builtin})
else()
    set(_fmp3_musca_qemu_default qemu-system-arm)
endif()
set(QEMU_SYSTEM_ARM_MUSCA_B1 ${_fmp3_musca_qemu_default} CACHE STRING
    "Path to qemu-system-arm for the musca-b1 machine (needs >= 11.0.1 for reliable 2-processor MHU/CPUWAIT support; see target_user.txt:47-55)")
unset(_fmp3_musca_qemu_builtin)
unset(_fmp3_musca_qemu_default)

#
#  実測でバージョンを確認し，11 未満なら警告する（黙って古い QEMU を
#  使ってしまう事故を防ぐ．8.2.2 では 2 コア SMP の MHU が未実装で
#  クロスコア IPI が機能しないことがある．1コアは影響を受けない）．
#
execute_process(
    COMMAND ${QEMU_SYSTEM_ARM_MUSCA_B1} --version
    OUTPUT_VARIABLE _fmp3_qemu_version_output
    RESULT_VARIABLE _fmp3_qemu_version_result
    OUTPUT_STRIP_TRAILING_WHITESPACE
)
if(_fmp3_qemu_version_result EQUAL 0 AND _fmp3_qemu_version_output MATCHES "version ([0-9]+)\\.")
    set(_fmp3_qemu_major ${CMAKE_MATCH_1})
    message(STATUS "fmp3_core: QEMU_SYSTEM_ARM_MUSCA_B1 version = ${_fmp3_qemu_version_output}")
    if(_fmp3_qemu_major LESS 11)
        message(WARNING
            "fmp3_core: QEMU_SYSTEM_ARM_MUSCA_B1='${QEMU_SYSTEM_ARM_MUSCA_B1}' reports "
            "major version ${_fmp3_qemu_major} (< 11). target_user.txt:47-55 notes that "
            "older QEMU may lack MHU emulation, which breaks cross-core IPI for "
            "FMP3_PRC_NUM=2 builds. 1-processor builds are unaffected. Override with "
            "-DQEMU_SYSTEM_ARM_MUSCA_B1=<path to qemu-system-arm >= 11.0.1> if available "
            "(e.g. /home/honda/qemu-build/install/bin/qemu-system-arm).")
    endif()
    unset(_fmp3_qemu_major)
else()
    message(WARNING
        "fmp3_core: could not determine the version of "
        "QEMU_SYSTEM_ARM_MUSCA_B1='${QEMU_SYSTEM_ARM_MUSCA_B1}' (is it installed / on PATH?).")
endif()
unset(_fmp3_qemu_version_output)
unset(_fmp3_qemu_version_result)

#
#  QEMU による実行（cmake --build <dir> --target run）
#
#  Musca-B1 はボード構成として2コアが固定されているため -smp オプションは
#  不要（単一の -kernel を両コアが参照する．二次コアの起動は QEMU 側では
#  なくカーネル側の target_mprc_initialize が CPUWAIT レジスタを操作して
#  行う）．-device loader も -bios none も不要（polarfire と違い，直接
#  _kernel_start から起動する）．
#
#  出典: target/musca_b1_gcc/target_user.txt:75-83
#
set(FMP3_RUN_COMMAND
    ${QEMU_SYSTEM_ARM_MUSCA_B1} -machine musca-b1 -cpu cortex-m33
    -kernel $<TARGET_FILE:fmp> -nographic
    -semihosting-config enable=on,target=native
)
```

- [ ] **Step 4: configure が通ることを確認する**

Run: `cmake --preset musca_b1 2>&1 | grep -E 'fmp3_core: QEMU|Configuring done'`
Expected: `-- fmp3_core: QEMU_SYSTEM_ARM_MUSCA_B1 version = QEMU emulator version 11.0.1` と `Configuring done`（`/home/honda/qemu-build/install/bin/qemu-system-arm` が既定で選ばれ，かつバージョンが 11 以上のため WARNING が出ないことの確認）。

**negative control**（8.2.2 を明示すると WARNING が出ることの確認）:
```bash
rm -rf build/musca_b1
cmake --preset musca_b1 -DQEMU_SYSTEM_ARM_MUSCA_B1=/usr/bin/qemu-system-arm 2>&1 \
    | grep -E 'fmp3_core: QEMU|Warning' 
```
Expected: `-- fmp3_core: QEMU_SYSTEM_ARM_MUSCA_B1 version = QEMU emulator version 8.2.2 ...` と、`CMake Warning ... major version 8 (< 11) ...` を含む WARNING（FATAL ではない。confirm後、既定に戻す）:
```bash
rm -rf build/musca_b1
cmake --preset musca_b1 >/dev/null
```

- [ ] **Step 5: 1コアで実際に起動することを確認する（タイムアウト付き・プロセス残留無し）**

Run:
```bash
cmake --build build/musca_b1 2>&1 | tail -5
timeout --signal=KILL 15 /home/honda/qemu-build/install/bin/qemu-system-arm \
    -machine musca-b1 -cpu cortex-m33 \
    -kernel build/musca_b1/fmp -nographic \
    -semihosting-config enable=on,target=native \
    < /dev/null > /tmp/musca_b1_1core_run.log 2>&1
echo "rc=$?"
cat /tmp/musca_b1_1core_run.log
```
Expected: `rc=0`（`target_exit` のセミホスティング `SYS_EXIT` により QEMU 自身が正常終了する。`timeout` のタイムアウトに引っかからないはず）。ログに次が含まれる:
```
TOPPERS/FMP3 Kernel Release 3.4.0 for ARM Musca-B1
Processor 1 start.
local_inirtn exinf = 1, counter = 1
```
（正確な行順・追加のログ行は実測を優先し，上記に一致しない場合は現物のログを基準に記述を訂正すること。`counter` の値は `global_inirtn` が `counter=1` を設定した直後に `local_inirtn` が `counter++` で消費するため 1 になるはずだが，`sample1.cfg` の他の `ATT_INI` 登録順によっては変わりうる）。

Run（プロセスが残っていないことの確認）:
```bash
pgrep -f "qemu-system-arm.*musca-b1" ; echo "pgrep rc=$?"
```
Expected: `pgrep rc=1`（該当プロセス無し）。

- [ ] **Step 6: negative control（タイムアウトが実際に機能することの確認）**

`-semihosting-config` を落とすと `target_exit` の `bkpt 0xab` が捕捉されずカーネルが `while(1);` で無限ループするか，あるいは QEMU 側が例外を出す。ここでは代わりに，意図的に短いタイムアウトを与えて「タイムアウトが本当に効く」ことを確認する（`sample1` はタスク切替でしばらく動き続けるため，1秒では正常終了しない）:
```bash
timeout --signal=KILL 1 /home/honda/qemu-build/install/bin/qemu-system-arm \
    -machine musca-b1 -cpu cortex-m33 \
    -kernel build/musca_b1/fmp -nographic \
    -semihosting-config enable=on,target=native \
    < /dev/null > /tmp/musca_b1_1core_timeout.log 2>&1
echo "rc=$?"
```
Expected: `rc=137`（`SIGKILL` による強制終了。`timeout` の仕組みそのものが機能していることの確認）。

```bash
pgrep -f "qemu-system-arm.*musca-b1" ; echo "pgrep rc=$?"
```
Expected: `pgrep rc=1`（`timeout --signal=KILL` 経由でも子プロセスが残らないことの確認。もし残っていたら `pkill -KILL -f "qemu-system-arm.*musca-b1"` で掃除してから次の Step へ進む）。

- [ ] **Step 7: `cmake --build --target run` からも同じ結果になることを確認する**

Run:
```bash
timeout --signal=KILL 15 cmake --build build/musca_b1 --target run \
    < /dev/null > /tmp/musca_b1_run_target.log 2>&1
echo "rc=$?"
grep -c "Processor 1 start" /tmp/musca_b1_run_target.log
```
Expected: `rc=0`、`1`（`run` ターゲット経由でも Step 5 と同じ出力が得られることの確認。`USES_TERMINAL` の都合でログの体裁は多少変わりうるが，バナー行の有無は同じはず）。

- [ ] **Step 8: polarfire の regression を確認する**

Run:
```bash
timeout --signal=KILL 20 cmake --build build/polarfire_soc_kit-qemu --target run \
    < /dev/null > /tmp/polarfire_regression_run.log 2>&1
echo "rc=$?"
grep -c "Processor .* start" /tmp/polarfire_regression_run.log
```
Expected: `rc=0`、`4`（計画A Task 8 で確認済みの4プロセッサ起動が変わらず再現することの確認）。

- [ ] **Step 9: コミット**

```bash
git add target/musca_b1_gcc/target.cmake
git commit -m "build: musca_b1 のQEMU実行（1コア）を追加

QEMU_SYSTEM_ARM_MUSCA_B1 は /home/honda/qemu-build/install/bin/
qemu-system-arm（11.0.1）を既定にし，無ければ PATH の qemu-system-arm
にフォールバックする。版が11未満ならWARNINGを出す（システムの8.2.2を
黙って使って2コアが動かない事態を避けるため）。
1コアでの起動をQEMU 11.0.1・8.2.2の両方で確認し，実際に使う版を
STATUS/WARNINGログに記録する形にした。"
```

---

### Task 6: 2コア SMP（`FMP3_PRC_NUM=2`）のビルドと QEMU 起動確認

**Files:** なし（`musca_b1-2core` プリセットは Task 2 で作成済み。`FMP3_PRC_NUM` は計画Aで実装済みの汎用機構）

**Interfaces:**
- Consumes: `musca_b1-2core` プリセット（`FMP3_PRC_NUM=2` を注入）
- Produces: `build/musca_b1-2core/fmp`

このタスクは「1コアで通す → 2コアを試す」の後半にあたる。**2コアの QEMU 実行が失敗した場合，それが実装の不備か QEMU の制約かを切り分けられる形で進める**（Step 4 の新旧 QEMU 比較）。

- [ ] **Step 1: 2コアでビルドする**

Run:
```bash
rm -rf build/musca_b1-2core
cmake --preset musca_b1-2core
cmake --build build/musca_b1-2core 2>&1 | tee /tmp/musca_b1_2core_build.log | tail -20
echo "rc=${PIPESTATUS[1]}"
```
Expected: `rc=0`。

- [ ] **Step 2: `-DTNUM_PRCID=2` が実際に効いていることを確認する**

Run:
```bash
cmake --preset musca_b1-2core 2>&1 | grep 'FMP3_PRC_NUM\|FMP3_COMPILE_DEFS'
```
Expected:
```
-- fmp3_core: FMP3_PRC_NUM    = '2'
-- fmp3_core: FMP3_COMPILE_DEFS= TOPPERS_USE_QEMU;TOPPERS_OMIT_TECS;TOPPERS_CORTEX_M33;__TARGET_ARCH_THUMB=5;TOPPERS_ENABLE_TRUSTZONE;TNUM_PRCID=2
```
（`TNUM_PRCID=2` が末尾に付くことの確認。順序は `target.cmake`/`chip.cmake` の積み上げ順に依存するため，値が含まれていることだけを確認すればよい）

- [ ] **Step 3: 2プロセッサ分のシンボル・クラスが生成されていることを確認する（positive control。Task 4 Step 5 の negative control と対）**

Run:
```bash
arm-none-eabi-nm build/musca_b1-2core/fmp | grep -E "_kernel_pcb_prc[0-9]|_kernel_istack_prc[0-9]"
grep -n "CLS_PRC2\|CLS_ALL_PRC2" build/musca_b1-2core/generated/kernel_cfg.c | head -5
```
Expected: `_kernel_pcb_prc1` と `_kernel_pcb_prc2`（`_kernel_istack_prc1/2` も同様）の両方が現れる。`kernel_cfg.c` に `CLS_PRC2`／`CLS_ALL_PRC2` 由来の生成コードが含まれる（`target_class.trb` の `case $TNUM_PRCID when 2` 分岐が実際に選択されたことの確認）。

- [ ] **Step 4: 新しい QEMU（11.0.1）で2コア SMP を起動する**

Run:
```bash
timeout --signal=KILL 15 /home/honda/qemu-build/install/bin/qemu-system-arm \
    -machine musca-b1 -cpu cortex-m33 \
    -kernel build/musca_b1-2core/fmp -nographic \
    -semihosting-config enable=on,target=native \
    < /dev/null > /tmp/musca_b1_2core_new_qemu.log 2>&1
echo "rc=$?"
cat /tmp/musca_b1_2core_new_qemu.log
```
Expected: `rc=0`。ログに `Processor 1 start.` と `Processor 2 start.` の両方が含まれる（**出現順序は保証しない**。真の SMP 並行起動であれば順序がばらけうる。計画A Task 8 の polarfire 4プロセッサでの実測と同じ扱い）。`local_inirtn exinf = 1, ...` と `local_inirtn exinf = 2, ...` の両方が現れることも確認する。

Run（プロセス残留確認）:
```bash
pgrep -f "qemu-system-arm.*musca-b1" ; echo "pgrep rc=$?"
```
Expected: `pgrep rc=1`。

- [ ] **Step 5: 古い QEMU（8.2.2）での挙動を記録する（切り分け用の負制御）**

同じバイナリを，システムの古い QEMU（8.2.2）で実行し，結果を記録する。**ここで失敗しても実装の不備とは断定しない**（brief 記載の前提どおり，古い QEMU の MHU 実装状況に依存する既知のリスクである）。

Run:
```bash
timeout --signal=KILL 15 qemu-system-arm \
    -machine musca-b1 -cpu cortex-m33 \
    -kernel build/musca_b1-2core/fmp -nographic \
    -semihosting-config enable=on,target=native \
    < /dev/null > /tmp/musca_b1_2core_old_qemu.log 2>&1
echo "rc=$?"
cat /tmp/musca_b1_2core_old_qemu.log
```
Record: `rc` の値とログ全文を，このタスクの実行記録（`.superpowers/sdd/progress.md` 相当）に残す。**期待値は事前に断定しない**（Step 4 で新しい QEMU が成功した場合，ここでの結果が「新旧 QEMU の版差による違い」の実例として使える。もし ここも成功するなら，8.2.2 でも musca-b1 の MHU は十分実装されていたということであり，その場合は brief の前提を訂正して記録する）。

Run（プロセス残留確認）:
```bash
pgrep -f "qemu-system-arm.*musca-b1" ; echo "pgrep rc=$?"
```
Expected: `pgrep rc=1`。

- [ ] **Step 6: `cmake --build --target run` から確認する（既定 QEMU 経由）**

Run:
```bash
timeout --signal=KILL 15 cmake --build build/musca_b1-2core --target run \
    < /dev/null > /tmp/musca_b1_2core_run_target.log 2>&1
echo "rc=$?"
grep -c "Processor .* start" /tmp/musca_b1_2core_run_target.log
```
Expected: `rc=0`、`2`。

- [ ] **Step 7: 1コアビルド（`musca_b1`）が2コアビルドの影響を受けていないことを確認する（regression）**

Run:
```bash
timeout --signal=KILL 15 cmake --build build/musca_b1 --target run \
    < /dev/null > /tmp/musca_b1_1core_regression.log 2>&1
echo "rc=$?"
grep -c "Processor .* start" /tmp/musca_b1_1core_regression.log
```
Expected: `rc=0`、`1`（`musca_b1` と `musca_b1-2core` は別 `binaryDir` のためキャッシュが混ざらないはずだが，念のため両立を確認する）。

- [ ] **Step 8: polarfire の regression を確認する**

Run:
```bash
timeout --signal=KILL 20 cmake --build build/polarfire_soc_kit-qemu --target run \
    < /dev/null > /tmp/polarfire_regression_2.log 2>&1
echo "rc=$?"
grep -c "Processor .* start" /tmp/polarfire_regression_2.log
```
Expected: `rc=0`、`4`。

- [ ] **Step 9: コミット**

このタスクはコード変更を伴わない（`musca_b1-2core` プリセットは Task 2 で既に作成済み）。作業ディレクトリに意図しない変更が残っていないことを確認する:
```bash
git status --short
```
Expected: `build/` 配下以外に変更が無いこと。実行記録（QEMU 新旧の結果）は Task 7 の完了報告に含める。

---

### Task 7: `DIVERGENCE_MAP.md` への記録と最終回帰・完了条件チェック

**Files:**
- Modify: `DIVERGENCE_MAP.md`
- Modify: `CLAUDE.md`（現況セクションの更新。任意だが計画Aでも行った慣行に倣う）

**Interfaces:** なし（記録作業）

- [ ] **Step 1: pristine への追加ファイルを洗い出す**

Run:
```bash
git log --diff-filter=A --name-only --oneline -- arch/ target/ | grep -E '\.cmake$|presets\.json$'
```
Expected:
```
arch/riscv_gcc/common/arch.cmake
arch/riscv_gcc/polarfire_soc/chip.cmake
target/polarfire_soc_kit_gcc/target.cmake
target/polarfire_soc_kit_gcc/presets.json
arch/arm_m_gcc/common/arch.cmake
arch/arm_m_gcc/musca_b1/chip.cmake
target/musca_b1_gcc/target.cmake
target/musca_b1_gcc/presets.json
```
（polarfire 分は計画Aで既に `DIVERGENCE_MAP.md` に記録済み。musca_b1 分の4件が本タスクでの記録対象）

- [ ] **Step 2: `DIVERGENCE_MAP.md` に行を追加する**

`DIVERGENCE_MAP.md` の表に追加:
```
| arch/arm_m_gcc/common/arch.cmake | add | Makefile.core の CMake 版。上流の Makefile は残すが CMake ビルドからは参照しない | - |
| arch/arm_m_gcc/musca_b1/chip.cmake | add | Makefile.chip の CMake 版。--gc-sections は上流に無いため使わない（polarfire の l2lim 制約がこのターゲットには無い） | - |
| target/musca_b1_gcc/{target.cmake,presets.json} | add | Makefile.target の CMake 版。QEMU 専用ターゲット（実機非対応）。FMP3_LDSCRIPT_VIA_DRIVER_T は設定しない（既定 OFF＝-Wl,-T, のままでよい） | - |
```

- [ ] **Step 3: 全ターゲットの最終 regression を行う**

Run:
```bash
for preset in polarfire_soc_kit-qemu musca_b1 musca_b1-2core; do
    echo "=== ${preset} ==="
    rm -rf build/${preset}
    cmake --preset ${preset} >/dev/null
    cmake --build build/${preset} 2>&1 | tail -3
done
```
Expected: 3プリセットすべてでビルド成功（`rc` を明示的に見たい場合は各行の直後に `echo "rc=${PIPESTATUS[1]}"` を挟む）。

- [ ] **Step 4: 完了条件チェックリスト（AGENTS.md §6）を確認する**

```bash
git ls-tree upstream --name-only -r | grep -E '\.cmake$|CMakeLists|presets\.json'
echo "rc=$?"
```
Expected: `rc=1`（`upstream` ブランチに派生ファイルが無いことの確認。何も出力されないため grep が非マッチで rc=1 になる）。

```bash
grep -c "^| " DIVERGENCE_MAP.md
```
Expected: Task 1（`target/polarfire_soc_kit_gcc/{target.cmake,presets.json}` の既存行を更新のみ、新規行は0）と Task 7 Step 2（新規3行）を合わせた行数が増えていること。

- [ ] **Step 5: `CLAUDE.md` の現況セクションを更新する**

`CLAUDE.md` の該当箇所に、計画A2（musca_b1）が完了したこと、2ターゲット（`polarfire_soc_kit_gcc` / `musca_b1_gcc`）が CMake ビルド可能であること、汎用層の設計漏れ2件（`-T` 書式・`fmp` の順序依存）を修正したことを追記する。

- [ ] **Step 6: コミット**

```bash
git add DIVERGENCE_MAP.md CLAUDE.md
git commit -m "docs: 計画A2完了 — musca_b1をCMake化し、汎用層の設計漏れ2件を修正

musca_b1_gcc（ARM dual Cortex-M33 / QEMU）をpolarfire_soc_kit_gccに次ぐ
2つ目のCMake化ターゲットとして追加。汎用層(CMakeLists.txt)に手を入れずに
target/chip/arch の3ファイル＋ツールチェーン＋プリセットを足すだけで
1コア・2コアSMPともQEMU起動まで到達した。既知の設計漏れ（-T適用ロジックの
POLARFIRE_QEMU決め打ち）を修正し、加えてARM-M特有のoffset.h順序依存漏れを
発見・修正した（RISC-Vでは偶然表面化していなかった汎用層の欠陥）。
DIVERGENCE_MAPにpristineディレクトリへの追加4件を記録。"
```

---

## 1コア → 2コアの切り分け方（要約）

- **1コア（`musca_b1` プリセット）は QEMU の版に依存しない**（Task 5 で 11.0.1 と 8.2.2 の両方で成功することを確認する）。1コアが失敗したら、それは実装側の問題である。
- **2コア（`musca_b1-2core` プリセット）は QEMU の MHU/CPUWAIT 実装度合いに依存しうる**（Task 6）。Task 6 Step 4（新しい QEMU 11.0.1）と Step 5（古い QEMU 8.2.2）の結果を突き合わせることで、
  - 両方成功 → 実装は正しく、8.2.2 でも問題ない（brief の前提を訂正して記録）。
  - 新のみ成功、旧は失敗 → 実装は正しく、QEMU の版依存が実証される（想定どおり）。
  - **新でも失敗** → これは実装側の問題である可能性が高い（`FMP3_PRC_NUM=2` のコンパイル定義注入・`target_class.trb`/`target_kernel.trb` の2コア分岐・`target_ipi.c`/`chip_asm.inc` のブートハンドシェイクのいずれかを疑う。Task 6 Step 2/3 の静的確認（`TNUM_PRCID=2` の伝播、2プロセッサ分シンボルの生成）が通っているのに実行が失敗する場合、疑うべきは生成物ではなく実行時の初期化順序である）。

## Self-Review

**1. Spec coverage:**
- musca_b1 を第1波に追加する（brief 冒頭） → Task 2〜6。
- 既知の設計漏れ（`-T` の POLARFIRE_QEMU 決め打ち）を直す → Task 1 Step 1-4。
- `ARCHDIR`/`CHIPDIR`/`TARGETDIR` の書き分け → Task 3 Step 4（`target/musca_b1_gcc/target.cmake` 冒頭）。
- `FMP3_PRC_NUM=2` の実地検証（初めて必要になる） → Task 6。
- 1コア→2コアの順で切り分け可能な形にする → Task 4/5（1コア）→ Task 6（2コア）、および上記「切り分け方」節。
- QEMU 実行にタイムアウトを付け、プロセス残留を確認する → Task 5 Step 5/6、Task 6 Step 4/5。
- 期待出力は現物で確認してから書く → banner.c / target_syssvc.h / sample1.c を実際に読んで TARGET_NAME・メッセージ書式を確認済み（Task 5 Step 2 に反映）。
- polarfire が壊れていないことを各段階で確認する → 全タスクに regression ステップを配置（Task 1 Step 4/6, Task 2 Step 9, Task 3 Step 6, Task 4 Step 7, Task 5 Step 8, Task 6 Step 8）。
- positive/negative control の対 → 全タスクに配置（特に Task 1 の -T 書式、Task 4 の offset.h レース、Task 5/6 の QEMU 版・タイムアウト）。
- QEMU パスをキャッシュ変数にし、既定を安全側にする（コーディネータからの追加指示） → Task 5 Step 3 の `QEMU_SYSTEM_ARM_MUSCA_B1`。
→ ギャップ無し。

**2. Placeholder scan:** 「TBD」「適切に」「Task N と同様」といった記述が無いか全文を検索し、無いことを確認した（コード・コマンド・期待値はすべて具体的に記述、`sample1.cfg` の登録順に依存する箇所のみ「現物のログを基準に訂正すること」と明記し、断定を避けるべき箇所とそうでない箇所を区別した）。

**3. Type consistency:** `FMP3_LDSCRIPT_VIA_DRIVER_T`（Task 1 で導入 → Task 3 で「設定しない」という形で参照）、`FMP3_SYSSVC_TARGET_C_FILES`（Task 3 で `target_serial.c`/`pl011_uart.c` を積む → 計画Aの `fmp3_add_syssvc()` がそのまま読む、関数シグネチャ変更なし）、`QEMU_SYSTEM_ARM_MUSCA_B1`（Task 5 で定義 → Task 6 で読む）の変数名が全タスクを通じて一致していることを確認した。`generate_cfg_gen_files`（計画A Task 5 で定義済みのターゲット名）も変更していない。
