#!/bin/bash
#
#  KRIA(Kria) 実機 TTSP3 テストスイート実行スクリプト
#
#  使い方:
#    ./run_ttsp_kria.sh [-p PRC_NUM] [-d BUILDDIR] <test1> [test2 ...]
#  例:
#    ./run_ttsp_kria.sh -p 2 task1 flg1 mtskman1
#    ./run_ttsp_kria.sh -p 2 cpuexc2 cpuexc3 ... (MP含む。MP系はPRC_NUM=2必須)
#
#  前提:
#    - KRIAが電源ON、QSPIブートでU-Bootまで起動済み(DDR初期化済み)。
#    - Vitis(xsct,hw_server)とaarch64-none-elf gccにPATHが通せること。
#    - 本スクリプトと同じディレクトリに jtag_a53_kria_attach.tcl があること。
#
#  重要(Kria固有・ハマりどころ): TTSP3_HOWTO.md を必ず参照。
#    - rst -system / reset_apu は使用禁止(QSPI再ブート&DDR破壊)。本スクリプトは
#      使用コアのみ rst -processor し、FSBL初期化済みDDRへ dow する。
#    - コンソールUARTはデバイスノード番号が変動するため serial+intf で自動検出。
#    - hw_serverは1つ常駐させ各テストはattach(再起動の競合でNOOUTになるため)。
#
set -u

# ---- 設定 ----
VITIS_BIN="${VITIS_BIN:-/home/honda/tools/amd/2025.2.1/Vitis/bin}"
GCC_BIN="${GCC_BIN:-/usr/local/tools/arm-gnu-toolchain-14.3.rel1-x86_64-aarch64-none-elf/bin}"
HWS="$VITIS_BIN/hw_server"
XSCT="$VITIS_BIN/xsct -nodisp"                   # 2025.2.1 は -nodisp 必須(無いとXvfbエラー)
HERE="$(cd "$(dirname "$0")" && pwd)"
JTAG="$HERE/jtag_a53_kria_attach.tcl"
# fmp3_trunk のルート(本スクリプトは target/kria_arm64_gcc/tools/jtag にある)
SRCDIR="$(cd "$HERE/../../../.." && pwd)"
EXEC="$SRCDIR/test/testexec.rb"
CON_INTF="01"                                    # コンソール=PS UART1 = FTDI interface 01
export PATH="$GCC_BIN:$VITIS_BIN:$PATH"

PRC_NUM=2
BUILDDIR="build_kria_ttsp"
BOARD="${BOARD:-}"                               # 空なら carrier card から自動判別
while getopts "p:d:B:" o; do
  case "$o" in
    p) PRC_NUM="$OPTARG" ;;
    d) BUILDDIR="$OPTARG" ;;
    B) BOARD="$OPTARG" ;;
    *) echo "usage: $0 [-p PRC_NUM] [-d BUILDDIR] [-B kr260|kv260|kd240] <test...>"; exit 2 ;;
  esac
done
shift $((OPTIND-1))
TESTS="$*"
if [ -z "$TESTS" ]; then echo "テスト名を1つ以上指定してください"; exit 2; fi

# ---- 接続中の Kria ボードと コンソールUART を carrier card から自動判別 ----
#   KR_Carrier_Card=kr260 / ML_Carrier_Card=kv260 / KD_Carrier_Card=kd240
CON=""
for dev in /dev/ttyUSB*; do
  [ -e "$dev" ] || continue
  intf=$(udevadm info -q property -n "$dev" 2>/dev/null | sed -n 's/^ID_USB_INTERFACE_NUM=//p')
  [ "$intf" = "$CON_INTF" ] || continue
  model=$(udevadm info -q property -n "$dev" 2>/dev/null | sed -n 's/^ID_MODEL=//p')
  case "$model" in
    KR_Carrier_Card) det_board=kr260 ;;
    ML_Carrier_Card) det_board=kv260 ;;
    KD_Carrier_Card) det_board=kd240 ;;
    *) continue ;;
  esac
  CON="$dev"
  [ -z "$BOARD" ] && BOARD="$det_board"
  break
done
if [ -z "$CON" ]; then
  echo "ERROR: Kria コンソールUART(carrier card, intf=$CON_INTF)が見つかりません。"
  echo "       USBケーブル接続/ボード電源を確認してください(lsusbに 0403:6011 が必要)。"
  exit 1
fi
case "$BOARD" in kr260|kv260|kd240) ;; *) echo "ERROR: BOARD不正($BOARD)"; exit 2 ;; esac
export BOARD                                     # testexec→make がバナー名に使用
echo "[run_ttsp] board=$BOARD console=$CON PRC_NUM=$PRC_NUM builddir=$BUILDDIR"

# ---- hw_server を常駐(1つだけ) ----
if ! (ss -ltn 2>/dev/null | grep -q ':3121 '); then
  echo "[run_ttsp] hw_server を起動します"
  "$HWS" -stcp::3121 >/tmp/kria_hwserver.log 2>&1 &
  sleep 4
fi

# ---- ビルド準備 ----
mkdir -p "$BUILDDIR"; cd "$BUILDDIR" || exit 1
cat > TARGET_OPTIONS <<OPT
-T kria_arm64_gcc -w -S "test_svc.o syslog.o banner.o serial.o serial_cfg.o logtask.o chip_serial.o xuartps.o" -b "-Wl,--start-group -lc -lgcc -lnosys -Wl,--end-group" PRC_NUM=$PRC_NUM
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
  [ -f "$ELF" ] || ruby "$EXEC" build "$t" >/tmp/kria_build_$t.log 2>&1
  if [ ! -f "$ELF" ]; then echo "[$n] $t: NO_ELF (build失敗 /tmp/kria_build_$t.log)"; other=$((other+1)); continue; fi
  # 時間のかかる時間系テストはキャプチャを延長
  cap=15; case "$t" in cyclic1|malarm1|tmevt*|hrt1|dlynse|mstress1) cap=20;; esac
  LOG="/tmp/kria_run_$t.log"; : > "$LOG"
  # 【重要】cat の安全timeoutは大きく(dow転送が遅い回でも con 後 cap 秒を確実に捕捉)
  timeout 180 cat "$CON" > "$LOG" 2>/dev/null & CATPID=$!
  sleep 0.5
  timeout 90 $XSCT "$JTAG" "$(pwd)/$ELF" 0x0 "$PRC_NUM" >/tmp/kria_xsct_$t.log 2>&1
  sleep "$cap"
  kill "$CATPID" 2>/dev/null; wait "$CATPID" 2>/dev/null
  r=$(judge "$LOG")
  echo "[$n] $t: $r"
  case "$r" in PASS) pass=$((pass+1));; DONE) done_=$((done_+1));; SKIP) skip=$((skip+1));; FAIL*) fail=$((fail+1));; *) other=$((other+1));; esac
  sleep 1
done
echo "=== SUMMARY(KRIA HW PRC_NUM=$PRC_NUM): total=$n PASS=$pass DONE=$done_ SKIP=$skip FAIL=$fail OTHER=$other ==="
# 注: 長時間の連続実行(>15程度)でwedge/DDR異常が出たら、TTSP3_HOWTO.mdの
#     「ボード復旧」に従い con で再ブート、または電源再投入後に小バッチで再実行。
