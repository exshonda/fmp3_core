#
# KRIA 常駐hw_server接続版 起動スクリプト(全コア停止→使用ncoresコア起動)
#   引数: <load_obj> <entry> <ncores>
# 【重要】Kriaでは rst -system / reset_apu はDDRコントローラを乱すため使用禁止。
# DDRはQSPIブートのFSBLが初期化済(boot完走させておく)。
# ★Kria実機メモ: OS がブートしていると一部 A53 コアが UART1 を使用中・他コアは省電力
#   オフ(No Power)になりうる。使用コアだけ stop すると残りの OS コアが UART1 を汚すため、
#   【全A53コアをまず stop】してから、使用する ncores 個のみ rst -processor で起こす。
#   U-Boot 止め運用なら全コア quiescent なので全停止しても無害。
#
set load_obj [lindex $argv 0]
set entry    [lindex $argv 1]
set ncores   [lindex $argv 2]
if { $ncores eq "" } { set ncores 1 }
connect -url tcp:127.0.0.1:3121
after 200
# まず全A53コアを停止(OS等がUART1を汚すのを防ぐ。No Powerコアはcatchで無視)
for { set c 3 } { $c >= 0 } { incr c -1 } {
    if {![catch {targets -set -nocase -filter "name =~ \"*A53*$c\"" -index 1}]} {
        catch {stop}
    }
}
# 使用コアのみコアリセット(DDR保持。省電力オフのコアの起床も兼ねる)
for { set c [expr {$ncores - 1}] } { $c >= 0 } { incr c -1 } {
    targets -set -nocase -filter "name =~ \"*A53*$c\"" -index 1
    catch {rst -processor}
}
# ELFをDDRへロード(A53#0)
targets -set -nocase -filter {name =~"*A53*0"} -index 1
dow $load_obj
for { set c 1 } { $c < $ncores } { incr c } {
    targets -set -nocase -filter "name =~ \"*A53*$c\"" -index 1
    memmap -file $load_obj
}
configparams force-mem-access 0
# 使用コアのみ secondary→master の順に起動
for { set c [expr {$ncores - 1}] } { $c >= 0 } { incr c -1 } {
    targets -set -nocase -filter "name =~ \"*A53*$c\"" -index 1
    rwr pc $entry
    con
}
after 100
disconnect
exit
