# -*- coding: utf-8 -*-
#
#		パス2の生成スクリプトのターゲット依存部（RP2350 / RaspberryPi Pico 2用）
#
#  ARM-M のベクタテーブル・例外/割込みハンドラテーブル・割込み属性ビット
#  パターンを生成する．ASP3 の core_kernel.py（静的ベクタテーブル）を，FMP3
#  のプロセッサ単位（per-PRC）の構成へ拡張したものである．本ターゲットは
#  最大 2 プロセッサ（TNUM_PRCID=1 または 2）．
#
#  $Id: target_kernel.py (converted from target_kernel.trb by Claude Code Sonnet 5) $
#

#
#  有効な割込み番号，割込みハンドラ番号
#  例外番号 15（SysTick）以降が割込みである．
#
INHNO_VALID = {}
INTNO_VALID = {}
for prcid in range(1, TNUM_PRCID + 1):
    INHNO_VALID[prcid] = []
    INTNO_VALID[prcid] = []
    for intno in range(15, TMAX_INTNO + 1):
        INHNO_VALID[prcid].append((prcid << 16) | intno)
        INTNO_VALID[prcid].append((prcid << 16) | intno)

#
#  有効なCPU例外番号
#  2:NMI, 3:HardFault, 4:MemManage, 5:BusFault, 6:UsageFault, 12:DebugMonitor
#
EXCNO_VALID = {}
excno_list = [2, 3, 4, 5, 6, 12]
for prcid in range(1, TNUM_PRCID + 1):
    EXCNO_VALID[prcid] = []
    for excno in excno_list:
        EXCNO_VALID[prcid].append((prcid << 16) | excno)

#
#  CRE_ISRで使用できる割込み番号とそれに対応する割込みハンドラ番号
#
INTNO_CREISR_VALID = INTNO_VALID
INHNO_CREISR_VALID = INHNO_VALID

#
#  DEF_INH／DEF_EXCで使用できる割込みハンドラ番号／CPU例外ハンドラ番号
#
INHNO_DEFINH_VALID = INHNO_VALID
EXCNO_DEFEXC_VALID = EXCNO_VALID

#
#  CFG_INTで使用できる割込み番号と割込み優先度
#  最大優先度はBASEPRIレジスタでマスクできない優先度（内部優先度'0'）．
#  そのため，カーネル管理外の割込みでのみ指定可能．
#
INTNO_CFGINT_VALID = INTNO_VALID
INTPRI_CFGINT_VALID = list(range(-(1 << TBITW_IPRI), 0))

#
#  生成スクリプトのチップ依存部（チップ→コア→kernel.py）
#
IncludeTrb("chip_kernel.py")

#
#  ターゲット依存の定義
#
kernelCfgC.append("/*\n *  Target-dependent Definitions (ARM-M / Musca-B1)\n */\n")

#
#  ベクタテーブル（プロセッサ単位）と例外/割込みハンドラテーブル
#
for prcid in range(1, TNUM_PRCID + 1):
    #
    #  ベクタテーブル
    #
    #  ARMv8-M の VTOR は，ベクタテーブルのエントリ数を格納できる最小の
    #  2 のべき乗のサイズ境界にアライメントされていなければならない．
    #  TMAX_INTNO+1 エントリ（各 4byte）を格納できる境界を求めて指定する．
    #  プロセッサ単位にテーブルを並べると 2 番目以降が境界を割るため，全
    #  テーブルに明示的なアライメントを与える（単一プロセッサでは無害）．
    #
    vecttbl_bytes = (TMAX_INTNO + 1) * 4
    vecttbl_align = 32
    while vecttbl_align < vecttbl_bytes:
        vecttbl_align <<= 1
    kernelCfgC.add(f'__attribute__ ((section(".vector"),aligned({vecttbl_align})))')
    kernelCfgC.add(f"const FP _kernel_vector_table_prc{prcid}[] = {{")
    kernelCfgC.add("\t(FP)(TOPPERS_ISTKPT(_kernel_istack_prc" f"{prcid}, "
                   "ROUND_STK_T(DEFAULT_ISTKSZ))),   /* 0 initial SP */")
    kernelCfgC.add("\t(FP)_kernel_start,                 /* 1 reset handler */")
    for excno in range(2, 15):
        if excno in (8, 9, 10, 13):
            kernelCfgC.add(f"\t(FP)({GenResVectVal(excno)}),")
        elif excno == 11:
            kernelCfgC.add("\t(FP)(_kernel_svc_handler),       /* 11 SVCall handler */")
        elif excno == 14:
            kernelCfgC.add("\t(FP)(_kernel_pendsv_handler),    /* 14 PendSV handler */")
        else:
            excnoVal = (prcid << 16) | excno
            if (excnoVal in cfgData["DEF_EXC"] and
                    (cfgData["DEF_EXC"][excnoVal]["excatr"] & TA_DIRECT) != 0):
                kernelCfgC.add(f"\t(FP)({cfgData['DEF_EXC'][excnoVal]['exchdr']}), /* {excno} */")
            else:
                kernelCfgC.add(f"\t(FP)(_kernel_core_exc_entry), /* {excno} */")
    for inhnoVal in INHNO_VALID[prcid]:
        intno = inhnoVal & 0xffff
        if (inhnoVal in cfgData["DEF_INH"] and
                (cfgData["DEF_INH"][inhnoVal]["inhatr"] & TA_NONKERNEL) != 0):
            kernelCfgC.add(f"\t(FP)({cfgData['DEF_INH'][inhnoVal]['inthdr']}), /* {intno} */")
        else:
            kernelCfgC.add(f"\t(FP)(_kernel_core_int_entry), /* {intno} */")
    kernelCfgC.add2("};")

    #
    #  例外/割込みハンドラテーブル（例外番号 0〜14 ＋ 割込み）
    #
    kernelCfgC.add(f"const FP _kernel_exc_tbl_prc{prcid}[] = {{")
    for excno in range(0, 15):
        excnoVal = (prcid << 16) | excno
        if excnoVal in cfgData["DEF_EXC"]:
            kernelCfgC.add(f"\t(FP)({cfgData['DEF_EXC'][excnoVal]['exchdr']}), /* {excno} */")
        else:
            kernelCfgC.add(f"\t(FP)(_kernel_default_exc_handler), /* {excno} */")
    for inhnoVal in INHNO_VALID[prcid]:
        intno = inhnoVal & 0xffff
        if inhnoVal in cfgData["DEF_INH"]:
            kernelCfgC.add(f"\t(FP)({cfgData['DEF_INH'][inhnoVal]['inthdr']}), /* {intno} */")
        else:
            kernelCfgC.add(f"\t(FP)(_kernel_default_int_handler), /* {intno} */")
    kernelCfgC.add2("};")

#
#  ベクタテーブルのポインタテーブル（core_initialize が参照）
#
kernelCfgC.add("const FP* const _kernel_p_vector_table[TNUM_PRCID] = {")
for prcid in range(1, TNUM_PRCID + 1):
    if prcid > 1:
        kernelCfgC.add(",")
    kernelCfgC.append(f"\t_kernel_vector_table_prc{prcid}")
kernelCfgC.add()
kernelCfgC.add2("};")

#
#  例外/割込みハンドラテーブルのポインタテーブル（core_int_entry が参照）
#
kernelCfgC.add("const FP* const _kernel_p_exc_tbl[TNUM_PRCID] = {")
for prcid in range(1, TNUM_PRCID + 1):
    if prcid > 1:
        kernelCfgC.add(",")
    kernelCfgC.append(f"\t_kernel_exc_tbl_prc{prcid}")
kernelCfgC.add()
kernelCfgC.add2("};")

#
#  割込み属性が設定されているかを示すビットパターン（check_intno_cfg が参照）
#
if (TMAX_INTNO & 0x0f) == 0x00:
    bitpat_cfgint_num = (TMAX_INTNO >> 4)
else:
    bitpat_cfgint_num = (TMAX_INTNO >> 4) + 1

for prcid in range(1, TNUM_PRCID + 1):
    kernelCfgC.add(f"const uint32_t _kernel_bitpat_cfgint_prc{prcid}[{bitpat_cfgint_num}] = {{")
    for num in range(0, bitpat_cfgint_num):
        bitpat = 0
        for intno in range(num * 32, num * 32 + 32):
            intnoVal = (prcid << 16) | intno
            if intnoVal in cfgData["DEF_INH"]:
                bitpat |= (1 << (intno & 0x1f))
        kernelCfgC.add(f"\tUINT32_C(0x{bitpat:08x}),")
    kernelCfgC.add2("};")

kernelCfgC.add("const uint32_t *_kernel_p_bitpat_cfgint[TNUM_PRCID] = {")
for prcid in range(1, TNUM_PRCID + 1):
    if prcid > 1:
        kernelCfgC.add(",")
    kernelCfgC.append(f"\t_kernel_bitpat_cfgint_prc{prcid}")
kernelCfgC.add()
kernelCfgC.add2("};")
