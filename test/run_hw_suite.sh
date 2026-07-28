#!/bin/bash
#
#  実機(またはQEMU)でのテストプログラム一括ビルド・実行・判定 共通ハーネス
#
#  各ターゲット固有の「書き込み・リセット・コンソール捕捉」部分は
#  target/<TARGET>/tools/run_test.sh (ボードランナー) に切り出し、本スクリプトは
#  ビルド(testexec.rb呼び出し)と結果判定(TTSP_RESULT等の共通パターン)のみを担う。
#  これによりターゲット追加時の実装量とメンテナンスコストを下げる。
#
#  使い方:
#    run_hw_suite.sh <BUILD_DIR> <SRCDIR> <BOARD_RUNNER> <test1> [test2 ...]
#
#  BUILD_DIR    : TARGET_OPTIONS を配置済みのビルド作業ディレクトリ(OBJ-*が生成される)
#  SRCDIR       : テスト対象のソースツリー( .../fmp3_3.4 等、testexec.rbの親)
#  BOARD_RUNNER : 下記契約を満たす実行ファイル
#                   board_runner.sh <SRCDIR> <ELF_PATH> <TIMEOUT_SEC> <PRC_NUM>
#                 ボードへの書き込み・リセット・コンソール(またはセミホスティング)
#                 出力の捕捉を行い、捕捉したテキストを標準出力にそのまま出す。
#                 判定は行わない(本スクリプト側で共通判定する)。
#                 PRC_NUM は起動すべきコア数のヒント(未使用ならランナー側で無視してよい)。
#
#  環境変数 TIMEOUT_SEC, PRC_NUM で個々の値を上書き可能(未指定時 24 / 1)。
#
set -u
BUILD_DIR="$1"; SRCDIR="$2"; BOARD_RUNNER="$3"; shift 3
TESTS="$*"
TESTEXEC="$SRCDIR/test/testexec.rb"

[ -d "$BUILD_DIR" ] || { echo "BUILD_DIR無し: $BUILD_DIR" >&2; exit 2; }
[ -f "$TESTEXEC" ] || { echo "testexec.rb無し: $TESTEXEC" >&2; exit 2; }
[ -x "$BOARD_RUNNER" ] || { echo "BOARD_RUNNER無し/実行不可: $BOARD_RUNNER" >&2; exit 2; }

classify() {
  local log="$1"
  if grep -q 'TTSP_RESULT: *PASS' "$log" 2>/dev/null; then echo PASS
  elif grep -q 'TTSP_RESULT: *FAIL' "$log" 2>/dev/null; then echo FAIL
  elif grep -qiE 'Unexpected|assert|Unregistered Exception' "$log" 2>/dev/null; then echo FAIL
  elif grep -q 'All check points passed' "$log" 2>/dev/null; then echo PASS
  elif grep -qE 'TTSP_RESULT: *DONE|finished|finishes|not necessary' "$log" 2>/dev/null; then echo DONE
  elif [ -s "$log" ]; then echo HANG
  else echo NOOUT
  fi
}

RESULTS=""
for t in $TESTS; do
  TU=$(echo "$t" | tr 'a-z' 'A-Z')
  ( cd "$BUILD_DIR" && ruby "$TESTEXEC" build "$t" ) > "/tmp/hwsuite_build_${t}.log" 2>&1
  ELF="$BUILD_DIR/OBJ-$TU/fmp"
  if [ ! -x "$ELF" ]; then
    echo "BUILD_ERR $t"
    RESULTS="$RESULTS
BUILD_ERR $t"
    continue
  fi
  LOG="/tmp/hwsuite_run_${t}.log"
  "$BOARD_RUNNER" "$SRCDIR" "$ELF" "${TIMEOUT_SEC:-24}" "${PRC_NUM:-1}" > "$LOG" 2>/tmp/hwsuite_runner_${t}.err
  st=$(classify "$LOG")
  echo "$st $t"
  RESULTS="$RESULTS
$st $t"
  if [ "$st" = "FAIL" ]; then grep -iE 'Unexpected|assert|Unregistered Exception' "$LOG" | head -2 | sed 's/^/     /'; fi
  if [ "$st" = "HANG" ]; then tail -2 "$LOG" | sed 's/^/     /'; fi
done

echo
echo "================ SUMMARY ================"
for st in PASS DONE FAIL HANG NOOUT BUILD_ERR; do
  n=$(echo "$RESULTS" | grep -c "^$st ")
  echo "$st: $n"
done
echo "FAIL:"; echo "$RESULTS" | grep "^FAIL "
echo "HANG/NOOUT:"; echo "$RESULTS" | grep -E "^(HANG|NOOUT) "
echo "BUILD_ERR:"; echo "$RESULTS" | grep "^BUILD_ERR "
exit 0
