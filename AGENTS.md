# AGENTS.md — fmp3_core 操作の正本

FMP3 の派生版（CMake 一本化 + Python cfg、TECS レス）。上流 pristine は `fmp3_archive` から
**方式B（vendor import）**で取り込み、in-place 編集＋乖離台帳で管理する。まずこの文書に従うこと。

---

## 1. リポジトリの構成（root レイアウト＝asp3_core 同型）

```
.                             # pristine と派生ファイルが root 名前空間を共有
├── kernel/ arch/ target/ …   # ← 上流 pristine（fmp3_archive 由来。in-place 編集可）
├── cmake/  CMakeLists.txt  CMakePresets.json   # ← 派生: CMake 化
├── cfg_py/                  # ← 派生: Python cfg（pristine の cfg/ は使わない）
├── tools/import_upstream.sh # ← 上流取り込み（vendor ブランチへ）
├── tools/upstream_infra_exclude.txt   # archive 側 infra の除外リスト
├── tools/upstream_targets.txt         # 取り込む target/ の allowlist（無ければ全部入り）
├── UPSTREAM_VERSION          # 上流の非依存部 3.M.N
├── UPSTREAM_PRISTINE.txt     # 上流の固定コミット SHA（fmp3_archive）
└── DIVERGENCE_MAP.md         # pristine への乖離台帳
```

- ブランチ
  - **`upstream`**：pristine のみ（vendor ブランチ）。`import_upstream.sh` だけが更新する。手で触らない。
  - **`main`**：`upstream` をマージした上に派生（CMake/Python cfg/パッチ）を載せた開発本流。

---

## 2. 絶対ルール（HARD RULES）

1. **`upstream` ブランチは pristine のみ**。CMake・cfg_py・tools 等の派生ファイルを絶対に載せない。更新は `tools/import_upstream.sh` 経由のみ。
2. **pristine への改変は必ず `DIVERGENCE_MAP.md` に記録**（1 ファイル 1 行以上：対象・種別・理由・上流報告有無）。
3. **pristine の `cfg/` は使わない**。cfg 相当は `cfg_py/`（Python）で提供し、CMake から呼ぶ。`cfg/` は残すが参照しない（DIVERGENCE_MAP に「cfg を Python 実装へ置換」と記載）。
4. **pin を必ず記録**：`UPSTREAM_VERSION`（3.M.N）＋ `UPSTREAM_PRISTINE.txt`（archive の commit SHA）。
5. 上流追従は**マージで行う**（`git merge upstream`）。pristine をコピペで貼り直して履歴を壊さない。
6. **取り込む target は `tools/upstream_targets.txt` で決める**。ここに無い target は `upstream` に入らない。
   増やしたくなったら 1 行足して `import_upstream.sh` を再実行し、`git merge upstream` する。

---

## 3. 初回セットアップ

`upstream`（orphan）を作るには main に最低1コミット要る。派生 scaffold を先にコミットしてから取り込む。

```bash
git init -b main
# 1) 派生 scaffold を最初のコミットに（この AGENTS.md / CMake / cfg_py / tools 等）
git add -A
git commit -m "Bootstrap fmp3_core scaffold (CMake + Python cfg)"
# 2) pristine を vendor ブランチ upstream(orphan) へ取り込む（archive の pin を指定）
tools/import_upstream.sh <fmp3_archive-url-or-path> v3.1.0
# 3) 初回のみ unrelated histories を許可して pristine を main へマージ
git merge --allow-unrelated-histories --no-edit upstream
# 4) pin を記録して確定
#    UPSTREAM_PRISTINE.txt に .upstream_pending_sha の値、UPSTREAM_VERSION に 3.M.N
git commit -am "Record upstream pin v3.1.0"
```

以降は §4 の通り、`git merge upstream`（`--allow-unrelated-histories` 不要）で追従する。

---

## 4. 上流追従（新しい FMP3 リリースが出たとき）

```bash
git switch main
tools/import_upstream.sh <fmp3_archive-url-or-path> <新しい ref>   # 例: polarfire_soc_kit_gcc-20250420
git merge upstream                 # 3-way マージ。衝突は DIVERGENCE_MAP.md を見て自分の改変を再適用
# UPSTREAM_VERSION / UPSTREAM_PRISTINE.txt / DIVERGENCE_MAP.md を更新
git add -A && git commit           # マージ確定
cmake --preset <target> && cmake --build --preset <target>   # 回帰確認
```

- 衝突＝上流の変更点と自分の乖離が同じ箇所。DIVERGENCE_MAP.md の該当行を根拠に解決する。
- `import_upstream.sh` は `main` の作業ツリーを触らず、`upstream` ブランチだけを別 worktree で更新する。
- **`kernel/Makefile.kernel` の `KERNEL_FCSRCS`（22個）と `CMakeLists.txt`（`kernel/*.c` の
  `add_library(fmp3 ...)` 列挙。現在地は `CMakeLists.txt:448` 付近のコメント参照）を突き合わせる。**
  `CMakeLists.txt` はこの22個を手書きで列挙しているため、上流が `KERNEL_FCSRCS` にソースを
  追加・削除しても CMake 側は追従しない（サイレントに古いまま）。マージ後、両者の差分が
  無いことを確認してから回帰確認に進むこと。

---

## 5. やってはいけないこと

- `upstream` ブランチに派生ファイルを載せる／手で編集する。
- pristine を改変して DIVERGENCE_MAP.md に書かない。
- 上流を「コピーで上書き」して追従する（マージにする）。
- pristine の `cfg/` を CMake から参照する。

---

## 6. 完了条件チェックリスト

- [ ] `upstream` は pristine のみ（`git ls-tree upstream` に派生ファイルが無い）。
- [ ] pristine への改変がすべて DIVERGENCE_MAP.md にある。
- [ ] `UPSTREAM_PRISTINE.txt` が今の `upstream` の元 archive SHA と一致。
- [ ] `cmake --preset … && cmake --build …` が通る。
