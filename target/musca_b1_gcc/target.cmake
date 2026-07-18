#
#		ターゲット依存部の CMake 定義（ARM Musca-B1 / QEMU 用）
#
#  上流 target/musca_b1_gcc/Makefile.target の CMake 版．
#
#  外部（SDK）ターゲットのパス解決規約は polarfire_soc_kit_gcc/target.cmake
#  と同じ（ARCHDIR は ${FMP3_ROOT_DIR} 基準，CHIPDIR/TARGETDIR は
#  CMAKE_CURRENT_LIST_DIR 相対）．
#
set(ARCHDIR ${FMP3_ROOT_DIR}/arch/arm_m_gcc)
get_filename_component(CHIPDIR ${CMAKE_CURRENT_LIST_DIR}/../../arch/arm_m_gcc/musca_b1 ABSOLUTE)
set(TARGETDIR ${CMAKE_CURRENT_LIST_DIR})

#
#  QEMU 専用ターゲット（実機 Musca-B1 ボードでの動作確認は行っていない．
#  target_user.txt:42,149-150）．polarfire の POLARFIRE_QEMU のような
#  実機/QEMU 切替オプションは作らない．常に TOPPERS_USE_QEMU を定義する
#  （Makefile.target:19 の CDEFS := $(CDEFS) -DTOPPERS_USE_QEMU に相当。
#  target_kernel_impl.c の target_exit がこのマクロでセミホスティング
#  終了に分岐する）．
#
list(APPEND FMP3_COMPILE_DEFS TOPPERS_USE_QEMU)

#  Makefile.target:24-26
list(APPEND FMP3_INCLUDE_DIRS ${TARGETDIR})
list(APPEND FMP3_COMPILE_OPTIONS -mlittle-endian)
list(APPEND FMP3_LINK_OPTIONS -mlittle-endian)

#  非TECS版システムサービスを使う（polarfire と同じ理由．syssvc/syslog.h
#  等が参照する）
list(APPEND FMP3_COMPILE_DEFS TOPPERS_OMIT_TECS)

#
#  カーネルに含めるターゲット依存ソース（Makefile.target:31-32）
#
list(APPEND FMP3_TARGET_C_FILES
    ${TARGETDIR}/target_kernel_impl.c
    ${TARGETDIR}/target_timer.c
    ${TARGETDIR}/target_ipi.c
)

#
#  非TECS版 SIO ドライバ（Makefile.target:37-38）
#
#  polarfire と異なり，chip.cmake ではなく target.cmake がここを設定する
#  （上流 Makefile.chip に SYSSVC_COBJS の記述が無く，Makefile.target が
#  直接設定しているため。pl011_uart.c は target/musca_b1_gcc/ 直下にあり，
#  chip 層の持ち物ではない）．
#
list(APPEND FMP3_SYSSVC_TARGET_C_FILES
    ${TARGETDIR}/target_serial.c
    ${TARGETDIR}/pl011_uart.c
)

#
#  リンカスクリプトの定義（Makefile.target:43）
#
set(FMP3_LDSCRIPT ${TARGETDIR}/musca_b1.ld)

#
#  cfg に渡すファイル
#
list(APPEND FMP3_CFG_FILES            ${TARGETDIR}/target_kernel.cfg)
list(APPEND FMP3_CLASS_TRB_FILES      ${TARGETDIR}/target_class.trb)
list(APPEND FMP3_KERNEL_CFG_TRB_FILES ${TARGETDIR}/target_kernel.trb)
list(APPEND FMP3_CHECK_TRB_FILES      ${TARGETDIR}/target_check.trb)

#
#  チップ依存部（Makefile.target:48）
#
include(${CHIPDIR}/chip.cmake)

#
#  ★-T（リンカスクリプト指定）はここでは積まない．FMP3_LDSCRIPT の値
#    （cfg1_out / fmp の LINK_DEPENDS 追跡用）を確定させるだけに留める．
#    適用は CMakeLists.txt の1箇所に集約されている（FMP3_LDSCRIPT_VIA_DRIVER_T
#    で分岐。本ターゲットはこの変数を一切設定しないため既定の OFF のまま
#    ＝-Wl,-T, が使われる。picolibc.specs のような自動リンカスクリプト
#    注入は arm-none-eabi 既定の newlib specs には無いため，polarfire の
#    QEMU/picolibc 専用の問題は起きない。詳細は CMakeLists.txt の該当
#    コメントと計画A2 Task 1 参照）．
#
#  ★--gc-sections は使わない：上流 Makefile.chip / Makefile.core /
#    Makefile.target / sample/Makefile のいずれにも --gc-sections /
#    -ffunction-sections / -fdata-sections が無いことを実測で確認済み
#    （polarfire は l2lim 256KiB の制約から --gc-sections を必要としたが，
#    musca_b1 の FLASH 8MB/RAM 512KB にはその制約が無い）．したがって
#    polarfire 固有の cfg1_out --no-gc-sections ワークアラウンド
#    （FMP3_CFG1_OUT_LINK_OPTIONS）も不要（Task 4 で cfg1_out.syms に
#    TOPPERS_magic_number が --no-gc-sections 無しでも残ることを確認する）．
#
