# fmp3_core

TOPPERS/FMP3  をベースに **CMake 一本化 + Python cfg + TECS レス**へ再構成した派生版（`asp3_core` の FMP3 版）。

- 上流 pristine：[`fmp3_archive`](../FMP3/fmp3_archive)（方式B: vendor import で取り込み）
- 取り込み手順・運用の正本：[AGENTS.md](AGENTS.md)
- **別マシンで開発を始めるとき**：[docs/DEVELOPMENT.md](docs/DEVELOPMENT.md)（必要な道具の版・QEMU の罠・上書き方法）
- 上流との乖離台帳：[DIVERGENCE_MAP.md](DIVERGENCE_MAP.md)
- pin：`UPSTREAM_VERSION`（3.M.N）＋ `UPSTREAM_PRISTINE.txt`（archive commit SHA）

## 構成

- `upstream` ブランチ … pristine のみ（vendor ブランチ）
- `main` ブランチ … `upstream` をマージ＋派生（CMake / `cfg_py/` の Python cfg）
