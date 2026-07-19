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
#  $Id: mutex.py (converted from mutex.trb by Claude Code Sonnet 4.6) $
# 

#
#		ミューテックス機能の生成スクリプト
#

class MutexObject(KernelObject):
    def __init__(self):
        super().__init__("mtx", "mutex")

    def prepare(self, key, params):
        # mtxatrが無効の場合（E_RSATR）
        if not (params["mtxatr"] == TA_NULL
                or params["mtxatr"] == TA_TPRI
                or params["mtxatr"] == TA_CEILING):
            error_illegal_id("E_RSATR", params, "mtxatr", "mtxid")

        if params["mtxatr"] == TA_CEILING:
            # 優先度上限ミューテックスの場合
            if "ceilpri" not in params:
                error_sapi(None, params, "ceilpri must be specified", "mtxid")
            elif not (TMIN_TPRI <= params["ceilpri"]
                      and params["ceilpri"] <= TMAX_TPRI):
                error_illegal_id("E_PAR", params, "ceilpri", "mtxid")
        else:
            # 優先度上限ミューテックスでない場合
            if "ceilpri" in params:
                warning_api(params,
                            "%%ceilpri is ignored in %apiname of %mtxid")
            params["ceilpri"] = 0

    def generateInib(self, key, params):
        return (f"({params['mtxatr']}), "
                f"INT_PRIORITY({params['ceilpri']})")


#
#  ミューテックスに関する情報の生成
#
kernelCfgC.comment_header("Mutex Functions")
MutexObject().generate()
