#!/usr/bin/env bash
#
#		cfg_error_tests/run.sh -- エラー検出経路の最小回帰スイート
#
#  tools/cfg_equivalence.sh は正常系（sample1.cfg）の生成物を比較するだけで、
#  kernel/interrupt.py が実装する新規エラーチェック（E_RSATR）は一切踏まない。
#
#  ★アーキテクチャ上の実測結果（brief の想定と異なる点）:
#  cfg2/pass2.py の Cfg1Out.read() は、CFG_INT/DEF_INH 等の実パラメータ値
#  （intno/inhno/intatr/inhatr/intpri 等）を .cfg のテキストから直接ではなく、
#  「パス1が生成した cfg1_out.c を実ターゲット向けにコンパイル・リンクした
#  ELFのシンボルテーブル（nm）とメモリイメージ（objcopy -O srec）」から
#  読み取る（EXPTYPEパラメータは `valueof_<param>_<api_index><index>` という
#  シンボル名でコンパイル後の値を引く。TARGET_INTATR/TARGET_INHATR/
#  OMIT_MULTIPRC_INTERRUPT 等も `defined(...)` を実コンパイルして判定する）。
#  そのため、「pass1コマンドの.cfg引数を差し替えて実行するだけ」（当初案）
#  では kernel/interrupt.py のクラス整合性チェックは一切発火しない
#  （pass1はAPI呼び出しの構文解析のみで、クラス×プロセッサの意味検証は
#  pass2のテンプレート実行時にのみ行われるため）。本スクリプトは
#  pass1→（実コンパイル・リンク・nm・objcopyで.syms/.srecを新規生成）→pass2
#  のフルチェーンを、指定した.cfgについて毎回新規に再構築する。
#
#  使い方:
#    tools/cfg_error_tests/run.sh <build-preset-dir> <error.cfg> [expected-substring] [extra-cflags]
#  例:
#    tools/cfg_error_tests/run.sh build/polarfire_soc_kit-qemu \
#        tools/cfg_error_tests/e_rsatr_inhno_affinity.cfg "E_RSATR"
#
#  終了コード: 0=期待通り（ruby/pythonのrcとexpected-substringが一致した）
#              1=不一致（ruby/pythonでrcまたはメッセージが食い違う、
#                        あるいはexpected-substringが見つからない）
#              2=実行前提が満たされていない（ビルド未configure、cfg未検出、
#                        コンパイル/リンク自体が失敗、等）
#
set -u
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd -P)"
BUILD_DIR="${1:?usage: $0 <build-preset-dir> <error.cfg> [expected-substring] [extra-cflags]}"
CFG="${2:?usage: $0 <build-preset-dir> <error.cfg> [expected-substring] [extra-cflags]}"
EXPECT="${3:-}"
EXTRA_CFLAGS="${4:-}"

if [ ! -f "${BUILD_DIR}/build.ninja" ]; then
    echo "run.sh: ${BUILD_DIR}/build.ninja not found. configure first." >&2
    exit 2
fi
# ★実測で判明した罠: ninja -t commands は常に絶対パス（かつシンボリック
# リンクを解決した「物理パス」）でコマンドを出力する。BUILD_DIR が相対パス
# （例: "build/polarfire_soc_kit-qemu"）のままだと、以下の sed パターン
# （${BUILD_DIR}/generated/cfg1_out.c 等の文字列一致）が絶対パスの実コマンド
# に一致せず、置換が「黙って」効かなくなる。その結果コンパイル対象が指定した
# .cfgではなく実ビルドの古いcfg1_out.c（sample1.cfg由来）に差し替わり、
# E_RSATRを検出するはずが無関係のE_PARが出るなど、気付きにくい形で誤動作
# する（ここで一度実際に再現して原因を特定した）。
#
# ★上記の対処として単純に絶対化（cd && pwd）しただけでは不十分だった：
# この環境には /home/honda/TOPPERS/fmp3_core -> ./FMP3/fmp3_core という
# symlink が実在し、symlink 経由の cwd で本スクリプトを起動すると
# `pwd`（-P無し）は論理パス（symlinkを含んだパス）を返す。一方 ninja は
# 常に物理パスを出力するため、論理パスの BUILD_DIR では上と同じ「黙った
# 不一致」が symlink 経由でだけ再発する。`pwd -P` で物理パスに揃える。
BUILD_DIR="$(cd "${BUILD_DIR}" && pwd -P)"
if [ ! -f "${CFG}" ]; then
    echo "run.sh: cfg file not found: ${CFG}" >&2
    exit 2
fi
CFG_ABS="$(cd "$(dirname "${CFG}")" && pwd -P)/$(basename "${CFG}")"

WORK="$(mktemp -d /tmp/cfgerr.XXXXXX)"
RUBY_DIR="${WORK}/ruby"
PY_DIR="${WORK}/py"
COMPILE_DIR="${WORK}/compile"
mkdir -p "${RUBY_DIR}" "${PY_DIR}" "${COMPILE_DIR}"
echo "run.sh: work dir = ${WORK}"

FATAL=0

#
#  == Stage 1: pass1（構文解析。指定した.cfgをsample1.cfgの代わりに使う）==
#
PASS1_RAW="$(ninja -C "${BUILD_DIR}" -t commands generated/cfg1_out.timestamp 2>/dev/null | grep 'cfg_py/cfg\.py' | tail -1)"
if [ -z "${PASS1_RAW}" ]; then
    echo "run.sh: could not extract pass1 command from ninja -t commands" >&2
    exit 2
fi
# "cfg_py/cfg.py"より後ろ、次の"&&"までの引数部分を取り出し、依存ファイル
# 出力(-M)を落とし、末尾のCONFIG-FILE（sample1.cfg）を指定の.cfgへ差し替える。
PASS1_ARGS="$(echo "${PASS1_RAW}" | sed -E 's#^.*cfg_py/cfg\.py##; s# && .*$##; s#-M [^ ]+##')"
PASS1_ARGS_TEST="$(echo "${PASS1_ARGS}" | sed -E "s#[^ ]+/sample1\.cfg#${CFG_ABS}#")"
if [ "${PASS1_ARGS_TEST}" = "${PASS1_ARGS}" ]; then
    echo "run.sh: WARNING: sample1.cfg substitution pattern did not match" \
         "(pass1 command format may have changed)" >&2
fi

run_pass1() {
    local kind="$1" outdir="$2"
    if [ "${kind}" = "ruby" ]; then
        ( cd "${outdir}" && ruby "${ROOT}/cfg/cfg.rb" ${PASS1_ARGS_TEST} ) \
            > "${outdir}/pass1.log" 2>&1
    else
        ( cd "${outdir}" && python3 -B "${ROOT}/cfg_py/cfg.py" ${PASS1_ARGS_TEST} ) \
            > "${outdir}/pass1.log" 2>&1
    fi
}
run_pass1 ruby   "${RUBY_DIR}"; RUBY_P1RC=$?
run_pass1 python "${PY_DIR}";   PY_P1RC=$?
echo "== pass1 (parse ${CFG}) == ruby rc=${RUBY_P1RC} python rc=${PY_P1RC}"

if [ "${RUBY_P1RC}" != 0 ] || [ "${PY_P1RC}" != 0 ]; then
    echo "run.sh: pass1 failed for at least one engine (this .cfg is not usable" \
         "to test a pass2-level class/processor check; if pass1 itself is" \
         "expected to reject it, compare pass1.log directly instead)" >&2
    echo "--- ruby pass1.log ---";   cat "${RUBY_DIR}/pass1.log"
    echo "--- python pass1.log ---"; cat "${PY_DIR}/pass1.log"
    FATAL=1
fi
if [ ! -f "${RUBY_DIR}/cfg1_out.c" ] || [ ! -f "${PY_DIR}/cfg1_out.c" ]; then
    echo "run.sh: cfg1_out.c missing after pass1; cannot compile" >&2
    exit 2
fi
if ! diff -q "${RUBY_DIR}/cfg1_out.c" "${PY_DIR}/cfg1_out.c" > /dev/null 2>&1; then
    echo "run.sh: WARNING: ruby/python cfg1_out.c differ for this .cfg" \
         "(unexpected for syntactically-valid input; inspect ${WORK})" >&2
fi

if [ "${FATAL}" = "1" ]; then
    echo "run.sh: RESULT = FATAL (pass1)"
    echo "run.sh: work dir preserved at ${WORK} for inspection"
    exit 2
fi

#
#  == Stage 2: cfg1_out.c を実ターゲット向けにコンパイル・リンクし、
#     .syms/.srec を新規生成する（python側のcfg1_out.cを使う。pass1が
#     MATCHしている前提のもとでは ruby 側と同一のはず — Stage 1 で確認済み）==
#
#  start.S オブジェクトはcfg内容に依存しないため、既存ビルドの成果物を
#  そのまま再利用する（読み取り専用。再コンパイルしない）。
#
COMPILEB_RAW="$(ninja -C "${BUILD_DIR}" -t commands generated/cfg1_out.syms 2>/dev/null | grep -- '-c .*generated/cfg1_out\.c$')"
if [ -z "${COMPILEB_RAW}" ]; then
    echo "run.sh: could not extract cfg1_out.c compile command" >&2
    exit 2
fi
#  ★修正1: 以下の各置換は「一致しなければ黙って何もしない」sedの性質上、
#  一致しなかったことに気付かないまま古い成果物を使い続けてしまう罠がある
#  （BUILD_DIRのsymlink不一致で実際に踏んだ。冒頭コメント参照）。各置換の
#  前後を比較し、変化が無ければ「一致しなかった」ことの動かぬ証拠として
#  異常終了する（WARNINGで済ませない）。
COMPILEB_PRE="$(printf '%s\n' "${COMPILEB_RAW}" \
    | sed -E 's/ -MD -MT [^ ]+ -MF [^ ]+//' \
    | sed -E "s#-o CMakeFiles/cfg1_out\.dir/generated/cfg1_out\.c\.obj#-o ${COMPILE_DIR}/cfg1_out.c.obj#")"
COMPILEB_CMD="$(printf '%s\n' "${COMPILEB_PRE}" \
    | sed -E "s#-c ${BUILD_DIR}/generated/cfg1_out\.c#-c ${PY_DIR}/cfg1_out.c ${EXTRA_CFLAGS}#")"
if [ "${COMPILEB_CMD}" = "${COMPILEB_PRE}" ]; then
    echo "run.sh: FATAL: cfg1_out.c compile-command substitution did not match" \
         "(BUILD_DIR='${BUILD_DIR}' not found verbatim in ninja's compile command;" \
         "would silently compile the stale build's cfg1_out.c instead of this .cfg's" \
         "output -- refusing to continue)" >&2
    echo "  ninja command was: ${COMPILEB_PRE}" >&2
    exit 2
fi
bash -c "${COMPILEB_CMD}" > "${COMPILE_DIR}/compile.log" 2>&1
COMPILE_RC=$?
if [ "${COMPILE_RC}" != 0 ]; then
    echo "run.sh: compiling cfg1_out.c failed (rc=${COMPILE_RC})" >&2
    cat "${COMPILE_DIR}/compile.log" >&2
    exit 2
fi

LINK_RAW="$(ninja -C "${BUILD_DIR}" -t commands generated/cfg1_out.syms 2>/dev/null | grep -- '-o cfg1_out ')"
if [ -z "${LINK_RAW}" ]; then
    echo "run.sh: could not extract cfg1_out link command" >&2
    exit 2
fi
LINK_STEP1="$(printf '%s\n' "${LINK_RAW}" \
    | sed -E "s#CMakeFiles/cfg1_out\.dir/generated/cfg1_out\.c\.obj#${COMPILE_DIR}/cfg1_out.c.obj#")"
if [ "${LINK_STEP1}" = "${LINK_RAW}" ]; then
    echo "run.sh: FATAL: link-command cfg1_out.c.obj substitution did not match" >&2
    echo "  ninja command was: ${LINK_RAW}" >&2
    exit 2
fi
LINK_STEP2="$(printf '%s\n' "${LINK_STEP1}" \
    | sed -E "s#CMakeFiles/cfg1_out\.dir/([a-zA-Z0-9_./]*start\.S\.obj)#${BUILD_DIR}/CMakeFiles/cfg1_out.dir/\1#")"
if [ "${LINK_STEP2}" = "${LINK_STEP1}" ]; then
    echo "run.sh: FATAL: link-command start.S.obj substitution did not match" >&2
    echo "  ninja command was: ${LINK_STEP1}" >&2
    exit 2
fi
LINK_CMD="$(printf '%s\n' "${LINK_STEP2}" \
    | sed -E "s#-o cfg1_out #-o ${COMPILE_DIR}/cfg1_out #")"
if [ "${LINK_CMD}" = "${LINK_STEP2}" ]; then
    echo "run.sh: FATAL: link-command -o cfg1_out substitution did not match" >&2
    echo "  ninja command was: ${LINK_STEP2}" >&2
    exit 2
fi
bash -c "${LINK_CMD}" > "${COMPILE_DIR}/link.log" 2>&1
LINK_RC=$?
if [ "${LINK_RC}" != 0 ] || [ ! -f "${COMPILE_DIR}/cfg1_out" ]; then
    echo "run.sh: linking cfg1_out failed (rc=${LINK_RC})" >&2
    cat "${COMPILE_DIR}/link.log" >&2
    exit 2
fi

NM_RAW="$(ninja -C "${BUILD_DIR}" -t commands generated/cfg1_out.syms 2>/dev/null | grep 'nm_to_file.cmake')"
if [ -z "${NM_RAW}" ]; then
    echo "run.sh: could not extract nm command" >&2
    exit 2
fi
NM_STEP1="$(printf '%s\n' "${NM_RAW}" | sed -E "s#-DELF=[^ ]*/cfg1_out #-DELF=${COMPILE_DIR}/cfg1_out #")"
if [ "${NM_STEP1}" = "${NM_RAW}" ]; then
    echo "run.sh: FATAL: nm-command -DELF substitution did not match" >&2
    echo "  ninja command was: ${NM_RAW}" >&2
    exit 2
fi
NM_STEP2="$(printf '%s\n' "${NM_STEP1}" | sed -E "s#-DOUT=[^ ]*generated/cfg1_out\.syms#-DOUT=${COMPILE_DIR}/cfg1_out.syms#g")"
if [ "${NM_STEP2}" = "${NM_STEP1}" ]; then
    echo "run.sh: FATAL: nm-command -DOUT substitution did not match" >&2
    echo "  ninja command was: ${NM_STEP1}" >&2
    exit 2
fi
NM_CMD="$(printf '%s\n' "${NM_STEP2}" | sed -E "s#-DSYMS=[^ ]*generated/cfg1_out\.syms#-DSYMS=${COMPILE_DIR}/cfg1_out.syms#g")"
if [ "${NM_CMD}" = "${NM_STEP2}" ]; then
    echo "run.sh: FATAL: nm-command -DSYMS substitution did not match" >&2
    echo "  ninja command was: ${NM_STEP2}" >&2
    exit 2
fi
bash -c "${NM_CMD}" > "${COMPILE_DIR}/nm.log" 2>&1
NM_RC=$?

OBJCOPY_RAW="$(ninja -C "${BUILD_DIR}" -t commands generated/cfg1_out.srec 2>/dev/null | grep -- '-O srec')"
if [ -z "${OBJCOPY_RAW}" ]; then
    echo "run.sh: could not extract objcopy command" >&2
    exit 2
fi
OBJCOPY_STEP1="$(printf '%s\n' "${OBJCOPY_RAW}" | sed -E "s#[^ ]*/cfg1_out #${COMPILE_DIR}/cfg1_out #")"
if [ "${OBJCOPY_STEP1}" = "${OBJCOPY_RAW}" ]; then
    echo "run.sh: FATAL: objcopy-command cfg1_out (ELF input) substitution did not match" >&2
    echo "  ninja command was: ${OBJCOPY_RAW}" >&2
    exit 2
fi
OBJCOPY_CMD="$(printf '%s\n' "${OBJCOPY_STEP1}" | sed -E "s#[^ ]*generated/cfg1_out\.srec#${COMPILE_DIR}/cfg1_out.srec#")"
if [ "${OBJCOPY_CMD}" = "${OBJCOPY_STEP1}" ]; then
    echo "run.sh: FATAL: objcopy-command cfg1_out.srec (output) substitution did not match" >&2
    echo "  ninja command was: ${OBJCOPY_STEP1}" >&2
    exit 2
fi
bash -c "${OBJCOPY_CMD}" > "${COMPILE_DIR}/objcopy.log" 2>&1
OBJCOPY_RC=$?

if [ "${NM_RC}" != 0 ] || [ ! -s "${COMPILE_DIR}/cfg1_out.syms" ]; then
    echo "run.sh: nm/magic-number check failed (rc=${NM_RC})" >&2
    cat "${COMPILE_DIR}/nm.log" >&2
    exit 2
fi
if [ "${OBJCOPY_RC}" != 0 ] || [ ! -s "${COMPILE_DIR}/cfg1_out.srec" ]; then
    echo "run.sh: objcopy failed (rc=${OBJCOPY_RC})" >&2
    cat "${COMPILE_DIR}/objcopy.log" >&2
    exit 2
fi
echo "== compiled fresh cfg1_out ELF for this .cfg (.syms/.srec regenerated, not reused) =="

cp "${COMPILE_DIR}/cfg1_out.syms" "${RUBY_DIR}/cfg1_out.syms"
cp "${COMPILE_DIR}/cfg1_out.srec" "${RUBY_DIR}/cfg1_out.srec"
cp "${COMPILE_DIR}/cfg1_out.syms" "${PY_DIR}/cfg1_out.syms"
cp "${COMPILE_DIR}/cfg1_out.srec" "${PY_DIR}/cfg1_out.srec"

#
#  == Stage 3: pass2（kernel_cfg.c/h生成。kernel/interrupt.pyのE_RSATR等の
#     クラス×プロセッサ整合性チェックはここで実行される）==
#
KCFG_RAW="$(ninja -C "${BUILD_DIR}" -t commands generated/kernel_cfg.timestamp 2>/dev/null | grep 'cfg_py/cfg\.py' | tail -1)"
if [ -z "${KCFG_RAW}" ]; then
    echo "run.sh: could not extract kernel_cfg pass2 command" >&2
    exit 2
fi
KCFG_ARGS="$(echo "${KCFG_RAW}" | sed -E 's#^.*cfg_py/cfg\.py##; s# && .*$##')"

#  ★計画B Task 11（cutover）以降、KCFG_ARGS（実ninjaコマンドから抽出）は
#    .py パスを含む（target.cmake/arch.cmake が .py を積むようになった
#    ため）。python 側はそのまま実行し、ruby 側は逆に .py→.trb へ変換
#    してから pristine cfg/cfg.rb（オラクル）へ渡す（cutover前はこの逆
#    だった。tools/cfg_equivalence.sh の run_cmd と同じ理由）。
run_pass2() {
    local kind="$1" outdir="$2"
    if [ "${kind}" = "ruby" ]; then
        local rbargs
        rbargs="$(echo "${KCFG_ARGS}" | sed -E 's#(-[TC] [^ ]+)\.py#\1.trb#g')"
        ( cd "${outdir}" && ruby "${ROOT}/cfg/cfg.rb" ${rbargs} ) \
            > "${outdir}/kernel_cfg.log" 2>&1
    else
        ( cd "${outdir}" && python3 -B "${ROOT}/cfg_py/cfg.py" ${KCFG_ARGS} ) \
            > "${outdir}/kernel_cfg.log" 2>&1
    fi
}
run_pass2 ruby   "${RUBY_DIR}"; RUBY_RC=$?
run_pass2 python "${PY_DIR}";   PY_RC=$?

echo "[ruby] rc=${RUBY_RC}"
echo "[python] rc=${PY_RC}"

RESULT=0

#  ★修正2: rc一致＋expected-substring存在だけの判定では、片方のエンジンが
#  途中でtracebackを吐いて異常終了していても、たまたま両者のrcが一致し
#  （例：期待した診断が原因コードで先に出力された「後」にPython側が別の
#  未処理例外でクラッシュし、Pythonのデフォルト終了コードが偶然Rubyの意図
#  した終了コードと一致する等）、かつEXPECTがクラッシュ前に出力済みの診断
#  文言に一致してしまうと、RESULT=OKになりうる（実際に踏んだ：
#  kernel/interrupt.py の KeyError で再現）。rc/文字列一致とは独立に、ログに
#  Pythonのtracebackまたは Rubyのbacktraceが含まれていないかを必ず検査する。
check_no_crash() {
    local label="$1" logfile="$2"
    [ -f "${logfile}" ] || return 0
    if grep -q '^Traceback (most recent call last):' "${logfile}"; then
        echo "run.sh: [FAIL] ${label}: unhandled Python traceback detected" \
             "(engine crashed instead of reporting a clean diagnostic)"
        tail -15 "${logfile}"
        return 1
    fi
    if grep -qE '\.rb:[0-9]+:in `' "${logfile}"; then
        echo "run.sh: [FAIL] ${label}: unhandled Ruby backtrace detected" \
             "(engine crashed instead of reporting a clean diagnostic)"
        tail -15 "${logfile}"
        return 1
    fi
    return 0
}
for _log in \
    "ruby pass1:${RUBY_DIR}/pass1.log" \
    "python pass1:${PY_DIR}/pass1.log" \
    "ruby kernel_cfg:${RUBY_DIR}/kernel_cfg.log" \
    "python kernel_cfg:${PY_DIR}/kernel_cfg.log"
do
    _label="${_log%%:*}"
    _file="${_log#*:}"
    if ! check_no_crash "${_label}" "${_file}"; then
        RESULT=1
    fi
done

if [ "${RUBY_RC}" != "${PY_RC}" ]; then
    echo "run.sh: [MISMATCH] ruby rc=${RUBY_RC} != python rc=${PY_RC}"
    RESULT=1
fi

if [ -n "${EXPECT}" ]; then
    for kind in ruby python; do
        dir="${RUBY_DIR}"
        [ "${kind}" = "python" ] && dir="${PY_DIR}"
        if grep -qF "${EXPECT}" "${dir}/kernel_cfg.log"; then
            echo "[${kind}] expected substring found: OK"
        else
            echo "[${kind}] expected substring NOT found: FAIL"
            tail -10 "${dir}/kernel_cfg.log"
            RESULT=1
        fi
    done
    # ruby/pythonのメッセージ文言そのものが一致するかも記録する
    # （一致は保証としては求めないが、実測結果を可視化する）
    RUBY_MSG="$(grep -F "${EXPECT}" "${RUBY_DIR}/kernel_cfg.log" | head -1)"
    PY_MSG="$(grep -F "${EXPECT}" "${PY_DIR}/kernel_cfg.log" | head -1)"
    if [ -n "${RUBY_MSG}" ] && [ -n "${PY_MSG}" ]; then
        if [ "${RUBY_MSG}" = "${PY_MSG}" ]; then
            echo "run.sh: message text: IDENTICAL"
        else
            echo "run.sh: message text: DIFFERS (not treated as failure; see logs)"
            echo "  ruby:   ${RUBY_MSG}"
            echo "  python: ${PY_MSG}"
        fi
    fi
else
    # EXPECT未指定 = 正常系（negative control）: 両エンジンともエラー無し
    # であることを期待する。
    if [ "${RUBY_RC}" != 0 ] || [ "${PY_RC}" != 0 ]; then
        echo "run.sh: [UNEXPECTED] no EXPECT substring given (normal-case control)" \
             "but rc != 0 (ruby=${RUBY_RC}, python=${PY_RC})"
        RESULT=1
    fi
fi

if [ "${RESULT}" = "0" ]; then
    echo "run.sh: RESULT = OK"
else
    echo "run.sh: RESULT = FAIL"
fi
echo "run.sh: work dir preserved at ${WORK} for inspection"
exit "${RESULT}"
