# -*- coding: utf-8 -*-
#
#		ターゲット依存のクラス定義（ARM Musca-B1用）
#
#  Musca-B1 は SSE-200 を中核とする dual Cortex-M33（最大 2 コア）構成．
#  TNUM_PRCID に応じてクラスのリストを切り替える．
#
#  $Id: target_class.py (converted from target_class.trb by Claude Code Sonnet 5) $
#

#
#  クラスのリスト
#
globalVars.append("clsData")

if TNUM_PRCID == 1:
    clsData = {
        1: {"clsid": NumStr(1, "CLS_PRC1"),
            "initPrc": 1, "affinityPrcList": [1]},
        2: {"clsid": NumStr(2, "CLS_ALL_PRC1"),
            "initPrc": 1, "affinityPrcList": [1]},
    }

elif TNUM_PRCID == 2:
    clsData = {
        1: {"clsid": NumStr(1, "CLS_PRC1"),
            "initPrc": 1, "affinityPrcList": [1]},
        2: {"clsid": NumStr(2, "CLS_PRC2"),
            "initPrc": 2, "affinityPrcList": [2]},
        3: {"clsid": NumStr(3, "CLS_ALL_PRC1"),
            "initPrc": 1, "affinityPrcList": [1, 2]},
        4: {"clsid": NumStr(4, "CLS_ALL_PRC2"),
            "initPrc": 2, "affinityPrcList": [1, 2]},
    }
