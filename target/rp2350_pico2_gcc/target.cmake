#
#		ターゲット依存部の CMake 定義（RaspberryPi Pico 2 / RP2350用）
#
#  上流 target/rp2350_pico2_gcc/Makefile.target の CMake 版．
#
#  外部（SDK）ターゲットのパス解決規約は musca_b1_gcc/target.cmake /
#  polarfire_soc_kit_gcc/target.cmake と同じ（ARCHDIR は ${FMP3_ROOT_DIR}
#  基準，CHIPDIR/TARGETDIR は CMAKE_CURRENT_LIST_DIR 相対）．
#
set(ARCHDIR ${FMP3_ROOT_DIR}/arch/arm_m_gcc)
get_filename_component(CHIPDIR ${CMAKE_CURRENT_LIST_DIR}/../../arch/arm_m_gcc/rp2350 ABSOLUTE)
set(TARGETDIR ${CMAKE_CURRENT_LIST_DIR})

#  TECS を使用しない（非TECS版 SIO ドライバを用いる，Makefile.target:14-19）
list(APPEND FMP3_COMPILE_DEFS TOPPERS_OMIT_TECS)

#  コンパイルオプション（Makefile.target:22-26）
list(APPEND FMP3_INCLUDE_DIRS ${TARGETDIR})
list(APPEND FMP3_COMPILE_OPTIONS -mlittle-endian)
list(APPEND FMP3_LINK_OPTIONS -mlittle-endian)

#
#  カーネルに含めるターゲット依存ソース（Makefile.target:29-33）
#
list(APPEND FMP3_TARGET_C_FILES
    ${TARGETDIR}/target_kernel_impl.c
    ${TARGETDIR}/target_timer.c
)
list(APPEND FMP3_ARCH_C_FILES
    ${TARGETDIR}/image_def.S
)

#
#  リンカスクリプトの定義（Makefile.target:38）
#
set(FMP3_LDSCRIPT ${TARGETDIR}/rp2350_pico2.ld)

#
#  cfg に渡すファイル
#
list(APPEND FMP3_CFG_FILES            ${TARGETDIR}/target_kernel.cfg)
list(APPEND FMP3_CLASS_TRB_FILES      ${TARGETDIR}/target_class.py)
list(APPEND FMP3_KERNEL_CFG_TRB_FILES ${TARGETDIR}/target_kernel.py)
list(APPEND FMP3_CHECK_TRB_FILES      ${TARGETDIR}/target_check.py)

#
#  チップ依存部（Makefile.target:43）
#
include(${CHIPDIR}/chip.cmake)

#
#  ★実行手段は無い（本ターゲットは Phase D1＝ビルドのみ．上流
#    Makefile.target 自身の run: ターゲットが "書込み・実行は D2 で行う"
#    とコメントしている（Makefile.target:45-52）．QEMU に RP2350/Pico
#    相当のマシンは存在しないことを実測で確認済み：qemu-system-arm
#    8.2.2 / 11.0.1 いずれの `-machine help` にも rp2350/pico の記載なし，
#    QEMU 11.0.1 のソースツリーに rp2350/RP2350 の文字列が0件）．
#    FMP3_RUN_COMMAND をここでは定義しない → CMakeLists.txt の
#    `run` ターゲット自体が生成されない（意図的）．
#
