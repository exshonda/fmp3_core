#
# Global Variable
#
set psu_init     [lindex $argv 0]
set load_obj     [lindex $argv 1]
set zynqmp_utils [lindex $argv 2]
set entry        [lindex $argv 3]

connect
after 100

source  $zynqmp_utils

targets -set -nocase -filter {name =~"APU*"} -index 1
rst -system
after 2000
targets -set -nocase -filter {name =~"APU*"} -index 1
reset_apu

targets -set -nocase -filter {name =~"RPU*"} -index 1
clear_rpu_reset


targets -set -filter {level==0} -index 0

#fpga -file ./soc_fpga_top.bit

targets -set -nocase -filter {name =~"APU*" } -index 1

#loadhw -hw system.hdf -mem-ranges [list {0x80000000 0xbfffffff} {0x400000000 0x5ffffffff} {0x1000000000 0x7fffffffff}]
#configparams force-mem-access 1

targets -set -nocase -filter {name =~"APU*"} -index 1
source $psu_init
psu_init
#after 1000

psu_ps_pl_isolation_removal
#after 1000

psu_ps_pl_reset_config
catch {psu_protection}

targets -set -nocase -filter {name =~"*A53*0"} -index 1
dow $load_obj

targets -set -nocase -filter {name =~"*A53*1"} -index 1
memmap -file $load_obj

targets -set -nocase -filter {name =~"*A53*2"} -index 1
memmap -file $load_obj

targets -set -nocase -filter {name =~"*A53*3"} -index 1
memmap -file $load_obj

configparams force-mem-access 0

targets -set -nocase -filter {name =~"*A53*3"} -index 1
rwr pc $entry
con

targets -set -nocase -filter {name =~"*A53*2"} -index 1
rwr pc $entry
con

targets -set -nocase -filter {name =~"*A53*1"} -index 1
rwr pc $entry
con

targets -set -nocase -filter {name =~"*A53*0"} -index 1
con

