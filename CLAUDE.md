# CLAUDE.md

このプロジェクトの規約・手順は **AGENTS.md** を正本とします。
作業を始める前に必ず AGENTS.md を読んでください。

@AGENTS.md

---

## 現況（2026-07-18）

**pristine の取り込みは完了。派生（CMake / Python cfg）の実装はこれから。**

- `upstream` ブランチ作成済み・`main` へマージ済み。pristine（`kernel/` `arch/` `target/` …）はツリーにある。
- pin：`UPSTREAM_VERSION` = 3.4.0、`UPSTREAM_PRISTINE.txt` = `f3d29a4`
  （fmp3_archive の `stm32mp257f_dk_arm64_gcc-20260718` = release/3.4 の先端）。
- **取り込んだ target は 5 個だけ**（`tools/upstream_targets.txt` の allowlist）：
  `musca_b1_gcc` `rp2350_pico2_gcc` `polarfire_soc_kit_gcc`
  `kria_arm64_gcc` `kria_r5_gcc`。他は意図的に落としてある（DIVERGENCE_MAP.md 参照）。
  `m5stamp_esp32p4_gcc`（ESP32-P4 ターゲット依存部）は `fmp3_esp_idf`（別リポジトリ）で
  管理する方針のため除外（他5個＝このプロジェクトで使わない、とは理由が異なる）。
  `arch/riscv_gcc/esp32p4`（chip 依存部）は pristine のまま残っている。
- **polarfire_soc_kit_gcc は CMake でビルドでき、QEMU で動く**（計画A完了）。
  `cmake --preset polarfire_soc_kit-qemu && cmake --build build/polarfire_soc_kit-qemu --target run`
- ただし **cfg は `cfg_py/cfg.py`（pristine の Ruby `cfg/cfg.rb` へ委譲するシム）を使っている
  足場の状態**である。計画Bでシムを asp3_core 1.7.1 の本物のエンジンへ差し替える
  （DIVERGENCE_MAP.md の「期限付きの逸脱」参照）。
- 他の5ターゲットは未対応（`target.cmake` が無い）。

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
