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
| target/rp2350_pico2_gcc/{target.cmake,presets.json} | add | Makefile.target の CMake 版。ARM-M（arm_m_gcc/rp2350 chip 層）。QEMU に RP2350/Pico のマシンモデルが無い（8.2.2/11.0.1 とも `-machine help` で確認済み）ため `FMP3_RUN_COMMAND` は定義しない＝`run` ターゲット自体を生成しない（ビルド専用、意図的）。計画C Task 1 | - |
| arch/arm_m_gcc/rp2350/{chip.cmake,chip_kernel.py} | add | rp2350 chip 層。前例なし・全数新規（`fmp3_pico_sdk` に rp2350 は無い）。計画C Task 1 | - |
| target/rp2350_pico2_gcc/*.py（3個: target_check.py/target_class.py/target_kernel.py） | add | rp2350 ターゲット層。`target_class.py` は musca_b1 版とバイト同一で流用（クラス構造がARM-M共通のため）。計画C Task 1 | - |
| target/kria_arm64_gcc/{target.cmake,presets.json} | add | Makefile.target の CMake 版（KRIA SOM Cortex-A53、QEMU xlnx-zcu102）。QEMU の `-serial` 割当ての罠（USE_XUART1でUART1へ出力するがserial個数指定を誤るとUART0へ繋がり出力が無音で消える）を踏まえ `-serial null -serial mon:stdio` を使用。4コア構成のsecondary-core起動もQEMU 11.0.1で確認済み。計画C Task 6/7 | - |
| arch/arm64_gcc/common/arch.cmake | add | Makefile.core の CMake 版。**ROM イメージ形式フック（`FMP3_DUMP_FORMAT dump`）をここで宣言**（Makefile.core:34 `DUMP = dump` の翻訳。kria_arm64 は `.dump` 形式）。計画C Task 6 | - |
| arch/arm64_gcc/common/*.py（4個: core_check.py/core_kernel.py/core_offset.py/gic_kernel.py） | add | arm64_gcc コア共通層の Python テンプレート。前例なし・全数新規移植（asp3_core に arm64/GIC 実装なし）。計画C Task 4 | - |
| arch/arm64_gcc/zynqmp/{chip.cmake,chip_kernel.py} | add | kria_arm64 chip 層（ZynqMP APU/Cortex-A53）。前例なし・全数新規。計画C Task 4/6 | - |
| target/kria_arm64_gcc/*.py（3個: target_check.py/target_class.py/target_kernel.py） | add | kria_arm64 ターゲット層。前例なし・全数新規。計画C Task 7 | - |
| target/kria_r5_gcc/{target.cmake,presets.json} | add | Makefile.target の CMake 版（KRIA SOM Cortex-R5F、QEMU xlnx-zcu102 RPUクラスタ）。1コア（lockstep相当）・2コア（split mode, `FMP3_PRC_NUM=2`）を`FMP3_PRC_NUM`で分岐（kria_arm64と同じ形）。2コアの`FMP3_RUN_COMMAND`は`-global xlnx-zynqmp.rpu-secondary-start=true`と`-device loader`2個を要する（Task 12で判明、Task 13で`target.cmake`に反映。詳細は下記「解消済み事項」参照）。計画C Task 10/13 | - |
| arch/arm_gcc/common/arch.cmake | add | Makefile.core の CMake 版。本ファイルに `DUMP` の定義は無い（現物確認済み）ため `FMP3_DUMP_FORMAT` は宣言せず既定の srec のまま（kria_r5 は srec のまま正しい）。計画C Task 8 | - |
| arch/arm_gcc/common/*.py（3個: core_check.py/core_kernel.py/core_offset.py） | add | arm_gcc（Cortex-R5F/GIC）コア共通層の Python テンプレート。前例なし・全数新規移植。計画C Task 8 | - |
| arch/arm_gcc/zynqmp_r5/{chip.cmake,chip_kernel.py} | add | kria_r5 chip 層（ZynqMP RPU/Cortex-R5F）。`chip_kernel.py` はプライベート割込み（intno 0〜31）のみプロセッサ番号で符号化する（`(prcid << 16) | intno`）構造（グローバル割込み32〜186はintno自体は符号化しない）。前例なし・全数新規。計画C Task 8/10 | - |
| target/kria_r5_gcc/*.py（3個: target_check.py/target_class.py/target_kernel.py） | add | kria_r5 ターゲット層。前例なし・全数新規。計画C Task 10 | - |
| kernel/kernel_api.def | mod (dcre-port) | 動的生成（dcre 段階1）静的API `AID_TSK .notsk` / `DEF_MPK { .mpksz &mpk? }` の2行を追加。dcre extension（`extension/dcre/kernel/kernel_api.def`）と同一の記法。cfg 両エンジン共用のためこの1ファイルの改変で足りる | - |
| kernel/kernel.trb | mod (dcre-port) | `KernelObject`（`@aidapi`/`@noobj`/`@inibList` の汎化と `generate()` の AID 集計・`TNUM_S*ID`/`_kernel_tmax_s*id`/動的 inib ブロック/予約 CB・ポインタ表末尾の追加）と末尾の `DEF_MPK` → `mpksz`/`mpk` 出力ブロックを追加。dcre kernel.trb（asp3_core 1.7.1 拡張）の DIFF を FMP3（プロセッサ/クラス概念あり）向けに翻案。段階1では `kernel_api.def` に `AID_TSK` のみ登録済みのため、`@aidapi` が `$cfgData` に無いオブジェクト（tsk 以外）は新規追加ブロックを完全にスキップし既存出力を厳密保持（Task 2 Step 7 の管理された差分許容リストがタスク+mpk 関連の5項目のみである根拠）。レビュー指摘（2026-08-03）を受け、`DEF_MPK` 出力ブロックの先頭に AID_TSK と同一規約（`params0.has_key?(:class)` → `error_ercd("E_RSATR", ...)`、文言 `DEF_MPK must not be within a class`）のクラス外専用検査を追加（設計書 §157 の要求。Python 側 `kernel.py` も同時に追加、`tools/cfg_error_tests/dcre_mpk_in_class.cfg` で回帰確認） | - |
| kernel/task.trb | mod (dcre-port) | `_kernel_torder_table` のサイズトークンを `TNUM_TSKID`（総数）から `TNUM_STSKID`（静的数）へ変更（dcre task.trb と同じ変更点）。最終レビュー指摘（2026-08-04）を受け、同箇所に `TNUM_STSKID` が `KernelObject.generate` の恒常出力に依存する旨のコメントを追記 | - |
| kernel/kernel_check.trb | mod (dcre-port) | パス3（メモリ検査）に `DEF_MPK` の mpk 整列・非NULL検査ブロックを追加（dcre kernel_check.trb 相当） | - |
| kernel/kernel_sym.def | mod (dcre-port) | パス3が参照する `CHECK_MPK_ALIGN`/`CHECK_MPK_NONNULL`/`CHECK_MB_ALIGN` の3シンボルを追加（dcre kernel_sym.def と同一の3行） | - |
| kernel/kernel_impl.h | mod (dcre-port) | `TA_NOEXS`（`:199`）直後に `TA_MEMALLOC`（`0x8000`）マクロを追加。`TOPPERS_MACRO_ONLY` ガード内に、Task 2 が生成する `_kernel_mpksz`/`_kernel_mpk` を束ねる `extern const size_t mpksz` / `extern MB_T *const mpk`（istksz/istk セクション直後）、`extern bool_t mpk_valid`（kerflg_table セクション直後）、`initialize_mempool`/`malloc_mempool`/`aligned_alloc_mempool`/`free_mempool` の4関数 extern と `malloc_mpk`/`aligned_alloc_mpk`/`free_mpk` の Inline 3関数（exit_kernel セクション直後）を追加。dcre（`extension/dcre/kernel/kernel_impl.h`）の同一ブロックを転記し、FMP3 の既存セクション構造（`istksz_table`/`kerflg_table`/`exit_kernel` 等）に合わせて配置を分割した（内容はdcreと同一）。段階1（Task 3）はTA_MBALLOC・TARGET_TSKATR等ACRE_TSK系マクロは対象外（別タスクの範囲） | - |
| kernel/check.h | mod (dcre-port) | 末尾（`#endif` 直前）に dcre `check.h` の `ALIGNED`/`STKSZ_ALIGN`/`INTPTR_ALIGN`/`FUNC_ALIGN`/`STACK_ALIGN`/`MPF_ALIGN`/`MB_ALIGN`/`FUNC_NONNULL` マクロ群を追加（`CHECK_*_ALIGN`/`CHECK_FUNC_NONNULL` 未定義時は `true` に落ちる形。対応する `CHECK_*_ALIGN` の値自体は base（`e2fe7ac`）時点で既に各 `arch/*/common/core_kernel_impl.h` に pristine として存在する。riscv は `CHECK_STKSZ_ALIGN`/`CHECK_INTPTR_ALIGN`/`CHECK_FUNC_ALIGN`/`CHECK_STACK_ALIGN`/`CHECK_MPF_ALIGN`/`CHECK_MPK_ALIGN`/`CHECK_MB_ALIGN` を全て（7個）持ち、arm_m は `CHECK_STKSZ_ALIGN`/`CHECK_FUNC_ALIGN`/`CHECK_STACK_ALIGN`/`CHECK_MPF_ALIGN`/`CHECK_MB_ALIGN` の5個を持つ（arm_m に欠けているのは `CHECK_INTPTR_ALIGN` と `CHECK_MPK_ALIGN` の2個のみで、これらは check.h の `#else` で `true` にフォールバックする）。Task 1/2 で追加したものではない） | - |
| kernel/startup.c | mod (dcre-port) | `bool_t mpk_valid;` の定義を `TOPPERS_sta_ker` ブロック冒頭に追加。`sta_ker()` の `barrier_sync(2)` 直後（マスタプロセッサのみ）に、`mpk`/`mpksz` からカーネルメモリプール領域を初期化し `mpk_valid` を設定する処理を追加。ファイル末尾（`TOPPERS_extkerhdr` の後）に dcre `startup.c` の `TOPPERS_kermem` ブロック（`MEMPOOLCB` 構造体・`align_pointer`・`initialize_mempool`・`malloc_mempool`・`aligned_alloc_mempool`・`free_mempool`、`OMIT_MEMPOOL_DEFAULT` ガード付き）をそのまま転記 | - |
| kernel/allfunc.h | mod (dcre-port) | `startup.c` セクションに `TOPPERS_kermem` を追加（dcre allfunc.h と同一）。本ファイルは Task 3 の Files 一覧に無いが、この CMake ビルドは常に `ALLFUNC` を定義する（`CMakeLists.txt` の `target_compile_definitions(fmp3 PRIVATE ALLFUNC)`）ため、`kernel_impl.h` が `allfunc.h` を include して全 `TOPPERS_xxx` を定義する経路のみが使われる。`TOPPERS_kermem` が無いと `startup.c` 末尾のメモリプール関数群が `#ifdef TOPPERS_kermem` に阻まれて一切コンパイルされず、`sta_ker()` からの `initialize_mempool` 呼び出しがリンクエラーになるため、ビルド可能性を保つ上で必須の追加 | - |
| kernel/kernel_rename.def | mod (dcre-port) | `# startup.c` 節に `mpk_valid`/`initialize_mempool`/`malloc_mempool`/`aligned_alloc_mempool`/`free_mempool` を追加（ブリーフ Step 4 の指示どおり）。加えて、`istksz_table`/`istk_table` 等の並びの直後に `mpksz`/`mpk` を追加（ブリーフの明示リストには無いが、`kernel_impl.h` の `extern const size_t mpksz`/`extern MB_T *const mpk` が Task 2 生成の `_kernel_mpksz`/`_kernel_mpk` に解決されるために必須。無いとリンクエラーになることを確認した上での追加） | - |
| kernel/kernel_rename.h, kernel/kernel_unrename.h | mod (dcre-port) | `utils/genrename.rb kernel` で `kernel_rename.def` から再生成（手編集不可）。上記7シンボル分の `#define`/`#undef` が増えた。再生成時、`kernel_rename.def` に元々存在しなかった `mtxhook_scan_ceilmtx`/`mutex_scan_ceilmtx`/`mutex_drop_priority` の3エントリが本改修と無関係に消えた（Task 3 着手前の pristine 状態で同じ genrename を走らせても同じ3行が消えることを確認済み＝既存の生成物ドリフト）。`mutex_scan_ceilmtx`/`mtxhook_scan_ceilmtx` は現在のソースに実体が存在せず、`mutex_drop_priority`（`mutex.c:272`）は `Inline`（static）関数のため外部シンボルとしてのリネームが元来不要であり、削除は無害と判断した | - |
| kernel/task.h | mod (dcre-port) | `p_tcb_table` extern（旧 `:297`）の直後に `extern QUEUE free_tcb`/`extern const ID tmax_stskid`/`extern TINIB atinib_table[]`/`#define tnum_stsk` を追加（dcre task.h 差分と同型、ブリーフ Step 1 のコードを転記）。`TSKID(p_tcb)` マクロを、`atinib_table`（動的生成タスク範囲）と `tinib_table`（静的生成タスク範囲）の2レンジを判定する版へ置換（dcre 相当、spec §4.3 の式のまま）。AID 未使用時は `tnum_stsk == tnum_tsk` となり2レンジ判定の動的分岐（`tnum_tsk - tnum_stsk == 0`）が常に偽になるため、常に静的レンジの式に落ちて挙動不変 | - |
| kernel/task.c | mod (dcre-port) | `TOPPERS_tskini` ブロック冒頭に `QUEUE free_tcb;` の定義を追加。`initialize_task` の静的タスク初期化ループの境界を `tnum_tsk` → `tnum_stsk` に変更（AID 無しでは `tnum_stsk == tnum_tsk` のため無改変）。ループ末尾に、マスタプロセッサのみで動くdcre 相当の動的スロット初期化ブロック（`free_tcb` の初期化、`atinib_table[j].tskatr = TA_NOEXS`、`p_tcb->p_pcb = get_pcb(1)` で PRC1 固定＝Constraint 4）を追加（ブリーフ Step 2 のコードを転記）。動的タスク数0のときはループが空振りするため既存挙動を保つ | - |
| arch/arm_m_gcc/common/core_kernel_impl.h | mod (dcre-port) | `TSKINICTXB` 定義（`:118`）を閉じる既存の `#endif /* TOPPERS_MACRO_ONLY */` の直後に、新規の `#ifndef TOPPERS_MACRO_ONLY` ブロックとして `init_tskinictxb`（stk_top/stk_bottom の初期化）・`tskinictxb_memalloc_ptr`（`free_mpk` へ渡す先頭番地の取得）の2つの Inline 関数を追加（ブリーフ Step 3 のコードを転記。dcre の `GenerateTskinictxb` と同じ stk_top=先頭番地/stk_bottom=末尾番地の約束）。既存コードからの呼び出しは無く（Task 4 時点では未使用のヘルパ関数）、既存の関数・マクロは無改変のため挙動不変 | - |
| kernel/kernel_rename.def（Task 4） | mod (dcre-port) | `# task.c` 節に `free_tcb`/`tmax_stskid`/`atinib_table` の3シンボルを追加（ブリーフ Step 4 の指示どおり。dcre の `kernel_rename.def` では `tmax_stskid`/`atinib_table` は `# kernel_cfg.c` 節にあるが、ブリーフが明示的に `# task.c` 節への追加を指示しているため従った。`genrename.rb` は `#` 行を単なる見出しコメントとして扱うだけで節の帰属はリネーム結果に影響しない） | - |
| kernel/kernel_rename.h, kernel/kernel_unrename.h（Task 4） | mod (dcre-port) | `utils/genrename.rb kernel` で再生成。上記3シンボル分の `#define _kernel_free_tcb` 等/`#undef` が `task.c` 節に増えただけで、他の差分（既知の3エントリ消失を含む）は今回発生しなかった（`git diff --stat` で `kernel_rename.h`/`kernel_unrename.h` とも3行追加のみを確認） | - |
| include/kernel.h（Task 5） | mod (dcre-port) | `t_rtsk`（旧`:136`）直前に `T_CTSK`（dcre 同一のフィールド並び：tskatr/exinf/task/itskpri/stksz/stk）を追加。`act_tsk` 宣言（旧`:212`）直前に `extern ER_ID acre_tsk(const T_CTSK *pk_ctsk) throw();`/`extern ER del_tsk(ID tskid) throw();` の2宣言を追加（機能コード `TFN_ACRE_TSK`/`TFN_DEL_TSK` は `include/kernel_fncode.h` に既存のため追加不要） | - |
| kernel/kernel_impl.h（Task 5） | mod (dcre-port) | Task 3 が追加した `TA_MEMALLOC` ブロックの直後に、dcre `extension/dcre/kernel/kernel_impl.h:151-161` と同一の `#ifndef TARGET_TSKATR #define TARGET_TSKATR 0U #endif` / `#ifndef TARGET_MIN_STKSZ #define TARGET_MIN_STKSZ 1U #endif` を追加。ブリーフの Files 一覧には無いが、`acre_tsk` の `CHECK_VALIDATR(tskatr, TA_ACT\|TA_NOACTQUE\|TARGET_TSKATR)`/`CHECK_PAR(stksz >= TARGET_MIN_STKSZ)` が参照する2マクロは、FMP3 では cfg 側（`kernel_sym.def`/`task.py`）にしか存在せず C コードとしては未定義だったため、コンパイルのために必須の追加と判断した（`arm64_gcc`/`arm_gcc` の `core_kernel_impl.h` は `TARGET_TSKATR` を独自定義済みだが `TARGET_MIN_STKSZ` はどのターゲットにも無い） | - |
| kernel/check.h（Task 5） | mod (dcre-port) | `CHECK_ID`（E_ID）と `CHECK_PAR`（E_PAR）の間に、dcre `check.h:205` と同一の `CHECK_VALIDATR(atr, valid_atr)`（E_RSATR）マクロを追加。ブリーフの Files 一覧には無いが、`acre_tsk` の `CHECK_VALIDATR(tskatr, TA_ACT\|TA_NOACTQUE\|TARGET_TSKATR)` が参照するマクロが FMP3 の check.h に存在しなかったため、コンパイルのために必須の追加と判断した | - |
| kernel/task_manage.c | mod (dcre-port) | `act_tsk` ブロック直前に `acre_tsk`/`del_tsk` をブリーフの全文どおり追加（free_tcb の FIFO pop/push、`iprcid=1`/`affinity=全プロセッサ`固定、`E_NOID`→`E_NOMEM`の順のエラー判定、`TA_MEMALLOC` 時の `USE_TSKINICTXB` 分岐）。加えて `act_tsk`/`mact_tsk`/`can_act`/`mig_tsk`/`get_tst`/`chg_pri`/`get_pri`/`chg_spr` の8関数に、`acquire_glock()` 直後の最初の状態判定として `if (p_tcb->p_tinib->tskatr == TA_NOEXS) { ercd = E_NOEXS; } else ...` を挿入（既存ロジックは字下げのみで else 節へ繰り込み、AID 無し構成では `tskatr` が `TA_NOEXS` に一致することが無いため挙動不変）。`mact_tsk`/`mig_tsk`/`chg_spr` は ASP3 dcre に対応物が無い FMP3 独自関数のため、`act_tsk`/`get_tst`/`chg_pri`/`get_pri`（dcre 実在）の配置パターンを転用した | - |
| kernel/task_refer.c | mod (dcre-port) | `ref_tsk` に E_NOEXS 検査を追加。dcre `task_refer.c` は `tstat = p_tcb->tstat;` 以降の全体を else 節に包む構造のため、FMP3 版でも `p_pcb`/`tstat` 取得から `subpri`/`actcnt`/`actprc`/`prcid` の取出しと `ercd = E_OK;` までを丸ごと else 節へ1段字下げして繰り込んだ（ロジック自体は無変更） | - |
| kernel/task_sync.c | mod (dcre-port) | `wup_tsk`/`can_wup`/`rel_wai`/`rsm_tsk` は dcre 同様、E_NOEXS 判定を `acquire_glock()` 直後の最初の分岐として追加。`sus_tsk` のみ dcre `task_sync.c`（`p_tcb == p_runtsk && !dspflg` を第1分岐のまま維持し E_NOEXS を第2分岐に置く実装）に倣い、FMP3 の同型分岐 `p_tcb == p_selftsk && !(p_my_pcb->dspflg)`（E_CTX）を第1分岐のまま残し、E_NOEXS をその直後の第2分岐として追加した（自タスクは TA_NOEXS になり得ないため分岐順は結果に影響しない） | - |
| kernel/task_term.c | mod (dcre-port) | `ras_ter`/`ter_tsk` に E_NOEXS 検査を追加。両関数とも ASP3 dcre では E_NOEXS が最初の分岐だが、FMP3 版は dcre に無い「異なるプロセッサに割り付けられているタスクならエラー」という `p_tcb->p_pcb != p_my_pcb` 分岐が既存の第1分岐として存在する。存在しないタスク（E_NOEXS）の判定を他の状態判定より優先させる方針で、E_NOEXS をその `p_pcb` 分岐より前・最初の分岐として追加した（dcre に対応物が無いため設計判断。プロセッサ違いでの呼び出しでも常に E_NOEXS が優先される） | - |
| kernel/allfunc.h（Task 5） | mod (dcre-port) | `task_manage.c` セクション先頭（`TOPPERS_act_tsk` 直前）に `TOPPERS_acre_tsk`/`TOPPERS_del_tsk` の2行を追加（dcre allfunc.h と同一の配置）。`TOPPERS_kermem` は Task 3 で追加済みのため重複追加はしていない | - |
| kernel/Makefile.kernel | mod (dcre-port) | `task_manage =` の .o 列挙の先頭に `acre_tsk.o del_tsk.o` を追記（上流形式の維持目的。`KERNEL_FCSRCS` は無改変。CMake の `ALLFUNC` ビルドはこの行を参照しないため実ビルドには影響しない） | - |
| include/kernel.h（Task 6） | mod (dcre-port) | `COUNT_MPF_T`/`ROUND_MPF_T` 定義ブロック（`:568-569`）の直後に `COUNT_MB_T(sz)`/`ROUND_MB_T(sz)` の2行を追加（dcre `extension/dcre/include/kernel.h:674-675` と同一内容、`TOPPERS_COUNT_SZ`/`TOPPERS_ROUND_SZ` は既存の `:558` 付近を再利用、`MB_T` は `t_stddef.h:131` に既存）。Task 2 の DEF_MPK codegen（`kernel/kernel.py`/`kernel.trb`）はこの2マクロを `kernel_cfg.c` へ出力するが、定義側の移植が漏れており、DEF_MPK 構成を初めて実コンパイルした test_dcre1（Task 6）のビルドで未定義エラーとして発覚した。`cfg_equivalence.sh` は両エンジンの生成文字列を diff するだけでコンパイルしないため、両エンジンが同じ欠落を共有すると検出できない（検証の盲点）。dcre 同領域にある `TCNT_DTQMB`/`TCNT_PDQMB`/`TCNT_MPFMB` 等は段階3（dtq/pdq/mpf）用のため追加していない | - |
| test/test_dcre1.c | add (dcre-port) | 動的生成API（acre_tsk/del_tsk/DEF_MPK）の QEMU 回帰テスト本体（Task 6）。musca_b1-2core 上で、acre→act→自然終了、AID_TSK(2) のスロット枯渇からの del による同一ID再利用（free-list は dcre 同様 FIFO、del_tsk は queue_insert_prev で末尾へ・acre_tsk は queue_delete_next で先頭からのため、空きスロットが1個だけの状態で検証）、E_NOID、del_tsk の E_OBJ（静的タスク／非休止タスク）、ter_tsk 経由の強制終了→休止→del成功、削除済みIDへの E_NOEXS、stk=NULL 自動確保時の E_NOMEM と成功、mact_tsk による全プロセッサ affinity の実証、の8シナリオを検証する。ブリーフのコードをほぼ転記したが、PRC2 側の `check_point_prc` が `syssvc/test_svc.c` の `check_count[prcid-1]`（プロセッサ毎に独立したカウンタ）を使う実装であることが実機ログで判明し、ブリーフが仮定していた「PRC1/PRC2 共通の単調増加番号（cp11 等）」ではなく、PRC2 側は本テスト内で最初のチェックポイントとして 1 から数え直す必要があったため、`check_point_prc(11, 2)` → `check_point_prc(1, 2)`、以降の PRC1 側 `check_point(12)`/`check_finish(13)` → `check_point(11)`/`check_finish(12)` へ番号を1つ詰めて修正した（QEMU実行ログで `## Unexpected check point 2-11.` を確認した上での修正） | - |
| test/test_dcre1.cfg | add (dcre-port) | 上記テストのシステムコンフィギュレーションファイル。ブリーフの記述どおり、`AID_TSK(2)`/`DEF_MPK({ MPK_SIZE, NULL })` をクラス外（Task 2 の E_RSATR 検査対象の裏付け）に置く | - |
| test/test_dcre1.h | add (dcre-port) | 上記テストのヘッダ（優先度定数・`MPK_SIZE`・関数プロトタイプ）。`test_int2.h` と同型 | - |
| test/MANIFEST | mod (dcre-port) | `test_cpuexc9.c` の後・`test_dlynse.c` の前（アルファベット順）に `test_dcre1.c`/`test_dcre1.cfg`/`test_dcre1.h` の3行を追加 | - |
| test/testexec.rb | mod (dcre-port) | `"cpuexc10"` の後・`"dlynse"` の前に `"dcre1" => { SRC: "test_dcre1" },` を追加 | - |
| test/test_dcre2.c | add (dcre-port) | 動的生成API（acre_cyc/del_cyc/acre_alm/del_alm）の QEMU 回帰テスト本体（Task 7）。musca_b1-2core 上で、(A) acre_cyc(TNFY_HANDLER)→sta_cyc→発火→stp_cyc→del_cyc、(B) 動作中のままの del_cyc 成功（dcre意味論）、(C) 削除済みIDへの sta/stp/ref/msta_cyc の E_NOEXS、(D) E_PAR（cyctim=0／tmehdr=NULL／cycatr未定義ビット）と AID_CYC(2) 枯渇の E_NOID、(E) msta_cyc による PRC2 への移動と PRC2 側発火の実証（`sil_get_pid` で prcid==2 を確認）、(F) TNFY_SETVAR が notify_handler トランポリン経由で変数を設定すること、(G) alm 側の acre/sta/再sta/del と E_NOEXS/E_NOID、(H) 静的生成 CYC1/ALM1 への del が E_OBJ、の8シナリオを検証する。ブリーフのコードをそのまま実装した。ブリーフの Step 6 期待値コメントは PRC2 の初回チェックポイントを `Check point 1-2 passed.` としていたが、`syssvc/test_svc.c:136` の `syslog_2(LOG_NOTICE, "Check point %d-%d passed.", prcid, count)` は prcid が先・count が後のため実際の出力は `Check point 2-1 passed.` である（`check_point_prc(1, 2)` の呼び出し自体はブリーフ・実装ともに正しく、表示フォーマットの記述が逆だっただけ。QEMU実測ログで確認） | - |
| test/test_dcre2.cfg | add (dcre-port) | 上記テストのシステムコンフィギュレーションファイル。ブリーフの記述どおり、`CRE_CYC(CYC1, …)`/`CRE_ALM(ALM1, …)` を CLS_PRC1 内の静的オブジェクトとして置き、`AID_CYC(2)`/`AID_ALM(2)` をクラス外（Task 3 の E_RSATR 検査対象の裏付け）に置く。cyc/alm はメモリプールを使わないため `DEF_MPK` は無し | - |
| test/test_dcre2.h | add (dcre-port) | 上記テストのヘッダ（優先度定数・周期／アラーム時間定数・関数プロトタイプ）。`test_dcre1.h` と同型 | - |
| test/MANIFEST（Task 7） | mod (dcre-port) | `test_dcre1.h` の後・`test_dlynse.c` の前（アルファベット順）に `test_dcre2.c`/`test_dcre2.cfg`/`test_dcre2.h` の3行を追加 | - |
| test/testexec.rb（Task 7） | mod (dcre-port) | `"dcre1"` の後・`"dlynse"` の前に `"dcre2" => { SRC: "test_dcre2" },` を追加 | - |
| arch/arm_m_gcc/common/core_rename.def | mod (upstream-gap-fix) | ファイル先頭の `# core_kernel_impl.c` 節の直前に `# core_kernel_impl.h` 節を新設し `sense_lock`/`unlock_cpu` の2エントリを追加。cfg 生成コードが参照する `_kernel_sense_lock`/`_kernel_unlock_cpu` が未リネームで多重ISR構成がリンク不能（arm64 の `core_rename.def` には同節が存在するが arm_m には節ごと欠落していた、pristine 由来の既存ギャップ）。`_kernel_lock_cpu` の未定義参照は出なかったため `lock_cpu` は追加していない（最小差分） | 報告候補（段階1最終レビュー 上流報告候補 b） |
| arch/arm_m_gcc/common/core_rename.h | mod (upstream-gap-fix) | 上記 `.def` の変更を `utils/genrename.rb core`（`arch/arm_m_gcc/common` で実行）により再生成（手編集ではない）。`#define sense_lock _kernel_sense_lock` / `#define unlock_cpu _kernel_unlock_cpu` の2行が core_kernel_impl.c 節の直前に追加された。**副作用として** `svc_handler` の1行がタブ+スペース混在からタブのみへ整形された（同一シンボル・同一リネーム先で意味的な差分ではない。今回の2エントリ追加とは無関係に、変更前の `.def` からの再生成でも同じ整形差分が再現することを確認済み＝pristine 側の生成物が現行 `genrename.rb` の出力と既に乖離していた既存ドリフト。手編集で温存する選択肢は Constraint 2（生成物は手編集禁止）に反するため、regeneration の結果をそのまま採用した） | 報告候補（段階1最終レビュー 上流報告候補 b） |
| arch/arm_m_gcc/common/core_unrename.h | mod (upstream-gap-fix) | 上記と同時に再生成。`#undef sense_lock` / `#undef unlock_cpu` の2行を追加。他の差分なし | 報告候補（段階1最終レビュー 上流報告候補 b） |
| kernel/kernel_api.def（dcre段階2 Task 3） | mod (dcre-port) | 静的API `AID_CYC .nocyc` / `AID_ALM .noalm` の2行を追加（`AID_TSK`/`DEF_MPK` の次）。dcre extension（`extension/dcre/kernel/kernel_api.def`）と同一の記法 | - |
| kernel/kernel.trb（dcre段階2 Task 3） | mod (dcre-port) | `generate()` の `numObjid` 算出直後に、★訂正E「AID_xxx が1個以上あるのに静的オブジェクトが0個」を `E_OBJ`（`"#{@aidapi} requires at least one #{@api} in the system"`）として弾くガードを追加。汎化済みの `KernelObject`（段階1）が `kernel/kernel_api.def` に登録済みの `@aidapi` を自動的に拾うため、`cyclic.trb`/`alarm.trb` 側の `@inibList["T_NFYINFO"]` 登録と合わせるだけで `AID_CYC`/`AID_ALM` が有効化される（`kernel.trb` 自体への追加改変はガード1ブロックのみ） | - |
| kernel/cyclic.trb（dcre段階2 Task 3） | mod (dcre-port) | `CyclicObject#initialize` に `@inibList["T_NFYINFO"] = "acyc_nfyinfo_table"` を追加（dcre `cyclic.trb:48-51` と同一）。動的生成 nfyinfo テーブルの登録のみで、`generateInib`/`prepare` 等の既存ロジックは無変更 | - |
| kernel/alarm.trb（dcre段階2 Task 3） | mod (dcre-port) | `AlarmObject#initialize` に `@inibList["T_NFYINFO"] = "aalm_nfyinfo_table"` を追加（dcre `alarm.trb:48-51` と同一）。他は cyclic.trb と同様に無変更 | - |
| include/kernel.h（dcre段階2 Task 3） | mod (dcre-port) | `MPF_T` の typedef（旧`:131`）直後・`T_CTSK`（旧`:136`）手前に、dcre `extension/dcre/include/kernel.h:133-198` と同一の `T_NFY_HDR`/`T_NFY_VAR`/`T_NFY_IVAR`/`T_NFY_TSK`/`T_NFY_SEM`/`T_NFY_FLG`/`T_NFY_DTQ`/`T_ENFY_VAR`/`T_ENFY_DTQ`/`T_NFYINFO` の型群を追加（★訂正A/B）。FMP3 に `T_NFYINFO` 型が元々存在しなかったため、cyclic/alarm の動的生成 nfyinfo テーブル（Task 3 の `kernel_cfg.c` 恒常出力）が参照する型を初めて定義した。`TMEHDR`/`FLGPTN`/`MODE`/`intptr_t` は既存定義を再利用するため追加の include は不要。**追記（段階2 Task 6）**：`t_rcyc` 手前に `T_CCYC`、`t_ralm` 手前に `T_CALM` を追加（dcre 同一のフィールド並び）。`sta_cyc`/`sta_alm` 宣言の直前にそれぞれ `extern ER_ID acre_cyc(const T_CCYC *pk_ccyc) throw();`/`extern ER del_cyc(ID cycid) throw();`、`extern ER_ID acre_alm(const T_CALM *pk_calm) throw();`/`extern ER del_alm(ID almid) throw();` を追加（機能コード `TFN_ACRE_CYC`/`TFN_DEL_CYC`/`TFN_ACRE_ALM`/`TFN_DEL_ALM` は `include/kernel_fncode.h` に既存のため追加不要。返値型は段階1の `acre_tsk` に揃えて `ER_ID` とした――dcre の `.c` 側は `ER_UINT` を使うが両者は `int_t` の別名であり、宣言と定義で揃っていれば問題ない意図的な逸脱） | - |
| kernel/check.h（dcre段階2 Task 4） | mod (dcre-port) | `FUNC_NONNULL` ブロック（`:423-427`）の直後に `INTPTR_NONNULL(p_var)` を追加。dcre `extension/dcre/check.h:130-134` と同一内容（`CHECK_INTPTR_NONNULL` は既存の同名マクロと同様どのターゲットも定義していないため実質 `true`）。ブロックの前後関係は dcre（INTPTR_NONNULL が先・FUNC_NONNULL が後）と逆（FMP3 は FUNC_NONNULL が先・INTPTR_NONNULL が後）。段階1で `FUNC_NONNULL` 直後に置く配置がブリーフで既に指定されており、順序自体はどちらでも展開結果は同一のため機能的な差はない | - |
| kernel/kernel_impl.h（dcre段階2 Task 4） | mod (dcre-port) | `NFYHDR` の typedef（`:357-359`）直後に `extern ER check_nfyinfo(const T_NFYINFO *p_nfyinfo);` / `extern void notify_handler(EXINF exinf);` の2宣言を追加（dcre 同節と同一） | - |
| kernel/time_manage.c（dcre段階2 Task 4） | mod (dcre-port) | ファイル末尾（`TOPPERS_fch_hrt` 区画の直後）に dcre `extension/dcre/kernel/time_manage.c:220-296`（`TOPPERS_chknfy`）・`:302-375`（`TOPPERS_nfyhdr`）区画をそのまま転記（`diff -u` で差分ゼロを確認済み、Step 6 参照）。**転記のほかに、先頭の include 節へ `task.h`/`semaphore.h`/`eventflag.h`/`dataqueue.h` の4行を追加**（`check.h` の直後）。理由：転記した `check_nfyinfo` が使う `VALID_TSKID`/`VALID_SEMID`/`VALID_FLGID`/`VALID_DTQID`（`check.h`）はそれぞれ `tmax_tskid`/`tmax_semid`/`tmax_flgid`/`tmax_dtqid`（`task.h`/`semaphore.h`/`eventflag.h`/`dataqueue.h` の extern 宣言）を参照するが、FMP3 の既存 `time_manage.c` はこの4ヘッダを include していなかったため `_kernel_tmax_*` 未宣言でビルド不能だった（全8構成で発覚）。dcre `extension/dcre/kernel/time_manage.c:49-52` は元からこの4行を include しており、転記漏れではなく元々の include 節がこの4ヘッダを必要としていなかった（`chknfy`/`nfyhdr` 追加で初めて必要になった）ため、ブリーフの「check.h が include されていなければ追加し理由を書く」という指示を同種の欠落4件に適用した | - |
| kernel/allfunc.h（dcre段階2 Task 4） | mod (dcre-port) | `/* time_manage.c */` 節の `#define TOPPERS_fch_hrt` 直後に `TOPPERS_chknfy`/`TOPPERS_nfyhdr` の2行を追加（ALLFUNC ビルドで新区画を有効化するため必須。段階1 Task 3 の `TOPPERS_kermem` 追加と同じ理由） | - |
| kernel/Makefile.kernel（dcre段階2 Task 4） | mod (dcre-port) | `time_manage =` の .o 列挙末尾に `chknfy.o nfyhdr.o` を追記（上流形式の維持目的。`KERNEL_FCSRCS` は無改変。CMake の `ALLFUNC` ビルドはこの行を参照しないため実ビルドには影響しない） | - |
| kernel/kernel_rename.def（dcre段階2 Task 4） | mod (dcre-port) | `# spin_lock.c` 節と `# cyclic.c` 節のあいだに `# time_manage.c` 節を新設し `check_nfyinfo`/`notify_handler` の2エントリを追加 | - |
| kernel/kernel_rename.h・kernel_unrename.h（dcre段階2 Task 4） | mod (dcre-port, 生成物) | 上記 `.def` の変更を `utils/genrename.rb kernel`（`kernel/` で実行）により再生成（手編集ではない）。`git diff --stat` は両ファイルとも追加のみ（削除0行）。`#define check_nfyinfo _kernel_check_nfyinfo` / `#define notify_handler _kernel_notify_handler`（`kernel_rename.h`）、`#undef check_nfyinfo` / `#undef notify_handler`（`kernel_unrename.h`）が、他の節と同じ「`/* time_manage.c */` コメント見出し＋定義＋空行」という genrename.rb 標準の出力形式で追加された（見出し行込みで各ファイル+6行だが、新規シンボルは2個のみ・既存行の変更なし） | - |
| kernel/cyclic.h（dcre段階2 Task 5） | mod (dcre-port) | `#include "time_event.h"` 直後に `#include <queue.h>` を追加（`kernel_impl.h` は `queue.h` を include していないため）。`extern const ID tmax_cycid;` ブロックを `tmax_scycid`（静的生成周期通知のID番号の最大値）extern と `QUEUE free_cyccb`（free-list、CYCCB先頭に領域が無いため tmevtb 領域を流用）extern に拡張。`cycinib_table[]` extern の直後に `acycinib_table[]`／`acyc_nfyinfo_table[]`（動的生成用、kernel_cfg.c・RAM）extern を追加。`p_cyccb_table[]` extern の直後に `tnum_cyc`／`tnum_scyc` マクロ（`tnum_cyc` は `cyclic.c` から移設）と、新設の `CYCID(p_cyccb)` マクロ（FMP3 の CYCCB はポインタ表のため dcre の配列添字式が使えず、段階1 `TSKID`（`task.h:324-328`）と同型の CYCINIB ポインタによる2レンジ判定式にした）を追加。AID 無し構成では `tnum_scyc == tnum_cyc` となり `CYCID` は常に静的レンジの式に落ちる＝挙動不変 | - |
| kernel/cyclic.c（dcre段階2 Task 5） | mod (dcre-port) | `#define tnum_cyc ...` を削除（`cyclic.h` へ移設したため重複削除。`INDEX_CYC`/`get_cyccb` は無変更）。`initialize_cyclic` の静的ループ境界を `tnum_cyc` → `tnum_scyc` に変更。`QUEUE free_cyccb;` の定義を追加。ループ末尾に、マスタプロセッサのみで動く動的スロット初期化ブロック（`free_cyccb` の `queue_initialize`、`acycinib_table[j].cycatr = TA_NOEXS`、`p_cyccb->p_pcb = p_my_pcb`（呼出し時点でマスタ自身＝`get_pcb(1)`と同義）、`queue_insert_prev` で free-list 末尾へ挿入＝FIFO）を追加。ブリーフのコードを転記。AID 無し構成では `tnum_scyc == tnum_cyc` のため動的ループは空振りし、静的ループの境界変更も無影響＝挙動不変 | - |
| kernel/alarm.h（dcre段階2 Task 5） | mod (dcre-port) | cyclic.h と同型の変更。`#include <queue.h>` 追加。`tmax_almid` ブロックに `tmax_salmid` extern と `QUEUE free_almcb` extern を追加。`alminib_table[]` extern 直後に `aalminib_table[]`／`aalm_nfyinfo_table[]` extern を追加。`p_almcb_table[]` extern 直後に `tnum_alm`／`tnum_salm` マクロと `ALMID(p_almcb)` マクロ（CYCID と同型の2レンジ判定式）を追加。AID 無し構成では `tnum_salm == tnum_alm` となり挙動不変 | - |
| kernel/alarm.c（dcre段階2 Task 5） | mod (dcre-port) | cyclic.c と同型の変更。`#define tnum_alm ...` を削除（`alarm.h` へ移設）。`initialize_alarm` の静的ループ境界を `tnum_alm` → `tnum_salm` に変更。`QUEUE free_almcb;` の定義を追加。ループ末尾に、マスタプロセッサのみで動く動的スロット初期化ブロック（`free_almcb` の `queue_initialize`、`aalminib_table[j].almatr = TA_NOEXS`、`queue_insert_prev` によるFIFO free-list挿入）を追加。AID 無し構成では `tnum_salm == tnum_alm` のため挙動不変 | - |
| kernel/kernel_rename.def（dcre段階2 Task 5） | mod (dcre-port) | `# cyclic.c` 節に `free_cyccb`/`tmax_scycid`/`acycinib_table`/`acyc_nfyinfo_table` の4エントリ、`# alarm.c` 節に `free_almcb`/`tmax_salmid`/`aalminib_table`/`aalm_nfyinfo_table` の4エントリを追加（ブリーフ Step 5 の指示どおり）。生成名 `_kernel_tmax_scycid` 等は `kernel.trb`（Task 3 で追加した `AID_CYC`/`AID_ALM` 汎化ロジック `kernel.trb:186-188`）が出力する `_kernel_tmax_s#{@obj}id` と一致することを確認済み | - |
| kernel/kernel_rename.h・kernel_unrename.h（dcre段階2 Task 5） | mod (dcre-port, 生成物) | `utils/genrename.rb kernel`（`kernel/` で実行）で再生成（手編集ではない）。`git diff --stat` は両ファイルとも各+8行・削除0行（`# cyclic.c`/`# alarm.c` 節にそれぞれ4エントリの `#define`/`#undef` が追加されただけ、他の差分なし） | - |
| kernel/cyclic.c（dcre段階2 Task 6） | mod (dcre-port) | `TOPPERS_cycini` 区画の直後・`TOPPERS_sta_cyc` 区画の直前に `acre_cyc`/`del_cyc` を追加（ブリーフのコードを転記）。dcre からの適応点は4つ：(1) `lock_cpu`/`acquire_glock`・`release_glock`/`unlock_cpu` の対化（giant-lock 規約）、(2) 空判定を `tnum_cyc == 0` → `tnum_cyc == tnum_scyc`（FMP3 は静的分が別レンジのため）、(3) `iprcid`/`affinity`/`p_pcb` に `TOPPERS_MASTER_PRCID`/`TOPPERS_TEPP_PRC` を充填（Global Constraint 4、訂正C：`(1U<<TNUM_PRCID)-1` ではなく `TOPPERS_TEPP_PRC` を使う）、(4) free-list のリンクに `tmevtb` 領域を転用しているため 64bit 環境で `callback`/`arg` が上書きされている分の再設定（★訂正D）。さらに `sta_cyc`/`msta_cyc`/`stp_cyc`/`ref_cyc` の4関数に、`acquire_glock()` 直後・既存の状態判定の最初の分岐として `if (p_cyccb->p_cycinib->cycatr == TA_NOEXS) { ercd = E_NOEXS; } else { …既存本体… }` を挿入（段階1の existence-before-state 規約と同型。既存ロジックは字下げのみでbyte-preserve）。**`msta_cyc` は dcre に存在しない FMP3 固有関数であり上流に先例が無い**――`sta_cyc` への類推適用（段階1の `mig_tsk`/`ras_ter`/`ter_tsk` と同じ扱い）。`msta_cyc` はロック取得前に `p_cyccb->p_cycinib->iprcid`/`affinity` を読んで `CHECK_PRCID`/`CHECK_MIG` しており、TA_NOEXS スロットでは `affinity` が未定義値になり得るが、段階1の `mig_tsk` と同じ既知課題（ユーザ誤用経路の hardening、段階1最終レビューの deferred #1）として本段階でも触らない | - |
| kernel/alarm.c（dcre段階2 Task 6） | mod (dcre-port) | cyclic.c と同型の変更。`TOPPERS_almini` 区画の直後に `acre_alm`/`del_alm` を追加（同じ4適応点。`iprcid`/`affinity` に `TOPPERS_MASTER_PRCID`/`TOPPERS_TEPP_PRC`、free-list 転用領域の `callback`/`arg` 再設定＝訂正D）。`sta_alm`/`msta_alm`/`stp_alm`/`ref_alm` の4関数に同型の E_NOEXS 挿入（`p_almcb->p_alminib->almatr == TA_NOEXS` 判定、既存ロジックは字下げのみでbyte-preserve）。`msta_alm` も `msta_cyc` と同じ理由で CHECK_PRCID/CHECK_MIG の hardening は本段階で対処しない | - |
| kernel/allfunc.h（dcre段階2 Task 6） | mod (dcre-port) | `/* cyclic.c */` 節の `TOPPERS_cycini` 直後に `TOPPERS_acre_cyc`/`TOPPERS_del_cyc`、`/* alarm.c */` 節の `TOPPERS_almini` 直後に `TOPPERS_acre_alm`/`TOPPERS_del_alm` の各2行を追加（ALLFUNC ビルドで新規関数を有効化するため必須） | - |
| kernel/Makefile.kernel（dcre段階2 Task 6） | mod (dcre-port) | `cyclic =` の .o 列挙に `acre_cyc.o del_cyc.o` を、`alarm =` の .o 列挙に `acre_alm.o del_alm.o` を追記（上流形式の維持目的。`KERNEL_FCSRCS` は無改変。CMake の `ALLFUNC` ビルドはこの行を参照しないため実ビルドには影響しない） | - |

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

- **（2026-07-19 解消／Task 13）上記「2コア用の `cmake --build ... --target run` を
  今後整備する場合」の対応を実施した。** `target/kria_r5_gcc/target.cmake` の
  `FMP3_RUN_COMMAND` を `FMP3_PRC_NUM STREQUAL "2"` で分岐させ（`kria_arm64_gcc/
  target.cmake` の `FMP3_PRC_NUM` 分岐と同じ形）、2コア側に
  `-global xlnx-zynqmp.rpu-secondary-start=true` と `-device loader` 2個
  （`cpu-num=4`／`cpu-num=5`）を積んだ。修正後 `cmake --build build/kria_r5-2core
  --target run`（`timeout -k 5 25`）を実測：`Processor 1 start.` / `Processor 2 start.`
  の2行、`TASK1_1`/`TASK2_1` の周期出力が130行（20秒）、`rc=124`（タイムアウトに
  よる正常継続）、残存QEMUプロセス無しを確認した。修正前（1コア用の
  `FMP3_RUN_COMMAND` がそのまま `kria_r5-2core` にも使われていた状態）で
  バナー0行のまま `rc=124`（無反応のハング）になることは Task 12 が実測済み
  （上記ブロック）であり、本修正はその際に Task 12 自身が「今後整備する場合の
  対応」として書き残した変更点をそのまま適用したもの。**Task 13 では修正前の
  状態への revert-and-retest は行っていない**（Task 12 の実測記録を根拠として
  採用し，修正後の動作のみを新規に実測した。この点は推測ではなく，実施しな
  かったことの明示）。`target.cmake` は derived ファイル（pristine ではない）
  のため pristine 側の改変は無く、`target_kernel_impl.c` 等は無改変のまま。

- **（2026-07-19 解消／Task 13）`kria_r5` 用の `E_RSATR` エラーテスト variant
  （Task 11 が「follow-up gap」として未着手のまま残していたもの）を新規作成した。**
  `tools/cfg_error_tests/kria_r5_e_rsatr_intno_affinity.cfg`：`arch/arm_gcc/
  zynqmp_r5/chip_kernel.py:15-31` が「プライベート」割込み（intno 0〜31）だけを
  `(prcid << 16) | intno` でプロセッサ番号に符号化する構造を踏まえ、既存の
  IPI dispatch 用（intno 0〜3）と衝突しない未使用のプライベート範囲 intno=20 を
  選び、`CLS_PRC1`（割付け可能PRC1のみ）の囲みの中で PRC2 用に符号化された
  `(2 << 16) | 20` を `CFG_INT` することで、musca_b1/rp2350 の既存 variant と
  同型の affinity 不整合を作った。`kria_r5-2core` に対して実行し，
  ruby/python 両エンジンで `E_RSATR`（メッセージ文言も IDENTICAL）を確認
  （`tools/cfg_error_tests/run.sh build/kria_r5-2core \
  tools/cfg_error_tests/kria_r5_e_rsatr_intno_affinity.cfg "E_RSATR"` → exit=0）。
  negative control として `kria_r5`（1コア）に対して同じ `.cfg` を実行すると
  `E_PAR`（`INTNO_VALID[2]` 自体が存在しないため）になることも確認した
  （同コマンドの第3引数を `"E_PAR"` に変えて exit=0）。`tools/` は derived
  ファイルのため `DIVERGENCE_MAP.md` への記録義務は無いが，pristine の
  構造理解（chip_kernel.py の符号化ルール）に基づくテスト資産のため記録目的で
  ここに残す。

- **（2026-07-19 解消／Task 6・7・13）`kria_arm64_gcc` を追加する際に必要になった
  ROM イメージ形式フック（`FMP3_DUMP_FORMAT`）は Task 6/7 で
  `arch/arm64_gcc/common/arch.cmake` に実装済みだったが，同じ問題が
  `musca_b1_gcc`／`rp2350_pico2_gcc`（どちらも `arch/arm_m_gcc` コア共通層）
  にも存在することは「対処しない事項」として記録されたまま Task 13 まで
  未対応だった。** Task 13 で現物を再確認した：`arch/arm_m_gcc/common/
  Makefile.core:31` は `arch/arm64_gcc/common/Makefile.core:34` と同じ
  無条件代入 `DUMP = dump` を持つ（`sample/Makefile:133-134` の
  `ifndef DUMP: DUMP = srec` を include 順で後から上書きする、同一の理屈）。
  `arch/arm_m_gcc/common/arch.cmake` に `set(FMP3_DUMP_FORMAT dump)` を追加し
  （`arch/arm64_gcc/common/arch.cmake` と同じ場所・同じ書式）、`musca_b1_gcc`／
  `musca_b1-2core`／`rp2350_pico2_gcc` を srec から dump へ切り替えた。
  DUMPOPTS：`musca_b1_gcc`／`rp2350_pico2_gcc` の `Makefile.target` は
  `DUMPOPTS` を定義しない（`kria_arm64_gcc` のみ定義。現物確認済み）ため，
  上流でもフィルタ無し（全セクション）の `objdump -s` になる。CMake 汎用層は
  `FMP3_DUMPOPTS` 未定義時に既定で空文字列（`CMakeLists.txt:96-97`）にするため，
  ここで何も宣言しなければ上流と同じ挙動になる（実際に何も宣言していない）。
  影響確認：`rm -rf build/{musca_b1,musca_b1-2core,rp2350_pico2}` からの
  クリーンビルドで `generated/cfg1_out.dump`（`.srec` ではない）が生成される
  ことを確認し，`tools/cfg_equivalence.sh`（拡張子 `srec`／`dump` を自動判定，
  Task 6 で既に両対応済み）・`tools/cfg_error_tests/run.sh`（同じく Task 7 で
  両対応済み）を実行し，3構成とも exit=0（MATCH／RESULT=OK）を確認した。
  `kria_r5_gcc`（`arch/arm_gcc` コア共通層）は `arch/arm_gcc/common/
  Makefile.core` に `DUMP` の定義自体が無いことを確認済み（Task 8）のため対象外
  ＝現状の srec のままで正しい。全6ターゲットのROMイメージ形式は
  上流Makefileの実際の挙動と一致した：srec = polarfire・kria_r5、
  dump = musca_b1×2・rp2350・kria_arm64×2。

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

- **（2026-07-19 発見・未調査／Task 13）`kria_r5-2core` の QEMU 実行でも、上記と同型に
  見える `LOG_NOTICE("no time event is processed in hrt interrupt on PRC%d.")` が
  観測された。** ただし出力されるプロセッサが**PRC1**である点が musca_b1（PRC2）と
  逆である。Task 13 の Step 4 回帰確認（`cmake --build build/kria_r5-2core --target run`、
  `timeout -k 5 25`、20秒分の出力）で7回観測した（すべて `on PRC1.`）。musca_b1 は
  ARM-M の SysTick（`target_timer.c`）、kria_r5 は ZynqMP の TTC（`ttc_hrt.c`）と
  実装するハードウェアタイマが異なり、`hrt_clear_event_body()` 相当の実装が同じ機序を
  持つかどうかは**未確認**（コードは読んでいない）。カーネルは20秒間 PRC1/PRC2 双方の
  `TASK1_1`/`TASK2_1` 周期出力を継続しており（130行）、致命的でないことは実測で確認したが、
  根本原因の調査はTask 13のスコープ外（本タスクの4件の既知欠陥のいずれにも該当しない）
  のため行っていない。**事実（7回・PRC1・非致命的）と、musca_b1と「同型かもしれない」という
  推測は分けて記録する**。pristine は未改変（調査は既存のビルド成果物の実行のみで行った）。

## 期限付きの逸脱

| 対象 | 内容 | 解消条件 |
|---|---|---|
| （現在なし） | 2026-07-19 時点で期限付きの逸脱は無い。`cfg_py/cfg.py` の逸脱は計画B完了で解消済み（上記「解消済み事項」参照） | - |
