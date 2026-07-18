#
# KRIA(Kria) 実機 Cortex-R5F split 2コア 起動シーケンス
#   引数: <load_obj(ELF)> [zynqmp_utils.tcl]
#
# 【重要】KRIA(Kria)は rst -system でQSPI再ブート&DDR破壊が起きるため使用禁止。
# psu_init も不要(QSPIブートのFSBLがDDR/クロックを初期化済み)。
# FSBL初期化済みの状態に対し、RPUをsplitにしてR5を起動する:
#   enable_split_mode → clear_rpu_reset → 各R5に rst -processor + dow
#   (.vector→各コアTCM, .text→共有DDR, dowがentry設定) → R5#1,R5#0 を con。
#
set load_obj     [lindex $argv 0]
set zynqmp_utils [lindex $argv 1]
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

# 各R5をリセットしてELFをロード(.vector→TCM, .text→DDR(FSBL初期化済))
targets -set -nocase -filter {name =~"*R5*0"} -index 1
catch {stop}
rst -processor
dow $load_obj
targets -set -nocase -filter {name =~"*R5*1"} -index 1
catch {stop}
rst -processor
dow $load_obj
configparams force-mem-access 0

# R5#1 → R5#0 の順に起動
targets -set -nocase -filter {name =~"*R5*1"} -index 1
con
targets -set -nocase -filter {name =~"*R5*0"} -index 1
con
after 100
disconnect
exit
