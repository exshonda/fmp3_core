# DIVERGENCE_MAP — 上流 pristine への乖離台帳

上流：fmp3_archive（`UPSTREAM_VERSION` / `UPSTREAM_PRISTINE.txt` 参照）。
pristine を改変したら必ずここに記録する（マージ衝突解決の根拠になる）。

| path | 種別 | 内容・理由 | 上流報告 |
|------|------|-----------|----------|
| cfg/ | replace | cfg を `configurator/`（Python 実装）へ置換。pristine cfg は不使用 | - |
| (例) kernel/xxx.c | patch | 構造化ログのフック追加 | 未 |

種別: add=追加 / patch=部分改変 / replace=置換 / remove=削除
