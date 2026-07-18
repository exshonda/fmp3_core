#
#		チップ依存部の CMake 定義（ARM Musca-B1 / SSE-200 用）
#
#  target.cmake から include される（上流 Makefile.chip に相当）．
#
#  外部（SDK）ターゲットのパス解決規約（fmp3_esp_idf / fmp3_pico_sdk 等の
#  統合リポジトリから fmp3_core が submodule として使われる場合に備える。
#  arch/riscv_gcc/polarfire_soc/chip.cmake と同じ作法）：
#   - 共通 arch（arch/arm_m_gcc/common）は fmp3_core 側＝ARCHDIR
#   - チップ依存部（musca_b1）・target 依存部は，これを include する
#     target.cmake 側で解決される＝CHIPDIR／TARGETDIR
#  ARCHDIR／CHIPDIR／TARGETDIR は呼び出し元の target.cmake が設定済み
#  （本ファイルではハードコードしない）．
#
set(COREDIR ${ARCHDIR}/common)

#
#  コンパイルオプション（Makefile.chip:16-18）
#
#  Cortex-M33（ARMv8-M Mainline），Thumb 命令のみ．Musca-B1 の M33 は
#  FPU/DSP を搭載するが，本依存部はソフトウェア浮動小数点 ABI（soft-float）
#  でビルドする．
#
list(APPEND FMP3_COMPILE_OPTIONS
    -mcpu=cortex-m33
    -mthumb
    -mfloat-abi=soft
)
list(APPEND FMP3_LINK_OPTIONS
    -mcpu=cortex-m33
    -mthumb
    -mfloat-abi=soft
)
list(APPEND FMP3_COMPILE_DEFS TOPPERS_CORTEX_M33)

#
#  ARM アーキテクチャの世代（ARMv8-M Mainline，Makefile.chip:20-26）
#
#  __TARGET_ARCH_THUMB は旧 ARM 純正コンパイラの組込みマクロであり，
#  GNU 開発環境では定義されないため，明示的に定義する（5 = ARMv8-M）．
#
list(APPEND FMP3_COMPILE_DEFS __TARGET_ARCH_THUMB=5)

#
#  TrustZone 対応コアでのセキュア単独動作（Makefile.chip:28-38）
#
#  QEMU musca-b1（Cortex-M33）はリセット直後セキュア状態で動作する．
#  TrustZone 搭載コアでは，例外リターン値 EXC_RETURN にセキュア状態を
#  表すビットが必要なため，TOPPERS_ENABLE_TRUSTZONE を定義してセキュア用
#  の EXC_RETURN（0xfffffffd）を選択する．これが無いと，セキュア状態から
#  の例外リターン整合性チェックに失敗する．
#
list(APPEND FMP3_COMPILE_DEFS TOPPERS_ENABLE_TRUSTZONE)

list(APPEND FMP3_INCLUDE_DIRS ${CHIPDIR})

#
#  カーネルに含めるチップ依存ソース（Makefile.chip:45-46）
#
list(APPEND FMP3_ARCH_C_FILES
    ${CHIPDIR}/chip_kernel_impl.c
)

#
#  スタートアップモジュール（Makefile.chip:54-64）
#
#  START_OBJS を start.o（COREDIR/start.S）に設定し，LDFLAGS に -nostdlib
#  を追加する．
#
list(APPEND FMP3_START_FILES
    ${COREDIR}/start.S
)
list(APPEND FMP3_LINK_OPTIONS -nostdlib)

#
#  コア依存部（Makefile.chip:69）
#
include(${COREDIR}/arch.cmake)
