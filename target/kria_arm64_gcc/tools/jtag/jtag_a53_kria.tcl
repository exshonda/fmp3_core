#
# KRIA(Kria) 実機 A53 起動シーケンス (xsct/hw_server)
#   引数: <load_obj(ELF)> <entry> <ncores>
#
# KRIAはJTAGブートモードを持たず、電源投入でSOMのQSPIブートFW(FSBL)が
# PS初期化(DDR/クロック/UART)を完了する。よってJTAGからのpsu_initは不要
# (かつ SERDES PLLロック待ちでtimeoutするため避ける)。
# FSBL初期化済みのPSに対し、使用するA53コアのみ rst -processor で
# クリーンなリセット状態(MMU off)にし、ELFをDDRへdow、PC=entryで con。
#
set load_obj [lindex $argv 0]
set entry    [lindex $argv 1]
set ncores   [lindex $argv 2]
if { $ncores eq "" } { set ncores 1 }

connect
after 200

# 使用コアを高index→0 の順にリセット(rst -processor: 当該コアのみ)。
for { set c [expr {$ncores - 1}] } { $c >= 0 } { incr c -1 } {
    targets -set -nocase -filter "name =~ \"*A53*$c\"" -index 1
    catch {stop}
    rst -processor
}

# A53#0 に ELF をダウンロード(DDRはFSBL初期化済)
targets -set -nocase -filter {name =~"*A53*0"} -index 1
dow $load_obj

# 残コアにシンボル用 memmap
for { set c 1 } { $c < $ncores } { incr c } {
    targets -set -nocase -filter "name =~ \"*A53*$c\"" -index 1
    memmap -file $load_obj
}
configparams force-mem-access 0

# secondary→master(A53#0) の順に PC=entry, con
for { set c [expr {$ncores - 1}] } { $c >= 0 } { incr c -1 } {
    targets -set -nocase -filter "name =~ \"*A53*$c\"" -index 1
    rwr pc $entry
    con
}
after 100
disconnect
exit
