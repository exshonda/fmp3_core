#!/bin/bash
#
#  PolarFire SoC Kit (Icicle/Discovery) 実機ボードランナー(SoftConsole openocd+gdb方式)。
#  test/run_hw_suite.sh の BOARD_RUNNER契約を実装する:
#    run_test.sh <SRCDIR> <ELF_PATH> <TIMEOUT_SEC> <PRC_NUM>
#  コンソール捕捉テキストを標準出力にそのまま出す(判定は呼び出し側が行う)。
#
#  前提: SoftConsole-v2022.2-RISC-V-747 がインストール済み。詳細は
#  target/polarfire_soc_kit_gcc/TTSP3_HOWTO.md §2 を参照。
#  ロードが遅い(7〜9KB/s)ため TIMEOUT_SEC は40秒以上を推奨。
#
set -u
SRCDIR="$1"; ELF="$2"; TMO="${3:-45}"; PRC_NUM="${4:-1}"
SC="${SOFTCONSOLE_DIR:-/home/honda/Microchip/SoftConsole-v2022.2-RISC-V-747}"
OOCD="$SC/openocd/bin/openocd"
GDB="$SC/riscv-unknown-elf-gcc/bin/riscv64-unknown-elf-gdb"

[ -x "$OOCD" ] || { echo "openocd無し: $OOCD" >&2; exit 2; }
[ -x "$GDB" ] || { echo "gdb無し: $GDB" >&2; exit 2; }

# コンソール自動検出:
#  - Discovery Kit: FlashPro5(FT4232, 1514:2008)の if1 = MMUART1 (udevルール適用済み前提)
#  - Icicle Kit   : CP2108 の 1st port = MMUART0
CONSOLE=""
for f in /dev/serial/by-id/usb-Microsemi_Embedded_FlashPro5_*-if01-port0; do
  [ -e "$f" ] && { CONSOLE="$f"; break; }
done
if [ -z "$CONSOLE" ]; then
  for f in /dev/serial/by-id/usb-Silicon_Labs_CP2108*; do
    [ -e "$f" ] && { CONSOLE="$f"; break; }
  done
fi
[ -n "$CONSOLE" ] || { echo "PolarFireコンソールデバイス未検出" >&2; exit 2; }

# openocd(GDBサーバ)が未起動なら常駐起動(既存があれば再利用)
if ! (ss -ltn 2>/dev/null | grep -q ':3333 '); then
  ( export LD_LIBRARY_PATH="$SC/openocd/bin:$SC/fpServer/lib"
    "$OOCD" -c "set DEVICE MPFS" -f board/microsemi-riscv.cfg > /tmp/polarfire_oocd.log 2>&1 & )
  for i in $(seq 1 20); do
    ss -ltn 2>/dev/null | grep -q ':3333 ' && break
    sleep 0.5
  done
fi

RUNGDB=$(mktemp)
cat > "$RUNGDB" <<EOF
set pagination off
set confirm off
set mem inaccessible-by-default off
target extended-remote localhost:3333
set architecture riscv:rv64
file $ELF
monitor reset init
load
thread apply all set \$pc=_start
continue
EOF

stty -F "$CONSOLE" 115200 cs8 -cstopb -parenb -echo raw 2>/dev/null
LOG=$(mktemp)
timeout "$TMO" cat "$CONSOLE" > "$LOG" 2>/dev/null &
CAP=$!
sleep 0.3
timeout 60 "$GDB" -q -batch -x "$RUNGDB" > /tmp/polarfire_gdb_last.log 2>&1
wait $CAP 2>/dev/null
cat "$LOG"
rm -f "$LOG" "$RUNGDB"
