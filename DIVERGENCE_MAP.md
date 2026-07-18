# DIVERGENCE_MAP — 上流 pristine への乖離台帳

上流：fmp3_archive（`UPSTREAM_VERSION` / `UPSTREAM_PRISTINE.txt` 参照）。
pristine を改変したら必ずここに記録する（マージ衝突解決の根拠になる）。

| path | 種別 | 内容・理由 | 上流報告 |
|------|------|-----------|----------|
| cfg/ | none | **無改変**（`git diff upstream main -- cfg` は空）。AGENTS.md §2 規則3の文言（cfg 相当は `cfg_py/` で提供し CMake から呼ぶ）は満たすが，`cfg_py/cfg.py` は計画Aの間 pristine の `cfg/cfg.rb` へ委譲する薄いシムであり，**`cfg/` は「不使用」ではなく現用**（`CMakeLists.txt` の `CFG_SCRIPT_DEPS` が `cfg/cfg.rb` `cfg/pass1.rb` 等を直接列挙し，毎ビルドで実際に実行される＝load-bearing）。次の `git merge upstream` で `cfg/` に衝突が出た場合，この行だけで「置換済み・無関係」と誤解しないこと。詳細と解消条件は下記「期限付きの逸脱」の `cfg_py/cfg.py` 行を参照 | - |
| target/ | remove | 使わない target を取り込まない（imx8mm_evk_arm64_gcc / raspberrypi_pico_gcc / stm32mp257f_dk_arm64_gcc / zcu102_arm64_gcc / zybo_gcc / zybo_z7_gcc）。allowlist は `tools/upstream_targets.txt`。復活させたい場合は 1 行足して `import_upstream.sh` 再実行 → `git merge upstream` | - |
| target/m5stamp_esp32p4_gcc | remove | ESP32-P4 のターゲット依存部は本リポジトリでは管理しない。`fmp3_esp_idf`（別リポジトリ、`/home/honda/TOPPERS/ESP32/fmp3_esp_idf`）が chip 依存部・ターゲット依存部の両方を管理する方針にユーザが決定したため（2026-07-19）。上記6個（使わないから外す）とは除外理由が異なる点に注意。`arch/riscv_gcc/esp32p4`（chip 依存部）は上流追従の差分を見られる利点を保つため pristine のまま残す＝除外対象外。復活させたい場合は `tools/upstream_targets.txt` に 1 行足して `import_upstream.sh` 再実行 → `git merge upstream` | - |
| arch/riscv_gcc/common/arch.cmake | add | Makefile.core の CMake 版。上流の Makefile は残すが CMake ビルドからは参照しない | - |
| arch/riscv_gcc/polarfire_soc/chip.cmake | add | Makefile.chip の CMake 版。`-march` は上流の `rv64gc` ではなく `rv64imafdc`（ISA は同一。`rv64gc` は実在しない multilib ディレクトリ `rv64imafdc/lp64d` に解決され `crt0.o` が見つからないが、`rv64imafdc` は既定ディレクトリ `.` に解決される。ABI は `lp64d` のまま） | 未 |
| target/polarfire_soc_kit_gcc/{target.cmake,presets.json} | add | Makefile.target の CMake 版。Microchip SDK のソース16個を最終リンクに加える | - |

種別: add=追加 / patch=部分改変 / replace=置換 / remove=削除 / none=無改変（差分ゼロだが，
運用上の注意が必要なため記録目的で本表に載せている。現状 `cfg/` のみ）

## 既知・対処しない事項

- **`ld: ... has a LOAD segment with RWX permissions` 警告**（`cfg1_out` / `fmp` リンク時）。
  pristine の Microchip リンカスクリプト（`target/polarfire_soc_kit_gcc/sdk/.../mpfs-lim.ld` が
  全領域を rwx 宣言）に起因する無害な警告であり，**意図的に対処しない**（分離しようとする方が
  pristine への不要な改変を生み危険）。`cfg1_out` は実行しないため実害は無く，`fmp` も QEMU 実機で
  正常起動を確認済み。

## 期限付きの逸脱

| 対象 | 内容 | 解消条件 |
|---|---|---|
| `cfg_py/cfg.py`（計画Aの中身。Task 3） | AGENTS.md §2 規則3「pristine の `cfg/` は使わない。cfg 相当は `cfg_py/`（Python）で提供し、CMake から呼ぶ」の**文言は満たす**（CMake が呼ぶのは常に `cfg_py/cfg.py`）が、**精神には抵触する**（`cfg_py/cfg.py` は pristine の `cfg/cfg.rb` へ委譲する薄いシムであり、実行されるのは結局 Ruby 版である）。CMake パイプラインの正しさを、テンプレート Python 移植のバグと切り離して検証するための足場。 | 計画B（`cfg_py/` への asp3_core 1.7.1 エンジン移植とテンプレート移植）の完了時に、シムを本物のエンジンへ差し替える。以降 Ruby は `tools/cfg_equivalence.sh`（CMake 外）からのみ呼ぶオラクルとして残す。**この行が残っている間は AGENTS.md §6 の完了条件を満たさない。** |
