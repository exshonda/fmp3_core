# -*- coding: utf-8 -*-
#
#		パス2の生成スクリプトのチップ依存部（ZynqMP RPU用）
#
#  $Id: chip_kernel.py (converted from chip_kernel.trb by Claude Code Sonnet 5) $
#

#
#  使用できる割込み番号とそれに対応する割込みハンドラ番号
#
INTNO_VALID = {}
INHNO_VALID = {}
INTNO_GLOBAL = {}
INHNO_GLOBAL = {}
private_intno_list = list(range(0, 32))
global_intno_list = list(range(32, 187))
for prcid in range(1, TNUM_PRCID + 1):
    INTNO_VALID[prcid] = []
    INHNO_VALID[prcid] = []
    INTNO_GLOBAL[prcid] = {}
    INHNO_GLOBAL[prcid] = {}
    for intno in private_intno_list:
        INTNO_VALID[prcid].append((prcid << 16) | intno)
        INHNO_VALID[prcid].append((prcid << 16) | intno)
        INTNO_GLOBAL[prcid][intno] = (prcid << 16) | intno
        INHNO_GLOBAL[prcid][intno] = (prcid << 16) | intno
    for intno in global_intno_list:
        INTNO_VALID[prcid].append(intno)
        INHNO_VALID[prcid].append((prcid << 16) | intno)
        INTNO_GLOBAL[prcid][intno] = intno
        INHNO_GLOBAL[prcid][intno] = (prcid << 16) | intno

#
#  生成スクリプトのコア依存部
#
IncludeTrb("core_kernel.py")
