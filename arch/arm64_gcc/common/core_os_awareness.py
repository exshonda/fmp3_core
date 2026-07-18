# Core (AArch64 / GICv2) awareness helpers for gdb OS-awareness.
#
# 役割: コア（arm64 共通）依存の低レベル知識。
#       - GICv2 のレジスタ配置を知り，「指定 INTID の割込み許可/禁止状態」を GICD の
#         レジスタから読む処理を提供する。GICD のベースアドレスはチップ依存なので
#         引数で受け取る（chip_awareness が渡す）。
#       - arm64 共通部の割込みハンドラテーブル _kernel_p_inh_table（プロセッサ毎の
#         FP 配列, core_kernel_impl.h）から「指定 INTID のハンドラ番地」を引く。
#
# GIC レジスタは動的読み出しのため，実行中ターゲット/コアダンプへの接続が必要（要 halt）。
# ハンドラテーブルは const（.rodata）なので ELF 単体（静的）でも読める。

import gdb

# GICv2 Distributor レジスタオフセット（arch/arm64_gcc/common/gic_kernel_impl.h と一致）
_GICD_ISENABLER = 0x100   # 割込みイネーブルセット（read = 現在の許可状態）
_GICD_ISPENDR   = 0x200   # 割込みペンディングセット（read = ペンディング状態）


def _read32(addr):
    addr &= 0xFFFFFFFFFFFFFFFF
    return int(gdb.parse_and_eval("*(unsigned int *)0x%x" % addr)) & 0xFFFFFFFF


def _bit_in_reg(gicd_base, base_off, intid):
    reg = gicd_base + base_off + (intid // 32) * 4
    return bool(_read32(reg) & (1 << (intid % 32)))


def gicv2_int_enabled(gicd_base, intid):
    """GIC INTID が許可状態か（GICD_ISENABLER<n> の該当ビット）。"""
    return _bit_in_reg(gicd_base, _GICD_ISENABLER, intid)


def gicv2_int_pending(gicd_base, intid):
    """GIC INTID がペンディングか（GICD_ISPENDR<n> の該当ビット）。"""
    return _bit_in_reg(gicd_base, _GICD_ISPENDR, intid)


def inh_handler(prcidx, intid):
    """指定プロセッサ(0 始まり index)・INTID の割込みハンドラ番地を返す。

    arm64 共通部の _kernel_p_inh_table[TNUM_PRCID]（プロセッサ毎の FP 配列,
    INTID で添字付け）を読む。読めなければ None。
    """
    try:
        fp = gdb.parse_and_eval(
            "_kernel_p_inh_table[%d][%d]" % (int(prcidx), int(intid)))
        return int(fp) & 0xFFFFFFFFFFFFFFFF
    except gdb.error:
        return None


# ディスパッチ要求 IPI の番号（gic_ipi.h）: TOPPERS_TZ_S はセキュア SGI 12, 非 TZ は SGI 0
_IPINO_DISPATCH_CANDIDATES = (12, 0)


def is_dispatch_ipi_bypassed(intid, handler_addr):
    """INTID がディスパッチ要求 IPI で，asm 直行のバイパス処理になっているか。

    USE_BYPASS_IPI_DISPATCH_HANDER 定義時は DEF_INH されず（OMIT_DISPATCH_HANDLER で
    _kernel_dispatch_handler 自体が ELF に無い），ハンドラ表は default のまま
    core_support.S の IRQ 入口が INTID を比較して直接ディスパッチャへ分岐する。
    その状態を ELF 上の事実から判定する:
      ①INTID がディスパッチ IPI 番号 ②表のハンドラが _kernel_default_int_handler
      ③_kernel_dispatch_handler シンボルが無い
    """
    if intid not in _IPINO_DISPATCH_CANDIDATES or not handler_addr:
        return False
    try:
        gdb.parse_and_eval("_kernel_dispatch_handler")
        return False                      # ハンドラ登録あり（バイパス無効）
    except gdb.error:
        pass
    try:
        default = int(gdb.parse_and_eval("&_kernel_default_int_handler"))
    except gdb.error:
        return False
    return (int(handler_addr) & 0xFFFFFFFFFFFFFFFF) == (default & 0xFFFFFFFFFFFFFFFF)
