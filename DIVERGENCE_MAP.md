# DIVERGENCE_MAP — 上流 pristine への乖離台帳

上流：fmp3_archive（`UPSTREAM_VERSION` / `UPSTREAM_PRISTINE.txt` 参照）。
pristine を改変したら必ずここに記録する（マージ衝突解決の根拠になる）。

| path | 種別 | 内容・理由 | 上流報告 |
|------|------|-----------|----------|
| cfg/ | none | **無改変**（`git diff upstream main -- cfg` は空）。AGENTS.md §2 規則3（cfg 相当は `cfg_py/` で提供し CMake から呼ぶ、pristine の `cfg/` は使わない）を**文言・精神の両方で満たす**（2026-07-19、計画B Task 11 の cutover 完了により確定。下記「解消済み事項」参照）。`CMakeLists.txt` が呼ぶのは常に `cfg_py/cfg.py` であり，`cfg/cfg.rb` はコメント（`CMakeLists.txt:158-163`）にのみ現れる。`cfg/` は CMake のビルドグラフからは**完全に不使用**だが，`tools/cfg_equivalence.sh`（CMake 外）が差分等価性検査のオラクルとして引き続き呼ぶため，ファイル自体は削除しない。次の `git merge upstream` で `cfg/` に衝突が出た場合も，pristine 側の変更を素直に取り込んでよい（我々の改変は無い＝none） | - |
| target/ | remove | 使わない target を取り込まない（imx8mm_evk_arm64_gcc / raspberrypi_pico_gcc / stm32mp257f_dk_arm64_gcc / zcu102_arm64_gcc / zybo_gcc / zybo_z7_gcc）。allowlist は `tools/upstream_targets.txt`。復活させたい場合は 1 行足して `import_upstream.sh` 再実行 → `git merge upstream` | - |
| target/m5stamp_esp32p4_gcc | remove | ESP32-P4 のターゲット依存部は本リポジトリでは管理しない。`fmp3_esp_idf`（別リポジトリ、`/home/honda/TOPPERS/ESP32/fmp3_esp_idf`）が chip 依存部・ターゲット依存部の両方を管理する方針にユーザが決定したため（2026-07-19）。上記6個（使わないから外す）とは除外理由が異なる点に注意。`arch/riscv_gcc/esp32p4`（chip 依存部）は上流追従の差分を見られる利点を保つため pristine のまま残す＝除外対象外。復活させたい場合は `tools/upstream_targets.txt` に 1 行足して `import_upstream.sh` 再実行 → `git merge upstream` | - |
| arch/riscv_gcc/common/arch.cmake | add | Makefile.core の CMake 版。上流の Makefile は残すが CMake ビルドからは参照しない | - |
| arch/riscv_gcc/polarfire_soc/chip.cmake | add | Makefile.chip の CMake 版。`-march` は上流の `rv64gc` ではなく `rv64imafdc`（ISA は同一。`rv64gc` は実在しない multilib ディレクトリ `rv64imafdc/lp64d` に解決され `crt0.o` が見つからないが、`rv64imafdc` は既定ディレクトリ `.` に解決される。ABI は `lp64d` のまま） | 未 |
| target/polarfire_soc_kit_gcc/{target.cmake,presets.json} | add | Makefile.target の CMake 版。Microchip SDK のソース16個を最終リンクに加える。FMP3_LDSCRIPT_VIA_DRIVER_T=ON を宣言する（picolibc.specs の %{!T:-Tpicolibc.ld} が -Wl,-T, では防げないため。汎用層 CMakeLists.txt 側のトグルを読む形に改めた＝計画A2 Task 1） | - |
| arch/arm_m_gcc/common/arch.cmake | add | Makefile.core の CMake 版。上流の Makefile は残すが CMake ビルドからは参照しない | - |
| arch/arm_m_gcc/musca_b1/chip.cmake | add | Makefile.chip の CMake 版。--gc-sections は上流に無いため使わない（polarfire の l2lim 制約がこのターゲットには無い） | - |
| target/musca_b1_gcc/{target.cmake,presets.json} | add | Makefile.target の CMake 版。QEMU 専用ターゲット（実機非対応）。FMP3_LDSCRIPT_VIA_DRIVER_T は設定しない（既定 OFF＝-Wl,-T, のままでよい） | - |
| kernel/*.py（15個） | add | asp3_core 1.7.1 cfg エンジン用 Python テンプレート。12個は `fmp3_pico_sdk`（FMP3 3.3.0）から流用、`interrupt.py`/`kernel_check.py`/`kernel.py` の3個は 3.3.0→3.4.0(20260719) の Ruby 差分をパッチ適用。計画B | - |
| arch/riscv_gcc/{common,polarfire_soc}/*.py（5個） | add | polarfire chip 層の Python テンプレート。前例なし・全数新規移植（asp3_core の riscv .py は単一プロセッサ版で別物、fmp3_pico_sdk に riscv 実装なし）。計画B | - |
| arch/arm_m_gcc/common/*.py（3個） | add | musca_b1 が使う ARM-M コア共通層。`fmp3_pico_sdk` の同名 `.py` は3.3.0時代の古い構造（ベクタテーブル生成がcore側、チェックが2テーブルindex方式）のため単純コピー不可と判明し、pristine現物の `.trb`（3.4.0）から書き直した。計画B | - |
| arch/arm_m_gcc/musca_b1/chip_kernel.py | add | musca_b1 chip 層。前例なし・全数新規（`fmp3_pico_sdk` に musca_b1 は無い）。計画B | - |
| target/polarfire_soc_kit_gcc/*.py（3個） | add | polarfire ターゲット層。前例なし・全数新規。計画B | - |
| target/musca_b1_gcc/*.py（3個） | add | musca_b1 ターゲット層。前例なし・全数新規。`target_kernel.py` は3.4.0で `core_kernel.trb` から移動してきたベクタテーブル／例外テーブル生成ロジックを含む。計画B | - |

種別: add=追加 / patch=部分改変 / replace=置換 / remove=削除 / none=無改変（差分ゼロだが，
運用上の注意が必要なため記録目的で本表に載せている。現状 `cfg/` のみ）

## 既知・対処しない事項

- **`ld: ... has a LOAD segment with RWX permissions` 警告**（`cfg1_out` / `fmp` リンク時）。
  pristine の Microchip リンカスクリプト（`target/polarfire_soc_kit_gcc/sdk/.../mpfs-lim.ld` が
  全領域を rwx 宣言）に起因する無害な警告であり，**意図的に対処しない**（分離しようとする方が
  pristine への不要な改変を生み危険）。`cfg1_out` は実行しないため実害は無く，`fmp` も QEMU 実機で
  正常起動を確認済み。

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

- **`cfg_py/cfg.py` の `pass3()` が持つ非対称性: Ruby の `$timeStampFileName` グローバルに相当する
  Python 側の状態が pass3 の名前空間フィルタに阻まれ、`check.timestamp` が書かれない。**
  `kernel/kernel_check.py:57` は `timeStampFileName = "check.timestamp"` をtrb実行時の名前空間
  （`ns`）に設定するが、`cfg_py/cfg.py` の `pass3()` は `ns` から `g` へのマージを
  `g.update({k: v for k, v in ns.items() if k in g.get("globalVars", [])})`（`cfg_py/cfg.py:545-546`）
  という**明示的な allowlist（`globalVars`）越しにしか行わない**のに対し、`kernel_check.py` は
  `timeStampFileName` を `globalVars` に登録しない。したがって `main()` の
  `if g.get("timeStampFileName"): open(...)`（`cfg_py/cfg.py:782-783`）は常に偽のまま終わり、
  Python 経路では `check.timestamp` が一度も書かれない（Ruby の `cfg/cfg.rb:749-750` は
  `$timeStampFileName` が真のグローバル変数のため、この非対称性を持たない）。
  **製品ビルドへの影響を確認した**：`CMakeLists.txt` の `fmp3_cfg_check()`（pass3 を呼ぶ
  `add_custom_command(TARGET fmp POST_BUILD ...)`）は `check.timestamp` を `OUTPUT` として
  宣言しておらず、CMake のビルドグラフはこのファイルの有無を一切参照しない。実際に
  `build/{musca_b1,musca_b1-2core,polarfire_soc_kit-qemu,*-libonly}/generated/` を
  全数 `find` したが `check.timestamp` はどこにも存在せず（`kernel_cfg.timestamp`／
  `offset.timestamp`／`cfg1_out.timestamp` という別物の CMake 自身の追跡ファイルとは別）、
  一方で `fmp.syms`／`fmp.srec`（pass3 実行の副産物）は全構成で生成されていた＝pass3 自体は
  実行されているが `check.timestamp` だけが欠落している状態を実測で確認した。
  **結論：製品ビルドの成否・正しさには影響しない**（誰も読んでいない孤立した副産物が欠けているだけ）。
  Task 6 で発見時点では影響未確認だったが、Task 13 で影響なしと確定した。念のため
  pristine（`kernel/kernel_check.trb` 由来の翻訳）は未修正のまま残す（挙動を変えると
  差分等価性検査の前提が変わるため、修正するなら計画外で別途検討）。

- **`target/musca_b1_gcc/target_class.py`（`target_class.trb` 翻訳）の `clsid` は、
  `tools/cfg_equivalence.sh` の差分等価性検査では検証できない構造的な盲点がある**
  （計画B Task 8・9 で発見）。`clsid` は musca_b1（ARM-M・NVIC）では警告文言にしか現れず
  生成物（`kernel_cfg.c`／`kernel_cfg.h`）のバイト列には出力されない
  （`SecnameKernelData` が空文字列を返すため、セクション名にも漏れない）。
  さらに1コア構成では両クラスが `initPrc=1`／`affinityPrcBitmap=1` に縮退し差の出る
  フィールドが無くなるため、2コア構成でも「全部一致」の意味を過大評価しないこと。
  **polarfire（RISC-V・PLIC）は対象外**：`clsid` が `kernel_cfg.c` のセクション名
  （`.kernel_data_CLS_PRC1` 等）に漏れるため、既存のバイト比較で実質的に覆われている。
  `clsid` の意味的な正しさは `tools/cfg_error_tests/`（警告メッセージ経由の文言比較）で
  部分的に補っているが、`cfg_equivalence.sh` 単体の「一致」は `clsid` については無検証である
  ことを踏まえて読むこと。

- **（2026-07-19 外部レビュー指摘）`kria_arm64_gcc` を将来追加する際は ROM イメージ形式の
  フックが要る。** 現物を確認した：上流 `arch/arm64_gcc/common/Makefile.core:34` は
  `DUMP = dump` と定義し（他アーキではこの変数は既定で `sample/Makefile:133-134` の
  `ifndef DUMP: DUMP = srec` が効く）、`sample/Makefile` 側は `cfg1_out.$(DUMP)` という
  拡張子可変のファイル名で kernel_cfg 生成（`:402-406` 相当）・offset.h 生成（`:417-421` 相当）
  の両方の `--rom-image` 引数を切り替え、`cfg1_out.dump:` ルール（`$(OBJDUMP) -s $(DUMPOPTS)
  $(CFG1_OUT) > cfg1_out.dump`）が `.dump` 生成を担う。kria は `DUMPOPTS := -j .text -j
  .rodata` を使う。一方 CMake 汎用層（`CMakeLists.txt`）はこの可変性を持たず、
  `-O srec`／`.srec` 拡張子を決め打ちしている（`cfg1_out.srec` 生成: `:361-366`、
  offset.h 生成の `--rom-image`: `:418-420`、`fmp3_cfg_check()` の `--rom-image`:
  `:538-543`。いずれも実際にこの3箇所であることをこの修正時に確認済み）。
  受け側の `cfg_py/srecord.py`／`cfg_py/cfg.py:700-705` は `args.rom_image_file_name` の
  拡張子が `.srec` かどうかで `SRecord(..., "srec")` と `SRecord(..., "dump")` を切り替える
  実装が既にあり、**欠けているのは CMake 層の DUMP 変数フック（`objcopy -O srec` /
  `objdump -s` の分岐と `--rom-image` の拡張子切り替え）だけ**である。現5ターゲット
  （polarfire・musca_b1×2）はいずれも `srec` 系のみを使うため無害。kria_arm64_gcc を
  実装する時点で対応すること。

- **（2026-07-19 外部レビュー指摘）コンパイルオプションの結合順が上流と逆。** 現物を確認した：
  上流 `sample/Makefile:188` は `COPTS := -O2 $(COPTS)`（`-O2` を**先頭に**追加し、
  target 側が `Makefile.target` 等で後から積む `COPTS` はその**後ろ**に来る＝GCCの
  「最後に指定した `-O` が勝つ」規則により target 側が `-O2` を上書き可能）。一方
  `CMakeLists.txt` は `include(${FMP3_TARGET_DIR}/target.cmake)`（`:81`）で
  `FMP3_COMPILE_OPTIONS` に target 固有の最適化オプションが積まれた**後**に
  `list(APPEND FMP3_COMPILE_OPTIONS -g -Wall -O2)`（`:137`）を実行しており、
  `-O2` が target.cmake の指定より**後**＝上書きする側になる。上流と逆順。
  現5ターゲットの `target.cmake` はいずれも独自の `-O` を指定しない（`FMP3_COMPILE_OPTIONS`
  に `-Os`/`-O0` 等を積んでいない）ため実害は無いが、将来 target.cmake 側で
  `-Os`/`-O0` 等に上書きしたいターゲットを追加すると、上流と違って上書きできない
  （常に `-O2` に戻される）ことになる。対処は本レビュー対応のスコープ外と判断し記録に留める。

## 解消済み事項

- **（2026-07-19 解消）`cfg_py/cfg.py`（計画Aの中身）が pristine の `cfg/cfg.rb` へ委譲する薄いシムであった件。**
  計画B（`docs/superpowers/plans/2026-07-19-fmp3-cmake-b-python-cfg.md`）で asp3_core 1.7.1 の
  本物の Python cfg エンジンへ差し替えた。`.trb`（Ruby）テンプレート30ファイル・3963行を
  `.py`（Python）へ全数移植し、pristine の Ruby cfg（VERSION 1.7.1、オラクルと版が一致）との
  差分等価性検査（`tools/cfg_equivalence.sh`）で `cfg1_out.c`／`offset.h`／`kernel_cfg.c`／
  `kernel_cfg.h` の一致を polarfire・musca_b1（1コア・2コア）の全構成で確認済み。
  製品ビルドの `CFG_SCRIPT_DEPS` は `cfg_py/*.py` の5ファイルのみを指し、
  pristine の `cfg/*.rb` は CMake のビルドグラフに一切含まれない
  （AGENTS.md §2 規則3を文言・精神の両方で満たす）。Ruby は `tools/cfg_equivalence.sh`
  （CMake外）からのみ呼ばれるオラクルとして残る。以下の記述は、**再調査を避けるための記録として
  残す**（削除しない。計画A2 の HardFault 解消記録と同じ扱い）。

  <details>
  <summary>解消前の記録（期限付きの逸脱としての原文）</summary>

  `cfg_py/cfg.py`（計画Aの中身。Task 3）：AGENTS.md §2 規則3「pristine の `cfg/` は使わない。
  cfg 相当は `cfg_py/`（Python）で提供し、CMake から呼ぶ」の**文言は満たす**（CMake が呼ぶのは
  常に `cfg_py/cfg.py`）が、**精神には抵触する**（`cfg_py/cfg.py` は pristine の `cfg/cfg.rb` へ
  委譲する薄いシムであり、実行されるのは結局 Ruby 版である）。CMake パイプラインの正しさを、
  テンプレート Python 移植のバグと切り離して検証するための足場。
  解消条件：計画B（`cfg_py/` への asp3_core 1.7.1 エンジン移植とテンプレート移植）の完了時に、
  シムを本物のエンジンへ差し替える。以降 Ruby は `tools/cfg_equivalence.sh`（CMake 外）からのみ
  呼ぶオラクルとして残す。この行が残っている間は AGENTS.md §6 の完了条件を満たさない。

  </details>

- **（2026-07-19 解消）`target/musca_b1_gcc/target_timer.c` の HRT 状態がプロセッサ別でなく、
  2コア SMP が起動直後に HardFault で停止していた件。** 下記「未解決事項」に記録していた
  問題は、上流 20260719（`rp2350_pico2_gcc-20260719`、pin `b59797f14dedcb07020f96895903ca7fcd14a4af`）
  で**上流自身により修正済み**。我々が 2026-07-19 付で報告した上流報告書
  （`docs/upstream-reports/2026-07-19-musca_b1-2core-hardfault.md`）の指摘と一致する修正。
  `hrt_base`/`hrt_reload`/`hrt_last`/`hrt_fresh` が `static volatile` の単一変数から
  `[TNUM_PRCID]` の配列（`target_timer.c:58,63,68,75`）に変わり、`get_my_prcidx()` で
  プロセッサ別に添字アクセスするよう全アクセス箇所（9箇所）が改修された。
  `target_timer.h` の「単一プロセッサ前提」の記述も削除された。
  QEMU（`musca-b1`）で2コアビルドを再検証し、`Processor 1 start.` / `Processor 2 start.`
  の2行が出て HardFault なく走行を継続することを確認済み（回帰確認ログ参照）。
  以下の記述は、**再調査を避けるための記録として残す**（削除しない）。

  <details>
  <summary>解消前の記録（未解決事項としての原文）</summary>

  - **`target/musca_b1_gcc/target_timer.c` の HRT 状態がプロセッサ別でない疑いがあり、
    2コア SMP が起動直後に HardFault で停止する。** `target_timer.c` は 2コアSMP対応の
    `musca_b1_gcc` 依存部の一部だが、HRT の内部状態 `hrt_base`（:47）・`hrt_reload`（:52）は
    `static volatile` のグローバル変数として**1組しか**持たない（プロセッサ番号でインデックス
    された配列等になっていない）。一方 `target/musca_b1_gcc/target_kernel.h:59` は2コア時
    `TOPPERS_TEPP_PRC = 0x3`（PRC1・PRC2 の両方が時間イベント処理プロセッサ）と定義し、
    `target/musca_b1_gcc/target_user.txt` は「各コア内蔵の SysTick を…使用する
    （`target_timer.c`）」と書いている＝設計意図は per-core SysTick／per-core HRT 状態のはず。
    新旧 QEMU（11.0.1 / 8.2.2）の両方で、同一箇所・同一 PC のクラッシュを再現しており
    QEMU の版依存ではない（詳細: `.superpowers/sdd/a2-task-6-report.md`）。
    **ただし** `target_user.txt:13` は「この2コア構成を用いて FMP3 の2コア SMP を QEMU 上で
    検証するためのターゲットである」と明記しており、上記の状態不足と矛盾する。
    我々が踏んでいない前提条件（例：ビルド／cfg 側の別の設定）がある可能性は排除できないため、
    **「上流のバグ」と断定はせず、「強い証拠がある未解決事項」として記録する**。
    `target_timer.c` 自体は本タスクの方針により**未修正**（構造変更を伴うため、修正はユーザの
    判断待ち）。1コア構成（`musca_b1` プリセット）はこの問題の影響を受けず正常動作する。

  </details>

- **（2026-07-19 解消）`kernel/interrupt.py` の `INTNO_VALID_ALL` 等3変数が
  `list(set(...))` で構築され、Ruby版（`.values.flatten.uniq`、出現順保存）と違い順序不定
  だった件。** 外部レビュー指摘。現状はこの3変数が `in` によるメンバーシップ判定にしか
  使われておらず（本ファイル中で確認済み）実害は無かったが、将来これらを列挙生成に使うと
  Rubyと出力順が食い違いうる潜在的な地雷だったため、`list(dict.fromkeys(...))`
  （出現順を保った重複除去、Rubyの`.uniq`と同義）に置き換えて解消した
  （`kernel/interrupt.py`、修正3のコミットと同時に対応）。

- **（2026-07-19 解消／事前調査を否定）`chip_el3_initialize()`（`arch/arm64_gcc/zynqmp/
  chip_kernel_impl.c:55-81`）の System Timestamp Generator（`0xFF260000`）への無条件書き込みが
  QEMU 11.0.1 で同期外部アボートを起こす、という Task 4-6 着手前の事前調査の**強い状況証拠は，
  実行して確認した結果，再現しなかった**。**pristine は無改変のまま。**

  実測（`/home/honda/qemu-build/qemu-11.0.1/build-a64/qemu-system-aarch64`、`-M
  xlnx-zcu102,secure=on -smp 1|4 -m 2G -nographic -d guest_errors,unimp`）：
  1コア・4コアいずれも `timeout 20` で rc=124（タイムアウトによる正常継続）、
  デバッグログ（`-d guest_errors,unimp`）に `0xff260000`（STG）・`0xffd80000`
  （事前調査でもう一つのリスクとして挙げた PMU_GLOBAL）付近の "Unassigned mem"・
  外部アボートに類する記録は**一切無い**（`gic_dist_writeb: Bad offset ...` という
  GIC ディストリビュータへのバイト単位アクセス警告のみで、非致命的）。

  **実際に「バナーが1行も出ない」原因は別にあった**：本ターゲットは
  `USE_XUART1`（`target.cmake:83`、`Makefile.target:100-115` 由来）でコンソールを
  UART1（`0xFF010000`）に出す。QEMU の `hw/arm/xlnx-zynqmp.c`（`uart_addr =
  {0xFF000000, 0xFF010000}`、`serial_hd(i)` を `uart[i]` に割り当て）は `-serial` 引数の
  **個数**でどの UART にどのチャデブを繋ぐか決まるため、`-serial mon:stdio` を1個しか
  渡さないと index0＝UART0 に接続され、カーネルが実際に書き込む UART1 にはバックエンドが
  無くコンソール出力が**エラーも出さず黙って消える**（QEMU は UART1 自体はハードウェアとして
  正しく実装しているため `guest_errors` にも `unimp` にも掛からない）。`-serial null -serial
  mon:stdio`（UART0=null, UART1=mon:stdio）に直したところ、1コア・4コアとも
  `TOPPERS/FMP3 Kernel Release 3.4.0 for KR260 ...` のバナー・`Processor 1..4 start.`
  （4コアは4行）・サンプルタスクの周期出力まで到達することを確認した。

  加えて、`target.cmake` の `QEMU_SYSTEM_AARCH64_KRIA` の既定パス
  （`/home/honda/qemu-build/install/bin/qemu-system-aarch64`）も実機には存在せず
  （`install/bin` には musca_b1 用の `qemu-system-arm` のみがインストール済みで、
  aarch64 の 11.0.1 バイナリはビルドツリー `qemu-11.0.1/build-a64/` に置かれたまま
  未インストールだった）、無指定だと PATH 上の QEMU 8.2.2 にフォールバックしていた。
  ビルドツリーのパスも候補に加えるよう修正した。

  以上2件を `target/kria_arm64_gcc/target.cmake`（derived、pristine ではない）で
  修正した：(1) `-serial mon:stdio` → `-serial null -serial mon:stdio`、
  (2) `QEMU_SYSTEM_AARCH64_KRIA` の既定探索候補に `qemu-11.0.1/build-a64/
  qemu-system-aarch64` を追加。**`chip_kernel_impl.c` 等 pristine 側は一切変更していない**
  （`TOPPERS_USE_QEMU` ガードは不要と判断・未追加）。4コア構成（PMU_GLOBAL 経由の
  secondary core 起動）についても、同じ実測で `Processor 1/2/3/4 start.` の4行が
  問題なく出力されることを確認済みで、事前調査が挙げたもう一つのリスク
  （PMU_GLOBAL ポーリング）も実害としては顕在化しなかった。

- **（2026-07-19 検証完了）`kria_r5_gcc`（Cortex-R5F, RPUクラスタ）のQEMU実行検証
  （Task 12、上流`runqu`レシピの翻訳）。1コア（lockstep相当）・2コア（split mode）とも
  pristine 無改変で動作を確認した。**

  QEMUバージョン：`/home/honda/qemu-build/qemu-11.0.1/build-a64/qemu-system-aarch64
  --version` → `QEMU emulator version 11.0.1`（brief記載値と一致）。

  1コア（`kria_r5` プリセット、`target.cmake` の `FMP3_RUN_COMMAND` そのまま＝上流
  `runqu` の翻訳）：`timeout 20 qemu-system-aarch64 -M xlnx-zcu102 -smp 6 -m 2G
  -nographic -global xlnx-zynqmp.boot-cpu=rpu-cpu[0] -global
  cortex-r5f-arm-cpu.mp-affinity=0 -device loader,file=build/kria_r5/fmp,cpu-num=4
  -serial null -serial mon:stdio -d guest_errors,unimp` → rc=124（タイムアウトに
  よる正常継続）。`TOPPERS/FMP3 Kernel Release 3.4.0 for KR260 <Cortex-R5F> ...`
  バナー・`Processor 1 start.`・サンプルタスクの周期出力（20秒で44回)まで到達。
  `-d guest_errors,unimp` のログに `RPU_RPU_GLBL_CNTL`（`0xFF9A0000`）付近の
  異常アクセスは一切出力されなかった。**brief Step 3 の`TOPPERS_USE_QEMU`ガードは
  不要と判断し、pristine（`target_kernel_impl.c`）は無改変のまま。**

  2コア（`kria_r5-2core` プリセット、split mode, `FMP3_PRC_NUM=2`）：**brief Step 4 の
  提案コマンド（`-device loader` を `cpu-num=4`/`cpu-num=5` に2個並べるだけ）は
  そのままでは動かないことを実測で確認した**（brief記載の通り上流に前例が無い構成で
  あり，確信度が低いとの前置きは正しかった／このコマンド自体は誤り）。このコマンドで
  実行すると `timeout 20` で rc=124 になるが，バナーが**1行も**出力されない
  （1コアの rc=124＝正常継続とは似て非なる「無反応のハング」）。原因切り分けのため
  `-device loader` を `cpu-num=4` のみ（2コアビルドのバイナリを1コア同様に単独起動）
  にしても同じく無反応であることを確認し，2個目の `-device loader` 自体が原因では
  ないと判定した。`kernel/startup.c` の `barrier_sync()`（`TOPPERS_barsync`,
  複数箇所で `barrier_sync(1)`〜`barrier_sync(7)` を呼ぶ）が `TNUM_PRCID`（=2）個の
  プロセッサ全員の到達を待つ設計のため，RPU1（PRC2）が実行を始めない限り RPU0（PRC1）
  はバナー出力前の最初のバリアで無期限に停止する，と推測した。

  QEMU側の原因を特定：`hw/arm/xlnx-zynqmp.c`（`/home/honda/qemu-build/qemu-11.0.1/
  hw/arm/xlnx-zynqmp.c:242-267`）は，`boot-cpu` に指定されなかった RPU を既定で
  `start-powered-off=true`（電源断状態で生成）にする。コード中コメント：
  「Secondary CPUs start in powered-down state. When the "rpu-secondary-start"
  machine property is set, they instead start running from reset together with
  the boot CPU, which allows running an SMP guest on the RPU cluster under QEMU
  (there is no model of the LPD/CRL reset registers that the guest would
  otherwise use to release them).」（同ファイル254-260行）。すなわち実機では
  ソフトウェア（ブートローダ等）が LPD/CRL のリセット制御レジスタを叩いて RPU1 を
  解放するが，QEMU の xlnx-zynqmp モデルはそのレジスタ群を実装していないため，
  ゲスト側の操作では RPU1 を起こせない。`rpu-secondary-start`
  （`DEFINE_PROP_BOOL`、同ファイル949-950行）という machine property を立てる
  ことでのみ RPU1 が RPU0 と同時に reset から動き出す。

  `-global xlnx-zynqmp.rpu-secondary-start=true` を追加した以下のコマンドで
  再実行したところ，`Processor 1 start.` / `Processor 2 start.` の2行・
  両プロセッサのサンプルタスク周期出力（`TASK1_1`/`TASK2_1`）が安定して継続する
  ことを確認した（20秒実行で両プロセッサとも進行を継続、40秒（`timeout -k 5 40`）の
  再実行でも同様に PRC1/PRC2 とも カウント100まで進行し，フォールト・停止なし。
  `ps -eo pid,comm | grep qemu-system` で残存プロセス無しも確認）：
  ```
  qemu-system-aarch64 -M xlnx-zcu102 -smp 6 -m 2G -nographic \
      -global xlnx-zynqmp.boot-cpu=rpu-cpu[0] \
      -global xlnx-zynqmp.rpu-secondary-start=true \
      -global cortex-r5f-arm-cpu.mp-affinity=0 \
      -device loader,file=build/kria_r5-2core/fmp,cpu-num=4 \
      -device loader,file=build/kria_r5-2core/fmp,cpu-num=5 \
      -serial null -serial mon:stdio -d guest_errors,unimp
  ```
  musca_b1 の2コア事例（HardFaultで停止）と異なり，**kria_r5 の2コアは真の意味で
  安定動作した**。pristine 側の改修は一切不要だった（`target_kernel_impl.c` は
  無改変）。**`target.cmake` の `FMP3_RUN_COMMAND`（1コア用）はこのタスクの
  scope外につき未変更のまま**（brief の Files 節が `DIVERGENCE_MAP.md` のみを
  変更対象としているため）。2コア用の `cmake --build ... --target run` を
  今後整備する場合は，上記コマンド（`-device loader` 2個＋
  `-global xlnx-zynqmp.rpu-secondary-start=true`）を `target.cmake` 側に
  追加する対応が必要になる（現状のまま `kria_r5-2core` プリセットで
  `--target run` を実行すると，1コア用の `FMP3_RUN_COMMAND` が使われるため
  上記の「無反応のハング」を踏む）。

  回帰確認：他5ターゲット（`musca_b1`／`musca_b1-2core`／`polarfire_soc_kit-qemu`／
  `kria_arm64`／`kria_arm64-1core`／`rp2350_pico2`）を全て `rm -rf build/<preset>`
  してから configure・build し直し，全て configure rc=0・build rc=0・`fmp`
  生成物ありを確認した（regression無し）。

## 未解決事項（強い証拠はあるが断定はしない）

- **（2026-07-19 発見）`target/musca_b1_gcc/target_timer.c` の `hrt_clear_event_body()`
  （:228-233）が、2コア構成の PRC2（非タイムマスタ）で「割込みを発生させない」つもりの
  クリアを、SysTick が表現できる最大区間（24bit・40MHz ≒ 0.4194秒、`hrt_program(HRT_MAX_TICKS)`、
  `target_timer.h:52`）での再武装として実装しているため、既定の `sample1` 構成
  （PRC2 側に周期タイムイベントが1つも起動されない構成）では**約0.42秒ごとに不要な
  SysTick 割込みが発生し続ける**。`kernel/time_event.c:756-758` の
  `LOG_NOTICE("no time event is processed in hrt interrupt on PRC%d.")` が
  20秒の実行で47回（当方の再現では22秒で52回、25秒で59回）、**全て PRC2 でのみ**出力される
  ことを実測で確認済み（実測した通知間隔の平均 0.41947秒は、
  `HRT_MAX_TICKS/CPU_CLOCK_HZ = 16777215/40000000 = 0.419430秒`という計算値と一致）。
  機序は特定できたと判断している（詳細・上流への質問:
  `docs/upstream-reports/2026-07-19-musca_b1-2core-hardfault.md` の
  「上流20260719修正後に残る事象」節）。**致命的ではない**（カーネルは走行を継続し、
  実イベント登録時に SysTick は毎回上書きされるため周期タスクの精度には影響しないと
  コード読解で判断しているが、実機での精度実測は未実施）。この `hrt_clear_event_body()`
  自体は 20260719 の per-core 化改修で**変更されていない**（`git diff` で確認済み）。
  すなわち20260719以前から存在した挙動だが、それまでは PRC2 が起動直後に HardFault で
  停止していたため観測される機会が無く、HardFault 修正によって初めて表面化した。
  **上流のバグと断定はせず**、`hrt_clear_event_body()` の「クリア」の意図
  （SysTick に真の無期限停止手段が無いことへの次善策か、見落としか）を上流に確認したい
  未解決事項として記録する。pristine は未改変（調査は既存のビルド成果物の実行のみで行った）。

## 期限付きの逸脱

| 対象 | 内容 | 解消条件 |
|---|---|---|
| （現在なし） | 2026-07-19 時点で期限付きの逸脱は無い。`cfg_py/cfg.py` の逸脱は計画B完了で解消済み（上記「解消済み事項」参照） | - |
