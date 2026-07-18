# DIVERGENCE_MAP — 上流 pristine への乖離台帳

上流：fmp3_archive（`UPSTREAM_VERSION` / `UPSTREAM_PRISTINE.txt` 参照）。
pristine を改変したら必ずここに記録する（マージ衝突解決の根拠になる）。

| path | 種別 | 内容・理由 | 上流報告 |
|------|------|-----------|----------|
| cfg/ | replace | cfg を `configurator/`（Python 実装）へ置換。pristine cfg は不使用 | - |
| target/ | remove | 使わない target を取り込まない（imx8mm_evk_arm64_gcc / raspberrypi_pico_gcc / stm32mp257f_dk_arm64_gcc / zcu102_arm64_gcc / zybo_gcc / zybo_z7_gcc）。allowlist は `tools/upstream_targets.txt`。復活させたい場合は 1 行足して `import_upstream.sh` 再実行 → `git merge upstream` | - |

種別: add=追加 / patch=部分改変 / replace=置換 / remove=削除
