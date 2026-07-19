# CLAUDE.md

このプロジェクトの規約・手順は **AGENTS.md** を正本とします。
作業を始める前に必ず AGENTS.md を読んでください。

@AGENTS.md

---

## 現況（2026-07-19）

**計画A/A2/B/C（残り3ターゲット追加）まで完了。5ターゲット・8ビルド構成すべてが
CMakeでビルドでき、`cfg_equivalence.sh`（差分等価性検査）・`cfg_error_tests/run.sh`
（エラー検出経路の回帰）とも全構成exit=0、うち7構成（5ターゲット中rp2350以外の
4ターゲット・シングル/マルチコア計7プリセット）はQEMUで起動＋タスク走行まで確認済み。
cfgはasp3_core 1.7.1の本物のPythonエンジン（`cfg_py/`）で、Rubyの
シムだった時代は終わっている。**

- `upstream` ブランチ作成済み・`main` へマージ済み。pristine（`kernel/` `arch/` `target/` …）はツリーにある。
- pin：`UPSTREAM_VERSION` = 3.4.0、`UPSTREAM_PRISTINE.txt` = `b59797f14dedcb07020f96895903ca7fcd14a4af`
  （fmp3_archive の `rp2350_pico2_gcc-20260719` = release/3.4 の先端。
  musca_b1/rp2350_pico2 の simple パッケージのみ 20260719 に更新、indep は 3.4.0 のまま）。
- **取り込んだ target は 5 個**（`tools/upstream_targets.txt` の allowlist）：
  `musca_b1_gcc` `rp2350_pico2_gcc` `polarfire_soc_kit_gcc`
  `kria_arm64_gcc` `kria_r5_gcc`。他は意図的に落としてある（DIVERGENCE_MAP.md 参照）。
  `m5stamp_esp32p4_gcc`（ESP32-P4 ターゲット依存部）は `fmp3_esp_idf`（別リポジトリ）で
  管理する方針のため除外（他5個＝このプロジェクトで使わない、とは理由が異なる）。
  `arch/riscv_gcc/esp32p4`（chip 依存部）は pristine のまま残っている。
- **cfg は asp3_core 1.7.1 の本物の Python エンジン**（`cfg_py/`、計画B完了）。
  pristine の `cfg/cfg.rb` は `tools/cfg_equivalence.sh`（CMake外）が差分等価性検査の
  オラクルとして呼ぶだけで、CMake のビルドグラフには一切含まれない
  （AGENTS.md §2 規則3を文言・精神の両方で満たす）。
- **5ターゲット・8ビルド構成すべてが `cmake --preset <name> && cmake --build build/<name>`
  で exit=0**。うち7構成はQEMUで起動・タスク走行まで確認済み：

  | プリセット | アーキ | コア数 | QEMU起動 | 備考 |
  |---|---|---|---|---|
  | `polarfire_soc_kit-qemu` | RISC-V | 4 | ✅ `Processor 1..4 start.` 4行 | 計画A完了 |
  | `musca_b1` | Cortex-M33 | 1 | ✅ `Processor 1 start.` 1行 | 計画A2完了 |
  | `musca_b1-2core` | Cortex-M33 | 2 | ✅ `Processor 1/2 start.` 2行 | 旧HardFaultは上流20260719で解消済み（下記） |
  | `rp2350_pico2` | Cortex-M33 | 1 | ❌ ビルドのみ | QEMUにRP2350/Picoのマシンモデルが無い（上流も未対応）。計画C Task 1 |
  | `kria_arm64-1core` | AArch64 | 1 | ✅ `Processor 1 start.` 1行 | 計画C Task 6/7 |
  | `kria_arm64` | AArch64 | 4 | ✅ `Processor 1..4 start.` 4行 | 同上 |
  | `kria_r5` | Cortex-R5F | 1 | ✅ `Processor 1 start.` 1行 | 計画C Task 10/12 |
  | `kria_r5-2core` | Cortex-R5F | 2 (split) | ✅ `Processor 1/2 start.` 2行 | Task 12で原因特定・Task 13で`target.cmake`の`run`ターゲットを修正して到達 |

  実行例：`cmake --preset polarfire_soc_kit-qemu && cmake --build build/polarfire_soc_kit-qemu --target run`。
  `rp2350_pico2` のみ `FMP3_RUN_COMMAND` を意図的に定義していないため `run` ターゲット自体が無い。

- **旧・musca_b1 2コアSMPのHardFault問題は上流自身が解消済み**（上流20260719、
  `hrt_base`等がプロセッサ別配列化）。1コア／2コアともHardFaultなく走行を継続する。
  ただし **musca_b1-2core（PRC2）・kria_r5-2core（PRC1）で「no time event is
  processed in hrt interrupt」という非致命的な `LOG_NOTICE` が定期的に出る**
  （前者は機序を特定済み、後者は未調査。詳細は `DIVERGENCE_MAP.md` の「未解決事項」参照）。
- **ROMイメージ形式**（`FMP3_DUMP_FORMAT`）は上流Makefileの挙動に忠実：
  `srec` = polarfire・kria_r5、`dump` = musca_b1×2・rp2350・kria_arm64×2
  （Task 6/7でkria_arm64向けに実装、Task 13でmusca_b1/rp2350の同型の乖離を解消）。
- **汎用層 `CMakeLists.txt` は計画C（3ターゲット追加）でも変更していない**（ROMイメージ形式の
  フック自体は計画A2〜Bの時点で既に汎用化済み）。ターゲット追加のたびに触るのは
  `arch/*/*/{arch,chip}.cmake` と `target/*/target.cmake` のみ、という層の切り方が
  5ターゲット目まで保たれたことの実証。

```bash
# 上流 archive の位置（このリポジトリから見て ../fmp3_archive）
tools/import_upstream.sh ../fmp3_archive <ref>
```

---

## 全体像（これだけは先に理解する）

**pristine と派生ファイルが root 名前空間を共有する**のがこの構成の肝であり、事故の元でもある。
`kernel/` `arch/` `target/` `include/` `syssvc/` `library/` `cfg/` は上流の持ち物、
`cmake/` `cfg_py/` `tools/` `CMakeLists.txt` `CMakePresets.json` と各種 .md は派生の持ち物。
**ファイルがどちらの持ち物かで、載せてよいブランチが決まる。**

- `upstream` … pristine のみの vendor ブランチ。`tools/import_upstream.sh` だけが更新する。派生ファイルを載せない。
- `main` … `upstream` をマージした上に派生を載せた開発本流。

pristine を編集してよい（in-place 編集可）が、**編集したら必ず `DIVERGENCE_MAP.md` に 1 行足す**。
これはマージ衝突を解決するときの唯一の根拠になるので、後から書けない。

上流追従は**マージのみ**（`git merge upstream`）。コピーで上書きしない。

---

## 特に重要（AGENTS.md §2 HARD RULES より）

1. **`upstream` ブランチに派生ファイルを載せない。** 更新は `import_upstream.sh` 経由のみ、手で触らない。
2. **pristine への改変は必ず `DIVERGENCE_MAP.md` に記録**（対象・種別・理由・上流報告有無）。
3. **pristine の `cfg/` は使わない。** cfg 相当は `cfg_py/`（Python）が提供し、CMake から呼ぶ。
   `cfg/` はツリーに残るが**参照しない**。
4. **pin を必ず記録**：`UPSTREAM_VERSION`（3.M.N）＋ `UPSTREAM_PRISTINE.txt`（archive の commit SHA）。

詳細な取り込み・追従・完了条件はすべて AGENTS.md を参照。
