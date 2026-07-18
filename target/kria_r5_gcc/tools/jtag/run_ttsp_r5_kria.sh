#!/bin/bash
#
#  KRIA R5（Cortex-R5F）実機 TTSP3 テストスイート実行スクリプト
#
#  使い方:
#    ./run_ttsp_r5_kria.sh [-p PRC_NUM] [-d BUILDDIR] <test1> [test2 ...]
#  例:
#    ./run_ttsp_r5_kria.sh -p 1 task1 sem1 flg1
#    ./run_ttsp_r5_kria.sh -p 2 mtskman1 mmutex1   (MP系は PRC_NUM=2)
#
#  前提:
#    - Kria が電源ON，QSPIブートで U-Boot まで起動済み（DDR初期化済み）．
#    - Vitis(xsct,hw_server) と arm-none-eabi gcc に PATH が通せること
#      （VITIS_BIN / GCC_BIN で上書き可）．
#    - 本スクリプトと同じディレクトリに jtag_r5_kria_attach.tcl があること．
#
#  ★★最重要（NOOUT の最頻原因）★★
#    起動する R5 コア数（ncores）は，必ずバイナリの TNUM_PRCID（=PRC_NUM）と
#    一致させること．barrier_sync は TNUM_PRCID 個の全プロセッサ到達を待つため，
#    コア数不足だとバナー無出力（NOOUT）になる．本スクリプトは ncores を
#    PRC_NUM に一致させて起動する（R5 既定 TNUM_PRCID=1）．
#
#  ★ R5 と A53(APU) は同じ PS UART1（carrier card intf=01）を共有する．A53側で
#    別カーネルが走っているとコンソールが混信するので，クリーンな U-Boot 起動
#    状態で使うこと．
#
set -u

# ---- 設定 ----
VITIS_BIN="${VITIS_BIN:-/usr/local/tools/Vitis/2024.2/bin}"
GCC_BIN="${GCC_BIN:-/usr/local/tools/arm-gnu-toolchain-14.3.rel1-x86_64-aarch64-none-elf/bin}"
HWS="$VITIS_BIN/hw_server"
XSCT="$VITIS_BIN/xsct"
HERE="$(cd "$(dirname "$0")" && pwd)"
JTAG="$HERE/jtag_r5_kria_attach.tcl"
# fmp3 ソースのルート（本スクリプトは target/kria_r5_gcc/tools/jtag にある）
SRCDIR="$(cd "$HERE/../../../.." && pwd)"
EXEC="$SRCDIR/test/testexec.rb"
CON_INTF="01"                                    # コンソール=PS UART1 = carrier card intf=01
export PATH="$GCC_BIN:$VITIS_BIN:$PATH"
export QEMU=false                                # ★実機100MHz化（必須）

PRC_NUM=1
BUILDDIR="build_kria_r5_ttsp"
while getopts "p:d:" o; do
  case "$o" in
    p) PRC_NUM="$OPTARG" ;;
    d) BUILDDIR="$OPTARG" ;;
    *) echo "usage: $0 [-p PRC_NUM] [-d BUILDDIR] <test...>"; exit 2 ;;
  esac
done
shift $((OPTIND-1))
TESTS="$*"
if [ -z "$TESTS" ]; then echo "テスト名を1つ以上指定してください"; exit 2; fi

# ---- 接続中の Kria carrier card から コンソールUART を自動判別（intf=01）----
CON=""
for dev in /dev/ttyUSB*; do
  [ -e "$dev" ] || continue
  intf=$(udevadm info -q property -n "$dev" 2>/dev/null | sed -n 's/^ID_USB_INTERFACE_NUM=//p')
  [ "$intf" = "$CON_INTF" ] || continue
  model=$(udevadm info -q property -n "$dev" 2>/dev/null | sed -n 's/^ID_MODEL=//p')
  case "$model" in KR_Carrier_Card|ML_Carrier_Card|KD_Carrier_Card) CON="$dev"; break ;; esac
done
if [ -z "$CON" ]; then
  echo "ERROR: Kria コンソールUART(carrier card, intf=$CON_INTF)が見つかりません"
  echo "       USBケーブル接続/ボード電源を確認してください(lsusbに 0403:6011 が必要)．"
  exit 1
fi
echo "[run_ttsp_r5] console=$CON PRC_NUM=$PRC_NUM builddir=$BUILDDIR"

# ---- hw_server を常駐（1つだけ）----
if ! (ss -ltn 2>/dev/null | grep -q ':3121 '); then
  echo "[run_ttsp_r5] hw_server を起動します"
  "$HWS" -stcp::3121 >/tmp/kria_hwserver.log 2>&1 &
  sleep 4
fi

# ---- ビルド準備 ----
mkdir -p "$BUILDDIR"; cd "$BUILDDIR" || exit 1
cat > TARGET_OPTIONS <<OPT
-T kria_r5_gcc -w -S "test_svc.o syslog.o banner.o serial.o serial_cfg.o chip_serial.o logtask.o xuartps.o" -b "-Wl,--start-group -lc -lgcc -lnosys -Wl,--end-group" PRC_NUM=$PRC_NUM
OPT
echo "true" > TARGET_RUN
stty -F "$CON" 115200 raw -echo -echoe -echok -echoctl -echoke -ixon 2>/dev/null

judge() {  # $1=ログ
  if   grep -aq "TTSP_RESULT: PASS" "$1"; then echo PASS
  elif grep -aq "TTSP_RESULT: DONE" "$1"; then echo DONE
  elif grep -aq "TTSP_RESULT: FAIL" "$1"; then echo FAIL
  elif grep -aq "Unexpected check point" "$1"; then echo "FAIL(unexpected)"
  elif grep -aq "This test program is not necessary" "$1"; then echo SKIP
  elif grep -aq "All check points passed" "$1"; then echo PASS
  elif grep -aq "Processor 1 start" "$1"; then echo "RUNNING/HANG"
  else echo NOOUT; fi
}

n=0; pass=0; done_=0; fail=0; skip=0; other=0
for t in $TESTS; do
  n=$((n+1)); T=$(echo "$t" | tr a-z A-Z); ELF="OBJ-$T/fmp"
  [ -f "$ELF" ] || ruby "$EXEC" build "$t" >/tmp/kria_r5_build_$t.log 2>&1
  if [ ! -f "$ELF" ]; then echo "[$n] $t: NO_ELF (build失敗 /tmp/kria_r5_build_$t.log)"; other=$((other+1)); continue; fi
  cap=15; case "$t" in cyclic1|malarm1|tmevt*|hrt1|dlynse|mstress1) cap=20;; esac
  LOG="/tmp/kria_r5_run_$t.log"; : > "$LOG"
  timeout 180 cat "$CON" > "$LOG" 2>/dev/null & CATPID=$!
  sleep 0.5
  # ★ncores = PRC_NUM（起動コア数を TNUM_PRCID に一致させる）
  timeout 90 "$XSCT" "$JTAG" "$(pwd)/$ELF" "$PRC_NUM" >/tmp/kria_r5_xsct_$t.log 2>&1
  sleep "$cap"
  kill "$CATPID" 2>/dev/null; wait "$CATPID" 2>/dev/null
  r=$(judge "$LOG")
  echo "[$n] $t: $r"
  case "$r" in PASS) pass=$((pass+1));; DONE) done_=$((done_+1));; SKIP) skip=$((skip+1));; FAIL*) fail=$((fail+1));; *) other=$((other+1));; esac
  sleep 1
done
echo "=== SUMMARY(KRIA R5 HW PRC_NUM=$PRC_NUM): total=$n PASS=$pass DONE=$done_ SKIP=$skip FAIL=$fail OTHER=$other ==="
