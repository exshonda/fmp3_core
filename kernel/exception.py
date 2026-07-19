# -*- coding: utf-8 -*-
#
#   TOPPERS/FMP Kernel
#       Toyohashi Open Platform for Embedded Real-Time Systems/
#       Flexible MultiProcessor Kernel
# 
#   Copyright (C) 2015 by FUJI SOFT INCORPORATED, JAPAN
#   Copyright (C) 2015-2023 by Embedded and Real-Time Systems Laboratory
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
#  $Id: exception.py (converted from exception.trb by Claude Code Sonnet 4.6) $
# 

#
#		CPU例外管理機能の生成スクリプト
#

#
#  kernel_cfg.cの生成
#
kernelCfgC.comment_header("CPU Exception Management Functions")

#
#  DEF_EXCで使用できるCPU例外ハンドラ番号のデフォルト定義
#
if "EXCNO_DEFEXC_VALID" not in globals():
    EXCNO_DEFEXC_VALID = EXCNO_VALID

#
#  CPU例外ハンドラに関するエラーチェック
#
for _, params in cfgData["DEF_EXC"].items():
    # クラスの囲みの中に記述されていない場合（E_RSATR）
    if "class" not in params:
        error_ercd("E_RSATR", params, "%apiname must be within a class")
        params["class"] = TCLS_ERROR

    excno_all = [v for lst in EXCNO_DEFEXC_VALID.values() for v in lst]
    if params["excno"] not in excno_all:
        # excnoが有効範囲外の場合（E_PAR）
        error_illegal("E_PAR", params, "excno")
    else:
        # CPU例外が有効なプロセッサのリストを求める
        bitmap = 0
        for prcid, excno_list in EXCNO_DEFEXC_VALID.items():
            if params["excno"] in excno_list:
                bitmap |= (1 << (prcid - 1))

        # クラスの割付け可能プロセッサのチェック（E_RSATR）
        if bitmap != clsData[params["class"]]["affinityPrcBitmap"]:
            error_ercd("E_RSATR", params,
                       "the assignable processors of the class "
                       "in which %apiname of `%excno' is described must "
                       "correspond with the processors on which the CPU "
                       "exception occurs")

    # excatrが無効の場合（E_RSATR）
    if (params["excatr"] & ~(TARGET_EXCATR)) != 0:
        error_illegal_sym("E_RSATR", params, "excatr", "excno")

    # ターゲット依存のエラーチェック
    if "TargetCheckDefExc" in globals():
        TargetCheckDefExc(params)

#
#  CPU例外ハンドラのための標準的な初期化情報の生成
#
if not OMIT_INITIALIZE_EXCEPTION:
    kernelCfgC.add(f"""\
#define TNUM_DEF_EXCNO\t{len(cfgData["DEF_EXC"])}
const uint_t _kernel_tnum_def_excno = TNUM_DEF_EXCNO;
""")

    if len(cfgData["DEF_EXC"]) != 0:
        for _, params in cfgData["DEF_EXC"].items():
            kernelCfgC.add(
                f"EXCHDR_ENTRY({params['excno']}, "
                f"{params['excno'].val}, {params['exchdr']})")
        kernelCfgC.add("")

        kernelCfgC.add(
            "const EXCINIB _kernel_excinib_table[TNUM_DEF_EXCNO] = {")
        for index, (_, params) in enumerate(cfgData["DEF_EXC"].items()):
            if index > 0:
                kernelCfgC.add(",")
            kernelCfgC.append(
                f"\t{{ ({params['excno']}), ({params['excatr']}), "
                f"(FP)(EXC_ENTRY({params['excno']}, {params['exchdr']})), "
                f"{clsData[params['class']]['initPrc']}, "
                f"0x{clsData[params['class']]['affinityPrcBitmap']:x}U }}")
        kernelCfgC.add()
        kernelCfgC.add2("};")
    else:
        kernelCfgC.add2(
            "TOPPERS_EMPTY_LABEL(const EXCINIB, _kernel_excinib_table);")

#
#  CPU例外管理機能初期化関数の追加
#
initializeFunctions.append("_kernel_initialize_exception(p_my_pcb);")
