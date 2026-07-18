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

#
#  ★-T（リンカスクリプト指定）はここでは積まない．
#
#  FMP3_LDSCRIPT の値（cfg1_out / fmp の LINK_DEPENDS 追跡用）を確定
#  させるだけに留める．-T の適用は CMakeLists.txt の
#  include(target.cmake) 直後の1箇所（FMP3_LDSCRIPT_VIA_DRIVER_T で
#  書式を分ける実装。このターゲットは下で ON を宣言する。理由は同
#  ファイルのコメント参照）に集約する．
#
#  ここで積んでしまうと，将来 fmp 実行ファイルを組む Task が asp3_core
#  と同じパターンで -Wl,-T,${FMP3_LDSCRIPT} を足したときに
#    riscv64-unknown-elf-gcc ... -T mpfs-lim.ld -Wl,-T,mpfs-lim.ld
#  のように2回指定され，ld が
#    "linker script file '...' appears multiple times"
#  で fatal error になる（実リンカで再現済み）．
#
#  上流 Makefile.target:106-107 も同じ理由で，QEMU=1 のとき COPTS に
#  -T $(LDSCRIPT) を混ぜた直後に `LDSCRIPT =` で変数を空にしている．
#  上流が LDSCRIPT を空にするのは「-T が要らない」からではなく
#  「二重に渡さないため」である（-T 自体は要る）．
#
if(POLARFIRE_QEMU)
    #  Makefile.target:106
    #  --undefined=_kernel_mpfinib_table は --gc-sections で消えるカーネル
    #  構成テーブルを保持するためのもの．-T 自体（picolibc.specs の
    #  %{!T:-Tpicolibc.ld} を抑止する側）は CMakeLists.txt 側で積む．
    list(APPEND FMP3_LINK_OPTIONS -Wl,--undefined=_kernel_mpfinib_table)

    #
    #  Makefile.target:108-110
    #  cfg1_out のリンクは _start を参照しないため --gc-sections で
    #  TOPPERS_magic_number が除去される．これを抑止する．
    #
    list(APPEND FMP3_CFG1_OUT_LINK_OPTIONS -Wl,--no-gc-sections)

    #
    #  リンカスクリプト指定を「素の -T」にする．picolibc.specs の
    #  %{!T:-Tpicolibc.ld} は gcc ドライバの -T スイッチの有無だけを見て
    #  picolibc 既定のリンカスクリプトを追加注入するため，-Wl,-T,<file>
    #  では防げない（実測。CMakeLists.txt の -T 適用箇所のコメント参照）．
    #  適用そのものは CMakeLists.txt の1箇所に集約されているため，ここでは
    #  ON を宣言するだけに留める．
    #
    set(FMP3_LDSCRIPT_VIA_DRIVER_T ON)

    #
    #  QEMU による実行（cmake --build <dir> --target run）
    #
    #  ★asp3_core の polarfire の RUN_COMMAND は流用できない．
    #    icicle-kit マシンは既定で envm にリセットして HSS の起動を待つため，
    #    全ハートのリセット PC をカーネルのエントリ（_start）に向ける必要が
    #    ある．そのため5ハートすべてに -device loader を与え，-bios none で
    #    既定の OpenSBI を載せない．
    #    出典: target/polarfire_soc_kit_gcc/target_user.md:177-186
    #
    #    E51（hart0）は MPFS HAL により待機し，U54（hart1〜4＝PRC1〜4）で
    #    FMP3 が動作する．
    #
    set(QEMU_SYSTEM_RISCV64 qemu-system-riscv64
        CACHE STRING "Path to qemu-system-riscv64")
    set(FMP3_RUN_COMMAND
        ${QEMU_SYSTEM_RISCV64} -M microchip-icicle-kit -smp 5 -m 2G -nographic
        -serial mon:stdio -bios none
        -kernel $<TARGET_FILE:fmp>
        -device loader,file=$<TARGET_FILE:fmp>,cpu-num=0
        -device loader,file=$<TARGET_FILE:fmp>,cpu-num=1
        -device loader,file=$<TARGET_FILE:fmp>,cpu-num=2
        -device loader,file=$<TARGET_FILE:fmp>,cpu-num=3
        -device loader,file=$<TARGET_FILE:fmp>,cpu-num=4
    )
endif()
