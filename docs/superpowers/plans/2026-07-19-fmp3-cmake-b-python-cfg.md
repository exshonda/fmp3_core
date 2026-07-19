# fmp3_core CMake 化 計画B（Python cfg 移行と差分等価性検査）Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** `cfg_py/` の Ruby 委譲シム（計画A限りの足場）を asp3_core 1.7.1 の本物の Python cfg エンジンに差し替え、`.trb`（Ruby）テンプレートを `.py`（Python）へ全数移植し、pristine の Ruby cfg との差分等価性検査で正しさを証明したうえで、`polarfire_soc_kit_gcc` と `musca_b1_gcc` の製品ビルドを Python 経路へ切り替える。Ruby はその後 `tools/cfg_equivalence.sh`（CMake外）からのみ呼ばれるオラクルとして残る。

**Architecture:** cfg エンジン本体（`cfg.py`/`pass1.py`/`pass2.py`/`gen_file.py`/`srecord.py`）は `cfg_py/engine_next/`（本計画限りのステージング場所）で開発し、製品ビルド（`CMakeLists.txt` の `CFG_COMMAND`）は最終タスクまで既存の Ruby 委譲シム（`cfg_py/cfg.py`、計画A Task 3）を指したまま動かし続ける。これにより Task 1〜10 の全期間、polarfire 4コア・musca_b1 1/2コアの QEMU 回帰を毎タスク緑のまま保てる。テンプレート（`.py`）は最初から本来の配置（`kernel/*.py`・`arch/**/*.py`・`target/**/*.py`）に置く。pristine の `.trb` と並置しても、Ruby 側の `IncludeTrb` は `.trb` しか探さず、Python 側の `IncludeTrb` 呼び出しは移植先の `.py` ファイルが明示的に `"foo.py"` という文字列で指定するため、干渉しない（`cmake/trb_depends.cmake` の閉包計算も `IncludeTrb\("[^"]+"\)` を拡張子非依存で正規表現マッチするため無改造で動く）。差分等価性検査は `tools/cfg_equivalence.sh` が Ruby と Python を pass1 から独立実行し、`cfg1_out.c`／`offset.h`／`kernel_cfg.c`／`kernel_cfg.h` を比較する。最終タスクで `cfg_py/engine_next/` の中身を `cfg_py/` へ昇格し、CMake 側の `.trb`→`.py` 参照（8箇所）を書き換えて製品切替する。

**Tech Stack:** CMake 3.23+ / Ninja / Python 3.12（`cfg_py/` エンジン）/ ruby 3.2.3（オラクルのみ、`tools/cfg_equivalence.sh` から）/ `riscv64-unknown-elf-gcc` 13.2.0 / picolibc 1.8.6-2 / `arm-none-eabi-gcc` / newlib / `qemu-system-riscv64` 8.2.2 / `qemu-system-arm` 11.0.1（`/home/honda/qemu-build/install/bin/qemu-system-arm`）

## Global Constraints

- 設計書は `docs/superpowers/specs/2026-07-18-fmp3-cmake-design.md`（特に §5・§7・§8）。本計画は §8 の手順4〜7（`cfg_py/` へのエンジン移植・テンプレート移植・差分等価性検査＋エラー経路スイート・製品切替）を対象とする。
- **pristine（`kernel/`・`arch/`・`target/`）へ `.py` を追加したら必ず `DIVERGENCE_MAP.md` に1行足す**（AGENTS.md §2 規則2）。Task 12 でまとめて記録する（各タスクでは追加のみ行い、記録は Task 12 に集約する — 理由は Task 12 参照）。
- **`upstream` ブランチに派生ファイルを載せない**（AGENTS.md §2 規則1）。本計画の作業はすべて `main` 上で行う。
- **pristine の `cfg/`（Ruby）は製品ビルドの CMake から参照しない**（AGENTS.md §2 規則3）。Ruby を呼ぶのは `tools/cfg_equivalence.sh`（CMake外）のみに限定する。これは Task 11（製品切替）が完了して初めて満たされる。それまでは計画A由来の「期限付きの逸脱」（`cfg_py/cfg.py` が Ruby へ委譲するシム）が `DIVERGENCE_MAP.md` に記録されたまま残る。
- **製品ビルド（`cmake --build`）は Task 1〜10 の間、一度も壊してはならない。** `cfg_py/cfg.py`（Ruby 委譲シム）と `CMakeLists.txt` の `CFG_SCRIPT_DEPS`／各 `target.cmake`／`arch.cmake` の `.trb` 参照は、Task 11（cutover）まで一切変更しない。開発中の Python エンジン・テンプレートは `tools/cfg_equivalence.sh` を通じて CMake の外側から検証する。
- **各タスクの検証には positive control と negative control を対で含める。** `.superpowers/sdd/progress.md` に「検証したつもりで何も見ていなかった」欠陥が14件以上記録されている（例：`grep -c` が常に1を返すヘッダ行に一致していた、`pgrep -f` が自己マッチした、`--gc-sections` の negative control が想定した条件だけでは再現しなかった等）。同じ轍を踏まないこと。
- **回帰確認は `ps -eo pid,comm` で行う（`pgrep -f` は自己マッチの実績があるため使わない）。** QEMU 起動確認には必ずタイムアウトを付ける（`timeout 20 qemu-system-... ; echo "exit=$?"` の形。exit=124 はタイムアウト＝プロセスが起動して居座った証拠、実際の終了は上流仕様上 rc=137 が正常）。
- 変数接頭辞は既存どおり `FMP3_`。CMake 変数名（`FMP3_KERNEL_CFG_TRB_FILES` 等）は「TRB」を含んだまま変更しない — asp3_core 自身も Python 専用でありながら同じ命名を使っている（`asp3_core/CMakeLists.txt:136` 起点）ため、これは fmp3 側の設計ミスではなく確立された慣習であり、変更すると asp3 系との対応が崩れる。
- 上流 pin は `b59797f14dedcb07020f96895903ca7fcd14a4af`（`rp2350_pico2_gcc-20260719`、`UPSTREAM_VERSION` 3.4.0）で固定。本計画中に `tools/import_upstream.sh` は実行しない。

---

## 0. 実測結果（この計画のタスク粒度の根拠）

### 0.1 対象範囲の確定（2ターゲット：polarfire_soc_kit_gcc・musca_b1_gcc）

`build/musca_b1/build.ninja` と `build/polarfire_soc_kit-qemu/build.ninja` を実際に `grep -o '[a-zA-Z0-9_/.]*\.trb'` して求めた、各ターゲットが **実際に使う** `.trb` の閉包（`cmake/trb_depends.cmake` の `fmp3_trb_closure()` が計算する offset/kernel_cfg 用の閉包と一致）：

| 対象 | 使用する `.trb`（ディレクトリ別） |
|---|---|
| polarfire_soc_kit_gcc | `kernel/*.trb`（15）／`arch/riscv_gcc/common/{core_kernel,core_offset,plic_kernel}.trb`／`arch/riscv_gcc/polarfire_soc/chip_kernel.trb`／`target/polarfire_soc_kit_gcc/{target_kernel,target_class}.trb` |
| musca_b1_gcc | `kernel/*.trb`（15）／`arch/arm_m_gcc/common/{core_kernel,core_offset}.trb`／`arch/arm_m_gcc/musca_b1/chip_kernel.trb`／`target/musca_b1_gcc/{target_kernel,target_class}.trb` |

★**`arch/arm_m_gcc/common/core_kernel_v6m.trb`（301行）・`core_offset_v6m.trb`（73行）は musca_b1 では使われない。** `arch/arm_m_gcc/musca_b1/chip_kernel.trb` が `IncludeTrb("core_kernel.trb")` を呼ぶことを現物確認済み（v6m 系は Cortex-M0+ 系、musca_b1 は Cortex-M33＝v8m を使う `arch/arm_m_gcc/rp2040/chip_kernel.trb` が `IncludeTrb("core_kernel_v6m.trb")` を呼ぶのとは対照的）。よって v6m 系・`rp2040`／`rp2350`／`esp32p4`／`clic_kernel.trb` は本計画の移植対象に **含めない**。

★上記閉包には `target_check.trb`（pass3 が使う `FMP3_CHECK_TRB_FILES`）が出てこないが、これは **`fmp3_trb_closure()` が `FMP3_OFFSET_TRB_FILES`／`FMP3_KERNEL_CFG_TRB_FILES` にしか呼ばれておらず（`CMakeLists.txt:400-403`）、`FMP3_CHECK_TRB_FILES` の閉包は計算されていない**ためであり、「pass3 が `target_check.trb`／`core_check.trb`／`kernel_check.trb` を使わない」ことを意味しない（`CMakeLists.txt:519-520` の `PASS3_ARGS` は `CFG_CHECK_TRB_FILES` を直接使っており、pass3 は確実にこれらを読む。単に DEPENDS 追跡から漏れている既知の隙間であり、本計画のスコープ外。将来 `core_check.trb`／`kernel_check.trb` を編集しても pass3 が無警告に再実行されない可能性がある点は申し送り事項として Task 12 で記録する）。したがって `target_check.trb`／`core_check.trb`（riscv/arm_m 双方）／`kernel_check.trb` も **移植対象に含める**。

### 0.2 行数の実測（Ruby 基準。2ターゲットに実際に必要な30ファイルの合計 = 3963行）

`wc -l` の実測値：

| 分類 | ファイル | 現在の行数 |
|---|---|---|
| kernel/（15個） | alarm/cyclic/dataqueue/eventflag/exception/genoffset/interrupt/kernel_check/kernel/mempfix/mutex/pridataq/semaphore/spin_lock/task | 81/94/90/74/147/131/563/360/766/109/90/95/81/113/175 = **2969** |
| arch/riscv_gcc（5個・polarfire） | common/core_kernel(209)・common/core_check(92)・common/plic_kernel(79)・common/core_offset(33)・polarfire_soc/chip_kernel(36) | **449** |
| arch/arm_m_gcc（4個・musca_b1） | common/core_kernel(76)・common/core_offset(29)・common/core_check(97)・musca_b1/chip_kernel(9) | **211** |
| target/polarfire_soc_kit_gcc（3個） | target_kernel(11)・target_class(69)・target_check(11) | **91** |
| target/musca_b1_gcc（3個） | target_kernel(200)・target_class(34)・target_check(9) | **243** |
| **合計** | | **3963** |

（このユーザ事前見積もりは4384行だったが、それは `rp2040`／`rp2350`／`esp32p4`／`clic_kernel`／v6m系など本計画で使わないファイルを含めた数と推測される。§0.1 の閉包実測に基づき、本計画は3963行を対象とする。）

### 0.3 流用可否の実測による訂正（★最重要：ブリーフの前提の一部が誤っていた）

ブリーフは「`kernel/*.py` 15個・`arch/arm_m_gcc/common/*.py` 5個は `fmp3_pico_sdk`（FMP3 3.3.0 ベース）に全部ある」としていたが、**「ファイルが存在する」ことと「そのまま流用できる」ことは別**であり、実測すると3層に分かれる：

| 層 | 該当ファイル | 行数（Ruby基準） | 実測根拠 |
|---|---|---|---|
| **A. そのまま流用**（3.3.0→現pinで差分ゼロ） | `kernel/` の12個（interrupt・kernel_check・kernel を除く） | 1280 | `git -C fmp3_archive diff v3.3.0 b59797f14dedcb07020f96895903ca7fcd14a4af -- <各ファイル>` が全て空 |
| **B. 差分パッチで流用**（precedent あり・差分が小さい） | `kernel/interrupt.trb`（108行差分／563行）・`kernel/kernel_check.trb`（82行差分／360行）・`kernel/kernel.trb`（22行差分／766行） | 1689 | 同上 diff。**さらに実測**：`fmp3_pico_sdk/kernel/kernel_check.py` は既に `SYMBOL(name, True)` + `is not None` ガードを実装済み（ASP3由来のエンジンが先行対応していたため）。Ruby 側の3.4.0差分の大半（`SYMBOL(sym, true)` 化）は **Python側では既に完了しており**、実際に必要な追加は `OMIT_ISTK` 条件の追加1箇所のみ。§1.4のとおり `interrupt.trb`/`kernel.trb` は実際に追加パッチが要る |
| **C. 再構築が必要**（precedent はあるが3.4.0で構造が変わっている） | `arch/arm_m_gcc/common/{core_kernel,core_offset,core_check}.trb` | 202（現在値） | ★`core_kernel.trb` は v3.3.0 で276行あったが現pinで76行。**diff 290行**＝ファイル自体より大きい。理由：ベクタテーブル／例外テーブル／`_kernel_bitpat_cfgint` の生成コード（約150行）が `target_kernel.trb`（ターゲット依存部）へ移動した（`INHNO_VALID`/`INTNO_VALID` がターゲット依存になったため）。`fmp3_pico_sdk/arch/arm_m_gcc/common/core_kernel.py`（199行）は**この移動前の3.3.0構造のまま**であり、そのまま持ってくると現pinの `target_kernel.trb` が要求する分割と食い違う。`core_check.trb` も同様：pico_sdk の `core_check.py` は `_kernel_p_inh_table`/`_kernel_p_exc_table`（2テーブル・`index`添字）という **RISC-V 版と同じ古い方式**を実装しており、現pinの ARM-M `core_check.trb` が使う `_kernel_p_exc_tbl[prcidx]`（単一テーブル・`inhno`/`excno`値添字）とは別物（実測：`grep -n "_kernel_p_inh_table\|_kernel_p_exc_tbl"` で構造の違いを確認済み）。**§1.4 で全面書き直しの対象として扱う** |
| **D. 前例なし・全数新規** | `arch/riscv_gcc/`（5個・449行）／`arch/arm_m_gcc/musca_b1/chip_kernel.trb`（9行）／`target/polarfire_soc_kit_gcc/`（3個・91行）／`target/musca_b1_gcc/`（3個・243行） | 792 | `git cat-file -e v3.3.0:<path>` が全て失敗（riscv_gcc）。`fmp3_pico_sdk` に `target/polarfire_soc_kit_gcc/`・`target/musca_b1_gcc/`・`arch/arm_m_gcc/musca_b1/` 自体が存在しない（Pico専用リポジトリのため） |

**内訳合計の検算**：1280 + 1689 + 202 + 792 = 3963（§0.2 と一致）。

### 0.4 CMake 表面の実測（★ブリーフの前提を部分的に訂正）

ブリーフは「`CFG_SCRIPT_DEPS`（`cfg/*.rb` の5行）の差し替えだけで済むはず」としていたが、実測すると **それだけでは済まない**。`grep -rn "\.trb" --include=*.cmake .` の実測結果：

```
arch/arm_m_gcc/common/arch.cmake:27:    ${COREDIR}/core_offset.trb
arch/riscv_gcc/common/arch.cmake:21:    ${COREDIR}/core_offset.trb
target/musca_b1_gcc/target.cmake:64-66:  (target_class/target_kernel/target_check).trb（3行）
target/polarfire_soc_kit_gcc/target.cmake:112-114: 同上（3行）
```

**合計8箇所**の `.trb` → `.py` へのファイル名変更が、`CFG_SCRIPT_DEPS`（cfg エンジン本体の依存6行）に加えて必要。ただし訂正後も影響は小さい：
- 変数**名**（`FMP3_KERNEL_CFG_TRB_FILES` 等）は変更不要（asp3_core も Python 専用でこの命名を使う。§Global Constraints 参照）。
- `cmake/trb_depends.cmake` の閉包計算（`IncludeTrb\("[^"]+"\)` の正規表現マッチ）は拡張子非依存であり無改造で動く（実測：Python テンプレートは `IncludeTrb("core_kernel.py")` のように **`.py` を文字列に含めて呼ぶ**ことを `fmp3_pico_sdk/kernel/kernel.py:388` 等で確認済み）。
- したがって Task 11（cutover）の変更範囲は「`CFG_SCRIPT_DEPS` の6行」＋「8箇所の `.trb`→`.py` 書き換え」の合計14行程度に収まる。「CFG_SCRIPT_DEPS の差し替えだけ」ではないが、依然として小さい。

---

## File Structure

| ファイル | 責務 | Task |
|---|---|---|
| `cfg_py/engine_next/{cfg,pass1,pass2,gen_file,srecord}.py` | asp3_core 1.7.1 由来の cfg エンジン本体（ステージング、CMake からは呼ばれない） | 1 |
| `tools/cfg_equivalence.sh` | Ruby/Python 独立2重実行による差分等価性検査（CMake外） | 2, 6, 9 |
| `kernel/*.py`（15個） | カーネル共通テンプレート（pristine 並置） | 3 |
| `arch/riscv_gcc/common/{core_kernel,core_check,plic_kernel,core_offset}.py`／`arch/riscv_gcc/polarfire_soc/chip_kernel.py` | polarfire chip 層（全数新規） | 4 |
| `target/polarfire_soc_kit_gcc/{target_kernel,target_class,target_check}.py` | polarfire ターゲット層（全数新規） | 5 |
| `arch/arm_m_gcc/common/{core_kernel,core_offset,core_check}.py`／`arch/arm_m_gcc/musca_b1/chip_kernel.py` | musca_b1 chip 層（再構築＋新規） | 7 |
| `target/musca_b1_gcc/{target_kernel,target_class,target_check}.py` | musca_b1 ターゲット層（全数新規、ベクタテーブル生成含む） | 8 |
| `tools/cfg_error_tests/*.cfg`・`tools/cfg_error_tests/run.sh` | エラー検出経路の回帰スイート | 10 |
| `cfg_py/{cfg,pass1,pass2,gen_file,srecord}.py`（cutover後）／`CMakeLists.txt`／各 `arch.cmake`・`target.cmake`（8箇所） | 製品切替 | 11 |
| `DIVERGENCE_MAP.md` | pristine 追加分の記録・期限付き逸脱の解消 | 12 |

---

### Task 1: cfg エンジンのステージング移植と pass1 差分等価性（テンプレート非依存の先行証明）

**Files:**
- Create: `cfg_py/engine_next/cfg.py`（asp3_core 1.7.1 の `cfg/cfg.py` をそのまま複製）
- Create: `cfg_py/engine_next/pass1.py`
- Create: `cfg_py/engine_next/pass2.py`
- Create: `cfg_py/engine_next/gen_file.py`
- Create: `cfg_py/engine_next/srecord.py`

**Interfaces:**
- Consumes: なし（cfg エンジンは自己完結。ソースは `/home/honda/TOPPERS/ASP3CORE/asp3_core/cfg/{cfg,pass1,pass2,gen_file,srecord}.py`）
- Produces: `cfg_py/engine_next/cfg.py` が本計画全体を通じて「開発中の本物の Python cfg エンジン」の唯一の実体。以降のタスクは `python3 cfg_py/engine_next/cfg.py ...` で直接起動して検証する。CMake の `CFG_COMMAND` は Task 11 まで一切これを参照しない。

**背景（§0.4 で確認済みの事実）**：`gen_file.py`／`srecord.py` は `fmp3_pico_sdk`（1.7.0）と `asp3_core`（1.7.1）でバイト同一（`diff` 差分ゼロ）。`asp3_core/cfg/pass2.py:434` に `_class_proc()` が既にあり、`affinityPrcBitmap` を計算する（`pass2.py:453`）。`cfg.py --kernel` は `metavar="KERNEL"` の自由文字列（`choices` 制約なし）で `fmp` をそのまま受け付ける。したがって asp3_core 1.7.1 のエンジンは **無改造で FMP3 の `--kernel fmp` 経路を動かせる**（設計書 §1.3 の「FMP経路は両者バイト同一」の裏取り）。

- [ ] **Step 1: ディレクトリを作りエンジンを複製する**

```bash
mkdir -p /home/honda/TOPPERS/FMP3/fmp3_core/cfg_py/engine_next
cp /home/honda/TOPPERS/ASP3CORE/asp3_core/cfg/cfg.py \
   /home/honda/TOPPERS/ASP3CORE/asp3_core/cfg/pass1.py \
   /home/honda/TOPPERS/ASP3CORE/asp3_core/cfg/pass2.py \
   /home/honda/TOPPERS/ASP3CORE/asp3_core/cfg/gen_file.py \
   /home/honda/TOPPERS/ASP3CORE/asp3_core/cfg/srecord.py \
   /home/honda/TOPPERS/FMP3/fmp3_core/cfg_py/engine_next/
```

- [ ] **Step 2: バージョン文字列と単独起動を確認する（失敗の確認を兼ねる）**

Run:
```bash
python3 -B /home/honda/TOPPERS/FMP3/fmp3_core/cfg_py/engine_next/cfg.py --version
```
Expected: `cfg.py 1.7.1`（pristine `cfg/cfg.rb:58` の `VERSION = "1.7.1"` と一致することを確認。設計書 §1.3 のオラクル版一致の裏取り）。

- [ ] **Step 3: pass1 の独立比較を手動で行う（polarfire）**

pass1 は `-T`（テンプレート）を一切使わない（`CMakeLists.txt:287-298` に `CFG_OFFSET_TRB_FILES`／`CFG_KERNEL_CFG_TRB_FILES` が渡らないことを確認済み）ため、**テンプレート移植ゼロの状態でも Ruby と新エンジンの pass1 出力を直接比較できる**。既存ビルド済みの polarfire から実引数を取り出す：

```bash
cd /home/honda/TOPPERS/FMP3/fmp3_core
ninja -C build/polarfire_soc_kit-qemu -t commands generated/cfg1_out.timestamp \
  | grep 'cfg_py/cfg.py' > /tmp/pf_pass1_cmd.txt
cat /tmp/pf_pass1_cmd.txt
```
Expected: 1行のコマンドが得られる（`cd .../build/polarfire_soc_kit-qemu/generated && python3 -B .../cfg_py/cfg.py --pass 1 --kernel fmp -I... --api-table ... --symval-table ... -M ... target_kernel.cfg sample1.cfg && cmake -E cmake_transform_depfile ...`）。

```bash
mkdir -p /tmp/cfgeq_pf/ruby /tmp/cfgeq_pf/py
cd /tmp/cfgeq_pf/ruby
ruby /home/honda/TOPPERS/FMP3/fmp3_core/cfg/cfg.rb --pass 1 --kernel fmp \
  -I/home/honda/TOPPERS/FMP3/fmp3_core/target/polarfire_soc_kit_gcc \
  -I/home/honda/TOPPERS/FMP3/fmp3_core/target/polarfire_soc_kit_gcc/sdk/platform \
  -I/home/honda/TOPPERS/FMP3/fmp3_core/target/polarfire_soc_kit_gcc/sdk/boards/icicle-kit-es \
  -I/home/honda/TOPPERS/FMP3/fmp3_core/target/polarfire_soc_kit_gcc/sdk/boards/icicle-kit-es/platform_config/lim-debug \
  -I/home/honda/TOPPERS/FMP3/fmp3_core/arch/riscv_gcc/polarfire_soc \
  -I/home/honda/TOPPERS/FMP3/fmp3_core/arch/riscv_gcc/common \
  -I/home/honda/TOPPERS/FMP3/fmp3_core/arch/gcc \
  -I/home/honda/TOPPERS/FMP3/fmp3_core/include \
  -I/home/honda/TOPPERS/FMP3/fmp3_core \
  -I/home/honda/TOPPERS/FMP3/fmp3_core/sample \
  -I/tmp/cfgeq_pf/ruby \
  --api-table /home/honda/TOPPERS/FMP3/fmp3_core/kernel/kernel_api.def \
  --symval-table /home/honda/TOPPERS/FMP3/fmp3_core/arch/riscv_gcc/common/core_sym.def \
  --symval-table /home/honda/TOPPERS/FMP3/fmp3_core/kernel/kernel_sym.def \
  -M /tmp/cfgeq_pf/ruby/cfg1_out_c.d \
  /home/honda/TOPPERS/FMP3/fmp3_core/target/polarfire_soc_kit_gcc/target_kernel.cfg \
  /home/honda/TOPPERS/FMP3/fmp3_core/sample/sample1.cfg
echo "ruby exit=$?"

cd /tmp/cfgeq_pf/py
python3 -B /home/honda/TOPPERS/FMP3/fmp3_core/cfg_py/engine_next/cfg.py --pass 1 --kernel fmp \
  -I/home/honda/TOPPERS/FMP3/fmp3_core/target/polarfire_soc_kit_gcc \
  -I/home/honda/TOPPERS/FMP3/fmp3_core/target/polarfire_soc_kit_gcc/sdk/platform \
  -I/home/honda/TOPPERS/FMP3/fmp3_core/target/polarfire_soc_kit_gcc/sdk/boards/icicle-kit-es \
  -I/home/honda/TOPPERS/FMP3/fmp3_core/target/polarfire_soc_kit_gcc/sdk/boards/icicle-kit-es/platform_config/lim-debug \
  -I/home/honda/TOPPERS/FMP3/fmp3_core/arch/riscv_gcc/polarfire_soc \
  -I/home/honda/TOPPERS/FMP3/fmp3_core/arch/riscv_gcc/common \
  -I/home/honda/TOPPERS/FMP3/fmp3_core/arch/gcc \
  -I/home/honda/TOPPERS/FMP3/fmp3_core/include \
  -I/home/honda/TOPPERS/FMP3/fmp3_core \
  -I/home/honda/TOPPERS/FMP3/fmp3_core/sample \
  -I/tmp/cfgeq_pf/py \
  --api-table /home/honda/TOPPERS/FMP3/fmp3_core/kernel/kernel_api.def \
  --symval-table /home/honda/TOPPERS/FMP3/fmp3_core/arch/riscv_gcc/common/core_sym.def \
  --symval-table /home/honda/TOPPERS/FMP3/fmp3_core/kernel/kernel_sym.def \
  -M /tmp/cfgeq_pf/py/cfg1_out_c.d \
  /home/honda/TOPPERS/FMP3/fmp3_core/target/polarfire_soc_kit_gcc/target_kernel.cfg \
  /home/honda/TOPPERS/FMP3/fmp3_core/sample/sample1.cfg
echo "python exit=$?"

diff /tmp/cfgeq_pf/ruby/cfg1_out.c /tmp/cfgeq_pf/py/cfg1_out.c
echo "diff exit=$?"
```
Expected: `ruby exit=0` / `python exit=0` / `diff exit=0`（無出力）。**pass1 は Ruby 版 cfg.rb と asp3_core 1.7.1 エンジンでバイト同一の `cfg1_out.c` を生成する**ことの実証（テンプレート移植は一切不要な段階での証明）。

- [ ] **Step 4: musca_b1 でも同じ手順を行う（両ターゲットで実施。AGENTS指定）**

`ninja -C build/musca_b1 -t commands generated/cfg1_out.timestamp | grep 'cfg_py/cfg.py'` で musca_b1 の実引数を取り出し、Step 3 と同じ要領で `/tmp/cfgeq_mb/ruby` と `/tmp/cfgeq_mb/py` に対して実行・比較する。
Expected: 同じく `diff` が無出力・両方 exit=0。

- [ ] **Step 5: negative control — 意図的に壊れたエンジンで差分が出ることを確認する**

```bash
cp /home/honda/TOPPERS/FMP3/fmp3_core/cfg_py/engine_next/pass1.py /tmp/pass1_broken.py
# api-table から読んだ関数名の1文字を落として壊す（pass1.py の cfg1_out.c 書き出し処理を汚す）
python3 - <<'EOF'
import re
with open("/tmp/pass1_broken.py") as f:
    content = f.read()
# TOPPERS_magic_number という定数出力を意図的に別名へ変える
content2 = content.replace("TOPPERS_magic_number", "TOPPERS_magic_number_BROKEN")
assert content != content2, "置換対象が見つからず negative control が成立しない"
with open("/tmp/pass1_broken.py", "w") as f:
    f.write(content2)
EOF
mkdir -p /tmp/cfgeq_pf/py_broken
cd /tmp/cfgeq_pf/py_broken
python3 -B -c "
import sys
sys.path.insert(0, '/home/honda/TOPPERS/FMP3/fmp3_core/cfg_py/engine_next')
sys.argv = ['cfg.py'] + $(python3 -c "import shlex,sys; print(shlex.split(open('/tmp/pf_pass1_cmd.txt').read().split('&&')[0].split('cfg.py',1)[1]))" )
" 2>&1 | head -5
# 簡略版：直接 broken pass1.py を engine_next にコピーして再実行
cp /tmp/pass1_broken.py /home/honda/TOPPERS/FMP3/fmp3_core/cfg_py/engine_next/pass1.py
cd /tmp/cfgeq_pf/py_broken
python3 -B /home/honda/TOPPERS/FMP3/fmp3_core/cfg_py/engine_next/cfg.py --pass 1 --kernel fmp \
  $(cat /tmp/pf_pass1_cmd.txt | sed 's#.*cfg.py##; s#&&.*##') \
  > /dev/null 2>&1
diff /tmp/cfgeq_pf/ruby/cfg1_out.c /tmp/cfgeq_pf/py_broken/cfg1_out.c | head -5
echo "diff exit(broken)=$?"
# 元に戻す
git -C /home/honda/TOPPERS/FMP3/fmp3_core checkout -- cfg_py/engine_next/pass1.py 2>/dev/null || \
  cp /home/honda/TOPPERS/ASP3CORE/asp3_core/cfg/pass1.py /home/honda/TOPPERS/FMP3/fmp3_core/cfg_py/engine_next/pass1.py
```
Expected: `diff` が **非ゼロ**（`TOPPERS_magic_number` を含む行が食い違う）、`diff exit(broken)` が非ゼロ。壊した後は必ず `cfg_py/engine_next/pass1.py` を元の asp3_core 1.7.1 の内容に復元すること（最後の2行で実施）。

- [ ] **Step 6: 製品ビルドが無傷であることを確認する（回帰）**

```bash
cd /home/honda/TOPPERS/FMP3/fmp3_core
git status --short cfg_py/cfg.py CMakeLists.txt target/*/target.cmake arch/*/*/arch.cmake
```
Expected: 無出力（本タスクはこれらのファイルを一切変更していない）。

```bash
timeout 30 qemu-system-riscv64 -M microchip-icicle-kit -smp 5 -m 2G -nographic \
  -serial mon:stdio -bios none -kernel build/polarfire_soc_kit-qemu/fmp \
  -device loader,file=build/polarfire_soc_kit-qemu/fmp,cpu-num=0 \
  -device loader,file=build/polarfire_soc_kit-qemu/fmp,cpu-num=1 \
  -device loader,file=build/polarfire_soc_kit-qemu/fmp,cpu-num=2 \
  -device loader,file=build/polarfire_soc_kit-qemu/fmp,cpu-num=3 \
  -device loader,file=build/polarfire_soc_kit-qemu/fmp,cpu-num=4 > /tmp/pf_regress.log 2>&1
echo "exit=$?"
grep -c "^Processor .* start\.$" /tmp/pf_regress.log
```
Expected: `exit=124`（タイムアウトで正常。プロセスが起動して居座っていた証拠）、`grep -c` が `4`（4プロセッサ起動、計画A/A2の実績と同じ）。

- [ ] **Step 7: コミット**

```bash
cd /home/honda/TOPPERS/FMP3/fmp3_core
git add cfg_py/engine_next/cfg.py cfg_py/engine_next/pass1.py cfg_py/engine_next/pass2.py \
        cfg_py/engine_next/gen_file.py cfg_py/engine_next/srecord.py
git commit -m "build(cfg): asp3_core 1.7.1 の cfg エンジンをステージングへ移植

cfg_py/engine_next/ は本計画（計画B）限りのステージング場所。製品ビルドの
CMakeLists.txt / cfg_py/cfg.py（Ruby委譲シム）は Task 11 まで変更しない。
pass1 はテンプレートを使わないため、この時点で既に Ruby 版 cfg.rb と
バイト同一の cfg1_out.c を生成することを polarfire・musca_b1 両方で確認済み。"
```

---

### Task 2: `tools/cfg_equivalence.sh` の実装（pass1+pass2 全パイプライン対応）

**Files:**
- Create: `tools/cfg_equivalence.sh`

**Interfaces:**
- Consumes: `cfg_py/engine_next/cfg.py`（Task 1）。既存の CMake ビルド（`build/<preset>/`）が configure 済みであること。
- Produces: `tools/cfg_equivalence.sh <build-preset-dir> [--pass1-only]` — 終了コード0で「一致」、非0で「不一致または実行失敗」。標準出力に比較したファイルごとの結果を出す。

**設計**：`ninja -t commands` から実際の pass1／pass2(-O)／pass2(kernel_cfg) コマンドラインを抽出し、（a）`cfg_py/cfg.py`（現在Ruby委譲シム）呼び出しを `ruby cfg/cfg.rb` へ、（b）同じコマンドラインの `-T`/`-C` 引数の `.trb` を `.py` に置換したうえで `cfg_py/engine_next/cfg.py` 呼び出しへ、それぞれ機械的に変換して2つの独立ディレクトリで実行する。pass1 はテンプレートを使わないため両者とも同じ `.cfg`/`.def` を読むだけで済み、pass2 は各自が自分の pass1 で作った `cfg1_out.db`（Ruby=PStore／Python=pickle、自形式）を読む。`--rom-symbol`/`--rom-image` は Task 1 Step 3 で示した理由（nm/objcopy が生成する engine-agnostic な副産物）により、各パイプラインが自分の pass1 で作った `cfg1_out.c` をコンパイルして得る。

- [ ] **Step 1: スクリプトを書く**

`tools/cfg_equivalence.sh`:
```bash
#!/usr/bin/env bash
#
#		cfg_equivalence.sh -- Ruby cfg と Python cfg の差分等価性検査
#
#  pristine の cfg/cfg.rb（オラクル）と cfg_py/engine_next/cfg.py（開発中の
#  本物のPythonエンジン）を、pass1 から完全に独立した2本のパイプラインとして
#  走らせ、cfg1_out.c / offset.h / kernel_cfg.c / kernel_cfg.h を比較する。
#
#  AGENTS.md §2 規則3（pristineのcfg/はCMakeから参照しない）を守るため、
#  本スクリプトはCMake外に置かれ、CMakeのビルドグラフには一切登場しない。
#
#  使い方:
#    tools/cfg_equivalence.sh <build-preset-dir> [--pass1-only]
#  例:
#    tools/cfg_equivalence.sh build/polarfire_soc_kit-qemu
#    tools/cfg_equivalence.sh build/musca_b1
#    tools/cfg_equivalence.sh build/musca_b1-2core
#
set -u
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${1:?usage: $0 <build-preset-dir> [--pass1-only]}"
PASS1_ONLY="${2:-}"

if [ ! -f "${BUILD_DIR}/build.ninja" ]; then
    echo "cfg_equivalence.sh: ${BUILD_DIR}/build.ninja not found. configure first." >&2
    exit 2
fi

WORK="$(mktemp -d /tmp/cfgeq.XXXXXX)"
RUBY_DIR="${WORK}/ruby"
PY_DIR="${WORK}/py"
mkdir -p "${RUBY_DIR}" "${PY_DIR}"
echo "cfg_equivalence.sh: work dir = ${WORK}"

FAIL=0

#  ninja -t commands から、指定ターゲットに対する「最後に現れる」
#  cfg_py/cfg.py 呼び出し行を取り出す（対象ターゲットが依存する前段の
#  cfg呼び出し（pass1等）も一緒に表示されるため、最後の行が目的のコマンド）。
extract_cmd() {
    local target="$1"
    ninja -C "${BUILD_DIR}" -t commands "${target}" 2>/dev/null \
        | grep 'cfg_py/cfg\.py' | tail -1
}

#  1コマンド文字列を engine 種別に応じて変換して実行する。
#    kind = ruby | python
run_cmd() {
    local kind="$1" cmd="$2" outdir="$3"
    # "cd <dir> && python3 -B <root>/cfg_py/cfg.py <args...> && cmake -E ..." の
    # うち、"python3 -B .../cfg_py/cfg.py" 部分から後ろ、次の "&&" までを取り出す
    local args
    args="$(echo "${cmd}" | sed -E 's#^.*cfg_py/cfg\.py##; s# && .*$##')"
    if [ "${kind}" = "ruby" ]; then
        ( cd "${outdir}" && ruby "${ROOT}/cfg/cfg.rb" ${args} )
    else
        # -T/-C 引数の .trb を .py に、-M の depfile 出力先を outdir 配下に
        local pyargs
        pyargs="$(echo "${args}" | sed -E 's#(-[TC] [^ ]+)\.trb#\1.py#g')"
        ( cd "${outdir}" && python3 -B "${ROOT}/cfg_py/engine_next/cfg.py" ${pyargs} )
    fi
}

compare_stage() {
    local label="$1"; shift
    local files=("$@")
    local ok=1
    for f in "${files[@]}"; do
        if [ ! -f "${RUBY_DIR}/${f}" ] || [ ! -f "${PY_DIR}/${f}" ]; then
            echo "  [MISSING] ${f} (ruby: $( [ -f "${RUBY_DIR}/${f}" ] && echo yes || echo no ), python: $( [ -f "${PY_DIR}/${f}" ] && echo yes || echo no ))"
            ok=0
            continue
        fi
        if diff -q "${RUBY_DIR}/${f}" "${PY_DIR}/${f}" > /dev/null; then
            echo "  [OK]      ${f}"
        else
            echo "  [DIFFERS] ${f}"
            diff "${RUBY_DIR}/${f}" "${PY_DIR}/${f}" | head -20
            ok=0
        fi
    done
    if [ "${ok}" = "1" ]; then
        echo "cfg_equivalence.sh: ${label}: MATCH"
    else
        echo "cfg_equivalence.sh: ${label}: MISMATCH"
        FAIL=1
    fi
}

echo "== pass1 (cfg1_out.c) =="
PASS1_CMD="$(extract_cmd generated/cfg1_out.timestamp)"
if [ -z "${PASS1_CMD}" ]; then
    echo "cfg_equivalence.sh: could not extract pass1 command from ninja -t commands" >&2
    exit 2
fi
run_cmd ruby   "${PASS1_CMD}" "${RUBY_DIR}" > "${RUBY_DIR}/pass1.log" 2>&1
RUBY_RC=$?
run_cmd python "${PASS1_CMD}" "${PY_DIR}"   > "${PY_DIR}/pass1.log"   2>&1
PY_RC=$?
if [ "${RUBY_RC}" != 0 ] || [ "${PY_RC}" != 0 ]; then
    echo "cfg_equivalence.sh: pass1 execution failed (ruby rc=${RUBY_RC}, python rc=${PY_RC})"
    echo "--- ruby log ---";   cat "${RUBY_DIR}/pass1.log"
    echo "--- python log ---"; cat "${PY_DIR}/pass1.log"
    FAIL=1
fi
compare_stage "pass1" cfg1_out.c

if [ "${PASS1_ONLY}" = "--pass1-only" ]; then
    [ "${FAIL}" = "0" ] && echo "cfg_equivalence.sh: RESULT = MATCH (pass1-only)" || echo "cfg_equivalence.sh: RESULT = MISMATCH"
    exit "${FAIL}"
fi

echo "== compiling cfg1_out (shared inputs for both pipelines, per Task1 Step3 rationale) =="
#  cfg1_out.c は pass1 が MATCH した前提のもとでは Ruby/Python どちらの
#  出力を使っても同じバイナリになる。既存ビルドの cfg1_out ELF を再利用し、
#  nm/objcopy で得た .syms/.srec を両パイプラインの --rom-symbol/--rom-image
#  として共有する（設計書 §7.1: cfg1_out.dbはPStore/pickleで別形式のため
#  共有できないが、.syms/.srecはnm/objcopyの出力でエンジン非依存）。
SYMS="${BUILD_DIR}/generated/cfg1_out.syms"
SREC="${BUILD_DIR}/generated/cfg1_out.srec"
if [ ! -f "${SYMS}" ] || [ ! -f "${SREC}" ]; then
    echo "cfg_equivalence.sh: ${SYMS} / ${SREC} not found. Build cfg1_out_syms/cfg1_out_srec targets first:" >&2
    echo "  ninja -C ${BUILD_DIR} cfg1_out_syms cfg1_out_srec" >&2
    exit 2
fi

echo "== pass2 -O (offset.h) =="
OFFSET_CMD="$(extract_cmd generated/offset.timestamp)"
run_cmd ruby   "${OFFSET_CMD/--rom-symbol [^ ]*/--rom-symbol ${SYMS}}" "${RUBY_DIR}" \
    > "${RUBY_DIR}/offset.log" 2>&1
run_cmd python "${OFFSET_CMD/--rom-symbol [^ ]*/--rom-symbol ${SYMS}}" "${PY_DIR}" \
    > "${PY_DIR}/offset.log" 2>&1
compare_stage "offset" offset.h

echo "== pass2 (kernel_cfg.c/h) =="
KCFG_CMD="$(extract_cmd generated/kernel_cfg.timestamp)"
run_cmd ruby   "${KCFG_CMD}" "${RUBY_DIR}"  > "${RUBY_DIR}/kernel_cfg.log"  2>&1
run_cmd python "${KCFG_CMD}" "${PY_DIR}"    > "${PY_DIR}/kernel_cfg.log"   2>&1
compare_stage "kernel_cfg" kernel_cfg.c kernel_cfg.h

if [ "${FAIL}" = "0" ]; then
    echo "cfg_equivalence.sh: RESULT = MATCH"
else
    echo "cfg_equivalence.sh: RESULT = MISMATCH"
fi
echo "cfg_equivalence.sh: work dir preserved at ${WORK} for inspection"
exit "${FAIL}"
```

- [ ] **Step 2: 実行権限を付与する**

```bash
chmod +x /home/honda/TOPPERS/FMP3/fmp3_core/tools/cfg_equivalence.sh
```

- [ ] **Step 3: pass1-only モードで両ターゲット positive control**

```bash
cd /home/honda/TOPPERS/FMP3/fmp3_core
tools/cfg_equivalence.sh build/polarfire_soc_kit-qemu --pass1-only
echo "exit=$?"
tools/cfg_equivalence.sh build/musca_b1 --pass1-only
echo "exit=$?"
```
Expected: 両方とも `[OK]      cfg1_out.c` と `RESULT = MATCH (pass1-only)`、`exit=0`。

- [ ] **Step 4: negative control — スクリプト自身がちゃんと差分を検出できることの確認**

Task 1 Step 5 と同様に `cfg_py/engine_next/pass1.py` の `TOPPERS_magic_number` を一時的に別名へ壊し、`tools/cfg_equivalence.sh build/polarfire_soc_kit-qemu --pass1-only` を再実行する。
Expected: `[DIFFERS] cfg1_out.c` と `RESULT = MISMATCH`、`exit=1`。確認後、壊した `pass1.py` を asp3_core 原本へ復元する。

- [ ] **Step 5: フルパイプラインモードは pass2 が失敗することを確認する（この時点では正しい失敗）**

```bash
ninja -C build/polarfire_soc_kit-qemu cfg1_out_syms cfg1_out_srec
tools/cfg_equivalence.sh build/polarfire_soc_kit-qemu
echo "exit=$?"
```
Expected: `pass1` は `MATCH`。`offset`／`kernel_cfg` ステージは Python 側が `target_class.py`／`core_offset.py`／`target_kernel.py` を見つけられず失敗する（`FileNotFoundError` 等）ため `MISMATCH`、`exit=1`。**これは Task 2 時点で正しい失敗**（テンプレートがまだ存在しないため）。ログに「ファイルが見つからない」旨が出ていることを確認し、「スクリプトが壊れて常に不一致を返しているのではない」ことを pass1 側の MATCH と併せて確認する。

- [ ] **Step 6: 回帰確認（Task 1 と同様、polarfire QEMU 起動）**

Task 1 Step 6 と同じコマンドを再実行し、`exit=124` かつ `Processor .* start.` が4行であることを確認する。

- [ ] **Step 7: コミット**

```bash
cd /home/honda/TOPPERS/FMP3/fmp3_core
git add tools/cfg_equivalence.sh
git commit -m "build(cfg): 差分等価性検査スクリプト tools/cfg_equivalence.sh を追加

pass1（テンプレート非依存）は既に両ターゲットでMATCH。pass2以降は
テンプレート移植（Task 3以降）が進むにつれてMATCHするようになる想定。
CMake外に置き、Rubyオラクルを製品ビルドの依存グラフに一切入れない
（AGENTS.md §2 規則3）。"
```

---

### Task 3: `kernel/*.py` の移植（15個）

**Files:**
- Create: `kernel/{alarm,cyclic,dataqueue,eventflag,exception,genoffset,mempfix,mutex,pridataq,semaphore,spin_lock,task}.py`（12個、そのまま流用）
- Create: `kernel/interrupt.py`（差分パッチ）
- Create: `kernel/kernel_check.py`（差分パッチ）
- Create: `kernel/kernel.py`（差分パッチ）

**Interfaces:**
- Consumes: `fmp3_pico_sdk/kernel/*.py`（`/tmp/claude-1000/.../scratchpad/fmp3_pico_sdk/kernel/`）を流用元とする。
- Produces: `kernel/*.py`（15個）。後続タスク（Task 4〜9）の `IncludeTrb("kernel/xxx.py")` 呼び出し先。

- [ ] **Step 1: そのまま流用できる12個をコピーする（§0.3 層A、3.3.0→現pinで差分ゼロを確認済み）**

```bash
SDK=/tmp/claude-1000/-home-honda-TOPPERS-FMP3-fmp3-core/83e086ff-1095-4fbe-9020-0281a7d90d1d/scratchpad/fmp3_pico_sdk
DST=/home/honda/TOPPERS/FMP3/fmp3_core/kernel
for f in alarm cyclic dataqueue eventflag exception genoffset mempfix mutex pridataq semaphore spin_lock task; do
    cp "${SDK}/kernel/${f}.py" "${DST}/${f}.py"
done
```

- [ ] **Step 2: 移植した12個が構文的に正しいことを確認する（negative control 込み）**

```bash
cd /home/honda/TOPPERS/FMP3/fmp3_core
for f in alarm cyclic dataqueue eventflag exception genoffset mempfix mutex pridataq semaphore spin_lock task; do
    python3 -m py_compile "kernel/${f}.py" && echo "OK ${f}.py" || echo "FAIL ${f}.py"
done
# negative control: 明らかに壊れた構文が FAIL として検出されることの確認
echo "def broken(" > /tmp/broken_syntax.py
python3 -m py_compile /tmp/broken_syntax.py; echo "exit(broken)=$?"
```
Expected: 12個すべて `OK`。negative control は `exit(broken)` が非ゼロ（`py_compile` 自体が壊れた構文を検出できることの確認）。

- [ ] **Step 3: `kernel/interrupt.py` を差分パッチする**

`fmp3_pico_sdk/kernel/interrupt.py`（448行）をベースに複製し、以下の4箇所を編集する（Ruby側の差分は `git -C fmp3_archive diff v3.3.0 b59797f14dedcb07020f96895903ca7fcd14a4af -- kernel/interrupt.trb` で確認済み）：

```bash
cp /tmp/claude-1000/-home-honda-TOPPERS-FMP3-fmp3-core/83e086ff-1095-4fbe-9020-0281a7d90d1d/scratchpad/fmp3_pico_sdk/kernel/interrupt.py \
   /home/honda/TOPPERS/FMP3/fmp3_core/kernel/interrupt.py
```

**編集1**（CFG_INT ループ、`intno`有効範囲チェックの直前に `affinityPrcBitmap` 割当とチェックを追加。Ruby: `kernel/interrupt.trb:128-160`相当）。既存コード:
```python
    # intnoが有効範囲外の場合（E_PAR）
    if params["intno"] not in INTNO_VALID_ALL:
        error_illegal("E_PAR", params, "intno")
```
これを:
```python
    # 割込み要求を受け付けるプロセッサの設定
    if not OMIT_MULTIPRC_INTERRUPT:
        params["affinityPrcBitmap"] = clsData[params["class"]]["affinityPrcBitmap"]
    else:
        # 割付け可能プロセッサが複数あるクラスの囲み内にCFG_INTを記述した
        # 場合には，警告メッセージを出し，初期割付けプロセッサのみで割込み
        # 要求を受け付けるように設定する．
        initPrcBitmap = 1 << (clsData[params["class"]]["initPrc"] - 1)
        if clsData[params["class"]]["affinityPrcBitmap"] != initPrcBitmap:
            warning_api(params,
                        f"%%intno configured within the class "
                        f"{clsData[params['class']]['clsid']} "
                        f"is configured to be accepted by the processor "
                        f"{clsData[params['class']]['initPrc']} only.")
        params["affinityPrcBitmap"] = initPrcBitmap

    # intnoが有効範囲外の場合（E_PAR）
    if params["intno"] not in INTNO_VALID_ALL:
        error_illegal("E_PAR", params, "intno")
    else:
        # クラスIDがエラーの場合は，エラーチェックをスキップする
        if str(params["class"]) != "":
            for prcid in clsData[params["class"]]["affinityPrcList"]:
                if params["intno"] not in INTNO_CREISR_VALID[prcid]:
                    error_ercd("E_RSATR", params,
                               "the assignable processors "
                               "of the class in which %apiname of `%intno' is described "
                               "must be included in the set of processors "
                               "to which the interrupt is requested")
                    break
```
（`OMIT_MULTIPRC_INTERRUPT` は本ファイル冒頭で `if "OMIT_MULTIPRC_INTERRUPT" not in globals(): OMIT_MULTIPRC_INTERRUPT = False` として定義を追加する。Ruby版の `$OMIT_MULTIPRC_INTERRUPT` に対応する新設フラグ。）

**編集2**（DEF_INH ループ、`prcid` をローカル変数から `params["prcid"]` へ変更。Ruby: `interrupt.trb:213-282`相当）。既存:
```python
    prcid = clsData[params["class"]]["initPrc"]

    # inhnoが有効範囲外の場合（E_PAR）
    if params["inhno"] not in INHNO_VALID_ALL:
        error_illegal("E_PAR", params, "inhno")
    else:
        # クラスの初期割付けプロセッサが，割込みが要求されるプロセッサでない
        # 場合（E_RSATR）
        if params["inhno"] not in INHNO_VALID[prcid]:
            error_ercd("E_RSATR", params,
                       "the initial assignment processor "
                       "of the class in which %apiname of `%inhno' is described "
                       "must be the processor to which the interrupt is requested")
```
これを:
```python
    params["prcid"] = clsData[params["class"]]["initPrc"]

    # inhnoが有効範囲外の場合（E_PAR）
    if params["inhno"] not in INHNO_VALID_ALL:
        error_illegal("E_PAR", params, "inhno")
    else:
        # クラスの初期割付けプロセッサが，割込みハンドラ番号に対応するプロ
        # セッサでない場合（E_RSATR）
        if params["inhno"] not in INHNO_VALID[params["prcid"]]:
            error_ercd("E_RSATR", params,
                       "the initial assignment processor "
                       "of the class in which %apiname of `%inhno' is described "
                       "must be the processor corresponding to `%inhno'")
```
これ以降、同じループ内の残り2箇所の `prcid` 参照（`INHNO_CREISR_VALID[prcid]` と `toIntnoVal[prcid][...]`）も `params["prcid"]` に置き換える。さらにその直後、`intnoParams["affinity"] = ...` の行と、それに続く「CFG_INTとDEF_INHが異なるクラス」チェックを丸ごと以下に置き換える：
```python
            intnoParams = cfgData["CFG_INT"][intnoVal]

            # 割込みハンドラ番号に対応するプロセッサが，割込みハンドラ番号に
            # 対応する割込み要求ラインが属するクラスの割付け可能プロセッサに
            # 含まれていない場合（E_RSATR）
            if params["prcid"] not in clsData[intnoParams["class"]]["affinityPrcList"]:
                error_ercd("E_RSATR", params,
                           "the processor corresponding to "
                           "`%inhno' must be included in the set of processors "
                           "which can accept the interrupt")
            else:
                if OMIT_MULTIPRC_INTERRUPT \
                        and clsData[intnoParams["class"]]["initPrc"] != params["prcid"]:
                    warning_api(params,
                                "the processor corresponding to `%inhno' "
                                "is not the processor which accepts the interrupt")
```

**編集3**（CRE_ISR ループ、`isratr`チェックの直前に警告ブロックを追加。Ruby: `interrupt.trb:325-338`相当）：
```python
    # 割込みサービスルーチンを実行するプロセッサに関するチェック
    if OMIT_MULTIPRC_INTERRUPT:
        initPrcBitmap = 1 << (clsData[params["class"]]["initPrc"] - 1)
        if clsData[params["class"]]["affinityPrcBitmap"] != initPrcBitmap:
            warning_api(params,
                        f"`%isrid' created within the class "
                        f"{clsData[params['class']]['clsid']} "
                        f"is configured to be executed by the processor "
                        f"{clsData[params['class']]['initPrc']} only.")
```
（`isratr`チェックの直前、`for _, params in sorted(cfgData["CRE_ISR"].items()):` ブロックの先頭付近に挿入）。同じループ内の `intnoParams["affinity"] = clsData[params["class"]]["affinityPrcBitmap"]` の行は削除する（Ruby側で削除されているのと同じ）。

**編集4**（「割込み要求ラインに関するエラーチェック（2回目）」ループを丸ごと削除。Ruby: `interrupt.trb:353-368`相当が削除されている）：
```python
#
#  割込み要求ラインに関するエラーチェック（2回目）
#
for _, params in cfgData["CFG_INT"].items():
    # CFG_INTに対応するDEF_INH／CRE_ISRがない場合
    if "affinity" not in params:
        params["affinity"] = clsData[params["class"]]["affinityPrcBitmap"]

    # ターゲット依存のエラーチェック
    if "TargetCheckCfgInt2" in globals():
        TargetCheckCfgInt2(params)
```
このブロック全体を削除する。

**編集5**（`_kernel_cfgint_table` 等の生成部、`params["affinity"]` → `params["affinityPrcBitmap"]` へ改名。ファイル末尾付近、`f"0x{params['affinity']:x}U }}"` の1箇所）：
```python
                f"0x{params['affinityPrcBitmap']:x}U }}")
```

- [ ] **Step 4: `kernel/kernel_check.py` を差分パッチする（最小・機械的）**

```bash
cp /tmp/claude-1000/-home-honda-TOPPERS-FMP3-fmp3-core/83e086ff-1095-4fbe-9020-0281a7d90d1d/scratchpad/fmp3_pico_sdk/kernel/kernel_check.py \
   /home/honda/TOPPERS/FMP3/fmp3_core/kernel/kernel_check.py
```
`fmp3_pico_sdk/kernel/kernel_check.py:267` の `if not OMIT_ISTACK:` を以下へ変更する（`OMIT_ISTK` は新設フラグ。ファイル冒頭で `if "OMIT_ISTK" not in globals(): OMIT_ISTK = False` を追加）：
```python
if not OMIT_ISTACK and not OMIT_ISTK:
```
（`istkTable = SYMBOL("_kernel_istk_table", True)` 以下の `is not None` ガードは pico_sdk 側に既に実装済みであることを確認済み。他の変更は不要。）

- [ ] **Step 5: `kernel/kernel.py` を差分パッチする**

```bash
cp /tmp/claude-1000/-home-honda-TOPPERS-FMP3-fmp3-core/83e086ff-1095-4fbe-9020-0281a7d90d1d/scratchpad/fmp3_pico_sdk/kernel/kernel.py \
   /home/honda/TOPPERS/FMP3/fmp3_core/kernel/kernel.py
```
`kernel.py:466` 付近の `_kernel_istk_table` 生成ブロックを `if not OMIT_ISTK:` で囲む：
```python
    if not OMIT_ISTK:
        kernelCfgC.add("STK_T *const _kernel_istk_table[TNUM_PRCID] = {")
        for index, prcid in enumerate(range(1, TNUM_PRCID + 1)):
            if index > 0:
                kernelCfgC.add(",")
            kernelCfgC.append(f"\t{icsData[prcid]['istk']}")
        kernelCfgC.add()
        kernelCfgC.add2("};")
```
（`_kernel_istkpt_table` 生成ブロック（直後の `#ifdef TOPPERS_ISTKPT`）はガードしない。Ruby側の差分も istk_table のみを対象にしている）。`TDOM_KERNEL` のドメインチェックは pico_sdk の `kernel.py` に元々存在しないため、削除作業は不要（§0.3 で確認済み）。

- [ ] **Step 6: 3ファイルの構文チェックと negative control**

```bash
cd /home/honda/TOPPERS/FMP3/fmp3_core
for f in interrupt kernel_check kernel; do
    python3 -m py_compile "kernel/${f}.py" && echo "OK ${f}.py" || echo "FAIL ${f}.py"
done
python3 -c "
import ast
for f in ['interrupt', 'kernel_check', 'kernel']:
    src = open(f'kernel/{f}.py').read()
    assert 'affinity\"]' not in src or f != 'interrupt', f'{f}: 旧affinityキーが残存'
print('affinityPrcBitmap リネーム確認OK')
"
```
Expected: 3個とも `OK`。`affinity` 旧キー残存チェックも通過。

- [ ] **Step 7: 差分パッチ3ファイルの結果を Ruby 側の差分行数と突き合わせて桁違いの見落としがないことを確認する**

```bash
wc -l kernel/interrupt.py kernel/kernel_check.py kernel/kernel.py
diff /tmp/claude-1000/-home-honda-TOPPERS-FMP3-fmp3-core/83e086ff-1095-4fbe-9020-0281a7d90d1d/scratchpad/fmp3_pico_sdk/kernel/interrupt.py kernel/interrupt.py | grep -c '^[<>]'
diff /tmp/claude-1000/-home-honda-TOPPERS-FMP3-fmp3-core/83e086ff-1095-4fbe-9020-0281a7d90d1d/scratchpad/fmp3_pico_sdk/kernel/kernel_check.py kernel/kernel_check.py | grep -c '^[<>]'
diff /tmp/claude-1000/-home-honda-TOPPERS-FMP3-fmp3-core/83e086ff-1095-4fbe-9020-0281a7d90d1d/scratchpad/fmp3_pico_sdk/kernel/kernel.py kernel/kernel.py | grep -c '^[<>]'
```
Expected: 3つの diff 行数がいずれも小さい（数十行オーダー。Ruby側の差分108/82/22行に見合う規模であること）。ゼロや数百行になっていたら Step 3〜5 のパッチが漏れているか過剰である。

- [ ] **Step 8: フルチェーンでの意味検証は Task 6（polarfire）・Task 9（musca_b1）へ持ち越し**

本タスク単独では `-T` テンプレートが未完成（Task 4〜5、7〜8）のため、`tools/cfg_equivalence.sh` の offset/kernel_cfg ステージはまだ通らない。ここでは構文的正しさとリネーム漏れの不在のみを確認し、意味的な正しさは Task 6/9 の差分等価性検査で確定する。

- [ ] **Step 9: コミット**

```bash
cd /home/honda/TOPPERS/FMP3/fmp3_core
git add kernel/alarm.py kernel/cyclic.py kernel/dataqueue.py kernel/eventflag.py \
        kernel/exception.py kernel/genoffset.py kernel/mempfix.py kernel/mutex.py \
        kernel/pridataq.py kernel/semaphore.py kernel/spin_lock.py kernel/task.py \
        kernel/interrupt.py kernel/kernel_check.py kernel/kernel.py
git commit -m "build(cfg): kernel/*.py を移植（12個そのまま流用＋3個差分パッチ）

流用元 fmp3_pico_sdk（FMP3 3.3.0ベース）。interrupt.py/kernel_check.py/
kernel.py の3個は 3.3.0→現pin(20260719)の Ruby 差分（108/82/22行）を
Python側へ適用：affinityPrcBitmap改名・OMIT_MULTIPRC_INTERRUPT新設・
OMIT_ISTK新設・2巡目CFG_INTループの削除。フルチェーンでの意味検証は
Task 6/9（テンプレート全数が揃った時点）に持ち越す。"
```

---

### Task 4: `arch/riscv_gcc/**`（polarfire chip 層、449行、全数新規）の移植

**Files:**
- Create: `arch/riscv_gcc/common/core_kernel.py`
- Create: `arch/riscv_gcc/common/core_check.py`
- Create: `arch/riscv_gcc/common/plic_kernel.py`
- Create: `arch/riscv_gcc/common/core_offset.py`
- Create: `arch/riscv_gcc/polarfire_soc/chip_kernel.py`

**Interfaces:**
- Consumes: `kernel/*.py`（Task 3）。**前例なし**（asp3_core の riscv `.py` は単一プロセッサ版で別物、`fmp3_pico_sdk` に riscv 実装自体が無い。§0.3 層D）。翻訳元は本リポジトリの pristine `arch/riscv_gcc/{common,polarfire_soc}/*.trb`（現物、下記の行範囲で全文確認済み）。
- Produces: `IncludeTrb("core_kernel.py")` / `IncludeTrb("plic_kernel.py")` / `IncludeTrb("core_check.py")`（`target/polarfire_soc_kit_gcc/*.py` から呼ばれる。Task 5 が Consumes）。

**翻訳仕様（Ruby原文を関数・変数単位で示す。実装者はこの一覧を過不足なく満たすこと）**：

**`arch/riscv_gcc/common/core_kernel.py`**（原文 `arch/riscv_gcc/common/core_kernel.trb` 全209行）:
- `def DefineVariableSection(genFile, defvar, secname):`（`.trb:74-80`）— セクション属性つき変数定義の生成。`secname != ""` なら `f'{defvar} __attribute__((section("{secname}"),nocommon));'`、そうでなければ `f'{defvar};'` を `genFile.add()`。
- `def SecnameKernelData(cls):`（`.trb:85-91`）— 常に `""` を返す。
- `def SecnameStack(cls):`（`.trb:96-102`）— 常に `""` を返す。
- `def GenerateNativeSpn(params):`（`.trb:107-110`）— `kernelCfgC.add(f"LOCK _kernel_lock_{params['spnid']};")` し `f"((intptr_t) &_kernel_lock_{params['spnid']})"` を返す。**チップ依存部（`chip_kernel.py`）がスピンロックを定義しないため無条件定義でよい**（Ruby版の `$generate_native_spn_defined` ガードは ARM-M 用の仕組みであり、`polarfire_soc/chip_kernel.trb` にはネイティブスピンロック定義が無いことを現物確認済み。riscv 側は無条件版でよい）。
- `IncludeTrb("kernel/kernel.py")`（`.trb:115`）
- 続けて `.trb:116-209` の内容（コメントヘッダなし、直接コード）：`1.upto(TNUM_PRCID)` ループ3本（割込みハンドラテーブル・割込み要求ライン設定テーブル・CPU例外ハンドラテーブル）を `for prcid in range(1, TNUM_PRCID + 1):` へ翻訳。`$INHNO_VALID[prcid]` 等は `INHNO_VALID[prcid]`（グローバル変数、`chip_kernel.py` が設定）。`$cfgData[:DEF_INH].has_key?(x)` は `x in cfgData["DEF_INH"]`。`$USE_INTCFG_TABLE` は `USE_INTCFG_TABLE`（`chip_kernel.py`／`target_kernel.py` 等が設定する想定。未設定なら `if "USE_INTCFG_TABLE" not in globals(): USE_INTCFG_TABLE = False` を本ファイル冒頭に置く）。`sprintf("%05x", x)` は `f"{x:05x}"`。

**`arch/riscv_gcc/common/core_check.py`**（原文 `core_check.trb` 全92行）:
- `IncludeTrb("kernel/kernel_check.py")`（`.trb:49`）
- 割込みハンドラテーブルチェック（`.trb:52-71`）：`p_inhTable = SYMBOL("_kernel_p_inh_table")`、`for _, params in cfgData["DEF_INH"].items():` で `prcid = clsData[params["class"]]["initPrc"]`、`inhTable = PEEK(p_inhTable + (prcid - 1) * sizeof_void_ptr, sizeof_void_ptr)`、`inthdr = PEEK(inhTable + params["index"] * sizeof_FP, sizeof_FP)`、アライン／非NULLチェックは `error_wrong_id("E_PAR", params, "inthdr", "inhno", "not aligned"/"null")`。
- CPU例外ハンドラテーブルチェック（`.trb:74-92`）：同型を `p_excTable = SYMBOL("_kernel_p_exc_table")` / `cfgData["DEF_EXC"]` / `error_wrong_id("E_PAR", params, "exchdr", "excno", ...)` で。
- ★これは `fmp3_pico_sdk/arch/arm_m_gcc/common/core_check.py` と**同型**（2テーブル・`index`添字方式）である。Task 7（ARM-M側）で書く `core_check.py` とは方式が異なる（ARM-M側は単一テーブル `_kernel_p_exc_tbl[prcidx]`・`inhno`/`excno`値添字）ので**混同しないこと**。

**`arch/riscv_gcc/common/core_offset.py`**（原文 `core_offset.trb` 全33行）:
- `IncludeTrb("kernel/genoffset.py")`
- `offsetH.append(...)` で `TCB_p_tinib` `TCB_pc` `TCB_sp` `TCB_stk_top` `TCB_fpu_flag` `TINIB_exinf` `TINIB_task` `TINIB_stk_bottom` `PCB_p_runtsk` `PCB_p_schedtsk` `PCB_idstkpt` `PCB_idstktop` `PCB_lock_flag` `PCB_target_pcb` の14個の `#define` を出力（`arch/arm_m_gcc/common/core_offset.py`（Task 7 で作成）と対応するオフセット名は同一。値の式（`offsetof_TCB_p_tinib` 等）も同名グローバル変数として engine が提供する）。

**`arch/riscv_gcc/common/plic_kernel.py`**（原文 `plic_kernel.trb` 全79行）:
- `kernelCfgC.comment_header("PLIC Interrupt target Context Index Table")`
- `_kernel_plic_target_cidx_table[PLIC_TNUM_INTNO + 1]` の生成（`.trb:49-61`）：`plic_intno_list` の各 `intno` について、`cfgData["CFG_INT"]` にあれば `f"{pid2cidx(clsData[cfgData['CFG_INT'][intno]['class']]['initPrc'])}U"`、無ければ `"0xffU"`。
- `def TargetCheckCfgInt(params):`（`.trb:66-79`）— `(params["intno"] >> 16) == 0` かつ `clsData[params["class"]]["affinityPrcBitmap"] != (1 << (clsData[params["class"]]["initPrc"] - 1))` のとき `error_ercd("E_RSATR", params, "%%intno is configured to be accepted by more than one processors, which is not supported on this target.")`。

**`arch/riscv_gcc/polarfire_soc/chip_kernel.py`**（原文 `chip_kernel.trb` 全36行）:
- `INTNO_VALID = {}` / `INHNO_VALID = {}` / `plic_intno_list = list(range(0, 183))`（Ruby `[*(0..182)]`）
- `for prcid in range(1, TNUM_PRCID + 1):` で `INTNO_VALID[prcid] = list(plic_intno_list)`、`INHNO_VALID[prcid] = [(prcid << 16) | intno for intno in plic_intno_list]`
- `def pid2cidx(pid): return (pid - 1) * 2 + 1`
- `IncludeTrb("core_kernel.py")`
- `IncludeTrb("plic_kernel.py")`

- [ ] **Step 1: 5ファイルを新規作成する**

上記の翻訳仕様に従って `arch/riscv_gcc/common/{core_kernel,core_check,plic_kernel,core_offset}.py` と `arch/riscv_gcc/polarfire_soc/chip_kernel.py` を作成する。

- [ ] **Step 2: 構文チェック**

```bash
cd /home/honda/TOPPERS/FMP3/fmp3_core
for f in arch/riscv_gcc/common/core_kernel.py arch/riscv_gcc/common/core_check.py \
         arch/riscv_gcc/common/plic_kernel.py arch/riscv_gcc/common/core_offset.py \
         arch/riscv_gcc/polarfire_soc/chip_kernel.py; do
    python3 -m py_compile "$f" && echo "OK $f" || echo "FAIL $f"
done
```
Expected: 5個すべて `OK`。

- [ ] **Step 3: `IncludeTrb` 呼び出しの拡張子が全て `.py` であることを機械確認（negative control：意図的に `.trb` を混入させて検出できることを確認）**

```bash
grep -n 'IncludeTrb(' arch/riscv_gcc/common/*.py arch/riscv_gcc/polarfire_soc/*.py
# 全て .py で終わっていることを確認
! grep -n 'IncludeTrb(' arch/riscv_gcc/common/*.py arch/riscv_gcc/polarfire_soc/*.py | grep -q '\.trb"'
echo "no-trb-leak exit=$?"
# negative control: 一時的に .trb を混入させて検出できることを確認
sed -i 's/IncludeTrb("core_kernel.py")/IncludeTrb("core_kernel.trb")/' arch/riscv_gcc/polarfire_soc/chip_kernel.py
grep -n 'IncludeTrb(' arch/riscv_gcc/polarfire_soc/chip_kernel.py | grep -q '\.trb"'
echo "negative-control-detected exit=$?"
git checkout -- arch/riscv_gcc/polarfire_soc/chip_kernel.py
```
Expected: `no-trb-leak exit=0`（`.trb` の混入なし）、`negative-control-detected exit=0`（意図的に混入させた `.trb` が検出できる）。

- [ ] **Step 4: 意味検証は Task 6（polarfire フルチェーン）へ持ち越し。ここでは回帰のみ確認**

Task 1 Step 6 と同じ polarfire QEMU 回帰コマンドを再実行し、`exit=124` かつ `Processor .* start.` 4行を確認する（本タスクは製品ビルドに一切影響しないため、変化がないことの確認）。

- [ ] **Step 5: コミット**

```bash
cd /home/honda/TOPPERS/FMP3/fmp3_core
git add arch/riscv_gcc/common/core_kernel.py arch/riscv_gcc/common/core_check.py \
        arch/riscv_gcc/common/plic_kernel.py arch/riscv_gcc/common/core_offset.py \
        arch/riscv_gcc/polarfire_soc/chip_kernel.py
git commit -m "build(cfg): arch/riscv_gcc/** を新規移植（polarfire chip層、449行、前例なし）

asp3_coreのriscv .pyは単一プロセッサ版で別物、fmp3_pico_sdkにriscv実装は
無いため全数新規。翻訳元は pristine の同名 .trb（現物）。意味検証は
Task 6（polarfireフルチェーン差分等価性検査）で行う。"
```

---

### Task 5: `target/polarfire_soc_kit_gcc/*.py`（91行、全数新規）の移植

**Files:**
- Create: `target/polarfire_soc_kit_gcc/target_kernel.py`
- Create: `target/polarfire_soc_kit_gcc/target_class.py`
- Create: `target/polarfire_soc_kit_gcc/target_check.py`

**Interfaces:**
- Consumes: `arch/riscv_gcc/polarfire_soc/chip_kernel.py`（Task 4）。
- Produces: これら3ファイルが Task 6 で `tools/cfg_equivalence.sh` から `-T`/`-C` 引数として直接渡される最上位テンプレート。

**翻訳仕様**：

**`target/polarfire_soc_kit_gcc/target_kernel.py`**（原文 `target_kernel.trb` 全11行）:
- `IncludeTrb("chip_kernel.py")` のみ。

**`target/polarfire_soc_kit_gcc/target_class.py`**（原文 `target_class.trb` 全69行）:
- `globalVars.append("clsData")`（Ruby `$globalVars.push`）
- `TNUM_PRCID` の値（1/2/3/4）で分岐し `clsData` 辞書を構築。キーは整数クラスID、値は `{"clsid": NumStr(n, "CLS_xxx"), "initPrc": p, "affinityPrcList": [...]}`。**Ruby原文（Task準備時に全文確認済み、`target_class.trb:14-69`）の4分岐（1/2/3/4コア）を1件も欠かさず転記する**：
  - `TNUM_PRCID==1`: `{1: CLS_PRC1(initPrc=1,[1]), 2: CLS_ALL_PRC1(initPrc=1,[1])}`
  - `TNUM_PRCID==2`: `{1: CLS_PRC1(1,[1]), 2: CLS_PRC2(2,[2]), 3: CLS_ALL_PRC1(1,[1,2]), 4: CLS_ALL_PRC2(2,[1,2])}`
  - `TNUM_PRCID==3`: `{1: CLS_PRC1(1,[1]), 2: CLS_PRC2(2,[2]), 3: CLS_PRC3(3,[3]), 4: CLS_ALL_PRC1(1,[1,2,3]), 5: CLS_ALL_PRC2(2,[1,2,3]), 6: CLS_ALL_PRC3(3,[1,2,3])}`
  - `TNUM_PRCID==4`: `{1: CLS_PRC1(1,[1]), 2: CLS_PRC2(2,[2]), 3: CLS_PRC3(3,[3]), 4: CLS_PRC4(4,[4]), 5: CLS_ALL_PRC1(1,[1,2,3,4]), 6: CLS_ALL_PRC2(2,[1,2,3,4]), 7: CLS_ALL_PRC3(3,[1,2,3,4]), 8: CLS_ALL_PRC4(4,[1,2,3,4])}`
  （`NumStr` は asp3_core 1.7.1 エンジンが提供するクラス。コンストラクタ引数はRuby `NumStr.new(n, "name")` と同じ位置引数。`affinityPrcBitmap` はこの辞書から `_class_proc()`（`cfg_py/engine_next/pass2.py:434`）が自動計算するため、ここでは `affinityPrcList` のみを与える。）

**`target/polarfire_soc_kit_gcc/target_check.py`**（原文 `target_check.trb` 全11行）:
- `IncludeTrb("core_check.py")` のみ。

- [ ] **Step 1: 3ファイルを新規作成する**

- [ ] **Step 2: 構文チェックと `clsData` 辞書の網羅性チェック**

```bash
cd /home/honda/TOPPERS/FMP3/fmp3_core
python3 -m py_compile target/polarfire_soc_kit_gcc/target_kernel.py \
                       target/polarfire_soc_kit_gcc/target_class.py \
                       target/polarfire_soc_kit_gcc/target_check.py
echo "py_compile exit=$?"
python3 -c "
import re
src = open('target/polarfire_soc_kit_gcc/target_class.py').read()
for n in [1, 2, 3, 4]:
    assert f'TNUM_PRCID == {n}' in src or f'TNUM_PRCID==\"{n}\"' in src or str(n) in src, f'TNUM_PRCID={n} 分岐が見当たらない'
print('4分岐の存在チェックOK（簡易）')
"
```
Expected: `py_compile exit=0`。

- [ ] **Step 3: negative control — `clsData` の分岐を1つ削ると、後続の Task 6 で確実に検出可能な状態であることを確認**

```bash
cp target/polarfire_soc_kit_gcc/target_class.py /tmp/target_class_backup.py
python3 -c "
src = open('target/polarfire_soc_kit_gcc/target_class.py').read()
# TNUM_PRCID==4 の分岐だけ削れないため、ここでは構文的に壊す軽い確認に留める
assert 'CLS_ALL_PRC4' in src, 'positive control: 4コア分岐が存在すること自体をまず確認'
print('positive control OK: CLS_ALL_PRC4 が存在する')
"
```
（本格的な negative control ―― この分岐が実際に生成物へ反映されることの確認 ―― は Task 6 の差分等価性検査で行う。ここでは静的な存在確認のみ。）

- [ ] **Step 4: 回帰確認（Task 4 Step 4 と同じ）**

- [ ] **Step 5: コミット**

```bash
cd /home/honda/TOPPERS/FMP3/fmp3_core
git add target/polarfire_soc_kit_gcc/target_kernel.py \
        target/polarfire_soc_kit_gcc/target_class.py \
        target/polarfire_soc_kit_gcc/target_check.py
git commit -m "build(cfg): target/polarfire_soc_kit_gcc/*.py を新規移植（91行、前例なし）

target_class.py は TNUM_PRCID 1〜4 の4分岐すべてを転記。意味検証は
Task 6（polarfireフルチェーン差分等価性検査）で行う。"
```

---

### Task 6: polarfire フルチェーンの差分等価性検査（初のテンプレート全数チェック）

**Files:**
- Modify: なし（Task 1〜5 の成果物を初めて `tools/cfg_equivalence.sh` のフルパイプラインで検証する）

**Interfaces:**
- Consumes: `cfg_py/engine_next/cfg.py`（Task 1）、`tools/cfg_equivalence.sh`（Task 2）、`kernel/*.py`（Task 3）、`arch/riscv_gcc/**/*.py`（Task 4）、`target/polarfire_soc_kit_gcc/*.py`（Task 5）。
- Produces: なし（検証タスク）。Task 3〜5 のバグ修正がこのタスク内で発生した場合、修正はそれぞれ該当ファイルへ加える。

- [ ] **Step 1: cfg1_out の syms/srec を最新化する**

```bash
cd /home/honda/TOPPERS/FMP3/fmp3_core
ninja -C build/polarfire_soc_kit-qemu cfg1_out_syms cfg1_out_srec
```

- [ ] **Step 2: フルパイプラインを実行する（positive control：MATCH することの実演）**

```bash
tools/cfg_equivalence.sh build/polarfire_soc_kit-qemu
echo "exit=$?"
```
Expected: `pass1`／`offset`／`kernel_cfg` の3ステージすべて `[OK]`、`RESULT = MATCH`、`exit=0`。**不一致が出た場合はここで Task 3〜5 の該当ファイルを修正し、再実行する（このタスクの主目的）。**

- [ ] **Step 3: negative control ①（差分検出能力の実演。テンプレートを意図的に1箇所壊す）**

```bash
cp target/polarfire_soc_kit_gcc/target_class.py /tmp/tc_backup.py
python3 -c "
src = open('target/polarfire_soc_kit_gcc/target_class.py').read()
# initPrc=1 の CLS_PRC1 を initPrc=99 に書き換えて壊す
broken = src.replace('\"initPrc\": 1, \"affinityPrcList\": [1]', '\"initPrc\": 99, \"affinityPrcList\": [1]', 1)
assert broken != src, '置換対象が見つからない（Step2までの実装内容を確認）'
open('target/polarfire_soc_kit_gcc/target_class.py', 'w').write(broken)
"
tools/cfg_equivalence.sh build/polarfire_soc_kit-qemu
echo "exit(broken)=$?"
cp /tmp/tc_backup.py target/polarfire_soc_kit_gcc/target_class.py
```
Expected: `[DIFFERS]` が offset または kernel_cfg のいずれかで出る、`RESULT = MISMATCH`、`exit(broken)` が非ゼロ。**「壊れた比較も全部一致を返しうる」（progress.md 記載の実例）ことへの対策として必須。** 復元後、`tools/cfg_equivalence.sh` を再実行して `MATCH` に戻ることも確認する。

- [ ] **Step 4: negative control ②（`cfg1_out.c` を比較対象から外すと検出できなくなる、という設計書 §7.1 の主張自体の裏取り）**

```bash
sed -n '/^== pass1/,/^== compiling/p' tools/cfg_equivalence.sh | head -1
```
`cfg1_out.c` 比較（pass1ステージ）は Task 1〜2 で既に独立してMATCHを確認済みだが、ここでは「もし pass1 の静的API個数を減らすテンプレート改変をしたら、offset/kernel_cfg 側の比較だけでは気づけるか」を実演する：

```bash
python3 -c "
src = open('kernel/task.py').read()
print('TA_ACT' in src, 'sample1.cfg 側の CRE_TSK 個数と cfg1_out.c の静的API個数の対応を壊すテストは複雑なため、'
      '設計書の該当主張はcfg1_out.cの直接比較で担保されていることをコード上確認する')
"
grep -n 'cfg1_out.c' tools/cfg_equivalence.sh
```
Expected: `compare_stage "pass1" cfg1_out.c` が `tools/cfg_equivalence.sh` に存在すること（Task 2 Step 1 で実装済み）を再確認する。これにより「offset.h/kernel_cfg.c/hだけでなく cfg1_out.c も比較対象に含まれている」という設計書 §7.1 の要件が本タスクの検査で毎回検証されることを確認する。

- [ ] **Step 5: 生成された `kernel_cfg.c` にマルチプロセッサ意味論が現れていることを目視確認（計画A Task 5 と同じ着眼点）**

```bash
WORK=$(tools/cfg_equivalence.sh build/polarfire_soc_kit-qemu 2>&1 | grep 'work dir preserved' | sed 's/.*at //')
grep -c '_kernel_pcb_prc[1-4]' "${WORK}/py/kernel_cfg.c"
grep -c '_kernel_istack_prc[1-4]' "${WORK}/py/kernel_cfg.c"
grep -c 'CLS_PRC[1-4]\|CLS_ALL_PRC[1-4]' "${WORK}/py/kernel_cfg.c"
```
Expected: 計画A Task 5 の実測（`_kernel_pcb_prc1..4` / `_kernel_istack_prc1..4` / 8クラス）と同じ個数が Python 生成物にも現れる。

- [ ] **Step 6: 回帰確認**

Task 1 Step 6 と同じ polarfire QEMU 起動確認を再実行する（本タスクは `cfg_py/cfg.py` を経由しない検証のみのため、製品ビルドは無変化のはず）。

- [ ] **Step 7: コミット（Task 3〜5 への修正がある場合のみ。無ければコミット不要）**

```bash
cd /home/honda/TOPPERS/FMP3/fmp3_core
git status --short kernel/ arch/riscv_gcc/ target/polarfire_soc_kit_gcc/
# 変更があれば
git add <該当ファイル>
git commit -m "fix(cfg): polarfire差分等価性検査で見つかった不一致を修正

tools/cfg_equivalence.sh build/polarfire_soc_kit-qemu が MATCH になるまで
Task 3〜5 の該当ファイルを修正。"
```

---

### Task 7: `arch/arm_m_gcc/common/*.py` の再構成 と `arch/arm_m_gcc/musca_b1/chip_kernel.py` の新規移植

**Files:**
- Create: `arch/arm_m_gcc/common/core_kernel.py`（★再構成。単純コピー禁止）
- Create: `arch/arm_m_gcc/common/core_offset.py`（軽微な調整）
- Create: `arch/arm_m_gcc/common/core_check.py`（★全面書き直し）
- Create: `arch/arm_m_gcc/musca_b1/chip_kernel.py`（新規、9行相当）

**Interfaces:**
- Consumes: `kernel/*.py`（Task 3）。
- Produces: `target/musca_b1_gcc/*.py`（Task 8）から `IncludeTrb("chip_kernel.py")` 経由で呼ばれる。

**★重要な訂正（§0.3 層C）**：`fmp3_pico_sdk/arch/arm_m_gcc/common/{core_kernel,core_check}.py` を**単純コピーしてはならない**。両者とも 3.3.0 時代の古い構造のままであり、現pinの pristine `.trb` とは構造が異なる：
- `core_kernel.trb` は v3.3.0 の276行から現pinの76行へ大幅縮小され、ベクタテーブル／例外テーブル／`_kernel_bitpat_cfgint` の生成コード（約150行）が `target_kernel.trb`（ターゲット依存部）側へ移動した。
- `core_check.trb` は `_kernel_p_inh_table`/`_kernel_p_exc_table`（2テーブル・`index`添字、riscv版と同型）から `_kernel_p_exc_tbl[prcidx]`（単一テーブル・`inhno`/`excno`値添字）へ方式が変わった。

**翻訳仕様（pristine 現物 `arch/arm_m_gcc/common/{core_kernel,core_check,core_offset}.trb` を翻訳元とし、pico_sdk は Python の構文慣用句――`SYMBOL`/`PEEK`/`error_wrong_id`の呼び方、辞書アクセス――の参考にのみ使うこと。構造は真似ない）**：

**`arch/arm_m_gcc/common/core_kernel.py`**（原文 `core_kernel.trb` 全76行、現物 Task 準備時に全文確認済み）:
- `def DefineVariableSection(genFile, defvar, secname):`（`.trb:15-21`）— riscv版と同一ロジック。
- `def SecnameKernelData(cls): return ""`（`.trb:26-28`）
- `def SecnameStack(cls): return ""`（`.trb:33-35`）
- ネイティブスピンロック：`if not generate_native_spn_defined: def GenerateNativeSpn(params): ...`（`.trb:48-53`）。**riscvと違いガード付き**（チップ依存部＝ `musca_b1/chip_kernel.py` が先に定義する可能性があるため）。`generate_native_spn_defined` は本ファイル冒頭で `if "generate_native_spn_defined" not in globals(): generate_native_spn_defined = False` として既定 `False` を与える。musca_b1 の `chip_kernel.py`（本タスクの4番目の成果物）はネイティブスピンロックを定義しないため、この既定実装がそのまま使われる。
- `def GenerateTskinictxb(key, params):`（`.trb:58-64`）— `f"{{\t(void *)({params['tinib_stk']}), \t((void *)((char *)({params['tinib_stk']}) + ({params['tinib_stksz']}))), }}"` を返す。
- `def GenResVectVal(num): return 0`（`.trb:69-71`）
- `IncludeTrb("kernel/kernel.py")`（`.trb:76`）
- ★**ベクタテーブル・例外テーブル・`_kernel_bitpat_cfgint` の生成コードはここに置かない**（`target/musca_b1_gcc/target_kernel.py` 側、Task 8 で書く）。

**`arch/arm_m_gcc/common/core_offset.py`**（原文 `core_offset.trb` 全29行）:
- riscv版（Task 4）と全く同じ14個の `#define` を出力（`IncludeTrb("kernel/genoffset.py")` に続けて `offsetH.append(...)`）。**pico_sdk の `core_offset.py` は既にこの構造と一致しており（§0.3 層B寄り）、`IncludeTrb("kernel/genoffset.py")` の引数だけ `.py` に直せば流用可能**（現物確認済み：`fmp3_pico_sdk/arch/arm_m_gcc/common/core_offset.py` は30行で内容がほぼ一致。タブ幅などの表記差は無視してよい）。

**`arch/arm_m_gcc/common/core_check.py`**（原文 `core_check.trb` 全97行。★全面書き直し）:
- `def GetStackTskinictxb(key, params, tinib): return PEEK(tinib + offsetof_TINIB_TSKINICTXB_stk_top, sizeof_void_ptr)`（`.trb:53-55`）
- `IncludeTrb("kernel/kernel_check.py")`（`.trb:60`）
- `p_excTbl = SYMBOL("_kernel_p_exc_tbl")`（`.trb:69`。★pico_sdk の `_kernel_p_inh_table`/`_kernel_p_exc_table` 方式ではない、単一テーブル）
- `for _, params in cfgData["DEF_INH"].items():`（`.trb:71-83`）：`prcid = clsData[params["class"]]["initPrc"]`、`excTbl = PEEK(p_excTbl + (prcid - 1) * sizeof_void_ptr, sizeof_void_ptr)`、`inthdr = PEEK(excTbl + (params["inhno"].val & 0xffff) * sizeof_FP, sizeof_FP)`、アライン／非NULLは `error_wrong_id("E_PAR", params, "inthdr", "inhno", "not aligned"/"null")`。
- `for _, params in cfgData["DEF_EXC"].items():`（`.trb:85-97`）：同型を `excno` について（`error_wrong_id("E_PAR", params, "exchdr", "excno", ...)`）。

**`arch/arm_m_gcc/musca_b1/chip_kernel.py`**（原文 `chip_kernel.trb` 全9行）:
- `IncludeTrb("core_kernel.py")` のみ。

- [ ] **Step 1: 4ファイルを新規作成する（上記翻訳仕様に厳密に従う。pico_sdk の該当ファイルはコピー元にしない）**

- [ ] **Step 2: 構文チェックと「古い構造が紛れ込んでいない」ことの negative-pattern チェック**

```bash
cd /home/honda/TOPPERS/FMP3/fmp3_core
for f in arch/arm_m_gcc/common/core_kernel.py arch/arm_m_gcc/common/core_offset.py \
         arch/arm_m_gcc/common/core_check.py arch/arm_m_gcc/musca_b1/chip_kernel.py; do
    python3 -m py_compile "$f" && echo "OK $f" || echo "FAIL $f"
done
# core_kernel.py にベクタテーブル生成が紛れ込んでいないことの確認（あれば設計ミス）
! grep -q '_kernel_vector_table' arch/arm_m_gcc/common/core_kernel.py
echo "no-vector-table-leak exit=$?"
# core_check.py が pico_sdk の古い方式（_kernel_p_inh_table）を使っていないことの確認
! grep -q '_kernel_p_inh_table\|_kernel_p_exc_table' arch/arm_m_gcc/common/core_check.py
echo "no-old-check-scheme exit=$?"
grep -q '_kernel_p_exc_tbl' arch/arm_m_gcc/common/core_check.py
echo "has-new-check-scheme exit=$?"
```
Expected: 4個すべて `OK`。3つの exit がすべて `0`（negative パターン不在の確認2件、新方式存在の確認1件）。

- [ ] **Step 3: 回帰確認（musca_b1 1コア・2コア、polarfire。3構成すべて。本タスクは製品ビルドに影響しないはずの確認）**

```bash
cd /home/honda/TOPPERS/FMP3/fmp3_core
timeout 20 /home/honda/qemu-build/install/bin/qemu-system-arm -M musca-b1 -nographic \
  -kernel build/musca_b1/fmp > /tmp/mb1_regress.log 2>&1
echo "exit=$?"; grep -c "^Processor .* start\.$" /tmp/mb1_regress.log
timeout 20 /home/honda/qemu-build/install/bin/qemu-system-arm -M musca-b1 -smp 2 -nographic \
  -kernel build/musca_b1-2core/fmp > /tmp/mb2_regress.log 2>&1
echo "exit=$?"; grep -c "^Processor .* start\.$" /tmp/mb2_regress.log
```
（実際の起動コマンドは `target/musca_b1_gcc/target.cmake` の `FMP3_RUN_COMMAND` を `cmake --build build/musca_b1 --target run` 経由で得るのが確実。手打ちで再現する場合は `ninja -C build/musca_b1 -t commands run | tail -1` で正確なコマンドを取得してから実行すること。）
Expected: musca_b1 1コアは `exit=124`／`Processor .* start.` 1行、2コアは `exit=124`／2行。

- [ ] **Step 4: コミット**

```bash
cd /home/honda/TOPPERS/FMP3/fmp3_core
git add arch/arm_m_gcc/common/core_kernel.py arch/arm_m_gcc/common/core_offset.py \
        arch/arm_m_gcc/common/core_check.py arch/arm_m_gcc/musca_b1/chip_kernel.py
git commit -m "build(cfg): arch/arm_m_gcc/common/*.py を再構成（3.4.0構造へ）＋musca_b1/chip_kernel.py新規

fmp3_pico_sdkの該当.pyは3.3.0時代の古い構造（ベクタテーブル生成がcore側、
チェックが2テーブルindex方式）のままで単純コピー不可と判明。pristine
現物のcore_kernel.trb/core_check.trbを翻訳元として書き直した。
ベクタテーブル生成はtarget_kernel.py（Task8）側へ、チェックは単一テーブル
_kernel_p_exc_tbl方式（inhno/excno値添字）へ、それぞれ正しく分離・移行。"
```

---

### Task 8: `target/musca_b1_gcc/*.py`（243行、全数新規、ベクタテーブル生成含む）の移植

**Files:**
- Create: `target/musca_b1_gcc/target_kernel.py`（★ベクタテーブル生成ロジックを含む、200行相当。最重要・最大リスク）
- Create: `target/musca_b1_gcc/target_class.py`
- Create: `target/musca_b1_gcc/target_check.py`

**Interfaces:**
- Consumes: `arch/arm_m_gcc/musca_b1/chip_kernel.py`（Task 7）。
- Produces: Task 9 で `tools/cfg_equivalence.sh` の `-T`/`-C` 引数として直接渡される最上位テンプレート。

**翻訳仕様（pristine 現物 `target/musca_b1_gcc/*.trb` 全文、Task 準備時に確認済み）**：

**`target/musca_b1_gcc/target_kernel.py`**（原文 `target_kernel.trb` 全200行）:
1. `INHNO_VALID`/`INTNO_VALID` の構築（`.trb:15-24`）：`for prcid in range(1, TNUM_PRCID + 1):` で `INHNO_VALID[prcid] = [(prcid << 16) | intno for intno in range(15, TMAX_INTNO + 1)]`、`INTNO_VALID[prcid]` も同じ範囲（Ruby `15.upto(TMAX_INTNO)` → Python `range(15, TMAX_INTNO + 1)`。**上限が inclusive であることに注意**）。
2. `EXCNO_VALID` の構築（`.trb:30-37`）：`excno_list = [2, 3, 4, 5, 6, 12]`（NMI/HardFault/MemManage/BusFault/UsageFault/DebugMonitor）、各 `prcid` について `[(prcid << 16) | excno for excno in excno_list]`。
3. `INTNO_CREISR_VALID = INTNO_VALID`／`INHNO_CREISR_VALID = INHNO_VALID`／`INHNO_DEFINH_VALID = INHNO_VALID`／`EXCNO_DEFEXC_VALID = EXCNO_VALID`（`.trb:42-49`。**Pythonでは別名参照であり、後から `INTNO_VALID` を再代入すると `INTNO_CREISR_VALID` は追随しない点に注意**——ただしこの後これらは再代入されないため実害なし。asp3_core系エンジンの慣習どおり参照共有のままでよい）。
4. `INTNO_CFGINT_VALID = INTNO_VALID`／`INTPRI_CFGINT_VALID = list(range(-(1 << TBITW_IPRI), 0))`（`.trb:56-57`）。
5. `IncludeTrb("chip_kernel.py")`（`.trb:62`）
6. `kernelCfgC.append("/*\n *  Target-dependent Definitions (ARM-M / Musca-B1)\n */\n")`（`.trb:67-71`）
7. **プロセッサ単位のベクタテーブル・例外/割込みハンドラテーブル生成**（`.trb:76-146`。★最重要ブロック）。`for prcid in range(1, TNUM_PRCID + 1):` の中で：
   - アライメント計算：`vecttbl_bytes = (TMAX_INTNO + 1) * 4`、`vecttbl_align = 32` から `while vecttbl_align < vecttbl_bytes: vecttbl_align <<= 1`。
   - `kernelCfgC.add(f'__attribute__ ((section(".vector"),aligned({vecttbl_align})))')`
   - `kernelCfgC.add(f"const FP _kernel_vector_table_prc{prcid}[] = {{")`
   - エントリ0：`(FP)(TOPPERS_ISTKPT(_kernel_istack_prc{prcid}, ROUND_STK_T(DEFAULT_ISTKSZ)))`（初期SP）
   - エントリ1：`(FP)_kernel_start`（リセットハンドラ）
   - エントリ2〜14：`for excno in range(2, 15):` — `excno in (8, 9, 10, 13)` は `(FP)({GenResVectVal(excno)})`、`excno == 11` は `(FP)(_kernel_svc_handler)`、`excno == 14` は `(FP)(_kernel_pendsv_handler)`、それ以外は `excnoVal = (prcid << 16) | excno` を計算し `cfgData["DEF_EXC"]` に登録があり `excatr & TA_DIRECT` が立っていれば `(FP)({cfgData["DEF_EXC"][excnoVal]["exchdr"]})`、無ければ `(FP)(_kernel_core_exc_entry)`。
   - `for inhnoVal in INHNO_VALID[prcid]:` — `cfgData["DEF_INH"]` に登録があり `inhatr & TA_NONKERNEL` が立っていれば `(FP)({cfgData["DEF_INH"][inhnoVal]["inthdr"]})`、無ければ `(FP)(_kernel_core_int_entry)`。
   - `kernelCfgC.add2("};")`
   - 続けて `_kernel_exc_tbl_prc{prcid}[]`：`for excno in range(0, 15):` で `cfgData["DEF_EXC"]` にあれば `exchdr`、無ければ `_kernel_default_exc_handler`。`for inhnoVal in INHNO_VALID[prcid]:` で `cfgData["DEF_INH"]` にあれば `inthdr`、無ければ `_kernel_default_int_handler`。
8. ポインタテーブル `_kernel_p_vector_table[TNUM_PRCID]` と `_kernel_p_exc_tbl[TNUM_PRCID]`（`.trb:148-168`）：各プロセッサの `_kernel_vector_table_prc{n}`／`_kernel_exc_tbl_prc{n}` へのポインタ配列。
9. `_kernel_bitpat_cfgint_prc{n}` と `_kernel_p_bitpat_cfgint`（`.trb:170-200`）：`bitpat_cfgint_num = (TMAX_INTNO >> 4) + (0 if (TMAX_INTNO & 0x0f) == 0 else 1)`。各32ビット語について `DEF_INH` に登録済みの `intno` ビットを立てる（`riscv/core_kernel.py` の `_kernel_bitpat_cfgint` 相当。ARM-M 版は `DEF_INH` のみを見る点に注意——riscv版と条件が異なる可能性があるため pristine 現物を優先する）。

**`target/musca_b1_gcc/target_class.py`**（原文 `target_class.trb` 全34行）:
- `TNUM_PRCID` 1コア／2コアの2分岐（4コア分岐は無い。musca_b1 は最大2コア）：
  - `1`: `{1: CLS_PRC1(1,[1]), 2: CLS_ALL_PRC1(1,[1])}`
  - `2`: `{1: CLS_PRC1(1,[1]), 2: CLS_PRC2(2,[2]), 3: CLS_ALL_PRC1(1,[1,2]), 4: CLS_ALL_PRC2(2,[1,2])}`

**`target/musca_b1_gcc/target_check.py`**（原文 `target_check.trb` 全9行）:
- `IncludeTrb("core_check.py")` のみ。

- [ ] **Step 1: 3ファイルを新規作成する（特に `target_kernel.py` は上記9項目を過不足なく実装する）**

- [ ] **Step 2: 構文チェックと negative パターンチェック**

```bash
cd /home/honda/TOPPERS/FMP3/fmp3_core
for f in target/musca_b1_gcc/target_kernel.py target/musca_b1_gcc/target_class.py \
         target/musca_b1_gcc/target_check.py; do
    python3 -m py_compile "$f" && echo "OK $f" || echo "FAIL $f"
done
# TNUM_PRCID 3/4コア分岐が無いこと（musca_b1は最大2コア）
! grep -q 'CLS_PRC3\|CLS_PRC4' target/musca_b1_gcc/target_class.py
echo "no-3-4-core-branch exit=$?"
# ベクタテーブル生成の主要シンボルが揃っていることの確認
for sym in _kernel_vector_table_prc _kernel_exc_tbl_prc _kernel_p_vector_table \
           _kernel_p_exc_tbl _kernel_bitpat_cfgint_prc _kernel_p_bitpat_cfgint; do
    grep -q "$sym" target/musca_b1_gcc/target_kernel.py && echo "found: $sym" || echo "MISSING: $sym"
done
```
Expected: 3個すべて `OK`。`no-3-4-core-branch exit=0`。6シンボルすべて `found`。

- [ ] **Step 3: `range(15, TMAX_INTNO + 1)` の inclusive/exclusive 境界を明示的にテストする（positive/negative control）**

```bash
python3 -c "
# Ruby '15.upto(N)' は 15..N の inclusive。Python 'range(15, N+1)' が正しい変換。
# range(15, N) だと最後の割込み番号が1個抜ける（negative control）。
N = 20  # 仮のTMAX_INTNO
correct = list(range(15, N + 1))
wrong = list(range(15, N))
assert len(correct) == N - 15 + 1, '正しい変換の範囲確認'
assert len(wrong) == len(correct) - 1, '誤った変換（境界バグ）が実際に1個少なくなることの確認'
print('inclusive境界の変換ミスがあれば len(wrong) != len(correct) で検出できることを確認: OK')
"
grep -n 'TMAX_INTNO' target/musca_b1_gcc/target_kernel.py | grep -n 'range('
```
Expected: 該当箇所が `range(15, TMAX_INTNO + 1)` の形になっていることを目視確認する（`+ 1` の有無を見落としやすいため、この境界は Task 9 の差分等価性検査でも必ず再確認する）。

- [ ] **Step 4: 回帰確認（Task 7 Step 3 と同じ、musca_b1 1コア・2コア）**

- [ ] **Step 5: コミット**

```bash
cd /home/honda/TOPPERS/FMP3/fmp3_core
git add target/musca_b1_gcc/target_kernel.py target/musca_b1_gcc/target_class.py \
        target/musca_b1_gcc/target_check.py
git commit -m "build(cfg): target/musca_b1_gcc/*.py を新規移植（243行、前例なし）

target_kernel.py は core_kernel.trb から移動してきたベクタテーブル／
例外テーブル／_kernel_bitpat_cfgint 生成ロジックを含む（Task7の
core_kernel.py再構成と対を成す）。target_class.py は1/2コアの2分岐のみ
（musca_b1は最大2コア、4コア分岐は無い）。意味検証はTask 9で行う。"
```

---

### Task 9: musca_b1 フルチェーンの差分等価性検査（1コア・2コア）

**Files:**
- Modify: なし（検証タスク。不一致が見つかれば Task 3・7・8 の該当ファイルを修正する）

**Interfaces:**
- Consumes: Task 1〜3、7〜8 の全成果物。
- Produces: なし。

- [ ] **Step 1: 1コア構成（`build/musca_b1`）の syms/srec を最新化し、フルパイプラインを実行する**

```bash
cd /home/honda/TOPPERS/FMP3/fmp3_core
ninja -C build/musca_b1 cfg1_out_syms cfg1_out_srec
tools/cfg_equivalence.sh build/musca_b1
echo "exit=$?"
```
Expected: `RESULT = MATCH`、`exit=0`。

- [ ] **Step 2: 2コア構成（`build/musca_b1-2core`）でも同様に実行する（AGENTS指定：1コアだけでは不十分）**

```bash
ninja -C build/musca_b1-2core cfg1_out_syms cfg1_out_srec
tools/cfg_equivalence.sh build/musca_b1-2core
echo "exit=$?"
```
Expected: `RESULT = MATCH`、`exit=0`。**★2コア構成は `target_class.py` の分岐2（`CLS_PRC2`/`CLS_ALL_PRC2`）と `target_kernel.py` の `for prcid in range(1, TNUM_PRCID + 1):` ループを prcid=2 まで実際に回すため、1コア構成だけでは検出できないバグ（例：`target_class.py` の分岐2の transcription ミス、`_kernel_vector_table_prc2` の生成ミス）をここで検出する。**

- [ ] **Step 3: negative control — 2コア構成でのみ踏む分岐を意図的に壊す**

```bash
cp target/musca_b1_gcc/target_class.py /tmp/mb_tc_backup.py
python3 -c "
src = open('target/musca_b1_gcc/target_class.py').read()
broken = src.replace('\"initPrc\": 2, \"affinityPrcList\": [ 2 ]',
                      '\"initPrc\": 1, \"affinityPrcList\": [ 2 ]', 1)
assert broken != src, '置換対象が見当たらない（Step1実装内容を確認）'
open('target/musca_b1_gcc/target_class.py', 'w').write(broken)
"
tools/cfg_equivalence.sh build/musca_b1-2core
echo "exit(broken-2core)=$?"
tools/cfg_equivalence.sh build/musca_b1
echo "exit(broken-1core, 影響が及ばないはず)=$?"
cp /tmp/mb_tc_backup.py target/musca_b1_gcc/target_class.py
```
Expected: `exit(broken-2core)` が非ゼロ（MISMATCH検出）。`exit(broken-1core, 影響が及ばないはず)` は **1コア構成の分岐1は変更していないため MATCH のまま（exit=0）**——これにより「1コアだけの検査では2コア固有の分岐のバグを見逃す」ことを実演する（AGENTS指定の「両ターゲットで行うこと。片方だけでは不十分」の musca_b1 版・コア数版に相当する裏取り）。復元後、両方とも MATCH に戻ることを確認する。

- [ ] **Step 4: 生成された `kernel_cfg.c` の意味論を目視確認**

```bash
WORK1=$(tools/cfg_equivalence.sh build/musca_b1 2>&1 | grep 'work dir preserved' | sed 's/.*at //')
WORK2=$(tools/cfg_equivalence.sh build/musca_b1-2core 2>&1 | grep 'work dir preserved' | sed 's/.*at //')
grep -c '_kernel_pcb_prc1\b' "${WORK1}/py/kernel_cfg.c"
grep -c '_kernel_pcb_prc[12]\b' "${WORK2}/py/kernel_cfg.c"
grep -c '_kernel_vector_table_prc[12]\b' "${WORK2}/py/kernel_cfg.c"
```
Expected: 1コアは `_kernel_pcb_prc1` のみ（計画A2 Task 4 の実測と一致）、2コアは `_kernel_pcb_prc1`/`_kernel_pcb_prc2` と `_kernel_vector_table_prc1`/`_kernel_vector_table_prc2` が両方現れる。

- [ ] **Step 5: 回帰確認（musca_b1 1コア・2コア・polarfire、3構成すべて）**

- [ ] **Step 6: コミット（Task 3・7・8 への修正がある場合のみ）**

```bash
cd /home/honda/TOPPERS/FMP3/fmp3_core
git status --short kernel/ arch/arm_m_gcc/ target/musca_b1_gcc/
# 変更があれば
git add <該当ファイル>
git commit -m "fix(cfg): musca_b1差分等価性検査（1コア・2コア）で見つかった不一致を修正"
```

---

### Task 10: エラー検出経路の回帰スイート（§7.2）

**Files:**
- Create: `tools/cfg_error_tests/e_rsatr_intno_affinity.cfg`
- Create: `tools/cfg_error_tests/e_rsatr_inhno_affinity.cfg`
- Create: `tools/cfg_error_tests/omit_istk.cfg`
- Create: `tools/cfg_error_tests/run.sh`

**Interfaces:**
- Consumes: `cfg_py/engine_next/cfg.py`（Task 1）、`kernel/interrupt.py`（Task 3、新設エラーチェックの実装先）。
- Produces: `tools/cfg_error_tests/run.sh` — エラー系 `.cfg` を Ruby/Python 両方に通し、終了コードとエラーメッセージの一致を確認するスイート。

**背景（設計書 §7.2）**：差分等価性検査（Task 6・9）は正常系の `.cfg`（`sample1.cfg`）しか見ない。`kernel/interrupt.trb` の3.3.0→現pin差分（108行）の過半は新規エラーチェックの追加（§0.3、Task 3 Step 3 参照）である。Task 3 で実装した以下2つの新規エラーチェックを、実際にエラーを起こす `.cfg` で確認する（AGENTS指定：規模は最小でよいが新規チェックは1件ずつ覆う）：

1. **CFG_INT の E_RSATR**（`interrupt.trb` 編集1）：クラスの割付け可能プロセッサが、`intno` の割込み要求ラインが接続されたプロセッサ集合に含まれない場合。
2. **DEF_INH の E_RSATR**（`interrupt.trb` 編集2）：`inhno` に対応するプロセッサが、対応する割込み要求ラインのクラスの割付け可能プロセッサに含まれない場合。

- [ ] **Step 1: sample1.cfg をベースに最小のエラー誘発 `.cfg` を作る**

```bash
cd /home/honda/TOPPERS/FMP3/fmp3_core
head -20 sample/sample1.cfg
```
（`sample1.cfg` の構造 — `INCLUDE`／`CRE_TSK`／`CFG_INT`／`ATT_ISR` 等の書式 — を確認したうえで、最小の `.cfg` を書く。以下は Task 6/9 が確認した `polarfire`（4コア既定）を前提にした具体例。**実装時に `sample1.cfg` の実際の書式に合わせて調整すること**）：

`tools/cfg_error_tests/e_rsatr_intno_affinity.cfg`:
```
/*
 *  E_RSATR: CFG_INT の class が、intno が要求されるプロセッサ集合を
 *  含まない場合のエラーを確認する最小構成。
 *  クラス CLS_PRC1（プロセッサ1のみ割付け可能）の囲みの中で、
 *  存在しない／プロセッサ1が受け付けない intno を指定する。
 */
INCLUDE("target_kernel.cfg");

CLASS(CLS_PRC1) {
    CFG_INT(31, { TA_ENAINT | TA_EDGE, 1 });
};
```

`tools/cfg_error_tests/e_rsatr_inhno_affinity.cfg`:
```
/*
 *  E_RSATR: DEF_INH の inhno に対応するプロセッサが、対応する intno の
 *  クラスの割付け可能プロセッサに含まれない場合のエラーを確認する。
 */
INCLUDE("target_kernel.cfg");

CLASS(CLS_PRC1) {
    CFG_INT(31, { TA_ENAINT | TA_EDGE, 1 });
    DEF_INH(31, { TA_NULL, dummy_inthdr });
};
CLASS(CLS_PRC2) {
    DEF_INH(2031, { TA_NULL, dummy_inthdr2 });
};
```

（★実装者への注記：`INTNO_VALID[prcid]` の実際の値域はチップ依存であるため、上記の具体的な `intno` 値（`31`）は polarfire の PLIC 番号体系に合わせた仮の値である。実装時は Task 4 で移植した `arch/riscv_gcc/polarfire_soc/chip_kernel.py` の `plic_intno_list`（0〜182）を踏まえ、「プロセッサ2以降にしか割り付けられない `intno`」を選び直すこと。`dummy_inthdr`／`dummy_inthdr2` はハンドラの前方宣言が cfg のパースを通すために必要な場合、`sample1.cfg` の既存ハンドラ関数名を流用してよい。）

`tools/cfg_error_tests/omit_istk.cfg`:
```
/*
 *  kernel_check.trb の OMIT_ISTK 分岐（Task3 Step4）を正常系として通す
 *  最小確認。OMIT_ISTK はビルドオプション経由でしか有効化されないため、
 *  本ファイルは「エラーが出ない」ことを確認する対照（sample1.cfg のまま
 *  で足りるため新規cfgは不要という結論もありうる。実装時に判断する）。
 */
INCLUDE("sample1.cfg");
```

- [ ] **Step 2: `run.sh` を書く**

`tools/cfg_error_tests/run.sh`:
```bash
#!/usr/bin/env bash
#
#		cfg_error_tests/run.sh -- エラー検出経路の最小回帰スイート
#
#  差分等価性検査（tools/cfg_equivalence.sh）は正常系しか見ない。
#  ここでは新規追加されたエラーチェック（kernel/interrupt.py の
#  E_RSATR×2）を、実際にエラーを起こす.cfgに対してRuby/Python両方に
#  通し、「エラーで止まること」と「終了コード・出力の傾向が一致する
#  こと」を確認する。
#
set -u
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
TESTDIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${1:?usage: $0 <build-preset-dir> <error.cfg> [expected-substring]}"
CFG="${2:?usage: $0 <build-preset-dir> <error.cfg> [expected-substring]}"
EXPECT="${3:-}"

WORK="$(mktemp -d /tmp/cfgerr.XXXXXX)"
CMD="$(ninja -C "${BUILD_DIR}" -t commands generated/cfg1_out.timestamp 2>/dev/null \
    | grep 'cfg_py/cfg\.py' | tail -1)"
ARGS="$(echo "${CMD}" | sed -E 's#^.*cfg_py/cfg\.py##; s# && .*$##')"
# 対象.cfgをsample1.cfgの代わりに差し込む（最後の.cfg引数を置換）
ARGS_TEST="$(echo "${ARGS}" | sed -E "s#[^ ]+/sample1\.cfg#${CFG}#")"

FAIL=0
for kind in ruby python; do
    mkdir -p "${WORK}/${kind}"
    if [ "${kind}" = "ruby" ]; then
        ( cd "${WORK}/${kind}" && ruby "${ROOT}/cfg/cfg.rb" ${ARGS_TEST} ) \
            > "${WORK}/${kind}/out.log" 2>&1
    else
        PYARGS="$(echo "${ARGS_TEST}" | sed -E 's#(-[TC] [^ ]+)\.trb#\1.py#g')"
        ( cd "${WORK}/${kind}" && python3 -B "${ROOT}/cfg_py/engine_next/cfg.py" ${PYARGS} ) \
            > "${WORK}/${kind}/out.log" 2>&1
    fi
    RC=$?
    echo "[${kind}] rc=${RC}"
    if [ -n "${EXPECT}" ]; then
        if grep -q "${EXPECT}" "${WORK}/${kind}/out.log"; then
            echo "[${kind}] expected substring found: OK"
        else
            echo "[${kind}] expected substring NOT found: FAIL"
            tail -10 "${WORK}/${kind}/out.log"
            FAIL=1
        fi
    fi
done
echo "work dir: ${WORK}"
exit "${FAIL}"
```

- [ ] **Step 3: 実行権限を付与し、positive control（エラーが実際に検出されること）を確認する**

```bash
chmod +x /home/honda/TOPPERS/FMP3/fmp3_core/tools/cfg_error_tests/run.sh
cd /home/honda/TOPPERS/FMP3/fmp3_core
tools/cfg_error_tests/run.sh build/polarfire_soc_kit-qemu \
    tools/cfg_error_tests/e_rsatr_intno_affinity.cfg "E_RSATR"
echo "exit=$?"
tools/cfg_error_tests/run.sh build/polarfire_soc_kit-qemu \
    tools/cfg_error_tests/e_rsatr_inhno_affinity.cfg "E_RSATR"
echo "exit=$?"
```
Expected: 両方とも `[ruby] rc=<非ゼロ>`／`[python] rc=<非ゼロ>`（cfg はエラー検出時に非ゼロ終了する）、`E_RSATR` が両方のログに含まれ `exit=0`（スイート自体の判定成功）。

- [ ] **Step 4: negative control — 正常系（sample1.cfg そのもの）ではエラーが出ないことを対照として確認する**

```bash
tools/cfg_error_tests/run.sh build/polarfire_soc_kit-qemu sample/sample1.cfg ""
echo "exit=$?"
```
Expected: `[ruby] rc=0`／`[python] rc=0`（正常系ではエラーにならない。これが崩れていたら Task 10 で作ったエラー系 `.cfg` が実際は「常にエラーになる壊れたテスト」ではなく「特定条件でのみエラーになる正しいテスト」であることの対照確認）。

- [ ] **Step 5: musca_b1 でも同様に実行する（両ターゲットで実施）**

`e_rsatr_intno_affinity.cfg`/`e_rsatr_inhno_affinity.cfg` の `intno` 値は PLIC 番号体系（polarfire）を前提にしているため、musca_b1（NVIC、`TMAX_INTNO` の範囲が異なる）向けに同趣旨の `.cfg` をもう1組作成し（`tools/cfg_error_tests/musca_b1_e_rsatr_intno_affinity.cfg` 等）、`tools/cfg_error_tests/run.sh build/musca_b1 <該当cfg> "E_RSATR"` で確認する。

- [ ] **Step 6: 回帰確認**

- [ ] **Step 7: コミット**

```bash
cd /home/honda/TOPPERS/FMP3/fmp3_core
git add tools/cfg_error_tests/
git commit -m "test(cfg): エラー検出経路の最小回帰スイートを追加

差分等価性検査（tools/cfg_equivalence.sh）は正常系しか見ないため、
kernel/interrupt.py の新規E_RSATRチェック（CFG_INT/DEF_INHのプロセッサ
割付け不整合）を実際にエラーを起こすcfgで確認する。polarfire・musca_b1
両方をカバー。"
```

---

### Task 11: 製品切替（cutover）— Python cfg 経路への切替

**Files:**
- Modify: `cfg_py/cfg.py`（Ruby委譲シムの内容を `cfg_py/engine_next/cfg.py` の内容で置き換える）
- Modify: `cfg_py/pass1.py`（新規、`engine_next/pass1.py` から移動）
- Modify: `cfg_py/pass2.py`（新規、`engine_next/pass2.py` から移動）
- Modify: `cfg_py/gen_file.py`（新規、`engine_next/gen_file.py` から移動）
- Modify: `cfg_py/srecord.py`（新規、`engine_next/srecord.py` から移動）
- Modify: `CMakeLists.txt`（`CFG_SCRIPT_DEPS` の6行）
- Modify: `arch/arm_m_gcc/common/arch.cmake:27`（`core_offset.trb` → `core_offset.py`）
- Modify: `arch/riscv_gcc/common/arch.cmake:21`（同上）
- Modify: `target/musca_b1_gcc/target.cmake:64-66`（`target_class`/`target_kernel`/`target_check` の3行）
- Modify: `target/polarfire_soc_kit_gcc/target.cmake:112-114`（同上）
- Remove: `cfg_py/engine_next/`（空になったディレクトリを削除）

**Interfaces:**
- Consumes: Task 1〜10 の全成果物。Task 6・9 で `tools/cfg_equivalence.sh` が両ターゲットで `MATCH` していることが前提条件。
- Produces: 製品ビルド（`cmake --build`）が Python cfg 経路で動く最終状態。

- [ ] **Step 1: 前提条件を再確認する（cutover前の最終ゲート）**

```bash
cd /home/honda/TOPPERS/FMP3/fmp3_core
ninja -C build/polarfire_soc_kit-qemu cfg1_out_syms cfg1_out_srec
tools/cfg_equivalence.sh build/polarfire_soc_kit-qemu; echo "polarfire exit=$?"
ninja -C build/musca_b1 cfg1_out_syms cfg1_out_srec
tools/cfg_equivalence.sh build/musca_b1; echo "musca_b1-1core exit=$?"
ninja -C build/musca_b1-2core cfg1_out_syms cfg1_out_srec
tools/cfg_equivalence.sh build/musca_b1-2core; echo "musca_b1-2core exit=$?"
tools/cfg_error_tests/run.sh build/polarfire_soc_kit-qemu \
    tools/cfg_error_tests/e_rsatr_intno_affinity.cfg "E_RSATR"; echo "error-test exit=$?"
```
Expected: 全て `exit=0`。**1つでも非ゼロなら本タスクを中断し、Task 6/9/10 へ戻って修正する。**

- [ ] **Step 2: エンジン本体を昇格する**

```bash
cd /home/honda/TOPPERS/FMP3/fmp3_core
git mv cfg_py/engine_next/cfg.py cfg_py/cfg.py
git mv cfg_py/engine_next/pass1.py cfg_py/pass1.py
git mv cfg_py/engine_next/pass2.py cfg_py/pass2.py
git mv cfg_py/engine_next/gen_file.py cfg_py/gen_file.py
git mv cfg_py/engine_next/srecord.py cfg_py/srecord.py
rmdir cfg_py/engine_next
rm -rf cfg_py/__pycache__
```
（`cfg_py/cfg.py` は `git mv` により計画A由来のRuby委譲シム内容が上書きされる。）

- [ ] **Step 3: `CMakeLists.txt` の `CFG_SCRIPT_DEPS` を書き換える**

`CMakeLists.txt` の該当ブロック（現状）：
```cmake
set(CFG_SCRIPT_DEPS
    ${FMP3_ROOT_DIR}/cfg_py/cfg.py
    ${FMP3_ROOT_DIR}/cfg/cfg.rb
    ${FMP3_ROOT_DIR}/cfg/pass1.rb
    ${FMP3_ROOT_DIR}/cfg/pass2.rb
    ${FMP3_ROOT_DIR}/cfg/GenFile.rb
    ${FMP3_ROOT_DIR}/cfg/SRecord.rb
)
message(STATUS "fmp3_core: cfg = cfg_py/cfg.py (Plan-A shim -> pristine cfg/cfg.rb)")
```
を以下へ置き換える：
```cmake
set(CFG_SCRIPT_DEPS
    ${FMP3_ROOT_DIR}/cfg_py/cfg.py
    ${FMP3_ROOT_DIR}/cfg_py/pass1.py
    ${FMP3_ROOT_DIR}/cfg_py/pass2.py
    ${FMP3_ROOT_DIR}/cfg_py/gen_file.py
    ${FMP3_ROOT_DIR}/cfg_py/srecord.py
)
message(STATUS "fmp3_core: cfg = cfg_py/cfg.py (asp3_core 1.7.1 engine, Plan-B)")
```
（周辺のコメント — 「シムである」「計画Bで差し替える」等の記述 — も cutover 後の事実に合わせて更新する。該当箇所は `CMakeLists.txt` 冒頭付近の cfg パイプライン説明コメント。）

- [ ] **Step 4: 4つの `.cmake` ファイルの `.trb` 参照を `.py` へ書き換える（8行）**

`Edit` で以下を実施：
- `arch/arm_m_gcc/common/arch.cmake:27` — `${COREDIR}/core_offset.trb` → `${COREDIR}/core_offset.py`
- `arch/riscv_gcc/common/arch.cmake:21` — 同上
- `target/musca_b1_gcc/target.cmake:64-66`:
  ```cmake
  list(APPEND FMP3_CLASS_TRB_FILES      ${TARGETDIR}/target_class.py)
  list(APPEND FMP3_KERNEL_CFG_TRB_FILES ${TARGETDIR}/target_kernel.py)
  list(APPEND FMP3_CHECK_TRB_FILES      ${TARGETDIR}/target_check.py)
  ```
- `target/polarfire_soc_kit_gcc/target.cmake:112-114` — 同上

- [ ] **Step 5: `arch.cmake`／`target.cmake` に、他の `.trb` 由来の参照が残っていないか最終grepする（negative control）**

```bash
cd /home/honda/TOPPERS/FMP3/fmp3_core
grep -rn '\.trb' --include=*.cmake . | grep -v 'cmake/trb_depends.cmake'
```
Expected: **無出力**（`cmake/trb_depends.cmake` 自身のコメント文中の `.trb` という単語を除き、実ファイルパスとしての `.trb` 参照はゼロになっていること）。

- [ ] **Step 6: ビルドディレクトリを再configureし、両ターゲットをクリーンビルドする**

```bash
cd /home/honda/TOPPERS/FMP3/fmp3_core
rm -rf build/polarfire_soc_kit-qemu build/musca_b1 build/musca_b1-2core
cmake --preset polarfire_soc_kit-qemu 2>&1 | tail -5
cmake --build build/polarfire_soc_kit-qemu 2>&1 | tail -20
echo "polarfire build exit=$?"
cmake --preset musca_b1 2>&1 | tail -5
cmake --build build/musca_b1 2>&1 | tail -20
echo "musca_b1 build exit=$?"
cmake --preset musca_b1-2core 2>&1 | tail -5
cmake --build build/musca_b1-2core 2>&1 | tail -20
echo "musca_b1-2core build exit=$?"
```
Expected: 3構成すべて `configure`/`build` が成功（exit=0）。**ここで初めて製品ビルドが Python cfg 経路を通る。**

- [ ] **Step 7: 生成物の実体を確認する（positive control：本当に Python 経路が使われたことの確認）**

```bash
cd /home/honda/TOPPERS/FMP3/fmp3_core
ninja -C build/polarfire_soc_kit-qemu -t commands generated/kernel_cfg.timestamp | tail -1 | grep -o 'cfg_py/[a-z]*\.py'
```
Expected: `cfg_py/cfg.py`（Ruby ではなく Python 経路であることのコマンドライン上の確認。中身が本物のエンジンかどうかは、`cfg_py/cfg.py` の先頭に「Ruby委譲シム」の記述が残っていないことでも確認する）：
```bash
head -5 cfg_py/cfg.py
```
Expected: asp3_core 由来のヘッダコメント（`cfg.py` の本来の目的説明）であり、「計画A限りのRuby委譲シム」という記述は存在しない。

- [ ] **Step 8: 全回帰（polarfire 4コア・musca_b1 1コア・musca_b1 2コア。QEMUで実際に起動すること。タイムアウト必須）**

```bash
cd /home/honda/TOPPERS/FMP3/fmp3_core
timeout 30 qemu-system-riscv64 -M microchip-icicle-kit -smp 5 -m 2G -nographic \
  -serial mon:stdio -bios none -kernel build/polarfire_soc_kit-qemu/fmp \
  -device loader,file=build/polarfire_soc_kit-qemu/fmp,cpu-num=0 \
  -device loader,file=build/polarfire_soc_kit-qemu/fmp,cpu-num=1 \
  -device loader,file=build/polarfire_soc_kit-qemu/fmp,cpu-num=2 \
  -device loader,file=build/polarfire_soc_kit-qemu/fmp,cpu-num=3 \
  -device loader,file=build/polarfire_soc_kit-qemu/fmp,cpu-num=4 > /tmp/final_pf.log 2>&1
echo "polarfire exit=$?"; grep -c "^Processor .* start\.$" /tmp/final_pf.log

ninja -C build/musca_b1 -t commands run 2>/dev/null | tail -1 > /tmp/mb1_run_cmd.txt
timeout 20 bash -c "$(sed 's/^cd [^&]*&& //' /tmp/mb1_run_cmd.txt)" > /tmp/final_mb1.log 2>&1
echo "musca_b1-1core exit=$?"; grep -c "^Processor .* start\.$" /tmp/final_mb1.log

ninja -C build/musca_b1-2core -t commands run 2>/dev/null | tail -1 > /tmp/mb2_run_cmd.txt
timeout 20 bash -c "$(sed 's/^cd [^&]*&& //' /tmp/mb2_run_cmd.txt)" > /tmp/final_mb2.log 2>&1
echo "musca_b1-2core exit=$?"; grep -c "^Processor .* start\.$" /tmp/final_mb2.log
```
Expected: polarfire `exit=124`／4行、musca_b1-1core `exit=124`／1行、musca_b1-2core `exit=124`／2行。**（実際のコマンド抽出方法はビルド環境依存のため、`ninja -t commands run` で得られない場合は `cmake --build build/<preset> --target run` を直接タイムアウト付きで実行し、`ps -eo pid,comm | grep qemu` でプロセス残留の有無を確認する代替手順を取ること。）**

- [ ] **Step 9: プロセス残留がないことを確認する（negative control：QEMUが二重起動・ゾンビ化していないか）**

```bash
ps -eo pid,comm | grep -i qemu
echo "残留プロセス数: $(ps -eo pid,comm | grep -ci qemu)"
```
Expected: `0`（timeout により全て終了している）。

- [ ] **Step 10: `git status` で意図しない変更が混入していないことを確認する**

```bash
cd /home/honda/TOPPERS/FMP3/fmp3_core
git status --short
```
Expected: Step 2〜4 で意図的に変更したファイルのみが表示される（`cfg_py/`・`CMakeLists.txt`・4つの `.cmake` ファイル）。

- [ ] **Step 11: コミット**

```bash
cd /home/honda/TOPPERS/FMP3/fmp3_core
git add cfg_py/cfg.py cfg_py/pass1.py cfg_py/pass2.py cfg_py/gen_file.py cfg_py/srecord.py \
        CMakeLists.txt arch/arm_m_gcc/common/arch.cmake arch/riscv_gcc/common/arch.cmake \
        target/musca_b1_gcc/target.cmake target/polarfire_soc_kit_gcc/target.cmake
git commit -m "build(cfg): 製品ビルドをPython cfg経路へ切替（計画B ★目標達成★）

cfg_py/engine_next/ の asp3_core 1.7.1 エンジンを cfg_py/ へ昇格し、
計画A由来のRuby委譲シムを置き換えた。CFG_SCRIPT_DEPS を cfg_py/*.py の
5ファイルへ差し替え、arch.cmake x2・target.cmake x2 の8箇所の .trb 参照を
.py へ書き換えた。polarfire 4コア・musca_b1 1/2コアともクリーンビルド
からQEMU起動を再確認。Ruby（cfg/cfg.rb）は tools/cfg_equivalence.sh
（CMake外）からのみ呼ばれるオラクルとして残る。"
```

---

### Task 12: `DIVERGENCE_MAP.md` の更新（pristine追加分の記録・期限付き逸脱の解消）

**Files:**
- Modify: `DIVERGENCE_MAP.md`

**Interfaces:**
- Consumes: Task 3〜9・11 で pristine ディレクトリ（`kernel/`・`arch/`・`target/`）へ追加した全 `.py` ファイル一覧。
- Produces: `DIVERGENCE_MAP.md` の更新版。AGENTS.md §2 規則2・6 の完了条件を満たす。

（本タスクをまとめタスクにした理由：Task 3〜9 は「差分等価性検査が通るまで何度も同じファイルを修正しうる」開発中の状態であり、確定していないファイルを都度 `DIVERGENCE_MAP.md` に記録すると、Task 6/9 での修正のたびに記載を更新し直す手戻りが生じる。Task 11 の cutover で全ファイルが確定した後にまとめて記録する方が正確。）

- [ ] **Step 1: 追加された `.py` ファイルを洗い出す**

```bash
cd /home/honda/TOPPERS/FMP3/fmp3_core
git log --diff-filter=A --name-only --pretty=format: -- 'kernel/*.py' 'arch/riscv_gcc/**/*.py' 'arch/arm_m_gcc/**/*.py' 'target/polarfire_soc_kit_gcc/*.py' 'target/musca_b1_gcc/*.py' | sort -u
```
Expected: 15（kernel）+ 5（riscv_gcc）+ 4（arm_m_gcc common/musca_b1）+ 3（target/polarfire）+ 3（target/musca_b1）= 30ファイルが漏れなく列挙される。

- [ ] **Step 2: `DIVERGENCE_MAP.md` の表に `add` 行を追加する**

既存の表形式（`| path | 種別 | 内容・理由 | 上流報告 |`）に、ディレクトリ単位でまとめた行を追加する：

```markdown
| kernel/*.py（15個） | add | asp3_core 1.7.1 cfg エンジン用 Python テンプレート。12個は `fmp3_pico_sdk`（FMP3 3.3.0）から流用、`interrupt.py`/`kernel_check.py`/`kernel.py` の3個は 3.3.0→3.4.0(20260719) の Ruby 差分をパッチ適用。計画B | - |
| arch/riscv_gcc/{common,polarfire_soc}/*.py（5個） | add | polarfire chip 層の Python テンプレート。前例なし・全数新規移植（asp3_core の riscv .py は単一プロセッサ版で別物、fmp3_pico_sdk に riscv 実装なし）。計画B | - |
| arch/arm_m_gcc/common/*.py（3個） | add | musca_b1 が使う ARM-M コア共通層。`fmp3_pico_sdk` の同名 `.py` は3.3.0時代の古い構造（ベクタテーブル生成がcore側、チェックが2テーブルindex方式）のため単純コピー不可と判明し、pristine現物の `.trb`（3.4.0）から書き直した。計画B | - |
| arch/arm_m_gcc/musca_b1/chip_kernel.py | add | musca_b1 chip 層。前例なし・全数新規（`fmp3_pico_sdk` に musca_b1 は無い）。計画B | - |
| target/polarfire_soc_kit_gcc/*.py（3個） | add | polarfire ターゲット層。前例なし・全数新規。計画B | - |
| target/musca_b1_gcc/*.py（3個） | add | musca_b1 ターゲット層。前例なし・全数新規。`target_kernel.py` は3.4.0で `core_kernel.trb` から移動してきたベクタテーブル／例外テーブル生成ロジックを含む。計画B | - |
```

- [ ] **Step 3: 「期限付きの逸脱」セクションの `cfg_py/cfg.py` 行を解消済みへ移す**

既存の「期限付きの逸脱」表の該当行を削除し、「解消済み事項」セクション（既存の HardFault 解消記録と同じ形式）へ以下を追記する：

```markdown
- **（2026-07-19 解消）`cfg_py/cfg.py`（計画Aの中身）が pristine の `cfg/cfg.rb` へ委譲する薄いシムであった件。**
  計画B（`docs/superpowers/plans/2026-07-19-fmp3-cmake-b-python-cfg.md`）で asp3_core 1.7.1 の
  本物の Python cfg エンジンへ差し替えた。`.trb`（Ruby）テンプレート30ファイル・3963行を
  `.py`（Python）へ全数移植し、pristine の Ruby cfg（VERSION 1.7.1、オラクルと版が一致）との
  差分等価性検査（`tools/cfg_equivalence.sh`）で `cfg1_out.c`／`offset.h`／`kernel_cfg.c`／
  `kernel_cfg.h` の一致を polarfire・musca_b1（1コア・2コア）の全構成で確認済み。
  製品ビルドの `CFG_SCRIPT_DEPS` は `cfg_py/*.py` の5ファイルのみを指し、
  pristine の `cfg/*.rb` は CMake のビルドグラフに一切含まれない
  （AGENTS.md §2 規則3を文言・精神の両方で満たす）。Ruby は `tools/cfg_equivalence.sh`
  （CMake外）からのみ呼ばれるオラクルとして残る。
```

- [ ] **Step 4: 「既知・対処しない事項」に pass3（`FMP3_CHECK_TRB_FILES`）の DEPENDS 追跡の隙間を追記する（§0.1 で発見した申し送り事項）**

```markdown
- **`FMP3_CHECK_TRB_FILES`（pass3・`fmp3_cfg_check()` が使う `target_check.py`/`core_check.py`/
  `kernel_check.py` の連鎖）は `cmake/trb_depends.cmake` の `fmp3_trb_closure()` の対象外**
  （`fmp3_trb_closure()` は `FMP3_OFFSET_TRB_FILES`／`FMP3_KERNEL_CFG_TRB_FILES` にしか
  呼ばれていない。`CMakeLists.txt:400-403`）。したがって `core_check.py`／`kernel_check.py`
  を編集しても pass3 の POST_BUILD が無警告に再実行されない可能性がある
  （計画A2 Task 5 で kernel_cfg/offset 側に見つかった同型の Critical と同じ性質の隙間だが、
  pass3 側は未修正のまま残っている）。実害は「pass3 が古いチェックロジックのまま実行され続ける」
  ことであり、ビルド失敗はしない（気づきにくい）。**対処は本計画のスコープ外**（cfg パイプラインの
  依存追跡強化は計画A/A2/Bのいずれの目的でもない）。将来 `core_check.py`／`kernel_check.py` を
  編集する際は、`ninja -C <builddir> -t clean && ninja -C <builddir>` でクリーンビルドして
  変更を確実に反映させること。
```

- [ ] **Step 5: 更新内容を目視レビューする（positive control：AGENTS.md §6 完了条件チェックリストとの突き合わせ）**

```bash
cd /home/honda/TOPPERS/FMP3/fmp3_core
grep -c '| add |' DIVERGENCE_MAP.md
grep -A3 '## 期限付きの逸脱' DIVERGENCE_MAP.md
```
Expected: `add` 行が Task 12 Step 2 で追加した6行分増えている。「期限付きの逸脱」セクションの表に `cfg_py/cfg.py` 行が **残っていない**（negative control：もし削除し忘れていれば、この grep で該当行がヒットする）。

- [ ] **Step 6: コミット**

```bash
cd /home/honda/TOPPERS/FMP3/fmp3_core
git add DIVERGENCE_MAP.md
git commit -m "docs: DIVERGENCE_MAP.mdを計画B完了にあわせ更新

kernel/arch/target配下に追加した30個の.pyをadd記録。cfg_py/cfg.pyの
期限付き逸脱を解消済みへ移動。FMP3_CHECK_TRB_FILESがtrb_depends.cmakeの
閉包計算対象外である申し送り事項を既知・対処しない事項に追記。"
```

---

### Task 13: 最終回帰とステージング残骸の除去

**Files:**
- なし（検証・掃除のみ）

**Interfaces:**
- Consumes: Task 1〜12 の全成果物。
- Produces: なし。計画B の完了確認。

- [ ] **Step 1: リポジトリに `cfg_py/engine_next/` の残骸が無いことを確認する**

```bash
cd /home/honda/TOPPERS/FMP3/fmp3_core
[ -d cfg_py/engine_next ] && echo "FAIL: engine_next が残っている" || echo "OK: engine_next は存在しない"
find . -name '__pycache__' -not -path './build/*' | tee /tmp/pycache_leftover.txt
[ -s /tmp/pycache_leftover.txt ] && echo "残骸あり" || echo "OK: __pycache__残骸なし"
```
Expected: 両方 `OK`。

- [ ] **Step 2: `cfg_py/README.md` の記述を現状に合わせて更新する**

現在の内容（計画A時点の記述、"CMake から呼ばれる" のみで実体がシムかエンジンか触れていない）を確認し、実装がシムから本物のエンジンへ切り替わった旨を反映する：

```bash
cat /home/honda/TOPPERS/FMP3/fmp3_core/cfg_py/README.md
```
既存記述が「pristine の `cfg/`（旧コンフィギュレータ）の代替」で始まる場合はそのまま維持できる（cutover後も意味が変わらないため）。asp3_core 1.7.1 由来である旨を1行追記する程度に留める（大幅な書き換えは不要）。

- [ ] **Step 3: 全プリセットの configure→build→run をゼロから通す（最終ゲート）**

```bash
cd /home/honda/TOPPERS/FMP3/fmp3_core
rm -rf build
for preset in polarfire_soc_kit-qemu musca_b1 musca_b1-2core; do
    echo "=== ${preset} ==="
    cmake --preset "${preset}" > "/tmp/final_configure_${preset}.log" 2>&1
    echo "configure exit=$?"
    cmake --build "build/${preset}" > "/tmp/final_build_${preset}.log" 2>&1
    echo "build exit=$?"
done
```
Expected: 全3構成で `configure exit=0`／`build exit=0`。

- [ ] **Step 4: 3構成すべての QEMU 起動を再確認する（Task 11 Step 8-9 と同じ内容の最終確認）**

Task 11 Step 8・9 のコマンドを再実行し、`Processor .* start.` の行数（4／1／2）とプロセス残留ゼロを確認する。

- [ ] **Step 5: ライブラリ専用モード（`FMP3_LIBRARY_ONLY=ON`）が両ターゲットとも壊れていないことを確認する（計画A Task 9・A2で確立済みの回帰項目）**

```bash
cd /home/honda/TOPPERS/FMP3/fmp3_core
cmake --preset polarfire_soc_kit-qemu -B build/polarfire-libonly-final -DFMP3_LIBRARY_ONLY=ON > /tmp/pf_libonly.log 2>&1
cmake --build build/polarfire-libonly-final > /tmp/pf_libonly_build.log 2>&1
echo "polarfire libonly exit=$?"
ls build/polarfire-libonly-final/libfmp3.a && echo "libfmp3.a OK"
[ ! -f build/polarfire-libonly-final/fmp ] && echo "fmp実行ファイルが作られていないことを確認OK"

cmake --preset musca_b1 -B build/musca_b1-libonly-final -DFMP3_LIBRARY_ONLY=ON > /tmp/mb_libonly.log 2>&1
cmake --build build/musca_b1-libonly-final > /tmp/mb_libonly_build.log 2>&1
echo "musca_b1 libonly exit=$?"
ls build/musca_b1-libonly-final/libfmp3.a && echo "libfmp3.a OK"
```
Expected: 両方とも `exit=0`、`libfmp3.a` が生成され `fmp` 実行ファイルは生成されない。

- [ ] **Step 6: `git status` が完全にクリーンであることを最終確認する**

```bash
cd /home/honda/TOPPERS/FMP3/fmp3_core
git status --short
```
Expected: 無出力（Step 5 で作った `build/*-final` ディレクトリは `.gitignore` 済みであることを確認。含まれていなければ削除する）。

```bash
rm -rf build/polarfire-libonly-final build/musca_b1-libonly-final
```

- [ ] **Step 7: AGENTS.md §6 完了条件チェックリストとの最終突き合わせ**

```bash
cd /home/honda/TOPPERS/FMP3/fmp3_core
echo "--- upstream branch has no derived files ---"
git ls-tree -r upstream --name-only | grep -E '\.py$|CMakeLists|\.cmake$' | head
echo "（上記が空であること）"
echo "--- pristine changes recorded in DIVERGENCE_MAP.md ---"
git log --diff-filter=AM --name-only --pretty=format: -- 'kernel/*' 'arch/*' 'target/*' | \
    grep -v '\.trb$\|\.c$\|\.h$\|\.def$\|\.ld$\|Makefile\|\.txt$\|\.md$' | sort -u | wc -l
grep -c '| add |' DIVERGENCE_MAP.md
echo "（両者を突き合わせて記載漏れがないか目視確認）"
echo "--- UPSTREAM_PRISTINE.txt matches upstream branch origin ---"
cat UPSTREAM_PRISTINE.txt | tail -1
echo "--- cmake --preset && build passes ---"
echo "Task 13 Step 3 で確認済み"
```

- [ ] **Step 8: コミット（README.md 更新分のみ。他は検証タスクのため差分なし）**

```bash
cd /home/honda/TOPPERS/FMP3/fmp3_core
git add cfg_py/README.md
git commit -m "docs: cfg_py/README.mdをasp3_core 1.7.1エンジン実装完了にあわせ更新

計画B完了。全プリセット（polarfire_soc_kit-qemu / musca_b1 / musca_b1-2core）
のconfigure→build→QEMU起動、ライブラリ専用モード、AGENTS.md §6完了条件を
最終確認した。"
```

---

## タスク一覧（サマリ）

| Task | 内容 | 成果物 |
|---|---|---|
| 1 | cfg エンジンのステージング移植・pass1差分等価性の先行証明 | `cfg_py/engine_next/*.py`（5個） |
| 2 | `tools/cfg_equivalence.sh` の実装 | 差分等価性検査スクリプト |
| 3 | `kernel/*.py` の移植（15個） | 12個流用＋3個差分パッチ |
| 4 | `arch/riscv_gcc/**` の新規移植（polarfire、449行） | 5ファイル |
| 5 | `target/polarfire_soc_kit_gcc/*.py` の新規移植（91行） | 3ファイル |
| 6 | polarfire フルチェーン差分等価性検査 | positive/negative control実演 |
| 7 | `arch/arm_m_gcc/common/*.py` 再構成＋`musca_b1/chip_kernel.py`新規 | 4ファイル |
| 8 | `target/musca_b1_gcc/*.py` の新規移植（243行） | 3ファイル |
| 9 | musca_b1 フルチェーン差分等価性検査（1/2コア） | positive/negative control実演 |
| 10 | エラー検出経路の回帰スイート | `tools/cfg_error_tests/` |
| 11 | 製品切替（cutover） | `cfg_py/`本体差替・CMake 8箇所更新 |
| 12 | `DIVERGENCE_MAP.md` 更新 | add記録30件・期限付き逸脱の解消 |
| 13 | 最終回帰・残骸除去 | 計画B完了確認 |

---

## Self-Review（実施記録）

**1. Spec coverage**：ブリーフの要求事項を1つずつ確認した。
- 「`cfg_py/` を本物のPythonエンジンに差し替える」→ Task 1（ステージング）・Task 11（cutover）。
- 「`.trb` テンプレートを `.py` へ移植する」→ Task 3〜5・7〜8（全30ファイル、3963行）。
- 「pristine の Ruby cfg との差分等価性検査で正しさを検証する」→ Task 2・6・9。pass1のみならず `cfg1_out.c`／`offset.h`／`kernel_cfg.c/h` の全対象、positive/negative control、両ターゲット・両コア数を満たす。
- 「製品ビルドを Python 経路へ切替え、Ruby はオラクルとしてのみ残す」→ Task 11。AGENTS.md §2 規則3の逸脱解消 → Task 12。
- 「CFG_SCRIPT_DEPS の差し替えだけで済むはずという仮説の検証」→ §0.4 で実測し、8箇所の追加変更が必要という訂正結果を Task 11 に反映した。
- 「移植量を流用分と新規分を分けて正確に出す」→ §0.2〜0.3 で実測（3963行、A:1280/B:1689/C:202/D:792）。
- 「エラー検出経路の回帰スイート」→ Task 10。
- 「回帰項目（polarfire 4コア・musca_b1 1コア/2コア、タイムアウト必須、ps -eo pid,comm）」→ 全タスクの Step に組み込み済み。

**2. Placeholder scan**：「TBD」「適切に」「Task Nと同様」の直書きが無いか grep 確認した。Task 6/7/9 で「Task 1 Step 6 と同じコマンドを再実行する」という参照表現を使っている箇所があるが、これは実際にそのタスクの中に完全なコマンドが存在し実装者が遡って読める場合に限定して使っており、コード自体を省略しているわけではない（skill の禁止例「Similar to Task N（コードを省略）」とは異なり、同一の確認コマンド列を毎回転記すると数百行の冗長な重複になるため、直近タスクの完全なコマンドへの参照に留めた）。念のため Task 9 Step 5、Task 11 Step 8 は該当タスク内から辿れる直前ステップを明示した。

**3. Type consistency**：
- `OMIT_MULTIPRC_INTERRUPT`／`OMIT_ISTK` の変数名は Task 3 の Step 3・4 で導入し、以降どのタスクからも参照されないことを確認した（一貫）。
- `params["affinityPrcBitmap"]`（Task 3）は Task 4/7 のテーブル生成コードで参照される `affinityPrcBitmap` キーと綴りが一致することを確認した。
- `tools/cfg_equivalence.sh` の関数名（`extract_cmd`／`run_cmd`／`compare_stage`）は Task 2 で定義し、Task 6・9・11 から「`tools/cfg_equivalence.sh <build-dir>`」という外部インターフェースのみで呼ばれ、内部関数名への依存はない（疎結合）。
- `cfg_py/engine_next/` というステージングパスは Task 1 で作成し、Task 2・6・9・10 の検証コマンドが一貫して参照し、Task 11 で `git mv` により解消される。名前の変遷（`engine_next` → `cfg_py/` 直下）に矛盾がないことを確認した。

**4. 自己レビューで直した点**：
- 当初 Task 1 を「エンジン移植」、Task 2 を「等価性スクリプト」と分けて書いたが、Task 1 だけでは検証手段が手作業になり再現性が低いため、Task 1 に手動での pass1 比較（教育的・原理の実演）を残しつつ、Task 2 で恒久スクリプト化する構成にした（手作業→自動化の順序を明示）。
- 当初 `arch/arm_m_gcc/common/*.py` を「`fmp3_pico_sdk` から流用」としていたが、実測（v3.3.0での276行→現在の76行という差分）により pico_sdk 版が3.3.0時代の構造のままで単純コピー不可と判明したため、§0.3 に層Cとして明記し、Task 7 の説明文とTranslation仕様を「pico_sdk はコピー元にしない、pristine現物を翻訳元にする」よう全面的に書き直した。
- 当初「CFG_SCRIPT_DEPSの差し替えだけ」という前提をそのまま書きかけたが、`grep -rn '\.trb' --include=*.cmake .` を実際に走らせたところ8箇所の追加参照が見つかったため、§0.4 で訂正し、Task 11 の Step 3-4 を分離してどちらも明示した。
- Task 10 のエラー系 `.cfg` の具体的な `intno` 値はチップ依存で確定できなかったため、プレースホルダにせず「polarfire PLIC番号体系を仮定した具体例＋実装時に値を選び直す旨の明示的な注記」という形で誠実に書いた（値そのものを保証はしないが、確認すべき条件と手順は具体的に書いた）。
- Task 9 に「1コアだけでは2コア固有のバグを見逃す」ことを実演する negative control（Step 3）を追加した（AGENTS指定の「片方だけでは不十分」という一般原則を、musca_b1のコア数という具体的な軸に対しても適用した）。
