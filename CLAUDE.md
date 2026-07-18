# CLAUDE.md

このプロジェクトの規約・手順は **AGENTS.md** を正本とします。
作業を始める前に必ず AGENTS.md を読んでください。

@AGENTS.md

---

## 現況（2026-07-18）

**このリポジトリはまだ bootstrap 前の scaffold である。** 以下を前提に判断すること。

- **コミットが 1 つも無い**（`git log` が `does not have any commits yet` を返す）。
- `upstream` ブランチは**未作成**。pristine（`kernel/` `arch/` `target/` …）は**まだ入っていない**。
- `UPSTREAM_PRISTINE.txt` は未記入（`<archive-commit-sha>` のまま）。
- `CMakeLists.txt` は雛形で、**ビルドは通らない**（`add_subdirectory` 無し）。`configurator/` も README のみで実装が無い。

したがって「ビルドして確認」は現時点では成立しない。まず AGENTS.md §3 の初回セットアップを行う。

```bash
# 上流 archive の位置（このリポジトリから見て ../FMP3/fmp3_archive）
tools/import_upstream.sh ../FMP3/fmp3_archive v3.1.0
```

---

## 全体像（これだけは先に理解する）

**pristine と派生ファイルが root 名前空間を共有する**のがこの構成の肝であり、事故の元でもある。
`kernel/` `arch/` `target/` `include/` `syssvc/` `library/` `cfg/` は上流の持ち物、
`cmake/` `configurator/` `tools/` `CMakeLists.txt` `CMakePresets.json` と各種 .md は派生の持ち物。
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
3. **pristine の `cfg/` は使わない。** cfg 相当は `configurator/`（Python）が提供し、CMake から呼ぶ。
   `cfg/` はツリーに残るが**参照しない**。
4. **pin を必ず記録**：`UPSTREAM_VERSION`（3.M.N）＋ `UPSTREAM_PRISTINE.txt`（archive の commit SHA）。

詳細な取り込み・追従・完了条件はすべて AGENTS.md を参照。
