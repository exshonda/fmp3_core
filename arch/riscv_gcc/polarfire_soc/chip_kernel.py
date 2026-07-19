# -*- coding: utf-8 -*-
#
#    パス2の生成スクリプトのチップ依存部（PolarFire SoC 用）
#
#  $Id: chip_kernel.py (converted from chip_kernel.trb by Claude Code Sonnet 5) $
#

#
#  使用できる割込み番号とそれに対応する割込みハンドラ番号
#
INTNO_VALID = {}
INHNO_VALID = {}
plic_intno_list = list(range(0, 183))
for prcid in range(1, TNUM_PRCID + 1):
    INTNO_VALID[prcid] = []
    INHNO_VALID[prcid] = []
    for intno in plic_intno_list:
        INTNO_VALID[prcid].append(intno)
        INHNO_VALID[prcid].append((prcid << 16) | intno)


#
#  プロセッサIDからPLICのコンテキストINDEXへの変換
#
def pid2cidx(pid):
    return ((pid - 1) * 2) + 1


#
#  生成スクリプトのコア依存部
#
IncludeTrb("core_kernel.py")

#
#  生成スクリプトのPLIC依存部
#
IncludeTrb("plic_kernel.py")
