#!/usr/bin/env bash
#
# import_upstream.sh - fmp3_archive の pristine を vendor ブランチ(upstream)へ取り込む（方式B）。
#
#   使い方: tools/import_upstream.sh <archive-url-or-path> <ref>
#     <ref> は fmp3_archive のタグ/コミット（例: polarfire_soc_kit_gcc-20241224, v3.1.0）
#
#   動作:
#     1. archive の <ref> のツリーを取得し、archive 側 infra（tools/ docs/ RELEASES.csv 等）を除外。
#        さらに tools/upstream_targets.txt がある場合、そこに列挙した target/ 配下だけを残す
#        （allowlist。ファイルが無ければ target/ は全部入り）。
#     2. その pristine を vendor ブランチ `upstream`（pristine のみ）へコミット。
#        - main の作業ツリーは触らない（別 worktree で処理）。
#     3. 取り込み後、`git merge upstream` で main へ 3-way マージ（衝突は DIVERGENCE_MAP.md 参照で解決）。
#
set -euo pipefail
die(){ echo "ERROR: $*" >&2; exit 1; }
log(){ echo "[upstream] $*" >&2; }

REPO="$(git rev-parse --show-toplevel)" || die "git リポジトリ内で実行すること"
EXCL="${REPO}/tools/upstream_infra_exclude.txt"
TGTS="${REPO}/tools/upstream_targets.txt"
UPSTREAM_BRANCH="${UPSTREAM_BRANCH:-upstream}"
[ -f "$EXCL" ] || die "見つからない: $EXCL"
command -v tar >/dev/null || die "tar が必要"

ARCHIVE="${1:?usage: import_upstream.sh <archive-url-or-path> <ref>}"
REF="${2:?usage: import_upstream.sh <archive-url-or-path> <ref>}"

mapfile -t EXC < <(grep -vE '^\s*(#|$)' "$EXCL" | sed 's:/*$::')

tmp="$(mktemp -d)"
trap 'git -C "$REPO" worktree remove --force "$tmp/wt" 2>/dev/null || true; rm -rf "$tmp"; git -C "$REPO" worktree prune 2>/dev/null || true' EXIT
adir="$tmp/archive"; stage="$tmp/stage"; wt="$tmp/wt"
mkdir -p "$stage"

# --- archive 取得 & ref 解決 ---
git clone -q --no-checkout "$ARCHIVE" "$adir"
git -C "$adir" fetch -q --tags origin 2>/dev/null || true
# ^{commit} で peel する（annotated tag をそのまま使うと tag オブジェクトの SHA が pin に残る）
sha="$(git -C "$adir" rev-parse --verify "${REF}^{commit}")" || die "ref 解決不可: $REF"
log "archive ref '${REF}' = ${sha}"

# --- ツリー抽出（infra strip） ---
git -C "$adir" archive "$sha" | tar -x -C "$stage"
for p in "${EXC[@]}"; do rm -rf "${stage:?}/${p}"; done

# --- target/ の絞り込み（allowlist。ファイルが無ければ全部残す） ---
if [ -f "$TGTS" ]; then
  mapfile -t KEEP < <(grep -vE '^\s*(#|$)' "$TGTS" | sed 's:/*$::')
  [ "${#KEEP[@]}" -gt 0 ] || die "空の allowlist: $TGTS（全部入りにしたいならファイルごと消すこと）"
  [ -d "${stage}/target" ] || die "target/ が無い: ref '${REF}' は target を含まない？"
  # 綴り間違いを黙って通さない（存在しない target 名は即エラー）
  for k in "${KEEP[@]}"; do
    [ -d "${stage}/target/${k}" ] || die "allowlist の target が ref '${REF}' に無い: ${k}"
  done
  for d in "${stage}"/target/*/; do
    [ -d "$d" ] || continue
    name="$(basename "$d")"
    keep=0
    for k in "${KEEP[@]}"; do if [ "$name" = "$k" ]; then keep=1; break; fi; done
    if [ "$keep" -eq 0 ]; then log "drop target/${name}"; rm -rf "$d"; fi
  done
  log "kept targets: ${KEEP[*]}"
fi

# --- vendor worktree で upstream ブランチを更新 ---
if git -C "$REPO" show-ref --verify --quiet "refs/heads/${UPSTREAM_BRANCH}"; then
  git -C "$REPO" worktree add -q "$wt" "$UPSTREAM_BRANCH"
else
  git -C "$REPO" worktree add -q --detach "$wt"
  git -C "$wt" checkout -q --orphan "$UPSTREAM_BRANCH"
  git -C "$wt" rm -rq --cached . >/dev/null 2>&1 || true
fi

find "$wt" -mindepth 1 -maxdepth 1 ! -name .git -exec rm -rf {} +
cp -a "$stage/." "$wt/"
git -C "$wt" add -A
if git -C "$wt" diff --cached --quiet; then
  log "pristine に変化なし。コミットは作らない。"
else
  git -C "$wt" commit -q -m "Import fmp3 pristine ${REF} (${sha})"
  log "committed to '${UPSTREAM_BRANCH}'"
fi

echo "$sha" > "${REPO}/.upstream_pending_sha"
log "----"
log "次の手順（main で実行）:"
log "  1) git merge ${UPSTREAM_BRANCH}"
log "     └ 衝突は DIVERGENCE_MAP.md を参照して解決（自分の改変を再適用）"
log "  2) UPSTREAM_PRISTINE.txt に ${sha} を記録"
log "  3) UPSTREAM_VERSION（非依存部 3.M.N）・DIVERGENCE_MAP.md を更新して commit"
