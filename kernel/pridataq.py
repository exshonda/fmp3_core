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
#  $Id: pridataq.py (converted from pridataq.trb by Claude Code Sonnet 4.6) $
# 

#
#		優先度データキュー機能の生成スクリプト
#

class PridataqObject(KernelObject):
    def __init__(self):
        super().__init__("pdq", "pridataq")

    def prepare(self, key, params):
        params.setdefault("pdqmb", "NULL")

        # pdqatrが無効の場合（E_RSATR）
        if (params["pdqatr"] & ~(TA_TPRI)) != 0:
            error_illegal_id("E_RSATR", params, "pdqatr", "pdqid")

        # maxdpriが有効範囲外の場合（E_PAR）
        if not (TMIN_DPRI <= params["maxdpri"]
                and params["maxdpri"] <= TMAX_DPRI):
            error_illegal_id("E_PAR", params, "maxdpri", "pdqid")

        # pdqmbがNULLでない場合（E_NOSPT）
        if params["pdqmb"] != "NULL":
            error_illegal_id("E_NOSPT", params, "pdqmb", "pdqid")

        # 優先度データキュー管理領域
        if params["pdqcnt"] > 0:
            pdqmb_name = f"_kernel_pdqmb_{params['pdqid']}"
            DefineVariableSection(
                kernelCfgC,
                f"static PDQMB {pdqmb_name}[{params['pdqcnt']}]",
                SecnameKernelData(params["class"]))
            params["pdqinib_pdqmb"] = pdqmb_name
        else:
            params["pdqinib_pdqmb"] = "NULL"

    def generateInib(self, key, params):
        return (f"({params['pdqatr']}), ({params['pdqcnt']}), "
                f"({params['maxdpri']}), {params['pdqinib_pdqmb']}")


#
#  優先度データキューに関する情報の生成
#
kernelCfgC.comment_header("Priority Dataqueue Functions")
PridataqObject().generate()
