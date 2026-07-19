# -*- coding: utf-8 -*-
#
#		パス2の生成スクリプトのチップ依存部（RP2350用）
#
#  $Id: chip_kernel.py (converted from chip_kernel.trb by Claude Code Sonnet 5) $
#
#  ネイティブスピンロックは RP2350 の SIO ハードウェアスピンロック
#  （SIO_SPINLOCKn）に割り当てる．chip_sil.h で SIL スピンロック=15，
#  ジャイアントロック=30 を予約しているため，ネイティブスピンロックは
#  0 から順に割り当てる（最大 TMAX_NATIVE_SPN=14 個）．
#

#
#  ネイティブスピンロックの生成（コア依存部より先に定義し，そちらの既定実装を抑止する）
#
#  spninib の lock メンバに SIO_SPINLOCKn のアドレスを格納する．
#
rp2350_spinlock_index = 0

#  コア依存部に既定実装を定義させない（core_kernel.py 参照）
generate_native_spn_defined = True


def GenerateNativeSpn(params):
    global rp2350_spinlock_index
    ret = f"(intptr_t)RP2350_SIO_SPINLOCKn({rp2350_spinlock_index})"
    rp2350_spinlock_index += 1
    return ret


#
#  コア依存テンプレートのインクルード
#
IncludeTrb("core_kernel.py")
