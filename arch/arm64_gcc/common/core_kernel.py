# -*- coding: utf-8 -*-
#
#   TOPPERS/FMP Kernel
#       Toyohashi Open Platform for Embedded Real-Time Systems/
#       Flexible MultiProcessor Kernel
#
#   Copyright (C) 2015 by FUJI SOFT INCORPORATED, JAPAN
#   Copyright (C) 2015-2019 by Embedded and Real-Time Systems Laboratory
#               Graduate School of Information Science, Nagoya Univ., JAPAN
#
#   上記著作権者は，以下の(1)〜(4)の条件を満たす場合に限り，本ソフトウェ
#   ア（本ソフトウェアを改変したものを含む．以下同じ）を使用・複製・改
#   変・再配布（以下，利用と呼ぶ）することを無償で許諾する．
#   (1) 本ソフトウェアをソースコードの形で利用する場合には，上記の著作
#       権表示，この利用条件および下記の無保証規定が，そのままの形でソー
#       スコード中に含まれていること．
#   (2) 本ソフトウェアを，ライブラリ形式など，他のソフトウェア開発に使
#       用できる形で再配布する場合には，再配布に伴うドキュメント（利用
#       者マニュアルなど）に，上記の著作権表示，この利用条件および下記
#       の無保証規定を掲載すること．
#   (3) 本ソフトウェアを，機器に組み込むなど，他のソフトウェア開発に使
#       用できない形で再配布する場合には，次のいずれかの条件を満たすこ
#       と．
#     (a) 再配布に伴うドキュメント（利用者マニュアルなど）に，上記の著
#         作権表示，この利用条件および下記の無保証規定を掲載すること．
#     (b) 再配布の形態を，別に定める方法によって，TOPPERSプロジェクトに
#         報告すること．
#   (4) 本ソフトウェアの利用により直接的または間接的に生じるいかなる損
#       害からも，上記著作権者およびTOPPERSプロジェクトを免責すること．
#       また，本ソフトウェアのユーザまたはエンドユーザからのいかなる理
#       由に基づく請求からも，上記著作権者およびTOPPERSプロジェクトを
#       免責すること．
#
#   本ソフトウェアは，無保証で提供されているものである．上記著作権者お
#   よびTOPPERSプロジェクトは，本ソフトウェアに関して，特定の使用目的
#   に対する適合性も含めて，いかなる保証も行わない．また，本ソフトウェ
#   アの利用により直接的または間接的に生じたいかなる損害に関しても，そ
#   の責任を負わない．
#
#  $Id: core_kernel.py (converted from core_kernel.trb by Claude Code Sonnet 5) $
#

#
#		パス2の生成スクリプトのコア依存部（ARM用）
#

#
#  割込み要求ライン設定テーブルを使うかどうか（未設定ならFalse）
#
#  riscv_gcc/common/core_kernel.py と同じガード．chip_kernel.py／
#  target_kernel.py 等が設定する想定．Ruby版では未代入の $グローバル変数
#  参照はnil（偽）として扱われるが，Pythonでは未定義グローバル参照は
#  NameErrorになるためここでガードする．
#
if "USE_INTCFG_TABLE" not in globals():
    USE_INTCFG_TABLE = False

#
#  有効なCPU例外ハンドラ番号
#       EXCNO_CUR_SPX_SYNC(4) と EXCNO_CUR_SPX_SERR(7) のみ有効だが、
#       テーブル引きを行うため無効な番号も記載している
#
EXCNO_VALID = {}
excno_list = [
    EXCNO_CUR_SP0_SYNC, EXCNO_CUR_SP0_IRQ, EXCNO_CUR_SP0_FIQ, EXCNO_CUR_SP0_SERR,
    EXCNO_CUR_SPX_SYNC, EXCNO_CUR_SPX_IRQ, EXCNO_CUR_SPX_FIQ, EXCNO_CUR_SPX_SERR,
    EXCNO_L64_SYNC, EXCNO_L64_IRQ, EXCNO_L64_FIQ, EXCNO_L64_SERR,
    EXCNO_L32_SYNC, EXCNO_L32_IRQ, EXCNO_L32_FIQ, EXCNO_L32_SERR,
]
for prcid in range(1, TNUM_PRCID + 1):
    EXCNO_VALID[prcid] = []
    for excno in excno_list:
        EXCNO_VALID[prcid].append((prcid << 16) | excno)

#
#  DEF_EXCで使用できるCPU例外ハンドラ番号
#       EXCNO_CUR_SPX_SYNC(4) と EXCNO_CUR_SPX_SERR(7) のみ有効だが、
#       テーブル引きを行うため無効な番号も記載している
#
EXCNO_DEFEXC_VALID = {}
excno_list = [
    EXCNO_CUR_SP0_SYNC, EXCNO_CUR_SP0_IRQ, EXCNO_CUR_SP0_FIQ, EXCNO_CUR_SP0_SERR,
    EXCNO_CUR_SPX_SYNC, EXCNO_CUR_SPX_IRQ, EXCNO_CUR_SPX_FIQ, EXCNO_CUR_SPX_SERR,
    EXCNO_L64_SYNC, EXCNO_L64_IRQ, EXCNO_L64_FIQ, EXCNO_L64_SERR,
    EXCNO_L32_SYNC, EXCNO_L32_IRQ, EXCNO_L32_FIQ, EXCNO_L32_SERR,
]
for prcid in range(1, TNUM_PRCID + 1):
    EXCNO_DEFEXC_VALID[prcid] = []
    for excno in excno_list:
        EXCNO_DEFEXC_VALID[prcid].append((prcid << 16) | excno)


#
#  配置するセクションを指定した変数定義の生成
#
def DefineVariableSection(genFile, defvar, secname):
    if secname != "":
        genFile.add(f'{defvar} __attribute__((section("{secname}"),nocommon));')
    else:
        genFile.add(f"{defvar};")


#
#  カーネルのデータ領域のセクション名
#
def SecnameKernelData(cls):
    if cls != TCLS_NONE:
        return f".kernel_data_{clsData[cls]['clsid']}"
    else:
        return ""


#
#  スタック領域のセクション名
#
def SecnameStack(cls):
    if cls != TCLS_NONE:
        return f".stack_{clsData[cls]['clsid']}"
    else:
        return ""


#
#  ネイティブスピンロックの生成
#
def GenerateNativeSpn(params):
    kernelCfgC.add(f"LOCK _kernel_lock_{params['spnid']};")
    return f"((intptr_t) &_kernel_lock_{params['spnid']})"


#
#  ターゲット非依存部のインクルード
#
IncludeTrb("kernel/kernel.py")

#
#  割込みハンドラテーブル
#
kernelCfgC.comment_header("Interrupt Handler Table")

for prcid in range(1, TNUM_PRCID + 1):
    kernelCfgC.add(
        f"const FP _kernel_inh_table_prc{prcid}"
        f"[{len(INHNO_VALID[prcid])}] = {{")
    for index, inhnoVal in enumerate(INHNO_VALID[prcid]):
        if index > 0:
            kernelCfgC.add(",")
        kernelCfgC.append(f"\t/* 0x{inhnoVal:05x} */ ")
        if inhnoVal in cfgData["DEF_INH"]:
            kernelCfgC.append(
                f"(FP)({cfgData['DEF_INH'][inhnoVal]['inthdr']})")
            cfgData["DEF_INH"][inhnoVal]["index"] = index
        else:
            kernelCfgC.append("(FP)(_kernel_default_int_handler)")
    kernelCfgC.add()
    kernelCfgC.add2("};")

kernelCfgC.add("const FP* const _kernel_p_inh_table[TNUM_PRCID] = {")
for prcid in range(1, TNUM_PRCID + 1):
    if prcid > 1:
        kernelCfgC.add(",")
    kernelCfgC.append(f"\t_kernel_inh_table_prc{prcid}")
kernelCfgC.add()
kernelCfgC.add2("};")

#
#  割込み要求ライン設定テーブル
#
if USE_INTCFG_TABLE:
    kernelCfgC.comment_header("Interrupt Configuration Table")
    for prcid in range(1, TNUM_PRCID + 1):
        kernelCfgC.add(
            f"const uint8_t _kernel_intcfg_table_prc{prcid}"
            f"[{len(INTNO_VALID[prcid])}] = {{")
        for index, intnoVal in enumerate(INTNO_VALID[prcid]):
            if index > 0:
                kernelCfgC.add(",")
            kernelCfgC.append(f"\t/* 0x{intnoVal:05x} */ ")
            if intnoVal in cfgData["CFG_INT"]:
                kernelCfgC.append("1U")
            else:
                kernelCfgC.append("0U")
        kernelCfgC.add()
        kernelCfgC.add2("};")

kernelCfgC.add("const uint8_t* const _kernel_p_intcfg_table[TNUM_PRCID] = {")
for prcid in range(1, TNUM_PRCID + 1):
    if prcid > 1:
        kernelCfgC.add(",")
    kernelCfgC.append(f"\t_kernel_intcfg_table_prc{prcid}")
kernelCfgC.add()
kernelCfgC.add2("};")

#
#  CPU例外ハンドラテーブル
#
kernelCfgC.comment_header("CPU Exception Handler Table")

for prcid in range(1, TNUM_PRCID + 1):
    kernelCfgC.add(
        f"const FP _kernel_exc_table_prc{prcid}"
        f"[{len(EXCNO_VALID[prcid])}] = {{")
    for index, excnoVal in enumerate(EXCNO_VALID[prcid]):
        if index > 0:
            kernelCfgC.add(",")
        kernelCfgC.append(f"\t/* 0x{excnoVal:05x} */ ")
        if excnoVal in cfgData["DEF_EXC"]:
            kernelCfgC.append(
                f"(FP)({cfgData['DEF_EXC'][excnoVal]['exchdr']})")
            cfgData["DEF_EXC"][excnoVal]["index"] = index
        else:
            kernelCfgC.append("(FP)(_kernel_default_exc_handler)")
    kernelCfgC.add()
    kernelCfgC.add2("};")

kernelCfgC.add("const FP* const _kernel_p_exc_table[TNUM_PRCID] = {")
for prcid in range(1, TNUM_PRCID + 1):
    if prcid > 1:
        kernelCfgC.add(",")
    kernelCfgC.append(f"\t_kernel_exc_table_prc{prcid}")
kernelCfgC.add()
kernelCfgC.add2("};")
