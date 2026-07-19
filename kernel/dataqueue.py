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
#  $Id: dataqueue.py (converted from dataqueue.trb by Claude Code Sonnet 4.6) $
# 

#
#		データキュー機能の生成スクリプト
#

class DataqueueObject(KernelObject):
    def __init__(self):
        super().__init__("dtq", "dataqueue")

    def prepare(self, key, params):
        params.setdefault("dtqmb", "NULL")

        # dtqatrが無効の場合（E_RSATR）
        if (params["dtqatr"] & ~(TA_TPRI)) != 0:
            error_illegal_id("E_RSATR", params, "dtqatr", "dtqid")

        # dtqmbがNULLでない場合（E_NOSPT）
        if params["dtqmb"] != "NULL":
            error_illegal_id("E_NOSPT", params, "dtqmb", "dtqid")

        # データキュー管理領域
        if params["dtqcnt"] > 0:
            dtqmb_name = f"_kernel_dtqmb_{params['dtqid']}"
            DefineVariableSection(
                kernelCfgC,
                f"static DTQMB {dtqmb_name}[{params['dtqcnt']}]",
                SecnameKernelData(params["class"]))
            kernelCfgC.add()
            params["dtqinib_dtqmb"] = dtqmb_name
        else:
            params["dtqinib_dtqmb"] = "NULL"

    def generateInib(self, key, params):
        return (f"({params['dtqatr']}), ({params['dtqcnt']}), "
                f"{params['dtqinib_dtqmb']}")


#
#  データキューに関する情報の生成
#
kernelCfgC.comment_header("Dataqueue Functions")
DataqueueObject().generate()
