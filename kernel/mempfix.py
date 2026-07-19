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
#  $Id: mempfix.py (converted from mempfix.trb by Claude Code Sonnet 4.6) $
# 

#
#		固定長メモリプール機能の生成スクリプト
#

class MempfixObject(KernelObject):
    def __init__(self):
        super().__init__("mpf", "mempfix")

    def prepare(self, key, params):
        params.setdefault("mpf", "NULL")
        params.setdefault("mpfmb", "NULL")

        # mpfatrが無効の場合（E_RSATR）
        if (params["mpfatr"] & ~(TA_TPRI)) != 0:
            error_illegal_id("E_RSATR", params, "mpfatr", "mpfid")

        # blkcntが0の場合（E_PAR）
        if params["blkcnt"] == 0:
            error_illegal_id("E_PAR", params, "blkcnt", "mpfid")

        # blkszが0の場合（E_PAR）
        if params["blksz"] == 0:
            error_illegal_id("E_PAR", params, "blksz", "mpfid")

        # 固定長メモリプール領域
        if params["mpf"] == "NULL":
            mpf_name = f"_kernel_mpf_{params['mpfid']}"
            DefineVariableSection(
                kernelCfgC,
                (f"static MPF_T {mpf_name}"
                 f"[{params['blkcnt']} * COUNT_MPF_T({params['blksz']})]"),
                SecnameKernelData(params["class"]))
            params["mpfinib_mpf"] = mpf_name
        else:
            params["mpfinib_mpf"] = f"(void *)({params['mpf']})"

        # mpfmbがNULLでない場合（E_NOSPT）
        if params["mpfmb"] != "NULL":
            error_illegal_id("E_NOSPT", params, "mpfmb", "mpfid")

        # 固定長メモリプール管理領域
        mpfmb_name = f"_kernel_mpfmb_{params['mpfid']}"
        DefineVariableSection(
            kernelCfgC,
            f"static MPFMB {mpfmb_name}[{params['blkcnt']}]",
            SecnameKernelData(params["class"]))
        params["mpfinib_mpfmb"] = mpfmb_name

    def generateInib(self, key, params):
        return (f"({params['mpfatr']}), ({params['blkcnt']}), "
                f"ROUND_MPF_T({params['blksz']}), {params['mpfinib_mpf']}, "
                f"{params['mpfinib_mpfmb']}")


#
#  固定長メモリプールに関する情報の生成
#
kernelCfgC.comment_header("Fixed-sized Memorypool Functions")
MempfixObject().generate()
