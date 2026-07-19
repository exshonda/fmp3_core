#!/usr/bin/env bash
#
#		cfg_equivalence.sh -- Ruby cfg と Python cfg の差分等価性検査
#
#  pristine の cfg/cfg.rb（オラクル）と cfg_py/engine_next/cfg.py（開発中の
#  本物のPythonエンジン）を、pass1 から完全に独立した2本のパイプラインとして
#  走らせ、cfg1_out.c / offset.h / kernel_cfg.c / kernel_cfg.h を比較する。
#
#  AGENTS.md §2 規則3（pristineのcfg/はCMakeから参照しない）を守るため、
#  本スクリプトはCMake外に置かれ、CMakeのビルドグラフには一切登場しない。
#
#  使い方:
#    tools/cfg_equivalence.sh <build-preset-dir> [--pass1-only]
#  例:
#    tools/cfg_equivalence.sh build/polarfire_soc_kit-qemu
#    tools/cfg_equivalence.sh build/musca_b1
#    tools/cfg_equivalence.sh build/musca_b1-2core
#
#  終了コード: 0=一致 / 1=不一致 / 2=実行前提が満たされていない（設定エラー）
#
set -u
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${1:?usage: $0 <build-preset-dir> [--pass1-only]}"
PASS1_ONLY="${2:-}"

if [ ! -f "${BUILD_DIR}/build.ninja" ]; then
    echo "cfg_equivalence.sh: ${BUILD_DIR}/build.ninja not found. configure first." >&2
    exit 2
fi

WORK="$(mktemp -d /tmp/cfgeq.XXXXXX)"
RUBY_DIR="${WORK}/ruby"
PY_DIR="${WORK}/py"
mkdir -p "${RUBY_DIR}" "${PY_DIR}"
echo "cfg_equivalence.sh: work dir = ${WORK}"

FAIL=0

#  ninja -t commands から、指定ターゲットに対する「最後に現れる」
#  cfg_py/cfg.py 呼び出し行を取り出す（対象ターゲットが依存する前段の
#  cfg呼び出し（pass1等）も一緒に表示されるため、最後の行が目的のコマンド）。
extract_cmd() {
    local target="$1"
    ninja -C "${BUILD_DIR}" -t commands "${target}" 2>/dev/null \
        | grep 'cfg_py/cfg\.py' | tail -1
}

#  1コマンド文字列を engine 種別に応じて変換して実行する。
#    kind = ruby | python
run_cmd() {
    local kind="$1" cmd="$2" outdir="$3"
    # "cd <dir> && python3 -B <root>/cfg_py/cfg.py <args...> [&& <post>]" の
    # うち、"python3 -B .../cfg_py/cfg.py" 部分から後ろ、次の "&&" までを取り出す。
    local args
    args="$(echo "${cmd}" | sed -E 's#^.*cfg_py/cfg\.py##; s# && .*$##')"
    if [ "${kind}" = "ruby" ]; then
        ( cd "${outdir}" && ruby "${ROOT}/cfg/cfg.rb" ${args} )
    else
        # -T/-C 引数の .trb を .py に置換して Python エンジン呼び出しへ渡す。
        # --rom-symbol/--rom-image の絶対パスはそのまま（両パイプライン共通で
        # BUILD_DIR/generated 配下の既存 .syms/.srec を参照する。後述コメント参照）。
        local pyargs
        pyargs="$(echo "${args}" | sed -E 's#(-[TC] [^ ]+)\.trb#\1.py#g')"
        ( cd "${outdir}" && python3 -B "${ROOT}/cfg_py/engine_next/cfg.py" ${pyargs} )
    fi
}

compare_stage() {
    local label="$1"; shift
    local files=("$@")
    local ok=1
    for f in "${files[@]}"; do
        if [ ! -f "${RUBY_DIR}/${f}" ] || [ ! -f "${PY_DIR}/${f}" ]; then
            echo "  [MISSING] ${f} (ruby: $( [ -f "${RUBY_DIR}/${f}" ] && echo yes || echo no ), python: $( [ -f "${PY_DIR}/${f}" ] && echo yes || echo no ))"
            ok=0
            continue
        fi
        if [ ! -s "${RUBY_DIR}/${f}" ] || [ ! -s "${PY_DIR}/${f}" ]; then
            echo "  [EMPTY]   ${f} (ruby size=$(stat -c%s "${RUBY_DIR}/${f}"), python size=$(stat -c%s "${PY_DIR}/${f}"))"
            ok=0
            continue
        fi
        if diff -q "${RUBY_DIR}/${f}" "${PY_DIR}/${f}" > /dev/null; then
            echo "  [OK]      ${f}"
        else
            echo "  [DIFFERS] ${f}"
            diff "${RUBY_DIR}/${f}" "${PY_DIR}/${f}" | head -20
            ok=0
        fi
    done
    if [ "${ok}" = "1" ]; then
        echo "cfg_equivalence.sh: ${label}: MATCH"
    else
        echo "cfg_equivalence.sh: ${label}: MISMATCH"
        FAIL=1
    fi
}

echo "== pass1 (cfg1_out.c) =="
PASS1_CMD="$(extract_cmd generated/cfg1_out.timestamp)"
if [ -z "${PASS1_CMD}" ]; then
    echo "cfg_equivalence.sh: could not extract pass1 command from ninja -t commands" >&2
    exit 2
fi
run_cmd ruby   "${PASS1_CMD}" "${RUBY_DIR}" > "${RUBY_DIR}/pass1.log" 2>&1
RUBY_RC=$?
run_cmd python "${PASS1_CMD}" "${PY_DIR}"   > "${PY_DIR}/pass1.log"   2>&1
PY_RC=$?
if [ "${RUBY_RC}" != 0 ] || [ "${PY_RC}" != 0 ]; then
    echo "cfg_equivalence.sh: pass1 execution failed (ruby rc=${RUBY_RC}, python rc=${PY_RC})"
    echo "--- ruby log ---";   cat "${RUBY_DIR}/pass1.log"
    echo "--- python log ---"; cat "${PY_DIR}/pass1.log"
    FAIL=1
fi
compare_stage "pass1" cfg1_out.c

if [ "${PASS1_ONLY}" = "--pass1-only" ]; then
    [ "${FAIL}" = "0" ] && echo "cfg_equivalence.sh: RESULT = MATCH (pass1-only)" || echo "cfg_equivalence.sh: RESULT = MISMATCH"
    echo "cfg_equivalence.sh: work dir preserved at ${WORK} for inspection"
    exit "${FAIL}"
fi

echo "== compiling cfg1_out (shared inputs for both pipelines, per Task1 Step3 rationale) =="
#  cfg1_out.c は pass1 が MATCH した前提のもとでは Ruby/Python どちらの
#  出力を使っても同じバイナリになる。既存ビルドの cfg1_out ELF から nm/objcopy
#  で得た .syms/.srec（generated/cfg1_out.syms, generated/cfg1_out.srec）を、
#  両パイプラインで共有する（設計書 §7.1: cfg1_out.dbはPStore/pickleで別形式
#  のため共有できないが、.syms/.srecはnm/objcopyの出力でエンジン非依存）。
SYMS="${BUILD_DIR}/generated/cfg1_out.syms"
SREC="${BUILD_DIR}/generated/cfg1_out.srec"
if [ ! -f "${SYMS}" ] || [ ! -f "${SREC}" ]; then
    echo "cfg_equivalence.sh: ${SYMS} / ${SREC} not found. Build cfg1_out_syms/cfg1_out_srec targets first:" >&2
    echo "  ninja -C ${BUILD_DIR} cfg1_out_syms cfg1_out_srec" >&2
    exit 2
fi
#  ★実測で判明した罠: pass2.rb / pass2.py の Cfg1Out.read()（両エンジンとも）は
#  --rom-symbol/--rom-image のCLI引数とは無関係に、CFG1_OUT_SYMS="cfg1_out.syms"
#  / CFG1_OUT_SREC="cfg1_out.srec" という「裸の相対ファイル名」を cwd から
#  ハードコードで読む（--rom-symbol/--rom-image が渡す絶対パスは、テンプレート
#  内で個別に参照する $romSymbol テーブル専用で、pass1→pass2 の主ハンドオフとは
#  別経路）。これをコピーしないと RUBY_DIR/PY_DIR どちらも
#  "No such file or directory - cfg1_out.syms" で同一に失敗し、「両方とも
#  MISSING」を「テンプレート未実装によるTask2時点の想定内失敗」と誤読しかねない
#  （offset/kernel_cfg比較の意味が失われる）。両ディレクトリへ明示的に複製する。
cp "${SYMS}" "${RUBY_DIR}/cfg1_out.syms"
cp "${SREC}" "${RUBY_DIR}/cfg1_out.srec"
cp "${SYMS}" "${PY_DIR}/cfg1_out.syms"
cp "${SREC}" "${PY_DIR}/cfg1_out.srec"

echo "== pass2 -O (offset.h) =="
OFFSET_CMD="$(extract_cmd generated/offset.timestamp)"
if [ -z "${OFFSET_CMD}" ]; then
    echo "cfg_equivalence.sh: could not extract offset command from ninja -t commands" >&2
    exit 2
fi
run_cmd ruby   "${OFFSET_CMD}" "${RUBY_DIR}" > "${RUBY_DIR}/offset.log" 2>&1
run_cmd python "${OFFSET_CMD}" "${PY_DIR}"   > "${PY_DIR}/offset.log"   2>&1
compare_stage "offset" offset.h

echo "== pass2 (kernel_cfg.c/h) =="
KCFG_CMD="$(extract_cmd generated/kernel_cfg.timestamp)"
if [ -z "${KCFG_CMD}" ]; then
    echo "cfg_equivalence.sh: could not extract kernel_cfg command from ninja -t commands" >&2
    exit 2
fi
run_cmd ruby   "${KCFG_CMD}" "${RUBY_DIR}"  > "${RUBY_DIR}/kernel_cfg.log"  2>&1
run_cmd python "${KCFG_CMD}" "${PY_DIR}"    > "${PY_DIR}/kernel_cfg.log"   2>&1
compare_stage "kernel_cfg" kernel_cfg.c kernel_cfg.h

if [ "${FAIL}" = "0" ]; then
    echo "cfg_equivalence.sh: RESULT = MATCH"
else
    echo "cfg_equivalence.sh: RESULT = MISMATCH"
fi
echo "cfg_equivalence.sh: work dir preserved at ${WORK} for inspection"
exit "${FAIL}"
