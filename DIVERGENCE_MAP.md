# DIVERGENCE_MAP — 上流 pristine への乖離台帳

上流：fmp3_archive（`UPSTREAM_VERSION` / `UPSTREAM_PRISTINE.txt` 参照）。
pristine を改変したら必ずここに記録する（マージ衝突解決の根拠になる）。

| path | 種別 | 内容・理由 | 上流報告 |
|------|------|-----------|----------|
| cfg/ | replace | cfg を `cfg_py/`（Python 実装）へ置換。pristine cfg は不使用 | - |
| target/ | remove | 使わない target を取り込まない（imx8mm_evk_arm64_gcc / raspberrypi_pico_gcc / stm32mp257f_dk_arm64_gcc / zcu102_arm64_gcc / zybo_gcc / zybo_z7_gcc）。allowlist は `tools/upstream_targets.txt`。復活させたい場合は 1 行足して `import_upstream.sh` 再実行 → `git merge upstream` | - |
| target/m5stamp_esp32p4_gcc | remove | ESP32-P4 のターゲット依存部は本リポジトリでは管理しない。`fmp3_esp_idf`（別リポジトリ、`/home/honda/TOPPERS/ESP32/fmp3_esp_idf`）が chip 依存部・ターゲット依存部の両方を管理する方針にユーザが決定したため（2026-07-19）。上記6個（使わないから外す）とは除外理由が異なる点に注意。`arch/riscv_gcc/esp32p4`（chip 依存部）は上流追従の差分を見られる利点を保つため pristine のまま残す＝除外対象外。復活させたい場合は `tools/upstream_targets.txt` に 1 行足して `import_upstream.sh` 再実行 → `git merge upstream` | - |

種別: add=追加 / patch=部分改変 / replace=置換 / remove=削除
