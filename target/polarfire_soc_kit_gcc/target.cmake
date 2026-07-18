#
#		ターゲット依存部の CMake 定義（PolarFire SoC Kit 用）
#
#  上流 target/polarfire_soc_kit_gcc/Makefile.target の CMake 版．
#
#  外部（SDK）ターゲットのパス解決規約（fmp3_esp_idf / fmp3_pico_sdk 等の
#  統合リポジトリから fmp3_core が submodule として使われる場合に備える）：
#   - 共通 arch（arch/riscv_gcc/common）は fmp3_core 側＝ARCHDIR
#     （${FMP3_ROOT_DIR} 基準：常に fmp3_core リポジトリ内にある）
#   - チップ依存部・target 依存部は，本ファイル（target.cmake）の置き場所
#     基準＝CMAKE_CURRENT_LIST_DIR 相対で解決する．
#  polarfire_soc_kit_gcc は chip も target も fmp3_core リポジトリ内に
#  あるため今は ARCHDIR 基準と同じ値になるが，決め打ちしない．
#
set(ARCHDIR ${FMP3_ROOT_DIR}/arch/riscv_gcc)
get_filename_component(CHIPDIR ${CMAKE_CURRENT_LIST_DIR}/../../arch/riscv_gcc/polarfire_soc ABSOLUTE)
set(TARGETDIR ${CMAKE_CURRENT_LIST_DIR})

#
#  QEMU（microchip-icicle-kit）向けビルド設定
#
#  上流 Makefile.target:17-22 の QEMU=1 に相当．ボードを Icicle Kit に，
#  C ライブラリを picolibc に切り替える．
#
option(POLARFIRE_QEMU "Build for QEMU microchip-icicle-kit (OFF: real board)" ON)

if(POLARFIRE_QEMU)
    set(FMP3_BOARD MPFS_ICICLE_KIT)
    set(FMP3_RISCV_SPECS "--specs=picolibc.specs")   #  Makefile.target:20
    list(APPEND FMP3_COMPILE_DEFS TOPPERS_USE_QEMU)  #  Makefile.target:21
else()
    set(FMP3_BOARD MPFS_DISCOVERY_KIT)               #  Makefile.target:28
endif()

#  Makefile.target:83
list(APPEND FMP3_COMPILE_DEFS ${FMP3_BOARD})

#  Makefile.target:46
list(APPEND FMP3_COMPILE_DEFS
    TOPPERS_OMIT_BSS_INIT
    TOPPERS_OMIT_DATA_INIT
)

#  非TECS版システムサービスを使う（syssvc/syslog.h 等が参照する）
list(APPEND FMP3_COMPILE_DEFS TOPPERS_OMIT_TECS)

#  Makefile.target:43, :75
list(APPEND FMP3_INCLUDE_DIRS
    ${TARGETDIR}
    ${TARGETDIR}/sdk/platform
)

#
#  ボード毎の include とリンカスクリプト（Makefile.target:85-95）
#
if(FMP3_BOARD STREQUAL "MPFS_ICICLE_KIT")
    list(APPEND FMP3_INCLUDE_DIRS
        ${TARGETDIR}/sdk/boards/icicle-kit-es
        ${TARGETDIR}/sdk/boards/icicle-kit-es/platform_config/lim-debug
    )
    set(FMP3_LDSCRIPT
        ${TARGETDIR}/sdk/boards/icicle-kit-es/platform_config/lim-debug/linker/mpfs-lim.ld)
else()
    list(APPEND FMP3_INCLUDE_DIRS
        ${TARGETDIR}/sdk/boards/mpfs-discovery-kit
        ${TARGETDIR}/sdk/boards/mpfs-discovery-kit/platform_config/lim-debug
    )
    set(FMP3_LDSCRIPT
        ${TARGETDIR}/sdk/boards/mpfs-discovery-kit/platform_config/lim-debug/linker/mpfs-lim.ld)
endif()

#
#  カーネルに含めるターゲット依存ソース（Makefile.target:117）
#
list(APPEND FMP3_TARGET_C_FILES
    ${TARGETDIR}/target_kernel_impl.c
)

#
#  Microchip SDK のソース（Makefile.target:52-65）
#
#  上流は SYSSVC_COBJS / SYSSVC_ASMOBJS として最終リンクに加えている．
#  カーネルライブラリには入れない（asp3_core の polarfire にはこの層が
#  無く，流用できない部分）．
#
set(SDKDIR ${TARGETDIR}/sdk)
list(APPEND FMP3_SDK_C_FILES
    ${TARGETDIR}/sdk_entry.c
    ${SDKDIR}/platform/mpfs_hal/startup_gcc/system_startup.c
    ${SDKDIR}/platform/mpfs_hal/common/nwc/mss_io.c
    ${SDKDIR}/platform/mpfs_hal/common/nwc/mss_nwc_init.c
    ${SDKDIR}/platform/mpfs_hal/common/nwc/mss_pll.c
    ${SDKDIR}/platform/mpfs_hal/common/nwc/mss_sgmii.c
    ${SDKDIR}/platform/mpfs_hal/common/mss_beu.c
    ${SDKDIR}/platform/mpfs_hal/common/mss_irq_handler_stubs.c
    ${SDKDIR}/platform/mpfs_hal/common/mss_l2_cache.c
    ${SDKDIR}/platform/mpfs_hal/common/mss_mpu.c
    ${SDKDIR}/platform/mpfs_hal/common/mss_peripherals.c
    ${SDKDIR}/platform/mpfs_hal/common/mss_plic.c
    ${SDKDIR}/platform/mpfs_hal/common/mss_pmp.c
    ${SDKDIR}/platform/mpfs_hal/common/mss_util.c
)
list(APPEND FMP3_SDK_ASM_FILES
    ${SDKDIR}/platform/mpfs_hal/startup_gcc/mss_entry.S
    ${SDKDIR}/platform/mpfs_hal/startup_gcc/mss_utils.S
)

#
#  cfg に渡すファイル（sample/Makefile:309-319 の TARGET_*_TRB / _CFG）
#
list(APPEND FMP3_CFG_FILES            ${TARGETDIR}/target_kernel.cfg)
list(APPEND FMP3_CLASS_TRB_FILES      ${TARGETDIR}/target_class.trb)
list(APPEND FMP3_KERNEL_CFG_TRB_FILES ${TARGETDIR}/target_kernel.trb)
list(APPEND FMP3_CHECK_TRB_FILES      ${TARGETDIR}/target_check.trb)

#
#  チップ依存部
#
include(${CHIPDIR}/chip.cmake)

#
#  最終リンクのオプション（Makefile.target:47, :106）
#
list(APPEND FMP3_LINK_OPTIONS -Wl,--gc-sections)

if(POLARFIRE_QEMU)
    #  Makefile.target:106
    #  picolibc.specs が picolibc.ld を追加する（%{!T:-Tpicolibc.ld}）のを
    #  -T で抑止し，--gc-sections で消えるカーネル構成テーブルを保持する．
    list(APPEND FMP3_LINK_OPTIONS
        -T ${FMP3_LDSCRIPT}
        -Wl,--undefined=_kernel_mpfinib_table
    )

    #
    #  Makefile.target:108-110
    #  cfg1_out のリンクは _start を参照しないため --gc-sections で
    #  TOPPERS_magic_number が除去される．これを抑止する．
    #
    list(APPEND FMP3_CFG1_OUT_LINK_OPTIONS -Wl,--no-gc-sections)
else()
    #
    #  実機ビルドでは -T を COPTS に混ぜる理由（picolibc.ld の抑止）が
    #  無いので，通常どおりリンカへ渡す．
    #
    list(APPEND FMP3_LINK_OPTIONS -Wl,-T,${FMP3_LDSCRIPT})
endif()
