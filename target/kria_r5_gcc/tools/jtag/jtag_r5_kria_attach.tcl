#
# KRIA(Kria) 実機 Cortex-R5F 起動シーケンス（常駐 hw_server へ attach・ncores 可変）
#   引数: <load_obj(ELF)> <ncores> [zynqmp_utils.tcl]
#     ncores : 起動する R5 コア数（= バイナリの TNUM_PRCID）．1 又は 2．
#
# 【重要】KRIA(Kria)は rst -system でQSPI再ブート&DDR破壊が起きるため使用禁止。
# psu_init も不要(QSPIブートのFSBLがDDR/クロックを初期化済み)。
# FSBL初期化済みの状態に対し、RPUをsplitにして R5 を起動する:
#   enable_split_mode → clear_rpu_reset → 使用 ncores 個の R5 に rst -processor
#   + dow (.vector→各コアTCM, .text→共有DDR, dowがentry設定) → R5#(n-1)..#0 を con。
#
# ★★最重要（NOOUT の最頻原因）★★
#   ncores は必ずバイナリの TNUM_PRCID（=PRC_NUM, R5既定は 1）と一致させること。
#   barrier_sync は起動時に TNUM_PRCID 個の全プロセッサ到達を待つ（バナー出力より
#   前）。不一致だとバナー無出力(NOOUT)に見えるが，ボード/UART故障ではない。
#
set load_obj     [lindex $argv 0]
set ncores       [lindex $argv 1]
set zynqmp_utils [lindex $argv 2]
if { $ncores eq "" } { set ncores 1 }
if { $zynqmp_utils eq "" } {
    set zynqmp_utils /usr/local/tools/Vitis/2024.2/scripts/vitis/util/zynqmp_utils.tcl
}
connect -url tcp:127.0.0.1:3121
after 200
source $zynqmp_utils

# RPUをsplitモードへ(rst -system無し)。enable_split_mode内のmwrのため先にターゲット選択
targets -set -nocase -filter {name =~"APU*"} -index 1
enable_split_mode
targets -set -nocase -filter {name =~"RPU*"} -index 1
catch {clear_rpu_reset}

# 使用 ncores 個の R5 をリセットして ELF をロード(.vector→TCM, .text→DDR(FSBL初期化済))
for { set c 0 } { $c < $ncores } { incr c } {
    targets -set -nocase -filter "name =~ \"*R5*$c\"" -index 1
    catch {stop}
    rst -processor
    dow $load_obj
}
configparams force-mem-access 0

# R5#(ncores-1) → ... → R5#0 の順に起動（セカンダリ→マスタ）
for { set c [expr {$ncores - 1}] } { $c >= 0 } { incr c -1 } {
    targets -set -nocase -filter "name =~ \"*R5*$c\"" -index 1
    con
}
after 100
disconnect
exit
