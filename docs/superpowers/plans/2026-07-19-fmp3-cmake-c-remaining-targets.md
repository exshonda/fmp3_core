# fmp3_core CMake化 — 残り3ターゲット追加 実装計画

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** `rp2350_pico2_gcc`・`kria_arm64_gcc`・`kria_r5_gcc` の3ターゲットをCMakeでビルド可能にし，Pythonのcfgテンプレート移植・差分等価性検査・実行検証（QEMUまたは記録された制約）を行う。

**Architecture:** 既存の汎用層（`CMakeLists.txt`・`cfg_py/`・`cmake/`）と計画A/A2/Bで確立したレイヤ構造（`target.cmake → chip.cmake → arch.cmake`、`.trb→.py`並置、`tools/cfg_equivalence.sh`によるRubyとの差分等価性検査）をそのまま踏襲する。新規に要る汎用層の変更は1箇所のみ（`kria_arm64_gcc`のROMイメージ形式が`srec`ではなく`dump`であるためのフック）。

**Tech Stack:** CMake 3.23+ / Ninja / Python3（cfg_py エンジン、asp3_core 1.7.1移植）/ Ruby cfg（オラクル、`tools/cfg_equivalence.sh`専用）/ 各ターゲットのクロスツールチェーン / QEMU。

## Global Constraints

- **`upstream`ブランチに派生ファイルを載せない**。すべての新規`.py`/`.cmake`/`presets.json`は`main`のみ（AGENTS.md §2 規則1）。
- **pristineへの改変は必ず`DIVERGENCE_MAP.md`に記録**（対象・種別・理由・上流報告有無。AGENTS.md §2 規則2）。
- **pristineの`cfg/`は使わない**。cfgは`cfg_py/cfg.py`（Python, asp3_core 1.7.1移植）経由のみ（AGENTS.md §2 規則3）。
- **上流追従はマージのみ**、コピー上書き禁止（AGENTS.md §2 規則5）。
- **`TNUM_PRCID`の値そのものでCMakeを分岐させない**（設計書§3）。CMakeの分岐材料はターゲットが宣言する変数（`FMP3_LDSCRIPT_VIA_DRIVER_T`のような明示トグル）であり，チップの実コア数や割込み制御器の種類ではない。
- **各タスクの検証はpositive controlとnegative controlを対で示す**。「全部一致」は比較が壊れていても得られる（`.superpowers/sdd/progress.md`に23件の実例）。
- **`git add`はpathspec指定**（`-A`禁止）。`--amend`禁止。
- コミット前に`tools/cfg_equivalence.sh`と既存3構成（polarfire・musca_b1 1コア/2コア）の回帰を必ず確認する。

---

## 現況・事前調査で確定した事実（実装者はこれを前提にしてよい）

### 移植量の実測（本計画の作業直前に現物で確認済み）

| ターゲット | 既存流用（そのまま/ヘッダのみ変更） | 新規翻訳が必要 | 新規行数（.trb原文） |
|---|---|---|---|
| `rp2350_pico2_gcc` | `target/musca_b1_gcc/{target_kernel,target_class,target_check}.py`を**ほぼ無変更で流用**（`target_kernel.trb`/`target_class.trb`/`target_check.trb`は`diff`でコメント1行以外バイト同一と確認済み）。`arch/arm_m_gcc/common/*.py`（core層）は既存のまま無改造で流用 | `arch/arm_m_gcc/rp2350/chip_kernel.py`（RP2350 SIOスピンロック割当のみ、他は`IncludeTrb`1行） | 31行 |
| `kria_arm64_gcc` | なし（`arch/arm64_gcc/`配下に`.py`は1個も無い）。ただし**`target_class.py`は`target/polarfire_soc_kit_gcc/target_class.py`とバイト同一構造**（`target_class.trb`をdiffしてコメント行以外差分ゼロと確認済み）なので実質コピー | `core_kernel.py`/`core_check.py`/`core_offset.py`/`gic_kernel.py`/`chip_kernel.py`/`target_kernel.py`/`target_check.py`（`target_class.py`はコピー） | 538行（core_kernel 223, core_check 92, core_offset 36, gic_kernel 62, chip_kernel 34, target_kernel 11, target_check 11） |
| `kria_r5_gcc` | なし（`arch/arm_gcc/`配下に`.py`は1個も無い）。**`target_class.py`は`target/musca_b1_gcc/target_class.py`とバイト同一構造**（同様にdiff確認済み）なので実質コピー | `core_kernel.py`/`core_check.py`/`core_offset.py`/`chip_kernel.py`/`target_kernel.py`/`target_check.py`（`target_class.py`はコピー、`gic_kernel.trb`は本ターゲットの`IncludeTrb`連鎖に入らないため不要と確認済み） | 435行（core_kernel 215, core_check 92, core_offset 33, chip_kernel 39, target_kernel 11, target_check 11） |

`core_check.py`（arm64/arm共通の2テーブル・index方式）は**`arch/riscv_gcc/common/core_check.py`（計画Bで移植済み）と完全に同一の構造**であることを`.trb`のdiffで確認済み（コメント中のコピーライト年・`$Id`・アーキ名のみ差分）。翻訳は同エンジンの流用でよい。

### 汎用層に手を入れる必要がある箇所（`kria_arm64_gcc`のみ、確定）

上流`arch/arm64_gcc/common/Makefile.core:34`は`DUMP = dump`（他アーキは`sample/Makefile:133-134`の`ifndef DUMP: DUMP = srec`が既定）。`kria_arm64_gcc/Makefile.target:97`は`DUMPOPTS := $(DUMPOPTS) -j .text -j .rodata`。

一方 `CMakeLists.txt`は`objcopy -O srec`と`.srec`拡張子を3箇所で決め打ちしている（現物のこの計画着手時点の行番号）：
1. `CMakeLists.txt:361-366`（`cfg1_out.srec`生成）
2. `CMakeLists.txt:414-425`中の`:420`（offset.h生成の`--rom-image`）
3. `CMakeLists.txt:523-584`の`fmp3_cfg_check()`関数内`:577-581`（pass3の`--rom-image`）

受け側`cfg_py/cfg.py:700-705`は`args.rom_image_file_name`の拡張子で`SRecord(..., "srec")`/`SRecord(..., "dump")`を自動判別済み（現物確認済み）。**欠けているのはCMake層のフックのみ**。Task 3でこれを直す。

### `kria_arm64_gcc`のQEMU実行に関わる重大な発見（現物のQEMU 11.0.1ソースとFMP3 pristineを突き合わせて発見。実行未確認）

`/home/honda/qemu-build/qemu-11.0.1/hw/arm/xlnx-zynqmp.c`は`APU_ADDR=0xfd5c0000`（RVBARレジスタ）と`CRF_ADDR=0xfd1a0000`（`RST_FPD_APU`書き込みで`arm_set_cpu_on_and_reset()`を呼ぶ）を実装しており，これは`chip_kernel_impl.c`の非SYSMON（単独動作）経路が使う`APU_RVBARADDR*`・`CRF_APB_RST_FPD_APU`と**アドレス一致**する（`zynqmp.h:113-128`）。secondary core起動の枠組み自体はQEMUに実装がある。

**ただし`chip_el3_initialize()`（`arch/arm64_gcc/zynqmp/chip_kernel_impl.c:55-81`）は`XIOU_SCNTRS_BASEADDR=0xFF260000`（System Timestamp Generator）へ**無条件に**（`TOPPERS_USE_QEMU`等のガード無しに）書き込む**。この0xFF260000領域はQEMU 11.0.1の`xlnx-zynqmp.c`に一切実装が無く（`grep`で0件），かつ`xlnx_zynqmp_create_unimp_mmio()`の`unimp_areas`（`serdes`のみ）にも入っていない。QEMUの`unassigned_mem_accepts()`は`false`を返すため，`target/arm/cpu.c`の`do_transaction_failed`経由で**同期外部アボートが注入される**（`system/memory.c`/`system/physmem.c`のコードで確認済み）。この初期化はマスタ・セカンダリ問わず全プロセッサで最も早い段階（EL3）で実行されるため，**1コアであっても影響する可能性が高い**。

姉妹プロジェクト`/home/honda/TOPPERS/ASP3CORE/asp3_core`の`zcu102_arm64_gcc`（同じ`arch/arm64_gcc/zynqmp`をQEMUで動作実績あり）は，まさにこの初期化を`TOPPERS_USE_QEMU`でスキップする改変を行っている（`docs/dev/qemu-target-a64.md:83`「STG初期化＋CNTFRQ設定は`TOPPERS_USE_QEMU`時スキップ」）。**これは前例のある対処法である。**

**この計画では「実行して確かめてから判断する」方針を取る**（Task 7）。断定はしない。

### `kria_r5_gcc`のQEMU実行は上流が既に用意している（高確度）

`target/kria_r5_gcc/Makefile.target:87-102`に**上流自身が書いた**`runqu`/`runqug`ターゲットがある：

```make
QEMU_UPSTREAM = qemu-system-aarch64
runqu: $(OBJFILE)
	$(QEMU_UPSTREAM) -M xlnx-zcu102 -smp 6 -m 2G -nographic \
	 -global xlnx-zynqmp.boot-cpu=rpu-cpu[0] \
	 -global cortex-r5f-arm-cpu.mp-affinity=0 \
	 -device loader,file=$(OBJFILE),cpu-num=4 \
	 -serial null -serial mon:stdio
```

`-smp 6`でR5Fクラスタ（2個）が生成され（`xlnx-zynqmp.c:210-276`の`xlnx_zynqmp_get_rpu_number()`），`cpu-num=4`はグローバルCPU index（A53×4が0-3、RPUが4-5）に対応する。Makefile.target自身のコメントに`QEMU`変数の既定値`true`（`:47`）まであり，**この経路は上流が実際に動作確認した形跡が強い**（`kria_arm64_gcc`には同種の記述が一切無いのと対照的）。Task 12でこれを`FMP3_RUN_COMMAND`へそのまま翻訳する。

ただし`target_kernel_impl.c:183-188`が`RPU_RPU_GLBL_CNTL`（`0xFF9A0000`）へ無条件アクセスしており，これもQEMU 11.0.1のソースに実装が見当たらない（`grep`で0件）。`kria_arm64_gcc`のSTGと同型のリスクだが，上流の`runqu`が実在し詳細な調達コメントを持つことから，**このリスクは`kria_arm64_gcc`より低いと判断し**，Task 12で実行して確かめる（先回りしてpristineを改変しない）。

### `rp2350_pico2_gcc`にQEMUは無い（訂正）

タスク依頼の前提「QEMU: musca_b1と同系」は**誤り**。`qemu-system-arm`（8.2.2・11.0.1いずれも）の`-machine help`にRP2350/Picoに対応するマシンが存在しないことを確認済み（QEMU 11.0.1のソースツリーにも`rp2350`/`RP2350`の文字列は0件）。上流自身の`Makefile.target`も`run:`ターゲットで`"Phase D1 はビルドのみ．書込み・実行は D2 で行う．"`とコメントしており，実行検証を意図的に持たない。**本計画でもビルド専用として扱う**（Task 2）。

### コンパイルオプション結合順の問題（DIVERGENCE_MAP.md記載）は3ターゲットいずれも不要

`rp2350_pico2_gcc`・`kria_arm64_gcc`・`kria_r5_gcc`のいずれの`Makefile.chip`/`Makefile.target`も独自の`-O`オプションを持たないことを確認済み（`-mcpu`/`-mfpu`/`-mfloat-abi`等はあるが最適化レベル指定は無い）。**この計画では対応不要**、記録のまま据え置く。

### `--gc-sections`は3ターゲットいずれも不要

`rp2350_pico2_gcc`・`kria_arm64_gcc`・`kria_r5_gcc`のいずれの上流Makefileにも`--gc-sections`/`-ffunction-sections`/`-fdata-sections`は無い（`grep`で確認済み）。`FMP3_CFG1_OUT_LINK_OPTIONS`は設定不要（musca_b1と同じ扱い）。ただしTask 2/6/11の検証で`cfg1_out.syms`に`TOPPERS_magic_number`が実際に残ることを確認する。

### クラス別セクション回収は3ターゲットで2種類

- `rp2350_pico2_gcc`：`.ld`に`.kernel_data_*`/`.stack*`の明示回収なし（`grep`で確認済み）＝ARM-M型（`SecnameKernelData`/`SecnameStack`が常に`""`を返す。musca_b1と同じ）。
- `kria_arm64_gcc`・`kria_r5_gcc`：`.ld`（`kria.ld:81-82`／`kria_r5.ld:63-64`）が`*(.kernel_data_*)`/`*(.stack*)`を明示回収（設計書§3の「kria型」）。

---

## Task一覧

1. `rp2350_pico2_gcc`：cfgテンプレート移植とCMake層
2. `rp2350_pico2_gcc`：ビルド・差分等価性検査・回帰（ビルド専用）
3. 汎用層：ROMイメージ形式フック（`FMP3_DUMP_FORMAT`）の追加
4. `kria_arm64_gcc`：core/chip層cfgテンプレート移植
5. `kria_arm64_gcc`：target層cfgテンプレート移植
6. `kria_arm64_gcc`：CMake層とツールチェーン
7. `kria_arm64_gcc`：ビルド・差分等価性検査・QEMU実行検証（STG初期化の実地判定を含む）
8. `kria_r5_gcc`：core/chip層cfgテンプレート移植
9. `kria_r5_gcc`：target層cfgテンプレート移植
10. `kria_r5_gcc`：CMake層
11. `kria_r5_gcc`：ビルド・差分等価性検査
12. `kria_r5_gcc`：QEMU実行検証（上流`runqu`の翻訳）
13. 最終回帰・`DIVERGENCE_MAP.md`更新・完了条件チェック

---

### Task 1: `rp2350_pico2_gcc` — cfgテンプレート移植とCMake層

**Files:**
- Create: `arch/arm_m_gcc/rp2350/chip_kernel.py`
- Create: `target/rp2350_pico2_gcc/target_kernel.py`
- Create: `target/rp2350_pico2_gcc/target_class.py`
- Create: `target/rp2350_pico2_gcc/target_check.py`
- Create: `arch/arm_m_gcc/rp2350/chip.cmake`
- Create: `target/rp2350_pico2_gcc/target.cmake`
- Create: `target/rp2350_pico2_gcc/presets.json`
- Modify: `CMakePresets.json`

**Interfaces:**
- Consumes: `arch/arm_m_gcc/common/{core_kernel,core_check,core_offset}.py`（計画Bで移植済み，無改造で流用）、`cmake/presets-base.json`、`cmake/toolchain-arm-none-eabi.cmake`（いずれも既存）。
- Produces: `FMP3_TARGET=rp2350_pico2_gcc`でconfigureできるプリセット`rp2350_pico2`。以降のタスクはこれを消費しない（本計画最後のTask 13でのみ回帰対象として再利用）。

- [ ] **Step 1: `arch/arm_m_gcc/rp2350/chip_kernel.py`を作成する**

原文`arch/arm_m_gcc/rp2350/chip_kernel.trb`（31行）の翻訳。`GenerateNativeSpn`をコア依存部より先に定義してデフォルト実装を抑止する点が要（`arch/arm_m_gcc/common/core_kernel.py:54-61`の`generate_native_spn_defined`ガードが対応）。

```python
# -*- coding: utf-8 -*-
#
#		パス2の生成スクリプトのチップ依存部（RP2350用）
#
#  $Id: chip_kernel.py (converted from chip_kernel.trb by Claude Code Sonnet 5) $
#
#  ネイティブスピンロックは RP2350 の SIO ハードウェアスピンロック
#  （SIO_SPINLOCKn）に割り当てる．chip_sil.h で SIL スピンロック=15，
#  ジャイアントロック=30 を予約しているため，ネイティブスピンロックは
#  0 から順に割り当てる（最大 TMAX_NATIVE_SPN=14 個）．
#

#
#  ネイティブスピンロックの生成（コア依存部より先に定義し，そちらの既定実装を抑止する）
#
#  spninib の lock メンバに SIO_SPINLOCKn のアドレスを格納する．
#
rp2350_spinlock_index = 0

#  コア依存部に既定実装を定義させない（core_kernel.py 参照）
generate_native_spn_defined = True


def GenerateNativeSpn(params):
    global rp2350_spinlock_index
    ret = f"(intptr_t)RP2350_SIO_SPINLOCKn({rp2350_spinlock_index})"
    rp2350_spinlock_index += 1
    return ret


#
#  コア依存テンプレートのインクルード
#
IncludeTrb("core_kernel.py")
```

- [ ] **Step 2: `target/rp2350_pico2_gcc/{target_kernel,target_class,target_check}.py`を作成する**

`target_kernel.trb`/`target_class.trb`/`target_check.trb`は`target/musca_b1_gcc/`の同名`.trb`と**コメント（ターゲット名・`$Id`）以外バイト同一**（本タスク着手前に`diff`で確認済み）。既に移植済みの`target/musca_b1_gcc/{target_kernel,target_class,target_check}.py`をコピーし，ヘッダコメントの3行目のみ書き換える。

```bash
cd /home/honda/TOPPERS/FMP3/fmp3_core
cp target/musca_b1_gcc/target_kernel.py target/rp2350_pico2_gcc/target_kernel.py
cp target/musca_b1_gcc/target_class.py  target/rp2350_pico2_gcc/target_class.py
cp target/musca_b1_gcc/target_check.py  target/rp2350_pico2_gcc/target_check.py
sed -i 's/ターゲット依存部（ARM Musca-B1用）/ターゲット依存部（RP2350 \/ RaspberryPi Pico 2用）/' \
    target/rp2350_pico2_gcc/target_kernel.py
sed -i 's/ターゲット依存のクラス定義（ARM Musca-B1用）/ターゲット依存のクラス定義（RP2350 \/ RaspberryPi Pico 2用）/' \
    target/rp2350_pico2_gcc/target_class.py
```

`target_check.py`はターゲット名を含まない汎用コメント（`チェックパスの生成スクリプトのターゲット依存部`のみ）なので書き換え不要。

- [ ] **Step 3: 構文チェック**

```bash
cd /home/honda/TOPPERS/FMP3/fmp3_core
for f in arch/arm_m_gcc/rp2350/chip_kernel.py \
         target/rp2350_pico2_gcc/target_kernel.py \
         target/rp2350_pico2_gcc/target_class.py \
         target/rp2350_pico2_gcc/target_check.py; do
    python3 -m py_compile "$f" && echo "OK $f" || echo "FAIL $f"
done
```
Expected: 4個すべて`OK`。

- [ ] **Step 4: negative control — コピーが実際にmusca_b1と中身が異なることを確認する**

コピーしただけで満足しない（バイト同一のまま放置していないか）。

```bash
cd /home/honda/TOPPERS/FMP3/fmp3_core
diff target/musca_b1_gcc/target_kernel.py target/rp2350_pico2_gcc/target_kernel.py
echo "diff exit=$?"
```
Expected: exit=1（コメント行の差分が出る＝実際に書き換わっている）。exit=0だったらStep 2のsedが効いていない。

- [ ] **Step 5: `arch/arm_m_gcc/rp2350/chip.cmake`を作成する**

`arch/arm_m_gcc/rp2350/Makefile.chip`のCMake版。`arch/arm_m_gcc/musca_b1/chip.cmake`と同型（`COREDIR`/`ARCHDIR`/`CHIPDIR`/`TARGETDIR`の書き分け規約は呼び出し元`target.cmake`が設定する）。

```cmake
#
#		チップ依存部の CMake 定義（RP2350 / Cortex-M33 用）
#
#  target.cmake から include される（上流 Makefile.chip に相当）．
#
set(COREDIR ${ARCHDIR}/common)

#
#  コンパイルオプション（Makefile.chip:16-18）
#
#  Cortex-M33（ARMv8-M Mainline）．RP2350 の M33 は FPU を搭載するが，
#  Phase D1 ではソフトウェア浮動小数点 ABI を用いる（musca_b1 と同様）．
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
#  ARM アーキテクチャの世代（Makefile.chip:20-26）
#
list(APPEND FMP3_COMPILE_DEFS __TARGET_ARCH_THUMB=5)

#
#  TrustZone 対応コアでのセキュア単独動作（Makefile.chip:28-38）
#
#  RP2350 の bootROM は ARM イメージをセキュア状態で起動する．
#
list(APPEND FMP3_COMPILE_DEFS TOPPERS_ENABLE_TRUSTZONE)

list(APPEND FMP3_INCLUDE_DIRS ${CHIPDIR})

#
#  カーネルに含めるチップ依存ソース（Makefile.chip:44-45）
#
list(APPEND FMP3_ARCH_C_FILES
    ${CHIPDIR}/chip_kernel_impl.c
    ${CHIPDIR}/chip_ipi.c
)

#
#  非TECS版 SIO ドライバ（Makefile.chip:48-49）
#
list(APPEND FMP3_SYSSVC_TARGET_C_FILES
    ${CHIPDIR}/chip_serial.c
)

#
#  コア依存部（Makefile.chip:64）
#
include(${COREDIR}/arch.cmake)
```

- [ ] **Step 6: `target/rp2350_pico2_gcc/target.cmake`を作成する**

`Makefile.target`のCMake版。musca_b1の`target.cmake`と同じ書き分け規約。

```cmake
#
#		ターゲット依存部の CMake 定義（RaspberryPi Pico 2 / RP2350用）
#
#  上流 target/rp2350_pico2_gcc/Makefile.target の CMake 版．
#
set(ARCHDIR ${FMP3_ROOT_DIR}/arch/arm_m_gcc)
get_filename_component(CHIPDIR ${CMAKE_CURRENT_LIST_DIR}/../../arch/arm_m_gcc/rp2350 ABSOLUTE)
set(TARGETDIR ${CMAKE_CURRENT_LIST_DIR})

#  Makefile.target:20-26
list(APPEND FMP3_COMPILE_DEFS TOPPERS_OMIT_TECS)

#  Makefile.target:30-31
list(APPEND FMP3_INCLUDE_DIRS ${TARGETDIR})
list(APPEND FMP3_COMPILE_OPTIONS -mlittle-endian)
list(APPEND FMP3_LINK_OPTIONS -mlittle-endian)

#
#  カーネルに含めるターゲット依存ソース（Makefile.target:34-36）
#
list(APPEND FMP3_TARGET_C_FILES
    ${TARGETDIR}/target_kernel_impl.c
    ${TARGETDIR}/target_timer.c
)
list(APPEND FMP3_ARCH_C_FILES
    ${TARGETDIR}/image_def.S
)

#
#  リンカスクリプトの定義（Makefile.target:39）
#
set(FMP3_LDSCRIPT ${TARGETDIR}/rp2350_pico2.ld)

#
#  cfg に渡すファイル
#
list(APPEND FMP3_CFG_FILES            ${TARGETDIR}/target_kernel.cfg)
list(APPEND FMP3_CLASS_TRB_FILES      ${TARGETDIR}/target_class.py)
list(APPEND FMP3_KERNEL_CFG_TRB_FILES ${TARGETDIR}/target_kernel.py)
list(APPEND FMP3_CHECK_TRB_FILES      ${TARGETDIR}/target_check.py)

#
#  チップ依存部（Makefile.target:43）
#
include(${CHIPDIR}/chip.cmake)

#
#  ★実行手段は無い（本ターゲットは Phase D1＝ビルドのみ．上流
#    Makefile.target 自身の run: ターゲットが "書込み・実行は D2 で行う"
#    とコメントしている．QEMU に RP2350/Pico 相当のマシンは存在しない
#    ことを実測で確認済み：qemu-system-arm 8.2.2 / 11.0.1 いずれの
#    `-machine help` にも rp2350/pico の記載なし，QEMU 11.0.1 の
#    ソースツリーに rp2350/RP2350 の文字列が0件）．
#    FMP3_RUN_COMMAND をここでは定義しない → CMakeLists.txt の
#    `run` ターゲット自体が生成されない（意図的）．
#
```

- [ ] **Step 7: `target/rp2350_pico2_gcc/presets.json`を作成する**

```json
{
  "version": 4,
  "include": [
    "../../cmake/presets-base.json"
  ],
  "configurePresets": [
    {
      "name": "rp2350_pico2",
      "inherits": "_base",
      "displayName": "RaspberryPi Pico 2 (RP2350, dual Cortex-M33, build only)",
      "description": "Phase D1: ビルドのみ（実機書込み・実行は未対応。QEMUにもRP2350相当のマシンは存在しない）",
      "toolchainFile": "${sourceDir}/cmake/toolchain-arm-none-eabi.cmake",
      "cacheVariables": {
        "FMP3_TARGET": "rp2350_pico2_gcc"
      }
    }
  ],
  "buildPresets": [
    { "name": "rp2350_pico2", "configurePreset": "rp2350_pico2" }
  ]
}
```

- [ ] **Step 8: `CMakePresets.json`に追加する**

```json
  "include": [
    "target/polarfire_soc_kit_gcc/presets.json",
    "target/musca_b1_gcc/presets.json",
    "target/rp2350_pico2_gcc/presets.json"
  ]
```

- [ ] **Step 9: コミット**

```bash
cd /home/honda/TOPPERS/FMP3/fmp3_core
git add arch/arm_m_gcc/rp2350/chip_kernel.py arch/arm_m_gcc/rp2350/chip.cmake \
        target/rp2350_pico2_gcc/target_kernel.py target/rp2350_pico2_gcc/target_class.py \
        target/rp2350_pico2_gcc/target_check.py target/rp2350_pico2_gcc/target.cmake \
        target/rp2350_pico2_gcc/presets.json CMakePresets.json
git commit -m "build(cfg): rp2350_pico2_gcc のcfgテンプレートとCMake層を追加

target_kernel/target_class/target_checkはmusca_b1と.trb原文が
コメント以外バイト同一のため流用。chip_kernel.pyのみ新規（RP2350の
SIOスピンロック割当）。実行手段は無い（QEMUにRP2350相当のマシンが
存在しない。上流Makefile.target自身もPhase D1=ビルドのみと明記）。"
```

---

### Task 2: `rp2350_pico2_gcc` — ビルド・差分等価性検査・回帰（ビルド専用）

**Files:**
- No new files（検証のみ）

**Interfaces:**
- Consumes: Task 1の成果物一式。

- [ ] **Step 1: configure + build**

```bash
cd /home/honda/TOPPERS/FMP3/fmp3_core
cmake --preset rp2350_pico2
cmake --build build/rp2350_pico2 2>&1 | tail -40
echo "build exit=$?"
```
Expected: exit=0。`build/rp2350_pico2/fmp`が生成される。

- [ ] **Step 2: positive control — `TOPPERS_magic_number`が`cfg1_out.syms`に残っていることを確認**

```bash
grep -c TOPPERS_magic_number build/rp2350_pico2/generated/cfg1_out.syms
```
Expected: 1（`--gc-sections`を使わないため`FMP3_CFG1_OUT_LINK_OPTIONS`無しでも残る）。

- [ ] **Step 3: negative control — `--gc-sections`を一時的に足すと消えることを確認（汎用層のガードが機能する側の確認）**

```bash
cd /home/honda/TOPPERS/FMP3/fmp3_core
cmake --preset rp2350_pico2 -B build/rp2350_pico2-gcneg \
    -DCMAKE_EXE_LINKER_FLAGS="--gc-sections -ffunction-sections -fdata-sections"
cmake --build build/rp2350_pico2-gcneg 2>&1 | tail -30
echo "build exit=$?"
```
Expected: `cmake/check_magic_number.cmake`のガードが働き**ビルドが失敗する**（FATAL_ERROR）。失敗したら`rm -rf build/rp2350_pico2-gcneg`で片付ける。

- [ ] **Step 4: `tools/cfg_equivalence.sh`（Rubyとの差分等価性検査）**

```bash
cd /home/honda/TOPPERS/FMP3/fmp3_core
tools/cfg_equivalence.sh build/rp2350_pico2
echo "cfg_equivalence exit=$?"
```
Expected: exit=0（`[OK] MATCH`）。

- [ ] **Step 5: negative control — テンプレートを1箇所壊して差分が検出されることを確認**

```bash
cd /home/honda/TOPPERS/FMP3/fmp3_core
cp arch/arm_m_gcc/rp2350/chip_kernel.py /tmp/chip_kernel.py.bak
sed -i 's/rp2350_spinlock_index = 0/rp2350_spinlock_index = 99/' arch/arm_m_gcc/rp2350/chip_kernel.py
rm -rf build/rp2350_pico2-neg && cmake --preset rp2350_pico2 -B build/rp2350_pico2-neg
tools/cfg_equivalence.sh build/rp2350_pico2-neg
echo "negative cfg_equivalence exit=$? (expect 1)"
cp /tmp/chip_kernel.py.bak arch/arm_m_gcc/rp2350/chip_kernel.py
rm -rf build/rp2350_pico2-neg /tmp/chip_kernel.py.bak
```
Expected: exit=1（差分検出）。壊した状態のまま終わらないこと（コミット前に必ず元へ戻す）。

- [ ] **Step 6: `tools/cfg_error_tests/run.sh`の回帰**

```bash
cd /home/honda/TOPPERS/FMP3/fmp3_core
tools/cfg_error_tests/run.sh build/rp2350_pico2
echo "cfg_error_tests exit=$?"
```
Expected: exit=0（既存3構成と同じ挙動。rp2350はmusca_b1と同じarm_m_gcc経路なので新規のエラーパターンは生じない）。

- [ ] **Step 7: 既存3構成の回帰（polarfire・musca_b1 1コア/2コア）**

```bash
cd /home/honda/TOPPERS/FMP3/fmp3_core
for p in polarfire_soc_kit-qemu musca_b1 musca_b1-2core; do
    cmake --build build/$p 2>&1 | tail -5
    tools/cfg_equivalence.sh build/$p
    echo "$p cfg_equivalence exit=$?"
done
```
Expected: 3構成とも exit=0（本タスクは汎用層を一切触っていないため無変化）。

- [ ] **Step 8: コミット（検証記録のみ。コード変更が無ければコミット不要）**

Task 1のコミットに検証結果を紐づけるため，`.superpowers/sdd/progress.md`へ実測結果を追記する場合はここで。コード変更が無ければ本Stepはスキップしてよい。

---

### Task 3: 汎用層 — ROMイメージ形式フック（`FMP3_DUMP_FORMAT`）の追加

**Files:**
- Create: `cmake/objdump_to_file.cmake`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Consumes: `cmake/nm_to_file.cmake`（同じ「`>`を使わない」idiomを踏襲）。
- Produces: `FMP3_DUMP_FORMAT`（既定`srec`）と`FMP3_DUMPOPTS`（既定空文字列）という，ターゲットが宣言できる新しい変数。Task 6の`kria_arm64_gcc/target.cmake`がこれを`dump`/`"-j .text -j .rodata"`に設定して消費する。

**設計（設計書横展開チェックリスト・DIVERGENCE_MAP.md記載の欠落箇所への対応）:**

上流`arch/arm64_gcc/common/Makefile.core:34`の`DUMP = dump`に相当する変数を汎用層に持ち込む。`FMP3_LDSCRIPT_VIA_DRIVER_T`と同じ「ターゲットが宣言する」作法（`if(NOT DEFINED ...)`で既定を与え，`include(target.cmake)`より前に配置し，target.cmake側の`set()`で上書きされる）に揃える。

- [ ] **Step 1: `CMakeLists.txt`に`FMP3_DUMP_FORMAT`の既定値ブロックを追加する**

`FMP3_LDSCRIPT_VIA_DRIVER_T`のブロック（`if(NOT DEFINED FMP3_LDSCRIPT_VIA_DRIVER_T) ... endif()`、`include(${FMP3_TARGET_DIR}/target.cmake)`の直前）のすぐ後ろに追加する：

```cmake
#
#  cfg1_out / 最終ELF から作る ROM イメージの形式（cfg の --rom-image が
#  読む拡張子）．
#
#  上流は arch/arm64_gcc/common/Makefile.core:34 で DUMP = dump と再定義
#  する（他アーキでは sample/Makefile:133-134 の ifndef DUMP: DUMP = srec
#  が既定）．cfg_py/cfg.py:700-705 は拡張子で srec/dump を自動判別済み
#  なので，ここで欠けているのは objcopy -O srec と objdump -s の分岐
#  だけである．
#
#  «未定義のときだけ» 既定を与える（FMP3_LDSCRIPT_VIA_DRIVER_T と同じ
#  理由。target.cmake がこの後 include されて "dump" を set() する）．
#
if(NOT DEFINED FMP3_DUMP_FORMAT)
    set(FMP3_DUMP_FORMAT srec)
endif()
if(NOT DEFINED FMP3_DUMPOPTS)
    set(FMP3_DUMPOPTS "")
endif()
```

- [ ] **Step 2: `cmake/objdump_to_file.cmake`を新規作成する**

`cmake/nm_to_file.cmake`と同じ理由（`>`リダイレクトのシェル依存回避）。

```cmake
#
#  objdump -s <opts> <elf> の出力をファイルに書く（">" を使わないため，
#  nm_to_file.cmake と同じ理由）．
#
#  使い方:
#    cmake -DOBJDUMP=<objdump> -DELF=<elf> -DDUMPOPTS="-j .text -j .rodata"
#          -DOUT=<file> -P objdump_to_file.cmake
#
#  DUMPOPTS はスペース区切りの1本の文字列として渡す（CMakeのリストは
#  ";"区切りでコマンドライン上のクォートが煩雑になるため）．
#
separate_arguments(_fmp3_dumpopts_list UNIX_COMMAND "${DUMPOPTS}")
execute_process(
    COMMAND ${OBJDUMP} -s ${_fmp3_dumpopts_list} ${ELF}
    OUTPUT_VARIABLE _dump
    RESULT_VARIABLE _rc
    ERROR_VARIABLE  _err
)
if(NOT _rc EQUAL 0)
    message(FATAL_ERROR "objdump failed (${_rc}): ${_err}")
endif()
file(WRITE ${OUT} "${_dump}")
```

- [ ] **Step 3: `CMakeLists.txt`の`CFG1_OUT_SREC_FILE`を`CFG1_OUT_ROM_IMAGE_FILE`へ改名し，形式で分岐させる**

変数名`CFG1_OUT_SREC_FILE`は「srec決め打ち」を示唆する誤解を招く名前になるため改名する（影響範囲: 定義1箇所＋使用2箇所）。

`set(CFG1_OUT_SREC_FILE ${CFG_GEN_DIR}/cfg1_out.srec)` を：

```cmake
set(CFG1_OUT_ROM_IMAGE_FILE ${CFG_GEN_DIR}/cfg1_out.${FMP3_DUMP_FORMAT})
```

に変更する。

`add_custom_command(OUTPUT ${CFG1_OUT_SREC_FILE} COMMAND ${CMAKE_OBJCOPY} -O srec -S $<TARGET_FILE:cfg1_out> ${CFG1_OUT_SREC_FILE} DEPENDS cfg1_out COMMENT "Generating cfg1_out.srec")` を：

```cmake
if(FMP3_DUMP_FORMAT STREQUAL "dump")
    add_custom_command(
        OUTPUT ${CFG1_OUT_ROM_IMAGE_FILE}
        COMMAND ${CMAKE_COMMAND}
                -DOBJDUMP=${CMAKE_OBJDUMP}
                -DELF=$<TARGET_FILE:cfg1_out>
                "-DDUMPOPTS=${FMP3_DUMPOPTS}"
                -DOUT=${CFG1_OUT_ROM_IMAGE_FILE}
                -P ${FMP3_ROOT_DIR}/cmake/objdump_to_file.cmake
        DEPENDS cfg1_out ${FMP3_ROOT_DIR}/cmake/objdump_to_file.cmake
        COMMENT "Generating cfg1_out.dump"
    )
else()
    add_custom_command(
        OUTPUT ${CFG1_OUT_ROM_IMAGE_FILE}
        COMMAND ${CMAKE_OBJCOPY} -O srec -S $<TARGET_FILE:cfg1_out> ${CFG1_OUT_ROM_IMAGE_FILE}
        DEPENDS cfg1_out
        COMMENT "Generating cfg1_out.srec"
    )
endif()
```

`add_custom_target(cfg1_out_srec DEPENDS ${CFG1_OUT_SREC_FILE})` の参照も`${CFG1_OUT_ROM_IMAGE_FILE}`へ変更する。

- [ ] **Step 4: offset.h生成の`--rom-image`を`${CFG1_OUT_ROM_IMAGE_FILE}`へ変更する**

```cmake
COMMAND ${CFG_COMMAND} --pass 2 -O --kernel fmp
        ${CFG_INCLUDE_DIRS} ${CFG_CLASS_TRB_FILES} ${CFG_OFFSET_TRB_FILES}
        --rom-symbol ${CFG1_OUT_SYMS_FILE} --rom-image ${CFG1_OUT_ROM_IMAGE_FILE}
DEPENDS ${CFG_SCRIPT_DEPS} ${FMP3_CLASS_TRB_FILES} ${FMP3_OFFSET_TRB_FILES}
        ${CFG1_OUT_SYMS_FILE} ${CFG1_OUT_ROM_IMAGE_FILE} ${CFG1_OUT_TIMESTAMP}
        ${FMP3_OFFSET_TRB_CLOSURE}
```
（`DEPENDS`側の`${CFG1_OUT_SREC_FILE}`も改名する。）

- [ ] **Step 5: `fmp3_cfg_check()`関数内を形式分岐させる**

```cmake
COMMAND ${CMAKE_OBJCOPY} -O srec -S $<TARGET_FILE:${TARGET}>
        ${CFG_GEN_DIR}/${TARGET}.srec
COMMAND ${PASS3_ARGS}
        --rom-symbol ${CFG_GEN_DIR}/${TARGET}.syms
        --rom-image  ${CFG_GEN_DIR}/${TARGET}.srec
```

を，`add_custom_command(TARGET ${TARGET} POST_BUILD ...)`呼び出しの直前に以下を挿入したうえで置き換える：

```cmake
    if(FMP3_DUMP_FORMAT STREQUAL "dump")
        set(_fmp3_check_image ${CFG_GEN_DIR}/${TARGET}.dump)
        set(_fmp3_check_image_cmd
            ${CMAKE_COMMAND} -DOBJDUMP=${CMAKE_OBJDUMP} -DELF=$<TARGET_FILE:${TARGET}>
            "-DDUMPOPTS=${FMP3_DUMPOPTS}" -DOUT=${_fmp3_check_image}
            -P ${FMP3_ROOT_DIR}/cmake/objdump_to_file.cmake)
    else()
        set(_fmp3_check_image ${CFG_GEN_DIR}/${TARGET}.srec)
        set(_fmp3_check_image_cmd
            ${CMAKE_OBJCOPY} -O srec -S $<TARGET_FILE:${TARGET}> ${_fmp3_check_image})
    endif()
    add_custom_command(TARGET ${TARGET} POST_BUILD
        WORKING_DIRECTORY ${CFG_GEN_DIR}
        COMMAND ${CMAKE_COMMAND}
                -DNM=${CMAKE_NM}
                -DELF=$<TARGET_FILE:${TARGET}>
                -DOUT=${CFG_GEN_DIR}/${TARGET}.syms
                -P ${FMP3_ROOT_DIR}/cmake/nm_to_file.cmake
        COMMAND ${_fmp3_check_image_cmd}
        COMMAND ${PASS3_ARGS}
                --rom-symbol ${CFG_GEN_DIR}/${TARGET}.syms
                --rom-image  ${_fmp3_check_image}
        COMMENT "Running cfg pass 3 to check configuration"
    )
```

- [ ] **Step 6: 全既存3構成が無変化のままビルド・差分等価性検査が通ることを確認する（positive control：srec経路の無回帰）**

```bash
cd /home/honda/TOPPERS/FMP3/fmp3_core
for p in polarfire_soc_kit-qemu musca_b1 musca_b1-2core; do
    rm -rf build/$p && cmake --preset $p && cmake --build build/$p 2>&1 | tail -10
    ls -la build/$p/generated/cfg1_out.srec
    tools/cfg_equivalence.sh build/$p
    echo "$p cfg_equivalence exit=$?"
done
```
Expected: 3構成とも`cfg1_out.srec`が生成され（`.dump`ではない＝分岐のelse側が動いている），`cfg_equivalence.sh`はexit=0。`ls`が失敗したら変数の改名漏れ。

- [ ] **Step 7: negative control — `FMP3_DUMP_FORMAT=dump`を一時的に強制し，musca_b1で`.dump`が生成されonly srecでは無いことを確認する**

（本物のkria_arm64を待たずに，分岐が実際に生きていることをこの時点で検証する。）

```bash
cd /home/honda/TOPPERS/FMP3/fmp3_core
cmake --preset musca_b1 -B build/musca_b1-dumpcheck -DFMP3_DUMP_FORMAT=dump -DFMP3_DUMPOPTS="-j .text -j .rodata"
cmake --build build/musca_b1-dumpcheck --target cfg1_out 2>&1 | tail -20
ls build/musca_b1-dumpcheck/generated/cfg1_out.dump
echo "dump file exists: $?"
! test -f build/musca_b1-dumpcheck/generated/cfg1_out.srec
echo "srec file absent: $? (expect 0)"
rm -rf build/musca_b1-dumpcheck
```
Expected: `cfg1_out.dump`が存在し，`cfg1_out.srec`は存在しない（同じconfigureで両方生成されると分岐が効いていないか，DEPENDSの残骸を見ている可能性がある）。

★このnegative controlはmusca_b1に対して`objdump`分岐だけを試すものであり，musca_b1の製品ビルド自体をdump形式に変更するものではない（`-B`別ディレクトリを使い捨てる）。

- [ ] **Step 8: `tools/cfg_error_tests/run.sh`の回帰（3構成）**

```bash
cd /home/honda/TOPPERS/FMP3/fmp3_core
for p in polarfire_soc_kit-qemu musca_b1 musca_b1-2core; do
    tools/cfg_error_tests/run.sh build/$p
    echo "$p cfg_error_tests exit=$?"
done
```
Expected: 3構成ともexit=0（無変化）。

- [ ] **Step 9: コミット**

```bash
cd /home/honda/TOPPERS/FMP3/fmp3_core
git add CMakeLists.txt cmake/objdump_to_file.cmake
git commit -m "build(cmake): ROMイメージ形式(srec/dump)を汎用層でフック可能にする

上流はarch/arm64_gcc/common/Makefile.core:34でDUMP=dumpに再定義するが
CMake層はobjcopy -O srec / .srec拡張子を決め打ちしていた
（DIVERGENCE_MAP.md記載の既知の欠落）。FMP3_DUMP_FORMAT/FMP3_DUMPOPTS
をターゲットが宣言する形で追加。cfg_py/cfg.py側は拡張子判別済みのため
CMake層のフックのみで足りる。既存3構成はsrec経路のまま無回帰。
kria_arm64_gccがdump形式を使う（Task 6）。"
```

---

### Task 4: `kria_arm64_gcc` — core/chip層cfgテンプレート移植

**Files:**
- Create: `arch/arm64_gcc/common/core_kernel.py`
- Create: `arch/arm64_gcc/common/core_check.py`
- Create: `arch/arm64_gcc/common/core_offset.py`
- Create: `arch/arm64_gcc/common/gic_kernel.py`
- Create: `arch/arm64_gcc/zynqmp/chip_kernel.py`

**Interfaces:**
- Consumes: `kernel/kernel.py`・`kernel/kernel_check.py`・`kernel/genoffset.py`（既存，計画Bで移植済み）。翻訳元は本リポジトリのpristine `arch/arm64_gcc/{common,zynqmp}/*.trb`（現物，全文確認済み）。`EXCNO_CUR_SP0_SYNC`等16個の定数と`TCLS_NONE`は`arch/arm64_gcc/common/core_sym.def`のsymval-table経由で自動的にグローバルへ入る（現物確認済み。本タスクでの定義は不要）。
- Produces: `IncludeTrb("core_kernel.py")`／`IncludeTrb("core_check.py")`／`IncludeTrb("gic_kernel.py")`（Task 5の`target/kria_arm64_gcc/*.py`がConsumes）。

**翻訳仕様:**

- [ ] **Step 1: `arch/arm64_gcc/common/core_kernel.py`を作成する**（原文`core_kernel.trb`全223行）

`arch/riscv_gcc/common/core_kernel.py`（既存）と**同一構造**（`DefineVariableSection`/`SecnameKernelData`/`SecnameStack`/`GenerateNativeSpn`は文字通り同一。割込み/CPU例外テーブル生成のループ構造も同一）。差分は`EXCNO_VALID`/`EXCNO_DEFEXC_VALID`の初期化に使う例外番号リストが，riscv版の`range(16)`ではなく`core_sym.def`由来の16個の名前付き定数である点のみ。

```python
# -*- coding: utf-8 -*-
#
#		パス2の生成スクリプトのコア依存部（ARM64用）
#
#  $Id: core_kernel.py (converted from core_kernel.trb by Claude Code Sonnet 5) $
#

#
#  割込み要求ライン設定テーブルを使うかどうか（未設定ならFalse）
#
#  riscv_gcc/common/core_kernel.py と同じガード．chip_kernel.py／
#  target_kernel.py 等が設定する想定．Ruby版では未代入の $グローバル変数
#  参照はnil（偽）として扱われるが，Pythonでは未定義グローバル参照は
#  NameErrorになるためここでガードする．
#
if "USE_INTCFG_TABLE" not in globals():
    USE_INTCFG_TABLE = False

#
#  有効なCPU例外ハンドラ番号
#       EXCNO_CUR_SPX_SYNC(4) と EXCNO_CUR_SPX_SERR(7) のみ有効だが、
#       テーブル引きを行うため無効な番号も記載している
#
EXCNO_VALID = {}
excno_list = [
    EXCNO_CUR_SP0_SYNC, EXCNO_CUR_SP0_IRQ, EXCNO_CUR_SP0_FIQ, EXCNO_CUR_SP0_SERR,
    EXCNO_CUR_SPX_SYNC, EXCNO_CUR_SPX_IRQ, EXCNO_CUR_SPX_FIQ, EXCNO_CUR_SPX_SERR,
    EXCNO_L64_SYNC, EXCNO_L64_IRQ, EXCNO_L64_FIQ, EXCNO_L64_SERR,
    EXCNO_L32_SYNC, EXCNO_L32_IRQ, EXCNO_L32_FIQ, EXCNO_L32_SERR,
]
for prcid in range(1, TNUM_PRCID + 1):
    EXCNO_VALID[prcid] = []
    for excno in excno_list:
        EXCNO_VALID[prcid].append((prcid << 16) | excno)

#
#  DEF_EXCで使用できるCPU例外ハンドラ番号
#       EXCNO_CUR_SPX_SYNC(4) と EXCNO_CUR_SPX_SERR(7) のみ有効だが、
#       テーブル引きを行うため無効な番号も記載している
#
EXCNO_DEFEXC_VALID = {}
excno_list = [
    EXCNO_CUR_SP0_SYNC, EXCNO_CUR_SP0_IRQ, EXCNO_CUR_SP0_FIQ, EXCNO_CUR_SP0_SERR,
    EXCNO_CUR_SPX_SYNC, EXCNO_CUR_SPX_IRQ, EXCNO_CUR_SPX_FIQ, EXCNO_CUR_SPX_SERR,
    EXCNO_L64_SYNC, EXCNO_L64_IRQ, EXCNO_L64_FIQ, EXCNO_L64_SERR,
    EXCNO_L32_SYNC, EXCNO_L32_IRQ, EXCNO_L32_FIQ, EXCNO_L32_SERR,
]
for prcid in range(1, TNUM_PRCID + 1):
    EXCNO_DEFEXC_VALID[prcid] = []
    for excno in excno_list:
        EXCNO_DEFEXC_VALID[prcid].append((prcid << 16) | excno)


#
#  配置するセクションを指定した変数定義の生成
#
def DefineVariableSection(genFile, defvar, secname):
    if secname != "":
        genFile.add(f'{defvar} __attribute__((section("{secname}"),nocommon));')
    else:
        genFile.add(f"{defvar};")


#
#  カーネルのデータ領域のセクション名
#
def SecnameKernelData(cls):
    if cls != TCLS_NONE:
        return f".kernel_data_{clsData[cls]['clsid']}"
    else:
        return ""


#
#  スタック領域のセクション名
#
def SecnameStack(cls):
    if cls != TCLS_NONE:
        return f".stack_{clsData[cls]['clsid']}"
    else:
        return ""


#
#  ネイティブスピンロックの生成
#
def GenerateNativeSpn(params):
    kernelCfgC.add(f"LOCK _kernel_lock_{params['spnid']};")
    return f"((intptr_t) &_kernel_lock_{params['spnid']})"


#
#  ターゲット非依存部のインクルード
#
IncludeTrb("kernel/kernel.py")

#
#  割込みハンドラテーブル
#
kernelCfgC.comment_header("Interrupt Handler Table")

for prcid in range(1, TNUM_PRCID + 1):
    kernelCfgC.add(
        f"const FP _kernel_inh_table_prc{prcid}"
        f"[{len(INHNO_VALID[prcid])}] = {{")
    for index, inhnoVal in enumerate(INHNO_VALID[prcid]):
        if index > 0:
            kernelCfgC.add(",")
        kernelCfgC.append(f"\t/* 0x{inhnoVal:05x} */ ")
        if inhnoVal in cfgData["DEF_INH"]:
            kernelCfgC.append(
                f"(FP)({cfgData['DEF_INH'][inhnoVal]['inthdr']})")
            cfgData["DEF_INH"][inhnoVal]["index"] = index
        else:
            kernelCfgC.append("(FP)(_kernel_default_int_handler)")
    kernelCfgC.add()
    kernelCfgC.add2("};")

kernelCfgC.add("const FP* const _kernel_p_inh_table[TNUM_PRCID] = {")
for prcid in range(1, TNUM_PRCID + 1):
    if prcid > 1:
        kernelCfgC.add(",")
    kernelCfgC.append(f"\t_kernel_inh_table_prc{prcid}")
kernelCfgC.add()
kernelCfgC.add2("};")

#
#  割込み要求ライン設定テーブル
#
if USE_INTCFG_TABLE:
    kernelCfgC.comment_header("Interrupt Configuration Table")
    for prcid in range(1, TNUM_PRCID + 1):
        kernelCfgC.add(
            f"const uint8_t _kernel_intcfg_table_prc{prcid}"
            f"[{len(INTNO_VALID[prcid])}] = {{")
        for index, intnoVal in enumerate(INTNO_VALID[prcid]):
            if index > 0:
                kernelCfgC.add(",")
            kernelCfgC.append(f"\t/* 0x{intnoVal:05x} */ ")
            if intnoVal in cfgData["CFG_INT"]:
                kernelCfgC.append("1U")
            else:
                kernelCfgC.append("0U")
        kernelCfgC.add()
        kernelCfgC.add2("};")

kernelCfgC.add("const uint8_t* const _kernel_p_intcfg_table[TNUM_PRCID] = {")
for prcid in range(1, TNUM_PRCID + 1):
    if prcid > 1:
        kernelCfgC.add(",")
    kernelCfgC.append(f"\t_kernel_intcfg_table_prc{prcid}")
kernelCfgC.add()
kernelCfgC.add2("};")

#
#  CPU例外ハンドラテーブル
#
kernelCfgC.comment_header("CPU Exception Handler Table")

for prcid in range(1, TNUM_PRCID + 1):
    kernelCfgC.add(
        f"const FP _kernel_exc_table_prc{prcid}"
        f"[{len(EXCNO_VALID[prcid])}] = {{")
    for index, excnoVal in enumerate(EXCNO_VALID[prcid]):
        if index > 0:
            kernelCfgC.add(",")
        kernelCfgC.append(f"\t/* 0x{excnoVal:05x} */ ")
        if excnoVal in cfgData["DEF_EXC"]:
            kernelCfgC.append(
                f"(FP)({cfgData['DEF_EXC'][excnoVal]['exchdr']})")
            cfgData["DEF_EXC"][excnoVal]["index"] = index
        else:
            kernelCfgC.append("(FP)(_kernel_default_exc_handler)")
    kernelCfgC.add()
    kernelCfgC.add2("};")

kernelCfgC.add("const FP* const _kernel_p_exc_table[TNUM_PRCID] = {")
for prcid in range(1, TNUM_PRCID + 1):
    if prcid > 1:
        kernelCfgC.add(",")
    kernelCfgC.append(f"\t_kernel_exc_table_prc{prcid}")
kernelCfgC.add()
kernelCfgC.add2("};")
```

- [ ] **Step 2: `arch/arm64_gcc/common/core_check.py`を作成する**（原文`core_check.trb`全92行）

**`arch/riscv_gcc/common/core_check.py`（既存）とコメント以外バイト同一の構造**（`.trb`原文をdiffし，コピーライト年・`$Id`・アーキ名以外の差分がないことを確認済み）。中身をそのまま流用し，ヘッダコメントのみ書き換える。

```bash
cd /home/honda/TOPPERS/FMP3/fmp3_core
cp arch/riscv_gcc/common/core_check.py arch/arm64_gcc/common/core_check.py
sed -i 's/コア依存部（RISC-V用）/コア依存部（ARM64用）/' arch/arm64_gcc/common/core_check.py
```

- [ ] **Step 3: `arch/arm64_gcc/common/core_offset.py`を作成する**（原文`core_offset.trb`全36行）

riscv版と異なりフィールド名が違う（`T_EXCINF_pstate`ではなく`T_EXCINF_pstate`、`PCB_dspflg`/`EXC_FRAME_pstate`/`EXC_FRAME`/`PCB_p_locspn`が追加）。riscv版が持っていた「未定義グローバルをnilで補う」ガードは不要（`.trb`原文にその種の変数は無く，全フィールドが常に定義されることを確認済み）。

```python
# -*- coding: utf-8 -*-
#
#		オフセットファイル生成用テンプレートファイル（ARM64用）
#
#  $Id: core_offset.py (converted from core_offset.trb by Claude Code Sonnet 5) $
#

#
#  ターゲット非依存部のインクルード
#
IncludeTrb("kernel/genoffset.py")

#
#  フィールドのオフセットの定義の生成
#
offsetH.append(f"""\
#define TCB_p_tinib\t\t{offsetof_TCB_p_tinib}
#define TCB_sp\t\t\t{offsetof_TCB_sp}
#define TCB_pc\t\t\t{offsetof_TCB_pc}
#define TINIB_exinf\t\t{offsetof_TINIB_exinf}
#define TINIB_task\t\t{offsetof_TINIB_task}
#define TINIB_stksz\t\t{offsetof_TINIB_stksz}
#define TINIB_stk\t\t{offsetof_TINIB_stk}
#define T_EXCINF_pstate\t{offsetof_T_EXCINF_pstate}
#define PCB_p_runtsk\t{offsetof_PCB_p_runtsk}
#define PCB_p_schedtsk\t{offsetof_PCB_p_schedtsk}
#define PCB_excpt_nest_count\t{offsetof_PCB_excpt_nest_count}
#define PCB_istkpt\t\t{offsetof_PCB_istkpt}
#define PCB_idstkpt\t\t{offsetof_PCB_idstkpt}
#define PCB_p_exc_tbl\t{offsetof_PCB_p_exc_tbl}
#define PCB_p_inh_tbl\t{offsetof_PCB_p_inh_tbl}
#define PCB_dspflg\t\t{offsetof_PCB_dspflg}
#define EXC_FRAME_pstate\t{offsetof_EXC_FRAME_pstate}
#define EXC_FRAME\t\t{sizeof_EXC_FRAME}
#define PCB_p_locspn\t{offsetof_PCB_p_locspn}
""")
```

- [ ] **Step 4: `arch/arm64_gcc/common/gic_kernel.py`を作成する**（原文`gic_kernel.trb`全62行，実質ロジックは`TargetCheckCfgInt`関数のみ）

`arch/riscv_gcc/common/plic_kernel.py`（既存）の`TargetCheckCfgInt`関数と**Ruby原文が字面まで同一**（テーブル生成部分がPLICには有りGICには無いだけ）。

```python
# -*- coding: utf-8 -*-
#
#		パス2の生成スクリプトのコア依存部（GIC用）
#
#  $Id: gic_kernel.py (converted from gic_kernel.trb by Claude Code Sonnet 5) $
#

#
#  CFG_INTのターゲット依存のチェック
#
def TargetCheckCfgInt(params):
    # 複数のプロセッサで割込みを受け付ける機能は，現時点ではサポートして
    # いない（動作するはずであるが，テストしていない）ため，割付け可能プ
    # ロセッサが複数あるクラスの囲み内にCFG_INTを記述した場合には，
    # E_RSATRエラーとする［NGKI5184］．
    if ((params["intno"] >> 16) == 0) and (
        clsData[params["class"]]["affinityPrcBitmap"]
        != (1 << (clsData[params["class"]]["initPrc"] - 1))
    ):
        error_ercd("E_RSATR", params,
                   "%%intno is configured "
                   "to be accepted by more than one processors, "
                   "which is not supported on this target.")
```

- [ ] **Step 5: `arch/arm64_gcc/zynqmp/chip_kernel.py`を作成する**（原文`chip_kernel.trb`全34行）

```python
# -*- coding: utf-8 -*-
#
#		パス2の生成スクリプトのチップ依存部（KRIA / ZynqMP用）
#
#  $Id: chip_kernel.py (converted from chip_kernel.trb by Claude Code Sonnet 5) $
#

#
#  使用できる割込み番号とそれに対応する割込みハンドラ番号
#
INTNO_VALID = {}
INHNO_VALID = {}
INTNO_GLOBAL = {}
INHNO_GLOBAL = {}
private_intno_list = list(range(0, 32))
global_intno_list = list(range(32, 187))
for prcid in range(1, TNUM_PRCID + 1):
    INTNO_VALID[prcid] = []
    INHNO_VALID[prcid] = []
    INTNO_GLOBAL[prcid] = {}
    INHNO_GLOBAL[prcid] = {}
    for intno in private_intno_list:
        INTNO_VALID[prcid].append((prcid << 16) | intno)
        INHNO_VALID[prcid].append((prcid << 16) | intno)
        INTNO_GLOBAL[prcid][intno] = (prcid << 16) | intno
        INHNO_GLOBAL[prcid][intno] = (prcid << 16) | intno
    for intno in global_intno_list:
        INTNO_VALID[prcid].append(intno)
        INHNO_VALID[prcid].append((prcid << 16) | intno)
        INTNO_GLOBAL[prcid][intno] = intno
        INHNO_GLOBAL[prcid][intno] = (prcid << 16) | intno

#
#  生成スクリプトのコア依存部
#
IncludeTrb("gic_kernel.py")
IncludeTrb("core_kernel.py")
```

★`INTNO_GLOBAL`/`INHNO_GLOBAL`はRuby原文では`prcid`ごとの配列（`intno`をインデックスとして疎に書き込む）だが，`arch/arm64_gcc/common/core_kernel.trb`はこれを消費していない（`INHNO_VALID[prcid]`をリストとして直接列挙する方式であり，`INTNO_GLOBAL`/`INHNO_GLOBAL`を使うのは`kria_r5_gcc`側の`arch/arm_gcc/common/core_kernel.trb`だけ）。忠実な翻訳としてdict（`{intno: value}`）で保持する（intno域が連続しているため参照側の意味は変わらない）。**下流で未使用と分かっていても翻訳から省略しない**（誤って本当に使われている場合の見落としを避けるため）。

- [ ] **Step 6: 構文チェック**

```bash
cd /home/honda/TOPPERS/FMP3/fmp3_core
for f in arch/arm64_gcc/common/core_kernel.py arch/arm64_gcc/common/core_check.py \
         arch/arm64_gcc/common/core_offset.py arch/arm64_gcc/common/gic_kernel.py \
         arch/arm64_gcc/zynqmp/chip_kernel.py; do
    python3 -m py_compile "$f" && echo "OK $f" || echo "FAIL $f"
done
```
Expected: 5個すべて`OK`。

- [ ] **Step 7: `IncludeTrb`呼び出しの拡張子が全て`.py`であることを機械確認（negative control付き）**

```bash
cd /home/honda/TOPPERS/FMP3/fmp3_core
grep -n 'IncludeTrb(' arch/arm64_gcc/common/*.py arch/arm64_gcc/zynqmp/*.py
! grep -n 'IncludeTrb(' arch/arm64_gcc/common/*.py arch/arm64_gcc/zynqmp/*.py | grep -q '\.trb"'
echo "no-trb-leak exit=$?"
sed -i 's/IncludeTrb("core_kernel.py")/IncludeTrb("core_kernel.trb")/' arch/arm64_gcc/zynqmp/chip_kernel.py
grep -n 'IncludeTrb(' arch/arm64_gcc/zynqmp/chip_kernel.py | grep -q '\.trb"'
echo "negative-control-detected exit=$?"
git checkout -- arch/arm64_gcc/zynqmp/chip_kernel.py
```
Expected: `no-trb-leak exit=0`、`negative-control-detected exit=0`。

- [ ] **Step 8: 既存3構成の回帰（本タスクは新規ファイル追加のみで既存に影響しないはずの確認）**

```bash
cd /home/honda/TOPPERS/FMP3/fmp3_core
for p in polarfire_soc_kit-qemu musca_b1 musca_b1-2core; do
    tools/cfg_equivalence.sh build/$p
    echo "$p cfg_equivalence exit=$?"
done
```
Expected: 3構成ともexit=0（無変化のまま）。

- [ ] **Step 9: 意味検証はTask 7（kria_arm64フルチェーン）へ持ち越し。コミット**

```bash
cd /home/honda/TOPPERS/FMP3/fmp3_core
git add arch/arm64_gcc/common/core_kernel.py arch/arm64_gcc/common/core_check.py \
        arch/arm64_gcc/common/core_offset.py arch/arm64_gcc/common/gic_kernel.py \
        arch/arm64_gcc/zynqmp/chip_kernel.py
git commit -m "build(cfg): arch/arm64_gcc/** を新規移植（kria_arm64 core/chip層、538行、前例なし）

core_check.pyはarch/riscv_gcc/common/core_check.pyと.trb原文がコメント
以外バイト同一のため流用。core_kernel.py/gic_kernel.pyはriscv版と同型
構造（DefineVariableSection/SecnameKernelData/TargetCheckCfgInt等が
字面まで同一）だが例外番号リスト・GIC専用ロジックは新規翻訳。
core_offset.pyは完全新規（ARM64固有フィールド）。意味検証はTask 7へ。"
```

---

### Task 5: `kria_arm64_gcc` — target層cfgテンプレート移植

**Files:**
- Create: `target/kria_arm64_gcc/target_kernel.py`
- Create: `target/kria_arm64_gcc/target_class.py`
- Create: `target/kria_arm64_gcc/target_check.py`

**Interfaces:**
- Consumes: `arch/arm64_gcc/zynqmp/chip_kernel.py`・`arch/arm64_gcc/common/core_check.py`（Task 4）。
- Produces: `FMP3_KERNEL_CFG_TRB_FILES`/`FMP3_CLASS_TRB_FILES`/`FMP3_CHECK_TRB_FILES`が指す実体（Task 6の`target.cmake`がConsumes）。

- [ ] **Step 1: `target/kria_arm64_gcc/target_kernel.py`と`target_check.py`を作成する**（原文各11行，`IncludeTrb`1行のみ）

```python
# -*- coding: utf-8 -*-
#
#		パス2の生成スクリプトのターゲット依存部（KRIA Cortex-A53(AArch64)用）
#
#  $Id: target_kernel.py (converted from target_kernel.trb by Claude Code Sonnet 5) $
#

#
#  生成スクリプトのチップ依存部
#
IncludeTrb("chip_kernel.py")
```

```python
# -*- coding: utf-8 -*-
#
#		チェックパスの生成スクリプトのターゲット依存部
#
#  $Id: target_check.py (converted from target_check.trb by Claude Code Sonnet 5) $
#

#
#  生成スクリプトのコア依存部（チップ依存部は飛ばす）
#
IncludeTrb("core_check.py")
```

- [ ] **Step 2: `target/kria_arm64_gcc/target_class.py`を作成する**（原文`target_class.trb`全69行）

`target/polarfire_soc_kit_gcc/target_class.py`（既存）と**`.trb`原文がコメント以外バイト同一**（`diff target/kria_arm64_gcc/target_class.trb target/polarfire_soc_kit_gcc/target_class.trb`で確認済み。両ターゲットともTNUM_PRCID 1〜4で同じ`clsData`構造：`CLS_PRC1..4`＋`CLS_ALL_PRC1..4`）。コピーしてヘッダコメントのみ書き換える。

```bash
cd /home/honda/TOPPERS/FMP3/fmp3_core
cp target/polarfire_soc_kit_gcc/target_class.py target/kria_arm64_gcc/target_class.py
sed -i 's/ターゲット依存のクラス定義（Plarfire SoC Kit用）/ターゲット依存のクラス定義（KRIA用）/' \
    target/kria_arm64_gcc/target_class.py
```

- [ ] **Step 3: 構文チェック**

```bash
cd /home/honda/TOPPERS/FMP3/fmp3_core
for f in target/kria_arm64_gcc/target_kernel.py target/kria_arm64_gcc/target_check.py \
         target/kria_arm64_gcc/target_class.py; do
    python3 -m py_compile "$f" && echo "OK $f" || echo "FAIL $f"
done
```
Expected: 3個すべて`OK`。

- [ ] **Step 4: negative control — コピーが実際にpolarfireと中身が異なることを確認する**

```bash
cd /home/honda/TOPPERS/FMP3/fmp3_core
diff target/polarfire_soc_kit_gcc/target_class.py target/kria_arm64_gcc/target_class.py
echo "diff exit=$? (expect 1)"
```

- [ ] **Step 5: コミット**

```bash
cd /home/honda/TOPPERS/FMP3/fmp3_core
git add target/kria_arm64_gcc/target_kernel.py target/kria_arm64_gcc/target_check.py \
        target/kria_arm64_gcc/target_class.py
git commit -m "build(cfg): target/kria_arm64_gcc/*.py を新規移植（target層、91行）

target_class.pyはtarget/polarfire_soc_kit_gcc/target_class.pyと.trb原文が
コメント以外バイト同一（TNUM_PRCID 1-4のclsData構造が完全一致）のため
流用。target_kernel/target_checkはIncludeTrbのみの薄いファイル。
意味検証はTask 7へ。"
```

---

### Task 6: `kria_arm64_gcc` — CMake層とツールチェーン

**Files:**
- Create: `cmake/toolchain-aarch64-none-elf.cmake`
- Create: `arch/arm64_gcc/common/arch.cmake`
- Create: `arch/arm64_gcc/zynqmp/chip.cmake`
- Create: `target/kria_arm64_gcc/target.cmake`
- Create: `target/kria_arm64_gcc/presets.json`
- Modify: `CMakePresets.json`

**Interfaces:**
- Consumes: Task 3の`FMP3_DUMP_FORMAT`/`FMP3_DUMPOPTS`フック、Task 4/5の`.py`一式。
- Produces: `FMP3_TARGET=kria_arm64_gcc`でconfigureできるプリセット`kria_arm64`。

- [ ] **Step 1: `cmake/toolchain-aarch64-none-elf.cmake`を作成する**

`aarch64-none-elf-gcc`（Arm GNU Toolchain 14.3.Rel1）が`/usr/local/tools/arm-gnu-toolchain-14.3.rel1-x86_64-aarch64-none-elf/bin`にあり，本計画着手時点でこのディレクトリがPATHに含まれ`which aarch64-none-elf-gcc`で解決することを確認済み。`-dumpmachine`は`aarch64-none-elf`を返す（実測済み）。

```cmake
#
#		ツールチェーンファイル（AArch64 ベアメタル：aarch64-none-elf）
#
#  既定は aarch64-none-elf．別のプレフィックスを使う場合は
#  -DAARCH64_NONE_ELF_TOOLCHAIN_PREFIX=... で上書きできる．
#
#  実測（2026-07-19）: /usr/local/tools/arm-gnu-toolchain-14.3.rel1-
#  x86_64-aarch64-none-elf/bin が PATH に含まれ aarch64-none-elf-gcc
#  14.3.1 が解決する．PATH に無い環境では
#  -DAARCH64_NONE_ELF_TOOLCHAIN_PREFIX=<full-path>/aarch64-none-elf-
#  のようにフルパスを渡す．
#
set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR aarch64)

#
#  cmake/toolchain_check.cmake が照合する「このツールチェーンファイルが
#  期待する -dumpmachine パターン」．実測: aarch64-none-elf-gcc -dumpmachine
#  は "aarch64-none-elf" を返す．
#
if(NOT DEFINED FMP3_EXPECTED_TOOLCHAIN_MACHINE)
    set(FMP3_EXPECTED_TOOLCHAIN_MACHINE aarch64-none-elf)
endif()

if(NOT DEFINED AARCH64_NONE_ELF_TOOLCHAIN_PREFIX)
    set(AARCH64_NONE_ELF_TOOLCHAIN_PREFIX aarch64-none-elf-)
endif()

set(CMAKE_C_COMPILER   ${AARCH64_NONE_ELF_TOOLCHAIN_PREFIX}gcc)
set(CMAKE_ASM_COMPILER ${AARCH64_NONE_ELF_TOOLCHAIN_PREFIX}gcc)
set(CMAKE_OBJCOPY      ${AARCH64_NONE_ELF_TOOLCHAIN_PREFIX}objcopy CACHE FILEPATH "objcopy")
set(CMAKE_NM           ${AARCH64_NONE_ELF_TOOLCHAIN_PREFIX}nm      CACHE FILEPATH "nm")
set(CMAKE_OBJDUMP      ${AARCH64_NONE_ELF_TOOLCHAIN_PREFIX}objdump CACHE FILEPATH "objdump")

#  ベアメタルではリンクできる完全な実行ファイルを作れないため，
#  try_compile はスタティックライブラリで行う
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
```

- [ ] **Step 2: `arch/arm64_gcc/common/arch.cmake`を作成する**

`Makefile.core`のCMake版。`arch/riscv_gcc/common/arch.cmake`と同じ書き分け規約（`${FMP3_ROOT_DIR}`基準で自己解決）。**上流`Makefile.core`は`START_OBJS`/`-nostdlib`を持たない**（それはチップ層＝`Makefile.chip`側にある。riscv/arm_mとは異なる点。現物確認済み）ため，本ファイルでは`FMP3_START_FILES`を積まない。

```cmake
#
#		アーキテクチャ依存部の CMake 定義（ARM64 コア共通）
#
#  chip.cmake から include される（上流 Makefile.core に相当）．
#
#  本ファイルは常に fmp3_core（このリポジトリ）側にある共通コア層なので，
#  ${FMP3_ROOT_DIR} 基準で自己解決する（arch/riscv_gcc/common/arch.cmake・
#  arch/arm_m_gcc/common/arch.cmake と同じ作法）．
#
#  ★上流 Makefile.core（本ファイルの翻訳元）は START_OBJS / -nostdlib を
#    持たない（riscv_gcc・arm_m_gcc の Makefile.core とはこの点が異なる）．
#    start.S と -nostdlib は arch/arm64_gcc/zynqmp/Makefile.chip 側にあり，
#    チップ層（chip.cmake）が積む．本ファイルでは扱わない．
#
set(COREDIR ${FMP3_ROOT_DIR}/arch/arm64_gcc/common)
set(TOOLDIR ${FMP3_ROOT_DIR}/arch/gcc)

#  Makefile.core:44  CFG_TABS
list(APPEND FMP3_SYMVAL_TABLES
    ${COREDIR}/core_sym.def
)

#  Makefile.core:47  TARGET_OFFSET_TRB
list(APPEND FMP3_OFFSET_TRB_FILES
    ${COREDIR}/core_offset.py
)

#  Makefile.core:19-20
list(APPEND FMP3_INCLUDE_DIRS
    ${COREDIR}
    ${TOOLDIR}
)

#  Makefile.core:30-40  KERNEL_ASMOBJS core_support.o gic_support.o /
#  KERNEL_COBJS core_kernel_impl.o core_timer.o arm64.o gic_kernel_impl.o
#  （本ターゲットは OMIT_CORE_TIMER 未設定のため core_timer.o を含める．
#  SYSMON=ATF_S 時のみの atf_support.o は本ターゲット既定の単独動作
#  （非SYSMON）では不要なので含めない）
list(APPEND FMP3_ARCH_C_FILES
    ${COREDIR}/core_kernel_impl.c
    ${COREDIR}/core_timer.c
    ${COREDIR}/arm64.c
    ${COREDIR}/gic_kernel_impl.c
    ${COREDIR}/core_support.S
    ${COREDIR}/gic_support.S
)

#  Makefile.core:22（-lgcc のみ．libc は非リンク）
list(APPEND FMP3_LINK_LIBS gcc)

#  Makefile.core:23
list(APPEND FMP3_COMPILE_OPTIONS -mstrict-align)
```

- [ ] **Step 3: `arch/arm64_gcc/zynqmp/chip.cmake`を作成する**

`Makefile.chip`のCMake版。`start.S`/`-nostdlib`はここ（チップ層）が積む。

```cmake
#
#		チップ依存部の CMake 定義（KRIA / ZynqMP用）
#
#  target.cmake から include される（上流 Makefile.chip に相当）．
#
set(COREDIR ${ARCHDIR}/common)

#  Makefile.chip:12-14
list(APPEND FMP3_COMPILE_OPTIONS -mcpu=cortex-a53)
list(APPEND FMP3_LINK_OPTIONS -mcpu=cortex-a53)
list(APPEND FMP3_COMPILE_DEFS TOPPERS_CORTEX_A53)

list(APPEND FMP3_INCLUDE_DIRS ${CHIPDIR})

#  Makefile.chip:18  LDFLAGS := $(LDFLAGS) -N
list(APPEND FMP3_LINK_OPTIONS -Wl,-N)

#
#  カーネルに含めるチップ依存ソース（Makefile.chip:22-24）
#
list(APPEND FMP3_ARCH_C_FILES
    ${CHIPDIR}/chip_kernel_impl.c
)

#
#  非TECS版 SIO ドライバ（Makefile.chip:29、Cadence UART）
#
list(APPEND FMP3_SYSSVC_TARGET_C_FILES
    ${CHIPDIR}/chip_serial.c
    ${CHIPDIR}/xuartps.c
)

#
#  スタートアップモジュール（Makefile.chip:33-43）
#
#  ★このターゲットでは start.S / -nostdlib はコア層(arch.cmake)ではなく
#    チップ層(ここ)が積む（上流 Makefile.chip の構造どおり）．
#
list(APPEND FMP3_START_FILES
    ${COREDIR}/start.S
)
list(APPEND FMP3_LINK_OPTIONS -nostdlib)

#
#  コア依存部（Makefile.chip:48）
#
include(${COREDIR}/arch.cmake)
```

- [ ] **Step 4: `target/kria_arm64_gcc/target.cmake`を作成する**

```cmake
#
#		ターゲット依存部の CMake 定義（KRIA SOM Cortex-A53(AArch64) / QEMU 用）
#
#  上流 target/kria_arm64_gcc/Makefile.target の CMake 版．
#
set(ARCHDIR ${FMP3_ROOT_DIR}/arch/arm64_gcc)
get_filename_component(CHIPDIR ${CMAKE_CURRENT_LIST_DIR}/../../arch/arm64_gcc/zynqmp ABSOLUTE)
set(TARGETDIR ${CMAKE_CURRENT_LIST_DIR})

#
#  ROM イメージ形式（Makefile.core:34 の DUMP = dump に相当）．
#  汎用層 CMakeLists.txt（Task 3）の FMP3_DUMP_FORMAT フックを使う．
#
set(FMP3_DUMP_FORMAT dump)
set(FMP3_DUMPOPTS "-j .text -j .rodata")

#
#  対象 Kria ボード（バナー表示名のみに影響．PS は全 Kria で同一．
#  Makefile.target:16-36）
#
set(FMP3_BOARD "kr260" CACHE STRING "Kria board name (kr260 / kv260 / kd240)")
set_property(CACHE FMP3_BOARD PROPERTY STRINGS kr260 kv260 kd240)
if(FMP3_BOARD STREQUAL "kr260")
    list(APPEND FMP3_COMPILE_DEFS TOPPERS_KRIA_KR260)
elseif(FMP3_BOARD STREQUAL "kv260")
    list(APPEND FMP3_COMPILE_DEFS TOPPERS_KRIA_KV260)
elseif(FMP3_BOARD STREQUAL "kd240")
    list(APPEND FMP3_COMPILE_DEFS TOPPERS_KRIA_KD240)
else()
    message(FATAL_ERROR "FMP3_BOARD must be kr260, kv260 or kd240 (got: ${FMP3_BOARD})")
endif()

#  Makefile.target:41-43  FPUサポート
list(APPEND FMP3_COMPILE_DEFS USE_ARM64_FPU)

#
#  単独動作（SYSMON 未定義）のメモリ配置（Makefile.target:70-83）．
#  ATF_S/ATF_NS（外部ATF連携）は本計画のスコープ外（QEMUにATFを用意する
#  手段が無いため）．
#
if(NOT DEFINED FMP3_MEM_BASE)
    set(FMP3_MEM_BASE 0x00000000)
endif()
if(NOT DEFINED FMP3_MEM_SIZE)
    set(FMP3_MEM_SIZE 0x40000000)
endif()
list(APPEND FMP3_COMPILE_DEFS
    TOPPERS_TZ_S
    TOPPERS_MEM_BASE=${FMP3_MEM_BASE}
    TOPPERS_MEM_SIZE=${FMP3_MEM_SIZE}
    TOPPERS_32BIT_ABOVE_ADDR
    USE_ARM64_MMU_CONFIG_TABLE
)

#  Makefile.target:88-92
list(APPEND FMP3_INCLUDE_DIRS ${TARGETDIR})
list(APPEND FMP3_COMPILE_OPTIONS -mlittle-endian -gdwarf-4 -gstrict-dwarf)
list(APPEND FMP3_LINK_OPTIONS -mlittle-endian -Wl,--build-id=none)
list(APPEND FMP3_CFG1_OUT_LINK_OPTIONS -nostdlib)

#  Makefile.target:100-115  SIOP：SYSMON 未定義（単独動作）は XUART1
list(APPEND FMP3_COMPILE_DEFS USE_XUART1)

#  Makefile.target:130-132  TECS を使用しない
list(APPEND FMP3_COMPILE_DEFS TOPPERS_OMIT_TECS)

#
#  カーネルに含めるターゲット依存ソース（Makefile.target:120-122）
#
list(APPEND FMP3_TARGET_C_FILES
    ${TARGETDIR}/target_kernel_impl.c
)
list(APPEND FMP3_ARCH_C_FILES
    ${ARCHDIR}/common/psci_support.S
)

#  Makefile.target:127
set(FMP3_LDSCRIPT ${TARGETDIR}/kria.ld)

#
#  cfg に渡すファイル
#
list(APPEND FMP3_CFG_FILES            ${TARGETDIR}/target_kernel.cfg)
list(APPEND FMP3_CLASS_TRB_FILES      ${TARGETDIR}/target_class.py)
list(APPEND FMP3_KERNEL_CFG_TRB_FILES ${TARGETDIR}/target_kernel.py)
list(APPEND FMP3_CHECK_TRB_FILES      ${TARGETDIR}/target_check.py)

#
#  チップ依存部
#
include(${CHIPDIR}/chip.cmake)

#
#  QEMU（xlnx-zcu102）．v11.0.1 以降が必要（APU RVBAR／CRF リセット制御・
#  RPU クラスタ実装のバージョン帯。/usr/bin/qemu-system-aarch64 は 8.2.2
#  のため既定では使わない。musca_b1/target.cmake と同じバージョン
#  チェック方式）。
#
set(_fmp3_kria_arm64_qemu_builtin /home/honda/qemu-build/install/bin/qemu-system-aarch64)
if(EXISTS ${_fmp3_kria_arm64_qemu_builtin})
    set(_fmp3_kria_arm64_qemu_default ${_fmp3_kria_arm64_qemu_builtin})
else()
    set(_fmp3_kria_arm64_qemu_default qemu-system-aarch64)
endif()
set(QEMU_SYSTEM_AARCH64_KRIA ${_fmp3_kria_arm64_qemu_default} CACHE STRING
    "Path to qemu-system-aarch64 for the xlnx-zcu102 machine (needs >= 11.0.1)")
unset(_fmp3_kria_arm64_qemu_builtin)
unset(_fmp3_kria_arm64_qemu_default)

execute_process(
    COMMAND ${QEMU_SYSTEM_AARCH64_KRIA} --version
    OUTPUT_VARIABLE _fmp3_qemu_version_output
    RESULT_VARIABLE _fmp3_qemu_version_result
    OUTPUT_STRIP_TRAILING_WHITESPACE
)
if(_fmp3_qemu_version_result EQUAL 0 AND _fmp3_qemu_version_output MATCHES "version ([0-9]+)\\.")
    set(_fmp3_qemu_major ${CMAKE_MATCH_1})
    message(STATUS "fmp3_core: QEMU_SYSTEM_AARCH64_KRIA version = ${_fmp3_qemu_version_output}")
    if(_fmp3_qemu_major LESS 11)
        message(WARNING
            "fmp3_core: QEMU_SYSTEM_AARCH64_KRIA='${QEMU_SYSTEM_AARCH64_KRIA}' reports "
            "major version ${_fmp3_qemu_major} (< 11). The xlnx-zynqmp RPU cluster / "
            "APU RVBAR-CRF reset control this target relies on may be missing. "
            "Override with -DQEMU_SYSTEM_AARCH64_KRIA=<path to qemu-system-aarch64 >= 11.0.1>.")
    endif()
    unset(_fmp3_qemu_major)
else()
    message(WARNING
        "fmp3_core: could not determine the version of "
        "QEMU_SYSTEM_AARCH64_KRIA='${QEMU_SYSTEM_AARCH64_KRIA}' (is it installed / on PATH?).")
endif()
unset(_fmp3_qemu_version_output)
unset(_fmp3_qemu_version_result)

#
#  QEMU による実行（cmake --build <dir> --target run）
#
#  ★target_exit()（target_kernel_impl.c）はセミホスティング終了を持たず
#    while(true) の無限ループのため，QEMU は自然終了しない。実行検証は
#    timeout 併用が前提（musca_b1/polarfireと同様）．
#
#  ★secure=on で EL3 起動（TOPPERS_TZ_S の単独動作前提と一致）．
#  ★-smp の数はビルドの TNUM_PRCID（FMP3_PRC_NUM，既定4）と一致させる
#    こと（一致しないと Task 7 の判定に影響する）．既定はターゲットの
#    target_kernel.h の既定値である4に合わせる．
#
if(NOT FMP3_PRC_NUM STREQUAL "")
    set(_fmp3_kria_arm64_smp ${FMP3_PRC_NUM})
else()
    set(_fmp3_kria_arm64_smp 4)
endif()
set(FMP3_RUN_COMMAND
    ${QEMU_SYSTEM_AARCH64_KRIA} -M xlnx-zcu102,secure=on -smp ${_fmp3_kria_arm64_smp} -m 2G
    -nographic -serial mon:stdio
    -kernel $<TARGET_FILE:fmp>
)
unset(_fmp3_kria_arm64_smp)
```

- [ ] **Step 5: `target/kria_arm64_gcc/presets.json`を作成する**

```json
{
  "version": 4,
  "include": [
    "../../cmake/presets-base.json"
  ],
  "configurePresets": [
    {
      "name": "kria_arm64",
      "inherits": "_base",
      "displayName": "KRIA SOM Cortex-A53 (AArch64, QEMU xlnx-zcu102, 4 processors)",
      "description": "QEMU xlnx-zcu102 マシン向け（実機は別途JTAG。既定TNUM_PRCID=4）。QEMU >= 11.0.1 が必要",
      "toolchainFile": "${sourceDir}/cmake/toolchain-aarch64-none-elf.cmake",
      "cacheVariables": {
        "FMP3_TARGET": "kria_arm64_gcc"
      }
    },
    {
      "name": "kria_arm64-1core",
      "inherits": "kria_arm64",
      "displayName": "KRIA SOM Cortex-A53 (AArch64, QEMU xlnx-zcu102, 1 processor)",
      "description": "1コア（TNUM_PRCID=1）。単独動作のsecondary-core起動経路を踏まない最小構成",
      "cacheVariables": {
        "FMP3_PRC_NUM": "1"
      }
    }
  ],
  "buildPresets": [
    { "name": "kria_arm64",        "configurePreset": "kria_arm64" },
    { "name": "kria_arm64-1core",  "configurePreset": "kria_arm64-1core" },
    {
      "name": "run-kria_arm64",
      "configurePreset": "kria_arm64",
      "targets": [ "run" ]
    },
    {
      "name": "run-kria_arm64-1core",
      "configurePreset": "kria_arm64-1core",
      "targets": [ "run" ]
    }
  ]
}
```

- [ ] **Step 6: `CMakePresets.json`に追加する**

```json
  "include": [
    "target/polarfire_soc_kit_gcc/presets.json",
    "target/musca_b1_gcc/presets.json",
    "target/rp2350_pico2_gcc/presets.json",
    "target/kria_arm64_gcc/presets.json"
  ]
```

- [ ] **Step 7: configure + build**

```bash
cd /home/honda/TOPPERS/FMP3/fmp3_core
cmake --preset kria_arm64-1core
cmake --build build/kria_arm64-1core 2>&1 | tail -60
echo "build exit=$?"
```
Expected: exit=0。まず1コアから（4コアはTask 7で試す）。

- [ ] **Step 8: positive control — `cfg1_out.dump`（`.srec`ではない）が生成されていることを確認**

```bash
ls build/kria_arm64-1core/generated/cfg1_out.dump
! test -f build/kria_arm64-1core/generated/cfg1_out.srec
echo "srec absent exit=$? (expect 0)"
```

- [ ] **Step 9: 既存3構成とrp2350の回帰**

```bash
cd /home/honda/TOPPERS/FMP3/fmp3_core
for p in polarfire_soc_kit-qemu musca_b1 musca_b1-2core rp2350_pico2; do
    tools/cfg_equivalence.sh build/$p
    echo "$p cfg_equivalence exit=$?"
done
```
Expected: 4構成ともexit=0（無変化）。

- [ ] **Step 10: コミット**

```bash
cd /home/honda/TOPPERS/FMP3/fmp3_core
git add cmake/toolchain-aarch64-none-elf.cmake arch/arm64_gcc/common/arch.cmake \
        arch/arm64_gcc/zynqmp/chip.cmake target/kria_arm64_gcc/target.cmake \
        target/kria_arm64_gcc/presets.json CMakePresets.json
git commit -m "build(cmake): kria_arm64_gcc のCMake層とツールチェーンを追加

toolchain-aarch64-none-elf.cmake（aarch64-none-elf-gcc 14.3.1，
-dumpmachine=aarch64-none-elf実測済み）+ arch.cmake（上流Makefile.core
はSTART_OBJSを持たない点に注意）+ chip.cmake（start.S/-nostdlibは
こちら）+ target.cmake（FMP3_DUMP_FORMAT=dumpを宣言，QEMU 11.0.1+
バージョンチェック付き）。1コアでビルド成功・cfg1_out.dump生成を確認。
既存4構成は無回帰。意味検証・QEMU実行検証はTask 7へ。"
```

---

### Task 7: `kria_arm64_gcc` — ビルド・差分等価性検査・QEMU実行検証（STG初期化の実地判定を含む）

**Files:**
- 条件付きで Modify: `arch/arm64_gcc/zynqmp/chip_kernel_impl.c`（pristine改変。必要と判明した場合のみ）
- Modify: `DIVERGENCE_MAP.md`（pristine改変を行った場合、または「不要と確認できた」事実を記録する場合の両方で追記）

**Interfaces:**
- Consumes: Task 6までの全成果物。
- Produces: `kria_arm64`プリセットの完成（1コア・4コア）。

**方針**: Task 4-6着手前の事前調査（本計画冒頭）で「`chip_el3_initialize()`が`0xFF260000`（System Timestamp Generator）へ無条件アクセスし，QEMU 11.0.1のソースにこの領域の実装が無いため同期外部アボートが起きる可能性が高い」という**強い状況証拠**を得たが，**実行して確かめていない**。断定せず，このTaskで実際に確かめてから判断する。

- [ ] **Step 1: 差分等価性検査（1コア）**

```bash
cd /home/honda/TOPPERS/FMP3/fmp3_core
tools/cfg_equivalence.sh build/kria_arm64-1core
echo "cfg_equivalence exit=$?"
```
Expected: exit=0。ここでMISMATCHが出た場合はTask 4/5のテンプレート翻訳が原因（QEMU実行の可否とは独立の問題として先に解決する）。

- [ ] **Step 2: negative control — テンプレートを1箇所壊して差分が検出されることを確認**

TNUM_PRCID=4（既定）の`kria_arm64`プリセットで，polarfireと同型のclsData構造を壊す（生きている分岐を狙う。計画Bの教訓：`target_class.py`の最初の分岐だけを壊すとTNUM_PRCID=1のデッドコードになりうる）。

```bash
cd /home/honda/TOPPERS/FMP3/fmp3_core
cp target/kria_arm64_gcc/target_class.py /tmp/target_class.py.bak
sed -i '0,/"initPrc": 1, "affinityPrcList": \[1\]},/! {0,/"initPrc": 1, "affinityPrcList": \[1\]},/s//"initPrc": 1, "affinityPrcList": [99]},/}' \
    target/kria_arm64_gcc/target_class.py
diff /tmp/target_class.py.bak target/kria_arm64_gcc/target_class.py
rm -rf build/kria_arm64-neg && cmake --preset kria_arm64 -B build/kria_arm64-neg
tools/cfg_equivalence.sh build/kria_arm64-neg
echo "negative cfg_equivalence exit=$? (expect 1)"
cp /tmp/target_class.py.bak target/kria_arm64_gcc/target_class.py
rm -rf build/kria_arm64-neg /tmp/target_class.py.bak
```
Expected: exit=1。sedで意図した箇所（TNUM_PRCID=4分岐の2個目のクラス、デッドコードではない）が壊れたことを`diff`で必ず目視確認してから流す。壊した状態のままコミットしないこと。

- [ ] **Step 3: `tools/cfg_error_tests/run.sh`の回帰**

```bash
cd /home/honda/TOPPERS/FMP3/fmp3_core
tools/cfg_error_tests/run.sh build/kria_arm64-1core
echo "cfg_error_tests exit=$?"
```
Expected: exit=0。

- [ ] **Step 4: 4コアでビルド**

```bash
cd /home/honda/TOPPERS/FMP3/fmp3_core
cmake --preset kria_arm64
cmake --build build/kria_arm64 2>&1 | tail -60
echo "build exit=$?"
tools/cfg_equivalence.sh build/kria_arm64
echo "cfg_equivalence exit=$?"
```
Expected: 両方exit=0。

- [ ] **Step 5: QEMU実行を1コアでまず試す（STG初期化の実地判定・タイムアウト必須）**

```bash
cd /home/honda/TOPPERS/FMP3/fmp3_core
QEMU=/home/honda/qemu-build/install/bin/qemu-system-aarch64
timeout 20 $QEMU -M xlnx-zcu102,secure=on -smp 1 -m 2G -nographic \
    -serial mon:stdio -kernel build/kria_arm64-1core/fmp \
    -d guest_errors,unimp -D /tmp/kria_arm64_qemu.log
echo "qemu rc=$? (124/137ならタイムアウトで正常継続中，即座に0や1ならクラッシュの疑い)"
tail -30 /tmp/kria_arm64_qemu.log
ps -eo pid,comm | grep qemu-system || echo "no leftover qemu process"
```

**判定基準（断定しない。実測結果に従う）**:
- バナー（`TOPPERS/FMP3 Kernel Release ...`・`Processor 1 start.`相当）が出力され，`rc`がタイムアウト由来（124または137）→ **STG初期化は問題にならなかった**。Step 7へ進む。
- バナーが1文字も出ず，`/tmp/kria_arm64_qemu.log`に`Unassigned mem read`または同種のガード違反ログがあり，`0xff260000`付近を指す → **事前調査どおりの外部アボート**。Step 6でpristine修正を行う。
- それ以外の挙動（バナーが一部だけ出る等）→ 得られたログをそのまま記録し，`/tmp/kria_arm64_qemu.log`の内容を根拠にDIVERGENCE_MAP.mdの「未解決事項」へ強い証拠付きで記載する（断定はしない）。

- [ ] **Step 6（条件付き）: STG初期化に`TOPPERS_USE_QEMU`ガードを追加する（Step 5でクラッシュが実際に確認された場合のみ実施）**

前例：`/home/honda/TOPPERS/ASP3CORE/asp3_core`の`zcu102_arm64_gcc`（`docs/dev/qemu-target-a64.md:83`）が同じ理由で同じ対処を行っている。

`arch/arm64_gcc/zynqmp/chip_kernel_impl.c`の`chip_el3_initialize()`（現物のこの計画着手時点で`:55-81`）を，STGレジスタ書き込みのみ`TOPPERS_USE_QEMU`でガードする（`CNTFRQ_EL0_WRITE`はCPU内部レジスタでMMIOではないため無条件のまま）：

```c
	/*
	 *  Enable System Timestamp Generator - Secure
	 */
#ifndef TOPPERS_USE_QEMU
	sil_wrw_mem((void*)XIOU_SCNTRS_CNT_CNTRL_REG,
				sil_rew_mem((void*)XIOU_SCNTRS_CNT_CNTRL_REG) & ~XIOU_SCNTRS_CNT_CNTRL_REG_EN);

	sil_wrw_mem((void*)XIOU_SCNTRS_FREQ_REG, XIOU_SCNTRS_FREQ_HZ);

	sil_wrw_mem((void*)XIOU_SCNTRS_CNT_CNTRL_REG,
				sil_rew_mem((void*)XIOU_SCNTRS_CNT_CNTRL_REG) | XIOU_SCNTRS_CNT_CNTRL_REG_EN);
#endif /* TOPPERS_USE_QEMU */

	/* Initialize Generic Timer Freq */
	CNTFRQ_EL0_WRITE(XIOU_SCNTRS_FREQ_HZ);
```

`target.cmake`に`TOPPERS_USE_QEMU`を`FMP3_COMPILE_DEFS`へ追加する（musca_b1と同じ書き方。Task 6の`target.cmake`のFMP3_COMPILE_DEFSブロックへ`TOPPERS_USE_QEMU`を足す1行差分）。

修正後，Step 5を再実行してバナーが出ることを確認する。**negative control**: 修正を一時的に`git stash`で外し，同じコマンドで再びクラッシュ（バナー0行）することを確認してから`git stash pop`する（「直したから直った」のか「元々問題なかった」のかを区別する）。

- [ ] **Step 7: 4コアでQEMU実行を試す（PMU_GLOBAL経由のsecondary core起動。best-effort）**

```bash
cd /home/honda/TOPPERS/FMP3/fmp3_core
QEMU=/home/honda/qemu-build/install/bin/qemu-system-aarch64
timeout 20 $QEMU -M xlnx-zcu102,secure=on -smp 4 -m 2G -nographic \
    -serial mon:stdio -kernel build/kria_arm64/fmp \
    -d guest_errors,unimp -D /tmp/kria_arm64_qemu_smp4.log
echo "qemu rc=$?"
tail -40 /tmp/kria_arm64_qemu_smp4.log
ps -eo pid,comm | grep qemu-system || echo "no leftover qemu process"
```

事前調査で判明済みの追加リスク：`chip_mprc_initialize()`の非SYSMON経路（`chip_kernel_impl.c:150-193`）は`PMU_GLOBAL_REQ_PWRUP_STATUS`（`0xFFD80000+0x110`）を無条件にポーリング読出しし，この領域もQEMU 11.0.1のソースに実装が見当たらない。**ただし**セカンダリコアの起動そのもの（`CRF_APB_RST_FPD_APU`書き込み → `arm_set_cpu_on_and_reset()`）はQEMU側に実装があるため，**PMU_GLOBALのポーリングが「読んでも0が返り実害なく先に進む」か「外部アボートで止まる」かは実行してみないと分からない**。

- 4コアが起動する（`Processor 1/2/3/4 start.`相当が出力される）→ 何も直す必要はない。
- 起動しない場合，`/tmp/kria_arm64_qemu_smp4.log`を確認し，アボート箇所が`0xFFD80000`付近であれば，Step 6と同じ要領で`chip_mprc_initialize()`の非SYSMON分岐（PMU_GLOBALのポーリング全体）を`#ifndef TOPPERS_USE_QEMU`で囲む修正を検討する。**ただしこの分岐は`CRF_APB_RST_FPD_APU`の書き込み（実際にコアを起こす副作用）も含むため，単純にブロック全体をスキップするとコアが起動しなくなる。** ポーリング部分（`do { ... } while`）だけを`TOPPERS_USE_QEMU`時にスキップし，`APU_RVBARADDR*`書き込み・`CRF_APB_RST_FPD_APU`書き込みは残す，という部分的な修正が必要になる可能性が高い。**この修正は本Stepで一度だけ試み，`timeout`実行で症状が変わらない・原因箇所が特定できない場合は，1コア構成のみを本計画の達成範囲とし，4コアはDIVERGENCE_MAP.mdの「未解決事項」へ強い証拠付きで記録して次の計画へ持ち越す**（無理に解決を引き延ばさない）。

- [ ] **Step 8: 既存4構成の回帰（最終）**

```bash
cd /home/honda/TOPPERS/FMP3/fmp3_core
for p in polarfire_soc_kit-qemu musca_b1 musca_b1-2core rp2350_pico2; do
    tools/cfg_equivalence.sh build/$p
    echo "$p cfg_equivalence exit=$?"
done
```
Expected: 4構成ともexit=0。

- [ ] **Step 9: `DIVERGENCE_MAP.md`を更新する**

Step 6でpristineを改変した場合は`add`表へ1行追記（対象・種別`patch`・理由・上流報告有無）。Step 7で未解決のまま残した場合は「未解決事項」節へ，実測したログの抜粋（アドレス・rc・タイムアウトか即死か）を添えて追記する（既存の`hrt_clear_event_body()`の記載と同じ「強い証拠はあるが断定はしない」書式に揃える）。

- [ ] **Step 10: コミット**

```bash
cd /home/honda/TOPPERS/FMP3/fmp3_core
git add DIVERGENCE_MAP.md
# Step 6でpristineを直した場合のみ追加:
# git add arch/arm64_gcc/zynqmp/chip_kernel_impl.c target/kria_arm64_gcc/target.cmake
git commit -m "test(kria_arm64): QEMU実行検証を実施し結果をDIVERGENCE_MAP.mdへ記録

STG初期化(0xFF260000)がQEMU未実装領域への無条件アクセスである件を
実行して確認し，[実測結果に応じて記述: 問題なかった／
TOPPERS_USE_QEMUガードで解消した／未解決のまま記録した]。
4コア(PMU_GLOBAL経由のsecondary core起動)は[実測結果を記述]。"
```

---

### Task 8: `kria_r5_gcc` — core/chip層cfgテンプレート移植

**Files:**
- Create: `arch/arm_gcc/common/core_kernel.py`
- Create: `arch/arm_gcc/common/core_check.py`
- Create: `arch/arm_gcc/common/core_offset.py`
- Create: `arch/arm_gcc/zynqmp_r5/chip_kernel.py`

**Interfaces:**
- Consumes: `kernel/kernel.py`・`kernel/kernel_check.py`・`kernel/genoffset.py`（既存）。翻訳元は本リポジトリのpristine`arch/arm_gcc/{common,zynqmp_r5}/*.trb`（現物，全文確認済み）。`TMAX_INTNO`/`TMAX_INHNO`/`TMAX_EXCNO`は`arch/arm_gcc/common/core_sym.def`のsymval-table経由で自動的にグローバルへ入る（現物確認済み）。
- Produces: `IncludeTrb("core_kernel.py")`／`IncludeTrb("core_check.py")`（Task 9の`target/kria_r5_gcc/*.py`がConsumes）。**`arch/arm_gcc/common/gic_kernel.trb`は本ターゲットの`IncludeTrb`連鎖に入らないため移植しない**（`zynqmp_r5/chip_kernel.trb`が`IncludeTrb("gic_kernel.trb")`を呼んでいないことを現物確認済み。`TargetCheckCfgInt`が未定義のままなら`kernel/interrupt.py`の`"TargetCheckCfgInt" in globals()`ガードにより該当チェックがスキップされるだけで，これは上流の設計どおりであり本タスクの不備ではない）。

**翻訳仕様:**

- [ ] **Step 1: `arch/arm_gcc/common/core_kernel.py`を作成する**（原文`core_kernel.trb`全215行）

**arm64版（Task 4）と異なり，`0..TMAX_INHNO`の連番を`INHNO_GLOBAL[prcid][index]`で実値へ変換する方式**（原文`0.upto(TMAX_INHNO).each_with_index`は，Rubyの`0.upto(N)`が既に0起点の連番であるため`localInhnoVal`と`index`が常に等しい恒等変換になる。ここでは`range(TMAX_INHNO + 1)`1本に単純化して翻訳した）。

```python
# -*- coding: utf-8 -*-
#
#		パス2の生成スクリプトのコア依存部（ARM用）
#
#  $Id: core_kernel.py (converted from core_kernel.trb by Claude Code Sonnet 5) $
#

#
#  有効なCPU例外ハンドラ番号
#
EXCNO_VALID = {}
EXCNO_GLOBAL = {}
excno_list = [0, 1, 2, 3, 4, 5, 6, 7]
for prcid in range(1, TNUM_PRCID + 1):
    EXCNO_VALID[prcid] = []
    EXCNO_GLOBAL[prcid] = {}
    for excno in excno_list:
        EXCNO_VALID[prcid].append((prcid << 16) | excno)
        EXCNO_GLOBAL[prcid][excno] = (prcid << 16) | excno

#
#  DEF_EXCで使用できるCPU例外ハンドラ番号
#
EXCNO_DEFEXC_VALID = {}
excno_list = [0, 1, 2, 3, 5, 6, 7]
for prcid in range(1, TNUM_PRCID + 1):
    EXCNO_DEFEXC_VALID[prcid] = []
    for excno in excno_list:
        EXCNO_DEFEXC_VALID[prcid].append((prcid << 16) | excno)


#
#  配置するセクションを指定した変数定義の生成
#
def DefineVariableSection(genFile, defvar, secname):
    if secname != "":
        genFile.add(f'{defvar} __attribute__((section("{secname}"),nocommon));')
    else:
        genFile.add(f"{defvar};")


#
#  カーネルのデータ領域のセクション名
#
def SecnameKernelData(cls):
    if cls != TCLS_NONE:
        return f".kernel_data_{clsData[cls]['clsid']}"
    else:
        return ""


#
#  スタック領域のセクション名
#
def SecnameStack(cls):
    if cls != TCLS_NONE:
        return f".stack_{clsData[cls]['clsid']}"
    else:
        return ""


#
#  ネイティブスピンロックの生成
#
def GenerateNativeSpn(params):
    kernelCfgC.add(f"LOCK _kernel_lock_{params['spnid']};")
    return f"((intptr_t) &_kernel_lock_{params['spnid']})"


#
#  割込み要求ライン設定テーブルを使うかどうか（未設定ならFalse）
#
if "USE_INTCFG_TABLE" not in globals():
    USE_INTCFG_TABLE = False

#
#  ターゲット非依存部のインクルード
#
IncludeTrb("kernel/kernel.py")

#
#  割込みハンドラテーブル
#
#  ★arm64_gcc 版と異なり，INHNO_VALID[prcid] を直接列挙せず，
#    0..TMAX_INHNO の連番を INHNO_GLOBAL[prcid][index] で実値へ変換する
#    （chip_kernel.py（Task 8 Step 4）が INTNO_GLOBAL/INHNO_GLOBAL を
#    dict として供給する）．
#
kernelCfgC.comment_header("Interrupt Handler Table")

for prcid in range(1, TNUM_PRCID + 1):
    kernelCfgC.add(
        f"const FP _kernel_inh_table_prc{prcid}"
        f"[TMAX_INHNO + 1] = {{")
    for index in range(TMAX_INHNO + 1):
        if index > 0:
            kernelCfgC.add(",")
        inhnoVal = INHNO_GLOBAL[prcid][index]
        kernelCfgC.append(f"\t/* 0x{inhnoVal:05x} */ ")
        if inhnoVal in cfgData["DEF_INH"]:
            kernelCfgC.append(
                f"(FP)({cfgData['DEF_INH'][inhnoVal]['inthdr']})")
            cfgData["DEF_INH"][inhnoVal]["index"] = index
        else:
            kernelCfgC.append("(FP)(_kernel_default_int_handler)")
    kernelCfgC.add()
    kernelCfgC.add2("};")

kernelCfgC.add("const FP* const _kernel_p_inh_table[TNUM_PRCID] = {")
for prcid in range(1, TNUM_PRCID + 1):
    if prcid > 1:
        kernelCfgC.add(",")
    kernelCfgC.append(f"\t_kernel_inh_table_prc{prcid}")
kernelCfgC.add()
kernelCfgC.add2("};")

#
#  割込み要求ライン設定テーブル
#
if USE_INTCFG_TABLE:
    kernelCfgC.comment_header("Interrupt Configuration Table")
    for prcid in range(1, TNUM_PRCID + 1):
        kernelCfgC.add(
            f"const uint8_t _kernel_intcfg_table_prc{prcid}"
            f"[TMAX_INTNO + 1] = {{")
        for index in range(TMAX_INTNO + 1):
            if index > 0:
                kernelCfgC.add(",")
            intnoVal = INTNO_GLOBAL[prcid][index]
            kernelCfgC.append(f"\t/* 0x{intnoVal:05x} */ ")
            if intnoVal in cfgData["CFG_INT"]:
                kernelCfgC.append("1U")
            else:
                kernelCfgC.append("0U")
        kernelCfgC.add()
        kernelCfgC.add2("};")

kernelCfgC.add("const uint8_t* const _kernel_p_intcfg_table[TNUM_PRCID] = {")
for prcid in range(1, TNUM_PRCID + 1):
    if prcid > 1:
        kernelCfgC.add(",")
    kernelCfgC.append(f"\t_kernel_intcfg_table_prc{prcid}")
kernelCfgC.add()
kernelCfgC.add2("};")

#
#  CPU例外ハンドラテーブル
#
kernelCfgC.comment_header("CPU Exception Handler Table")

for prcid in range(1, TNUM_PRCID + 1):
    kernelCfgC.add(
        f"const FP _kernel_exc_table_prc{prcid}"
        f"[TMAX_EXCNO + 1] = {{")
    for index in range(TMAX_EXCNO + 1):
        if index > 0:
            kernelCfgC.add(",")
        excnoVal = EXCNO_GLOBAL[prcid][index]
        kernelCfgC.append(f"\t/* 0x{excnoVal:05x} */ ")
        if excnoVal in cfgData["DEF_EXC"]:
            kernelCfgC.append(
                f"(FP)({cfgData['DEF_EXC'][excnoVal]['exchdr']})")
            cfgData["DEF_EXC"][excnoVal]["index"] = index
        else:
            kernelCfgC.append("(FP)(_kernel_default_exc_handler)")
    kernelCfgC.add()
    kernelCfgC.add2("};")

kernelCfgC.add("const FP* const _kernel_p_exc_table[TNUM_PRCID] = {")
for prcid in range(1, TNUM_PRCID + 1):
    if prcid > 1:
        kernelCfgC.add(",")
    kernelCfgC.append(f"\t_kernel_exc_table_prc{prcid}")
kernelCfgC.add()
kernelCfgC.add2("};")
```

- [ ] **Step 2: `arch/arm_gcc/common/core_check.py`を作成する**（原文`core_check.trb`全92行）

**arm64_gcc版（Task 4 Step 2）と`.trb`原文がコピーライト年・`$Id`・アーキ名以外バイト同一**（本タスク着手前に`diff`で確認済み）。Task 4で作った`arch/arm64_gcc/common/core_check.py`をコピーし，ヘッダコメントのみ書き換える。

```bash
cd /home/honda/TOPPERS/FMP3/fmp3_core
cp arch/arm64_gcc/common/core_check.py arch/arm_gcc/common/core_check.py
sed -i 's/コア依存部（ARM64用）/コア依存部（ARM用）/' arch/arm_gcc/common/core_check.py
```

- [ ] **Step 3: `arch/arm_gcc/common/core_offset.py`を作成する**（原文`core_offset.trb`全33行）

arm64版と違うフィールド名を使う（`T_EXCINF_cpsr`、`TINIB_tskatr`ありPCB_dspflg等の4フィールド無し）。全フィールドが常に定義される（nilガード不要）。

```python
# -*- coding: utf-8 -*-
#
#		オフセットファイル生成用テンプレートファイル（ARM用）
#
#  $Id: core_offset.py (converted from core_offset.trb by Claude Code Sonnet 5) $
#

#
#  ターゲット非依存部のインクルード
#
IncludeTrb("kernel/genoffset.py")

#
#  フィールドのオフセットの定義の生成
#
offsetH.append(f"""\
#define TCB_p_tinib\t\t{offsetof_TCB_p_tinib}
#define TCB_sp\t\t\t{offsetof_TCB_sp}
#define TCB_pc\t\t\t{offsetof_TCB_pc}
#define TINIB_tskatr\t{offsetof_TINIB_tskatr}
#define TINIB_exinf\t\t{offsetof_TINIB_exinf}
#define TINIB_task\t\t{offsetof_TINIB_task}
#define TINIB_stksz\t\t{offsetof_TINIB_stksz}
#define TINIB_stk\t\t{offsetof_TINIB_stk}
#define T_EXCINF_cpsr\t{offsetof_T_EXCINF_cpsr}
#define PCB_p_runtsk\t{offsetof_PCB_p_runtsk}
#define PCB_p_schedtsk\t{offsetof_PCB_p_schedtsk}
#define PCB_excpt_nest_count\t{offsetof_PCB_excpt_nest_count}
#define PCB_istkpt\t\t{offsetof_PCB_istkpt}
#define PCB_idstkpt\t\t{offsetof_PCB_idstkpt}
#define PCB_p_exc_tbl\t{offsetof_PCB_p_exc_tbl}
#define PCB_p_inh_tbl\t{offsetof_PCB_p_inh_tbl}
""")
```

- [ ] **Step 4: `arch/arm_gcc/zynqmp_r5/chip_kernel.py`を作成する**（原文`chip_kernel.trb`全39行）

Task 4 Step 5（`kria_arm64_gcc`の`chip_kernel.py`）と`INTNO_VALID`/`INHNO_VALID`/`INTNO_GLOBAL`/`INHNO_GLOBAL`の構築ロジックが**同一**（`private_intno_list`/`global_intno_list`の範囲も同じ0-31/32-186）。差は`IncludeTrb("gic_kernel.py")`が無いことと，先頭に`CDEF`から取り込むTNUM_PRCID範囲チェック相当のコメントが付く点のみ。

```python
# -*- coding: utf-8 -*-
#
#		パス2の生成スクリプトのチップ依存部（ZynqMP RPU用）
#
#  $Id: chip_kernel.py (converted from chip_kernel.trb by Claude Code Sonnet 5) $
#

#
#  使用できる割込み番号とそれに対応する割込みハンドラ番号
#
INTNO_VALID = {}
INHNO_VALID = {}
INTNO_GLOBAL = {}
INHNO_GLOBAL = {}
private_intno_list = list(range(0, 32))
global_intno_list = list(range(32, 187))
for prcid in range(1, TNUM_PRCID + 1):
    INTNO_VALID[prcid] = []
    INHNO_VALID[prcid] = []
    INTNO_GLOBAL[prcid] = {}
    INHNO_GLOBAL[prcid] = {}
    for intno in private_intno_list:
        INTNO_VALID[prcid].append((prcid << 16) | intno)
        INHNO_VALID[prcid].append((prcid << 16) | intno)
        INTNO_GLOBAL[prcid][intno] = (prcid << 16) | intno
        INHNO_GLOBAL[prcid][intno] = (prcid << 16) | intno
    for intno in global_intno_list:
        INTNO_VALID[prcid].append(intno)
        INHNO_VALID[prcid].append((prcid << 16) | intno)
        INTNO_GLOBAL[prcid][intno] = intno
        INHNO_GLOBAL[prcid][intno] = (prcid << 16) | intno

#
#  生成スクリプトのコア依存部
#
IncludeTrb("core_kernel.py")
```

- [ ] **Step 5: 構文チェック**

```bash
cd /home/honda/TOPPERS/FMP3/fmp3_core
for f in arch/arm_gcc/common/core_kernel.py arch/arm_gcc/common/core_check.py \
         arch/arm_gcc/common/core_offset.py arch/arm_gcc/zynqmp_r5/chip_kernel.py; do
    python3 -m py_compile "$f" && echo "OK $f" || echo "FAIL $f"
done
```
Expected: 4個すべて`OK`。

- [ ] **Step 6: `IncludeTrb`呼び出しの拡張子確認（negative control付き）**

```bash
cd /home/honda/TOPPERS/FMP3/fmp3_core
grep -n 'IncludeTrb(' arch/arm_gcc/common/*.py arch/arm_gcc/zynqmp_r5/*.py
! grep -n 'IncludeTrb(' arch/arm_gcc/common/*.py arch/arm_gcc/zynqmp_r5/*.py | grep -q '\.trb"'
echo "no-trb-leak exit=$?"
sed -i 's/IncludeTrb("core_kernel.py")/IncludeTrb("core_kernel.trb")/' arch/arm_gcc/zynqmp_r5/chip_kernel.py
grep -n 'IncludeTrb(' arch/arm_gcc/zynqmp_r5/chip_kernel.py | grep -q '\.trb"'
echo "negative-control-detected exit=$?"
git checkout -- arch/arm_gcc/zynqmp_r5/chip_kernel.py
```
Expected: `no-trb-leak exit=0`、`negative-control-detected exit=0`。

- [ ] **Step 7: negative control — `gic_kernel.trb`を意図的に持ち出していないことの確認**

```bash
cd /home/honda/TOPPERS/FMP3/fmp3_core
! grep -rq "gic_kernel" arch/arm_gcc/zynqmp_r5/chip_kernel.py arch/arm_gcc/common/core_kernel.py
echo "no-gic-kernel-reference exit=$? (expect 0)"
```
Expected: exit=0（`gic_kernel.py`は本ターゲットで作らない設計どおり）。

- [ ] **Step 8: 既存4構成の回帰**

```bash
cd /home/honda/TOPPERS/FMP3/fmp3_core
for p in polarfire_soc_kit-qemu musca_b1 musca_b1-2core rp2350_pico2; do
    tools/cfg_equivalence.sh build/$p
    echo "$p cfg_equivalence exit=$?"
done
```
Expected: 4構成ともexit=0（無変化）。

- [ ] **Step 9: コミット**

```bash
cd /home/honda/TOPPERS/FMP3/fmp3_core
git add arch/arm_gcc/common/core_kernel.py arch/arm_gcc/common/core_check.py \
        arch/arm_gcc/common/core_offset.py arch/arm_gcc/zynqmp_r5/chip_kernel.py
git commit -m "build(cfg): arch/arm_gcc/** を新規移植（kria_r5 core/chip層、435行、前例なし）

core_check.pyはarch/arm64_gcc/common/core_check.pyと.trb原文がコメント
以外バイト同一のため流用。core_kernel.pyはarm64版と異なりTMAX_INTNO/
INHNO/EXCNO軸のGLOBAL辞書参照方式（0.upto(N).each_with_indexの恒等
変換をrange(N+1)へ単純化）。gic_kernel.trbは本ターゲットのIncludeTrb
連鎖に入らないため移植しない（zynqmp_r5/chip_kernel.trbが呼んでいない
ことを確認済み）。意味検証はTask 11へ。"
```

---

### Task 9: `kria_r5_gcc` — target層cfgテンプレート移植

**Files:**
- Create: `target/kria_r5_gcc/target_kernel.py`
- Create: `target/kria_r5_gcc/target_class.py`
- Create: `target/kria_r5_gcc/target_check.py`

**Interfaces:**
- Consumes: `arch/arm_gcc/zynqmp_r5/chip_kernel.py`・`arch/arm_gcc/common/core_check.py`（Task 8）。
- Produces: `FMP3_KERNEL_CFG_TRB_FILES`/`FMP3_CLASS_TRB_FILES`/`FMP3_CHECK_TRB_FILES`が指す実体（Task 10の`target.cmake`がConsumes）。

- [ ] **Step 1: `target/kria_r5_gcc/target_kernel.py`と`target_check.py`を作成する**（原文各11行）

```python
# -*- coding: utf-8 -*-
#
#		パス2の生成スクリプトのターゲット依存部（KRIA Cortex-R5F用）
#
#  $Id: target_kernel.py (converted from target_kernel.trb by Claude Code Sonnet 5) $
#

#
#  生成スクリプトのチップ依存部
#
IncludeTrb("chip_kernel.py")
```

```python
# -*- coding: utf-8 -*-
#
#		チェックパスの生成スクリプトのターゲット依存部
#
#  $Id: target_check.py (converted from target_check.trb by Claude Code Sonnet 5) $
#

#
#  生成スクリプトのコア依存部（チップ依存部は飛ばす）
#
IncludeTrb("core_check.py")
```

★原文`target_kernel.trb`のヘッダコメントは`（Zybo用）`という古いターゲットからのコピー&ペーストの残骸（`$Id`が`ertl-honda`の別コミットを指しており，`kria_r5_gcc`用に書き直されていない）。**pristineはそのまま残す**（改変対象ではない）が，本タスクで新規に書く`.py`側は正確なコメント（`KRIA Cortex-R5F用`）で書く（既存の`.py`移植でも忠実な翻訳とは「誤字も含めた一字一句の転写」ではなく「意味的に正しいコメント」を意味することは，`target/musca_b1_gcc/target_kernel.py`等の既存移植でも同様）。

- [ ] **Step 2: `target/kria_r5_gcc/target_class.py`を作成する**（原文`target_class.trb`全34行）

`target/musca_b1_gcc/target_class.py`（既存）と**`.trb`原文がコメント以外バイト同一**（`diff target/kria_r5_gcc/target_class.trb target/musca_b1_gcc/target_class.trb`相当の内容確認済み。両ターゲットともTNUM_PRCID 1〜2の`clsData`構造：`CLS_PRC1`＋`CLS_ALL_PRC1`、`CLS_PRC1/PRC2`＋`CLS_ALL_PRC1/PRC2`）。コピーしてヘッダコメントのみ書き換える。

```bash
cd /home/honda/TOPPERS/FMP3/fmp3_core
cp target/musca_b1_gcc/target_class.py target/kria_r5_gcc/target_class.py
sed -i 's/ターゲット依存のクラス定義（ARM Musca-B1用）/ターゲット依存のクラス定義（ZynqMP R5用）/' \
    target/kria_r5_gcc/target_class.py
```

- [ ] **Step 3: 構文チェック**

```bash
cd /home/honda/TOPPERS/FMP3/fmp3_core
for f in target/kria_r5_gcc/target_kernel.py target/kria_r5_gcc/target_check.py \
         target/kria_r5_gcc/target_class.py; do
    python3 -m py_compile "$f" && echo "OK $f" || echo "FAIL $f"
done
```
Expected: 3個すべて`OK`。

- [ ] **Step 4: negative control — コピーが実際にmusca_b1と中身が異なることを確認する**

```bash
cd /home/honda/TOPPERS/FMP3/fmp3_core
diff target/musca_b1_gcc/target_class.py target/kria_r5_gcc/target_class.py
echo "diff exit=$? (expect 1)"
```

- [ ] **Step 5: コミット**

```bash
cd /home/honda/TOPPERS/FMP3/fmp3_core
git add target/kria_r5_gcc/target_kernel.py target/kria_r5_gcc/target_check.py \
        target/kria_r5_gcc/target_class.py
git commit -m "build(cfg): target/kria_r5_gcc/*.py を新規移植（target層、56行）

target_class.pyはtarget/musca_b1_gcc/target_class.pyと.trb原文が
コメント以外バイト同一（TNUM_PRCID 1-2のclsData構造が完全一致）のため
流用。target_kernel/target_checkはIncludeTrbのみの薄いファイル
（pristineのtarget_kernel.trbヘッダコメントはZybo用の残骸だが，
新規に書く.py側は正確なコメントで書いた）。意味検証はTask 11へ。"
```

---

### Task 10: `kria_r5_gcc` — CMake層

**Files:**
- Create: `arch/arm_gcc/common/arch.cmake`
- Create: `arch/arm_gcc/zynqmp_r5/chip.cmake`
- Create: `target/kria_r5_gcc/target.cmake`
- Create: `target/kria_r5_gcc/presets.json`
- Modify: `CMakePresets.json`

**Interfaces:**
- Consumes: `cmake/toolchain-arm-none-eabi.cmake`（既存，Task 1で使ったのと同じファイル。プレフィックスは既定`arm-none-eabi-`のまま）。Task 8/9の`.py`一式。
- Produces: `FMP3_TARGET=kria_r5_gcc`でconfigureできるプリセット`kria_r5`。

- [ ] **Step 1: `arch/arm_gcc/common/arch.cmake`を作成する**

`Makefile.core`のCMake版。riscv_gcc/arm_m_gccと同じ構造（`START_OBJS`/`-nostdlib`はこの層にある。arm64_gccとは異なる点に注意）。

```cmake
#
#		アーキテクチャ依存部の CMake 定義（ARM(Cortex-R/A32) コア共通）
#
#  chip.cmake から include される（上流 Makefile.core に相当）．
#
#  本ファイルは常に fmp3_core（このリポジトリ）側にある共通コア層なので，
#  ${FMP3_ROOT_DIR} 基準で自己解決する．
#
set(COREDIR ${FMP3_ROOT_DIR}/arch/arm_gcc/common)
set(TOOLDIR ${FMP3_ROOT_DIR}/arch/gcc)

#  Makefile.core:33  CFG_TABS
list(APPEND FMP3_SYMVAL_TABLES
    ${COREDIR}/core_sym.def
)

#  Makefile.core:38  TARGET_OFFSET_TRB
list(APPEND FMP3_OFFSET_TRB_FILES
    ${COREDIR}/core_offset.py
)

#  Makefile.core:19-20
list(APPEND FMP3_INCLUDE_DIRS
    ${COREDIR}
    ${TOOLDIR}
)

#  Makefile.core:27-28  KERNEL_ASMOBJS core_support.o / KERNEL_COBJS core_kernel_impl.o
list(APPEND FMP3_ARCH_C_FILES
    ${COREDIR}/core_kernel_impl.c
    ${COREDIR}/core_support.S
)

#  Makefile.core:45  START_OBJS := start.o $(START_OBJS)
#  ★kria_r5_gcc では zynqmp_r5/Makefile.chip が START_OBJS の先頭に
#    chip_support.o を積んでから本ファイルが include される（上流Make
#    の評価順）ため，最終的な START_OBJS は「chip_support.o start.o」
#    に相当する．CMakeのリストは追加順のみが意味を持ち，実際にどちらが
#    ENTRYになるかはリンカスクリプトのENTRY()ディレクティブ／既定の
#    _start解決で決まるため，ここでは順序を上流と厳密一致させることに
#    こだわらず両方をFMP3_START_FILESに含める．
list(APPEND FMP3_START_FILES
    ${COREDIR}/start.S
)

#  Makefile.core:51  LDFLAGS := -nostdlib $(LDFLAGS)
list(APPEND FMP3_LINK_OPTIONS -nostdlib)

#  Makefile.core:22（-lgcc のみ．libc は非リンク）
list(APPEND FMP3_LINK_LIBS gcc)
```

- [ ] **Step 2: `arch/arm_gcc/zynqmp_r5/chip.cmake`を作成する**

```cmake
#
#		チップ依存部の CMake 定義（ZynqMP RPU用）
#
#  target.cmake から include される（上流 Makefile.chip に相当）．
#
set(COREDIR ${ARCHDIR}/common)

#
#  コンパイルオプション（Makefile.chip:16-19）
#
#  Cortex-R5（Rプロファイル）向け．FPU関連はターゲット依存部（target.cmake）
#  が指定する．
#
list(APPEND FMP3_COMPILE_OPTIONS -mcpu=cortex-r5)
list(APPEND FMP3_LINK_OPTIONS -mcpu=cortex-r5)
list(APPEND FMP3_COMPILE_DEFS
    __TARGET_ARCH_ARM=7
    __TARGET_PROFILE_R
    TOPPERS_CORTEX_R5
    TARGET_RESET_ENTRY=start_r5
)

list(APPEND FMP3_INCLUDE_DIRS ${CHIPDIR})

#
#  カーネルに関する定義（Makefile.chip:24-26）
#
list(APPEND FMP3_ARCH_C_FILES
    ${CHIPDIR}/chip_kernel_impl.c
    ${CHIPDIR}/gic_kernel_impl.c
    ${CHIPDIR}/ttc_hrt.c
)
list(APPEND FMP3_START_FILES
    ${CHIPDIR}/chip_support.S
)

#
#  非TECS版 SIO ドライバ（Makefile.chip:34、Cadence UART。SYSSVC_DIRS）
#
list(APPEND FMP3_SYSSVC_TARGET_C_FILES
    ${CHIPDIR}/chip_serial.c
    ${CHIPDIR}/xuartps.c
)

#
#  コア依存部（Makefile.chip:39）
#
include(${COREDIR}/arch.cmake)
```

★`Makefile.chip`は`gic_support.o`もKERNEL_ASMOBJSへ追加している（`arch/arm_gcc/common/gic_support.S`, `Makefile.chip:26`）。これは`FMP3_ARCH_C_FILES`ではなく`FMP3_START_FILES`ではなく専ら「libfmp3.a側」のオブジェクトなので`FMP3_ARCH_C_FILES`に含める：

```cmake
list(APPEND FMP3_ARCH_C_FILES
    ${COREDIR}/gic_support.S
)
```
これを上記`chip.cmake`のカーネル関連ブロック（`chip_kernel_impl.c`等の並び）に追記すること。

- [ ] **Step 3: `target/kria_r5_gcc/target.cmake`を作成する**

```cmake
#
#		ターゲット依存部の CMake 定義（KRIA SOM Cortex-R5F / QEMU 用）
#
#  上流 target/kria_r5_gcc/Makefile.target の CMake 版．
#
set(ARCHDIR ${FMP3_ROOT_DIR}/arch/arm_gcc)
get_filename_component(CHIPDIR ${CMAKE_CURRENT_LIST_DIR}/../../arch/arm_gcc/zynqmp_r5 ABSOLUTE)
set(TARGETDIR ${CMAKE_CURRENT_LIST_DIR})

#
#  対象 Kria ボード（Makefile.target:16-30）
#
set(FMP3_BOARD "kr260" CACHE STRING "Kria board name (kr260 / kv260 / kd240)")
set_property(CACHE FMP3_BOARD PROPERTY STRINGS kr260 kv260 kd240)
if(FMP3_BOARD STREQUAL "kr260")
    list(APPEND FMP3_COMPILE_DEFS TOPPERS_KRIA_KR260)
elseif(FMP3_BOARD STREQUAL "kv260")
    list(APPEND FMP3_COMPILE_DEFS TOPPERS_KRIA_KV260)
elseif(FMP3_BOARD STREQUAL "kd240")
    list(APPEND FMP3_COMPILE_DEFS TOPPERS_KRIA_KD240)
else()
    message(FATAL_ERROR "FMP3_BOARD must be kr260, kv260 or kd240 (got: ${FMP3_BOARD})")
endif()

#
#  FPUサポートとABI（Makefile.target:32-38）
#
#  Cortex-R5F は VFPv3-D16 を持つ．-mfloat-abi=hard を使う．
#
list(APPEND FMP3_COMPILE_DEFS USE_ARM_FPU_ALWAYS)
list(APPEND FMP3_COMPILE_OPTIONS -mfpu=vfpv3-d16 -mfloat-abi=hard)
list(APPEND FMP3_LINK_OPTIONS -mfpu=vfpv3-d16 -mfloat-abi=hard)

#
#  QEMU に関する定義（Makefile.target:40-51）
#
#  QEMU（xlnx-zcu102）のTTCモデルは133MHz固定（実機は100MHz）．既定は
#  QEMU向け（上流 Makefile.target 自身の既定 QEMU ?= true と同じ）．
#  実機向けには -DFMP3_KRIA_R5_QEMU=OFF でconfigureする．
#
option(FMP3_KRIA_R5_QEMU "Build for QEMU xlnx-zcu102 TTC clock (OFF: real Kria board, 100MHz TTC)" ON)
if(FMP3_KRIA_R5_QEMU)
    list(APPEND FMP3_COMPILE_DEFS TOPPERS_USE_QEMU TTC_CLK_HZ=133000000)
endif()

#  Makefile.target:56-59
list(APPEND FMP3_INCLUDE_DIRS ${TARGETDIR})
list(APPEND FMP3_COMPILE_OPTIONS -mlittle-endian)
list(APPEND FMP3_LINK_OPTIONS -mlittle-endian)
list(APPEND FMP3_COMPILE_DEFS
    USE_BYPASS_IPI_DISPATCH_HANDER
    TOPPERS_OMIT_USE_WFE
)

#
#  カーネルに含めるターゲット依存ソース（Makefile.target:64-65）
#
list(APPEND FMP3_TARGET_C_FILES
    ${TARGETDIR}/target_kernel_impl.c
)

#  Makefile.target:70
set(FMP3_LDSCRIPT ${TARGETDIR}/kria_r5.ld)

#
#  cfg に渡すファイル
#
list(APPEND FMP3_CFG_FILES            ${TARGETDIR}/target_kernel.cfg)
list(APPEND FMP3_CLASS_TRB_FILES      ${TARGETDIR}/target_class.py)
list(APPEND FMP3_KERNEL_CFG_TRB_FILES ${TARGETDIR}/target_kernel.py)
list(APPEND FMP3_CHECK_TRB_FILES      ${TARGETDIR}/target_check.py)

#
#  チップ依存部
#
include(${CHIPDIR}/chip.cmake)

#
#  QEMU（xlnx-zcu102, RPUクラスタ）．上流Makefile.target:87-102の
#  runqu/runqugターゲットをそのまま翻訳する．v11.0以降が必要（上流
#  コメント自身が明記）．
#
set(_fmp3_kria_r5_qemu_builtin /home/honda/qemu-build/qemu-11.0.1/build-a64/qemu-system-aarch64)
if(EXISTS ${_fmp3_kria_r5_qemu_builtin})
    set(_fmp3_kria_r5_qemu_default ${_fmp3_kria_r5_qemu_builtin})
else()
    set(_fmp3_kria_r5_qemu_default qemu-system-aarch64)
endif()
set(QEMU_SYSTEM_AARCH64_KRIA_R5 ${_fmp3_kria_r5_qemu_default} CACHE STRING
    "Path to qemu-system-aarch64 for the xlnx-zcu102 RPU cluster (needs >= 11.0.1)")
unset(_fmp3_kria_r5_qemu_builtin)
unset(_fmp3_kria_r5_qemu_default)

#
#  Makefile.target:89-94 (runqu) の翻訳．
#  ★-smp 6 必須（R5Fが2個生成されるのは smp が A53(4個)を超えたときのみ，
#    xlnx-zynqmp.c:210-217 の xlnx_zynqmp_get_rpu_number()）．
#  ★boot-cpu=rpu-cpu[0] でRPU0を電源ON，mp-affinity=0で実機同様Aff0=0．
#  ★cpu-num=4：グローバルCPU index（A53が0-3、RPUが4,5）でRPU0を指す．
#
set(FMP3_RUN_COMMAND
    ${QEMU_SYSTEM_AARCH64_KRIA_R5} -M xlnx-zcu102 -smp 6 -m 2G -nographic
    -global xlnx-zynqmp.boot-cpu=rpu-cpu[0]
    -global cortex-r5f-arm-cpu.mp-affinity=0
    -device loader,file=$<TARGET_FILE:fmp>,cpu-num=4
    -serial null -serial mon:stdio
)
```

- [ ] **Step 4: `target/kria_r5_gcc/presets.json`を作成する**

```json
{
  "version": 4,
  "include": [
    "../../cmake/presets-base.json"
  ],
  "configurePresets": [
    {
      "name": "kria_r5",
      "inherits": "_base",
      "displayName": "KRIA SOM Cortex-R5F (QEMU xlnx-zcu102 RPU cluster, 1 processor / lockstep)",
      "description": "上流Makefile.target付属のrunquレシピをそのまま踏襲。QEMU >= 11.0.1 が必要",
      "toolchainFile": "${sourceDir}/cmake/toolchain-arm-none-eabi.cmake",
      "cacheVariables": {
        "FMP3_TARGET": "kria_r5_gcc"
      }
    },
    {
      "name": "kria_r5-2core",
      "inherits": "kria_r5",
      "displayName": "KRIA SOM Cortex-R5F (QEMU xlnx-zcu102 RPU cluster, 2 processors split mode)",
      "description": "2コアsplitモード（TNUM_PRCID=2）",
      "cacheVariables": {
        "FMP3_PRC_NUM": "2"
      }
    }
  ],
  "buildPresets": [
    { "name": "kria_r5",        "configurePreset": "kria_r5" },
    { "name": "kria_r5-2core",  "configurePreset": "kria_r5-2core" },
    {
      "name": "run-kria_r5",
      "configurePreset": "kria_r5",
      "targets": [ "run" ]
    },
    {
      "name": "run-kria_r5-2core",
      "configurePreset": "kria_r5-2core",
      "targets": [ "run" ]
    }
  ]
}
```

- [ ] **Step 5: `CMakePresets.json`に追加する**

```json
  "include": [
    "target/polarfire_soc_kit_gcc/presets.json",
    "target/musca_b1_gcc/presets.json",
    "target/rp2350_pico2_gcc/presets.json",
    "target/kria_arm64_gcc/presets.json",
    "target/kria_r5_gcc/presets.json"
  ]
```

- [ ] **Step 6: configure + build（1コア）**

```bash
cd /home/honda/TOPPERS/FMP3/fmp3_core
cmake --preset kria_r5
cmake --build build/kria_r5 2>&1 | tail -60
echo "build exit=$?"
```
Expected: exit=0。

- [ ] **Step 7: positive control — `cfg1_out.srec`（arm_gccはDUMP既定`srec`のまま）が生成されていることを確認**

```bash
ls build/kria_r5/generated/cfg1_out.srec
grep -c TOPPERS_magic_number build/kria_r5/generated/cfg1_out.syms
```
Expected: ファイル存在＋`1`。

- [ ] **Step 8: 既存構成（rp2350, kria_arm64含む）の回帰**

```bash
cd /home/honda/TOPPERS/FMP3/fmp3_core
for p in polarfire_soc_kit-qemu musca_b1 musca_b1-2core rp2350_pico2 kria_arm64-1core; do
    tools/cfg_equivalence.sh build/$p
    echo "$p cfg_equivalence exit=$?"
done
```
Expected: 全てexit=0。

- [ ] **Step 9: コミット**

```bash
cd /home/honda/TOPPERS/FMP3/fmp3_core
git add arch/arm_gcc/common/arch.cmake arch/arm_gcc/zynqmp_r5/chip.cmake \
        target/kria_r5_gcc/target.cmake target/kria_r5_gcc/presets.json CMakePresets.json
git commit -m "build(cmake): kria_r5_gcc のCMake層を追加

arch.cmake（riscv_gcc/arm_m_gccと同じくSTART_OBJS/-nostdlibをここで
積む点がarm64_gccと異なる）+ chip.cmake（Cortex-R5、gic_kernel_impl.c/
ttc_hrt.c/gic_support.S）+ target.cmake（上流Makefile.target自身の
runqu/runqugレシピをFMP3_RUN_COMMANDへそのまま翻訳。QEMU 11.0.1+が
必要）。1コアでビルド成功・cfg1_out.srec/magic_number確認。既存5構成は
無回帰。意味検証・QEMU実行検証はTask 11/12へ。"
```

---

### Task 11: `kria_r5_gcc` — ビルド・差分等価性検査・回帰

**Files:**
- No new files（検証のみ）

**Interfaces:**
- Consumes: Task 8-10の全成果物。

- [ ] **Step 1: 差分等価性検査（1コア）**

```bash
cd /home/honda/TOPPERS/FMP3/fmp3_core
tools/cfg_equivalence.sh build/kria_r5
echo "cfg_equivalence exit=$?"
```
Expected: exit=0。

- [ ] **Step 2: negative control — テンプレートを1箇所壊して差分が検出されることを確認**

`arch/arm_gcc/common/core_kernel.py`の`EXCNO_DEFEXC_VALID`用リストを壊す（`DEF_EXC`を使うサンプルが無い場合デッドコードになりうるため，先に生きている枝を確認する）。より確実なのは`core_offset.py`の値を壊すこと（offset.hは必ず生成され両エンジンとも通る値なので確実に生きている）。

```bash
cd /home/honda/TOPPERS/FMP3/fmp3_core
cp arch/arm_gcc/common/core_offset.py /tmp/core_offset.py.bak
sed -i 's/#define TCB_sp\\t\\t\\t{offsetof_TCB_sp}/#define TCB_sp\\t\\t\\t99999/' arch/arm_gcc/common/core_offset.py
diff /tmp/core_offset.py.bak arch/arm_gcc/common/core_offset.py
rm -rf build/kria_r5-neg && cmake --preset kria_r5 -B build/kria_r5-neg
tools/cfg_equivalence.sh build/kria_r5-neg
echo "negative cfg_equivalence exit=$? (expect 1)"
cp /tmp/core_offset.py.bak arch/arm_gcc/common/core_offset.py
rm -rf build/kria_r5-neg /tmp/core_offset.py.bak
```
Expected: exit=1。`diff`で意図通りの1行差分になっていることを確認してから流す。

- [ ] **Step 3: `tools/cfg_error_tests/run.sh`の回帰**

```bash
cd /home/honda/TOPPERS/FMP3/fmp3_core
tools/cfg_error_tests/run.sh build/kria_r5
echo "cfg_error_tests exit=$?"
```
Expected: exit=0。

★`kria_r5_gcc`は`TargetCheckCfgInt`が未定義（Task 8で確認済み）のため，`E_RSATR`（複数プロセッサでの割込み受付制限）エラーテストが該当するなら**構造的に発火しない**可能性がある。`run.sh`のテストケース一覧を見て，該当ケースがあれば「polarfireのPLIC同様，このターゲットでは到達不能」という趣旨を`tools/cfg_error_tests/`のREADME等（無ければコメント）に記録する。既存のE_RSATRテストが失敗するなら，polarfire同様の「構造的に到達不能」注記が必要（`.superpowers/sdd/progress.md`のB Task 10「CFG_INTのE_RSATR検査はpolarfireのPLICでは構造的に到達不能」と同型の扱い）。

- [ ] **Step 4: 2コア（split mode）でビルド**

```bash
cd /home/honda/TOPPERS/FMP3/fmp3_core
cmake --preset kria_r5-2core
cmake --build build/kria_r5-2core 2>&1 | tail -60
echo "build exit=$?"
tools/cfg_equivalence.sh build/kria_r5-2core
echo "cfg_equivalence exit=$?"
```
Expected: 両方exit=0。

- [ ] **Step 5: 全構成の回帰（最終）**

```bash
cd /home/honda/TOPPERS/FMP3/fmp3_core
for p in polarfire_soc_kit-qemu musca_b1 musca_b1-2core rp2350_pico2 kria_arm64-1core; do
    tools/cfg_equivalence.sh build/$p
    echo "$p cfg_equivalence exit=$?"
done
```
Expected: 全てexit=0。

- [ ] **Step 6: コミット（コード変更が無ければ不要）**

検証のみのTaskのため，コード変更が生じなければコミット不要。Step 3で記録が必要になった場合のみコミットする。

---

### Task 12: `kria_r5_gcc` — QEMU実行検証（上流`runqu`の翻訳）

**Files:**
- Modify: `DIVERGENCE_MAP.md`（実測結果の記録）

**Interfaces:**
- Consumes: Task 10の`FMP3_RUN_COMMAND`。

- [ ] **Step 1: QEMUバージョン確認**

```bash
/home/honda/qemu-build/qemu-11.0.1/build-a64/qemu-system-aarch64 --version
```
Expected: `QEMU emulator version 11.0.1`。

- [ ] **Step 2: 1コア（lockstep相当）でQEMU実行（タイムアウト必須）**

```bash
cd /home/honda/TOPPERS/FMP3/fmp3_core
QEMU=/home/honda/qemu-build/qemu-11.0.1/build-a64/qemu-system-aarch64
timeout 20 $QEMU -M xlnx-zcu102 -smp 6 -m 2G -nographic \
    -global xlnx-zynqmp.boot-cpu=rpu-cpu[0] \
    -global cortex-r5f-arm-cpu.mp-affinity=0 \
    -device loader,file=build/kria_r5/fmp,cpu-num=4 \
    -serial null -serial mon:stdio \
    -d guest_errors,unimp -D /tmp/kria_r5_qemu.log
echo "qemu rc=$? (124/137ならタイムアウトで正常継続中)"
tail -30 /tmp/kria_r5_qemu.log
ps -eo pid,comm | grep qemu-system || echo "no leftover qemu process"
```

**判定基準（断定しない）**:
- バナー（`TOPPERS/FMP3 Kernel Release ...`・`Processor 1 start.`相当）が出る → 成功。Step 4へ。
- バナーが出ない → `/tmp/kria_r5_qemu.log`を確認する。事前調査で判明済みの候補は`RPU_RPU_GLBL_CNTL`（`0xFF9A0000`）への無条件アクセス（`target_kernel_impl.c:183-188`）。ログにこのアドレス付近の`Unassigned mem read`等が出ていれば，`kria_arm64_gcc`のSTG同様の`TOPPERS_USE_QEMU`ガードを検討する（Step 3）。

- [ ] **Step 3（条件付き）: `RPU_RPU_GLBL_CNTL`アクセスに`TOPPERS_USE_QEMU`ガードを追加する（Step 2でクラッシュが実際に確認された場合のみ）**

`target/kria_r5_gcc/target_kernel_impl.c:180-188`のフォールトログ有効化ブロックを次のように変更する：

```c
	/*
	 *  lockstepモードの場合，フォールトログを有効にする．
	 */
#ifndef TOPPERS_USE_QEMU
	if ((sil_rew_mem((uint32_t *) RPU_RPU_GLBL_CNTL)
							& RPU_RPU_GLBL_CNTL_SLSPLIT_MASK) == 0U) {
		sil_wrw_mem((uint32_t *) RPU_RPU_ERR_INJ,
					sil_rew_mem((uint32_t *) RPU_RPU_ERR_INJ)
									| RPU_RPU_ERR_INJ_FAULTLOGENABLE);
	}
#endif /* TOPPERS_USE_QEMU */
```

`target.cmake`は既に`FMP3_KRIA_R5_QEMU`が真のとき`TOPPERS_USE_QEMU`を定義済み（Task 10 Step 3）なので追加の変数定義は不要。修正後Step 2を再実行してバナーが出ることを確認する。negative control（`git stash`で修正を外して再現）も行う（Task 7 Step 6と同じ要領）。

- [ ] **Step 4: 2コア（split mode）でQEMU実行**

`runqu`は1コア（lockstep, `cpu-num=4`のみ）専用のため，2コアは`-device loader`をRPU1（`cpu-num=5`）にも追加する必要がある。上流に2コア用のレシピは無いため，1コア版から類推する：

```bash
cd /home/honda/TOPPERS/FMP3/fmp3_core
QEMU=/home/honda/qemu-build/qemu-11.0.1/build-a64/qemu-system-aarch64
timeout 20 $QEMU -M xlnx-zcu102 -smp 6 -m 2G -nographic \
    -global xlnx-zynqmp.boot-cpu=rpu-cpu[0] \
    -global cortex-r5f-arm-cpu.mp-affinity=0 \
    -device loader,file=build/kria_r5-2core/fmp,cpu-num=4 \
    -device loader,file=build/kria_r5-2core/fmp,cpu-num=5 \
    -serial null -serial mon:stdio \
    -d guest_errors,unimp -D /tmp/kria_r5_qemu_2core.log
echo "qemu rc=$?"
tail -40 /tmp/kria_r5_qemu_2core.log
ps -eo pid,comm | grep qemu-system || echo "no leftover qemu process"
```

**これは上流に前例が無い構成のため，1コアより結果への確信度は低い。** 起動しない場合はDIVERGENCE_MAP.mdの未解決事項へ記録し，1コア構成のみを本計画の達成範囲としてよい（無理に解決を引き延ばさない）。

- [ ] **Step 5: `DIVERGENCE_MAP.md`を更新する**

Step 3でpristineを改変した場合は`add`表へ1行追記。Step 4が未解決のまま残った場合は「未解決事項」節へ記録する。

- [ ] **Step 6: コミット**

```bash
cd /home/honda/TOPPERS/FMP3/fmp3_core
git add DIVERGENCE_MAP.md
# Step 3でpristineを直した場合のみ追加:
# git add target/kria_r5_gcc/target_kernel_impl.c
git commit -m "test(kria_r5): QEMU実行検証(上流runquレシピの翻訳)を実施

1コア(lockstep相当)は[実測結果を記述]。RPU_RPU_GLBL_CNTL(0xFF9A0000)
への無条件アクセスは[実測結果に応じて記述]。2コア(split mode)は
[実測結果を記述、上流に前例が無いため未解決なら記録に留める]。"
```

---

### Task 13: 最終回帰・`DIVERGENCE_MAP.md`更新・完了条件チェック

**Files:**
- Modify: `DIVERGENCE_MAP.md`
- Modify: `CLAUDE.md`（現況セクションがあれば更新）

**Interfaces:**
- Consumes: Task 1-12の全成果物。

- [ ] **Step 1: 全構成（8構成）のフルクリーンビルド**

```bash
cd /home/honda/TOPPERS/FMP3/fmp3_core
for p in polarfire_soc_kit-qemu musca_b1 musca_b1-2core rp2350_pico2 \
         kria_arm64-1core kria_arm64 kria_r5 kria_r5-2core; do
    rm -rf build/$p
    cmake --preset $p
    cmake --build build/$p 2>&1 | tail -20
    echo "=== $p build exit=$? ==="
done
```
Expected: 8構成すべてexit=0。**このステップでいずれかが壊れたら，該当タスクへ戻って直す**（Task 13で新たな問題を場当たり的に直さない）。

- [ ] **Step 2: 全構成の差分等価性検査**

```bash
cd /home/honda/TOPPERS/FMP3/fmp3_core
for p in polarfire_soc_kit-qemu musca_b1 musca_b1-2core rp2350_pico2 \
         kria_arm64-1core kria_arm64 kria_r5 kria_r5-2core; do
    tools/cfg_equivalence.sh build/$p
    echo "=== $p cfg_equivalence exit=$? ==="
done
```
Expected: 8構成すべてexit=0。

- [ ] **Step 3: 全構成の`cfg_error_tests`回帰**

```bash
cd /home/honda/TOPPERS/FMP3/fmp3_core
for p in polarfire_soc_kit-qemu musca_b1 musca_b1-2core rp2350_pico2 \
         kria_arm64-1core kria_arm64 kria_r5 kria_r5-2core; do
    tools/cfg_error_tests/run.sh build/$p
    echo "=== $p cfg_error_tests exit=$? ==="
done
```
Expected: 8構成すべてexit=0。

- [ ] **Step 4: 実行検証のサマリを再確認（QEMUが使える構成のみ）**

```bash
cd /home/honda/TOPPERS/FMP3/fmp3_core
# polarfire（5ハート、既存）
timeout 20 qemu-system-riscv64 -M microchip-icicle-kit -smp 5 -m 2G -nographic \
    -serial mon:stdio -bios none -kernel build/polarfire_soc_kit-qemu/fmp \
    -device loader,file=build/polarfire_soc_kit-qemu/fmp,cpu-num=0 \
    -device loader,file=build/polarfire_soc_kit-qemu/fmp,cpu-num=1 \
    -device loader,file=build/polarfire_soc_kit-qemu/fmp,cpu-num=2 \
    -device loader,file=build/polarfire_soc_kit-qemu/fmp,cpu-num=3 \
    -device loader,file=build/polarfire_soc_kit-qemu/fmp,cpu-num=4 | grep -c "Processor .* start\."
# musca_b1（既存2構成）・kria_arm64（Task 7で確立した経路）・kria_r5（Task 12）は
# 各自のFMP3_RUN_COMMANDで同様に確認する（cmake --build build/<preset> --target run）
ps -eo pid,comm | grep qemu-system || echo "no leftover qemu process"
```
Expected: polarfireは`4`（Processor 1/3/2/4 start.の4行）。他構成はTask 7/12で確立した期待値どおり。**プロセス残留が無いことを必ず確認する。**

- [ ] **Step 5: `DIVERGENCE_MAP.md`の`add`表に本計画で追加したpristine配下の新規ファイルを一括記録する**

```bash
cd /home/honda/TOPPERS/FMP3/fmp3_core
git log --diff-filter=A --name-only --pretty=format: -- \
    'arch/arm_m_gcc/rp2350/*.py' 'arch/arm_m_gcc/rp2350/*.cmake' \
    'arch/arm64_gcc/**/*.py' 'arch/arm64_gcc/**/*.cmake' \
    'arch/arm_gcc/**/*.py' 'arch/arm_gcc/**/*.cmake' \
    'target/rp2350_pico2_gcc/*.py' 'target/rp2350_pico2_gcc/*.cmake' \
    'target/kria_arm64_gcc/*.py' 'target/kria_arm64_gcc/*.cmake' \
    'target/kria_r5_gcc/*.py' 'target/kria_r5_gcc/*.cmake' \
    | sort -u
```

この出力（漏れ・余剰なし）を`DIVERGENCE_MAP.md`の`add`表へ，計画A2/B完了時と同じ書式（対象・種別`add`・理由・上流報告`-`）で追記する。9行程度になる想定（rp2350: chip_kernel.py+chip.cmake, target*.py×3+target.cmake の計6 + kria_arm64: core/chip/gic層5+target*.py 3+arch.cmake+chip.cmake+target.cmake の計11 + kria_r5: core/chip層4+target*.py3+arch.cmake+chip.cmake+target.cmake の計10。実際の出力を正とし，予想件数と食い違ったら出力を優先する）。

- [ ] **Step 6: `AGENTS.md` §6完了条件チェックリストを本計画の範囲で確認する**

```bash
cd /home/honda/TOPPERS/FMP3/fmp3_core
# upstreamブランチにpristineのみか
git ls-tree -r --name-only upstream | grep -E '\.(cmake|py)$|presets\.json$|CMakeLists\.txt$' | head
echo "（何も出なければOK）"
# pristineへの改変がDIVERGENCE_MAP.mdにあるか（本タスクで確認したTask 7/12のpristine改変も含む）
grep -c "kria_arm64\|kria_r5\|rp2350" DIVERGENCE_MAP.md
```
Expected: 1行目は空（`upstream`に派生ファイルなし）。2行目は0より大きい。

- [ ] **Step 7: `CLAUDE.md`の現況セクションを更新する（8構成すべてビルド・QEMU動作の実績を反映）**

現行の「polarfire・musca_b1のみ」という記述を，実際に達成できた構成（Task 7/12の結果に応じて）へ書き換える。断定せず，Task 7/12で未解決のまま残した項目（例：kria_arm64の4コアQEMU起動）があれば，それも明記する。

- [ ] **Step 8: 最終コミット**

```bash
cd /home/honda/TOPPERS/FMP3/fmp3_core
git add DIVERGENCE_MAP.md CLAUDE.md
git commit -m "docs: 計画C（残り3ターゲット追加）完了にあわせDIVERGENCE_MAP.md/CLAUDE.mdを更新

rp2350_pico2_gcc（ビルドのみ、QEMU非対応）・kria_arm64_gcc・kria_r5_gcc
の追加分をpristine add表へ一括記録。8構成（polarfire・musca_b1×2・
rp2350・kria_arm64×2・kria_r5×2）のクリーンビルド・差分等価性検査・
cfg_error_tests回帰がすべてexit=0であることを確認。"
```

---

## Self-Review（skill: superpowers:writing-plans に従い実施）

### 1. Spec coverage

依頼された要件をひとつずつ確認する：

- 「残り3ターゲットを追加する実装計画」→ Task 1-2（rp2350）、Task 3-7（kria_arm64）、Task 8-12（kria_r5）でカバー。
- 「推奨順序: rp2350 → kria_arm64 → kria_r5」→ Task番号の並びで踏襲。
- 「kria_arm64には汎用層のフックが要る」→ Task 3で対応。3箇所の行番号は本計画着手時点の現物で再確認済み（設計時点の記述361-366/418-420/538-543からTask 3実施前の実際の行番号361-366/414-425/523-584へ若干ずれていることも明記した）。
- 「kria_arm64の他の固有事情（BOARD/SYSMON/TOPPERS_MEM_BASE.../USE_ARM64_FPU/psci_support.S/DUMPOPTS/-Wl,--build-id=none/-Wl,-N/CFG1_OUT_LDFLAGS）」→ Task 6の`target.cmake`/`chip.cmake`ですべて現物確認のうえ反映（`-Wl,-N`はMakefile.chip:18の`-N`をリンカへ渡す書式として明示）。
- 「コンパイルオプションの結合順」→ 冒頭の事実整理で「3ターゲットいずれも不要と確認済み」と明記し，タスク化しなかった（上流Makefileを読んで判断せよという指示に対し，読んで「不要」と判断した根拠を残した）。
- 「テンプレート移植も必要。必要な移植量を実測すること」→ 冒頭の表で実測値（rp2350: 31行＋流用、kria_arm64: 538行、kria_r5: 435行）を明記。さらに事前調査で判明した「target_class.pyが既存ターゲットと実質コピーで済む」という追加の削減も反映。
- 「rp2350はarch/arm_m_gcc/**を流用できるはず。確認すること」→ Task 1で確認し流用した。
- 「kria_arm64はarch/arm64_gcc/**が全数未移植」→ Task 4で確認し全数移植した。
- 「kria_r5はarch/arm_gcc/**が全数未移植」→ Task 8で確認し全数移植した（ただしgic_kernel.trbは対象外と判明）。
- 「検証: cfg_equivalence.sh・QEMU起動（タイムアウト必須・プロセス残留確認）・既存3構成の回帰・cfg_error_tests」→ 各Taskに組み込み，Task 13で最終確認。
- 「期待出力は推測せず現物で確認すること」→ QEMUコマンド・toolchainの`-dumpmachine`・`.trb`のdiff等はすべて本計画執筆時に現物で確認した値を使用。ただしkria_arm64/kria_r5のQEMU実行結果そのものは**実行していない**ため，Task 7/12を「実行して確かめる」設計にし，断定を避けた。
- 「計画を書くだけ。実装はしないこと」→ 本計画中で実装（ファイル作成・git commit）は一切行っていない。
- 「positive controlとnegative controlを対で」→ 全Taskで対にして記載（Task 2/3/4/7/8/9/11/12）。

**ギャップとして残る点（正直に記載）**: 依頼文の「QEMU: musca_b1と同系」（rp2350）は現物確認の結果誤りと判明したため，計画はこれに従わず「QEMU無し・ビルド専用」へ変更した。これは依頼内容と異なる結論だが，「期待出力は推測せず現物で確認すること」という依頼自身の指示に従った結果である。

### 2. Placeholder scan

「TBD」「実装は別途」「適切な処理を追加」等のパターンで全文検索した：

```bash
grep -n "TBD\|後で\|別途検討\|適切に処理\|実装は省略\|Similar to Task" \
    /home/honda/TOPPERS/FMP3/fmp3_core/docs/superpowers/plans/2026-07-19-fmp3-cmake-c-remaining-targets.md
```

該当なし（Task 7/12の「実測結果に応じて記述」はコミットメッセージのテンプレートであり，実行しないと埋められない実測値のプレースホルダとして意図的に残したもの。これはコード実装のプレースホルダではなく，実行結果を後から埋める旨を明示した記述であり，「No Placeholders」が禁じる「TBD/後で実装」とは性質が異なる）。

### 3. Type consistency

- `FMP3_DUMP_FORMAT`/`FMP3_DUMPOPTS`（Task 3で定義）は Task 6/10 の`target.cmake`で一貫して同じ変数名・値の形式（前者は`srec`/`dump`の文字列，後者はスペース区切りの1文字列）で使われている。
- `CFG1_OUT_ROM_IMAGE_FILE`（Task 3で`CFG1_OUT_SREC_FILE`から改名）は，改名後の全使用箇所（offset生成の`--rom-image`、`add_custom_target(cfg1_out_srec ...)`のDEPENDS）を洗い出して記載した。
- Python翻訳内の関数名（`DefineVariableSection`/`SecnameKernelData`/`SecnameStack`/`GenerateNativeSpn`/`TargetCheckCfgInt`）はTask 4/8を通じて既存移植済みファイル（riscv_gcc/arm_m_gcc）と同一名・同一シグネチャで揃えた。
- `QEMU_SYSTEM_AARCH64_KRIA`（kria_arm64, Task 6）と`QEMU_SYSTEM_AARCH64_KRIA_R5`（kria_r5, Task 10）は別のキャッシュ変数名にした（同一マシン`xlnx-zcu102`だがビルド済みQEMUのパスが異なる：kria_arm64は`/home/honda/qemu-build/install/bin/`、kria_r5は`/home/honda/qemu-build/qemu-11.0.1/build-a64/`。両者は別々にビルドされた11.0.1バイナリであり，本計画は両方の実在をそれぞれ確認済み）。名前の衝突が無いことを確認した。

修正が必要な不整合は見つからなかった。

---

