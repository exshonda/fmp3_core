# Chip (STM32MP2) awareness helpers for gdb OS-awareness.
#
# 役割: チップ（SoC）依存の知識。STM32MP2 の GIC-400 Distributor ベースアドレスを持ち，
#       コア層(core_os_awareness, GICv2 レジスタ配置)を使って「指定 INTID の割込み
#       許可/禁止・ペンディング状態」を返す。
#
# core_os_awareness.py は arch/arm64_gcc/common にあるため，本ファイルからの相対パスで
# sys.path に追加してから import する（__file__ は Python import 時に設定される）。

import os
import sys

# 下位層(core)の import で .pyc を生成させない（ソースツリーに __pycache__ を残さない）。
# 上位層を経由せず本モジュールを直接 import した場合への防御。
sys.dont_write_bytecode = True

sys.path.insert(0, os.path.normpath(
    os.path.join(os.path.dirname(os.path.abspath(__file__)), "../common")))

import core_os_awareness

# STM32MP2 GIC-400 Distributor ベース（stm32mp2.h: GICD_BASE）
GICD_BASE = 0x4AC10000


def int_enabled(intid):
    """指定 INTID の割込み許可状態（True=許可 / False=禁止）。"""
    return core_os_awareness.gicv2_int_enabled(GICD_BASE, intid)


def int_pending(intid):
    """指定 INTID のペンディング状態（True=ペンディング）。"""
    return core_os_awareness.gicv2_int_pending(GICD_BASE, intid)


# 割込みハンドラ番地の取得（_kernel_p_inh_table）とディスパッチ IPI バイパス判定は
# arm64 共通部の知識なので core 層の実装をそのまま公開する（チップ固有の追加なし）。
inh_handler = core_os_awareness.inh_handler
is_dispatch_ipi_bypassed = core_os_awareness.is_dispatch_ipi_bypassed
