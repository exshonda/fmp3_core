#
#		チップ依存部の CMake 定義（ZynqMP RPU用）
#
#  target.cmake から include される（上流 Makefile.chip に相当）．
#
set(COREDIR ${ARCHDIR}/common)

#
#  コンパイルオプション（Makefile.chip:16-19）
#
#  Cortex-R5（Rプロファイル）向け．FPU関連はターゲット依存部（target.cmake）
#  が指定する。
#
list(APPEND FMP3_COMPILE_OPTIONS -mcpu=cortex-r5)
list(APPEND FMP3_LINK_OPTIONS -mcpu=cortex-r5)
list(APPEND FMP3_COMPILE_DEFS
    __TARGET_ARCH_ARM=7
    __TARGET_PROFILE_R
    TOPPERS_CORTEX_R5
    TARGET_RESET_ENTRY=start_r5
)

list(APPEND FMP3_INCLUDE_DIRS ${CHIPDIR})

#
#  カーネルに関する定義（Makefile.chip:24-26,29）
#
#  ★gic_support.o（KERNEL_ASMOBJS, Makefile.chip:29）と gic_kernel_impl.o
#    （KERNEL_COBJS, Makefile.chip:28）はどちらも arm_gcc/common/ 側の
#    ファイルを指す（現物確認：gic_kernel_impl.c/.h は
#    arch/arm_gcc/zynqmp_r5/ には無く arch/arm_gcc/common/ にのみ存在する。
#    KERNEL_DIRS が CHIPDIR と COREDIR の両方を含むため，このチップ層が
#    拾える）。ブリーフの Step 2 コード例は ${CHIPDIR}/gic_kernel_impl.c
#    としていたが，configure 時に「ファイルが無い」エラーになることを
#    実測して COREDIR に修正した。FMP3_START_FILES ではなく libfmp3.a側の
#    FMP3_ARCH_C_FILES に含める。
#
list(APPEND FMP3_ARCH_C_FILES
    ${CHIPDIR}/chip_kernel_impl.c
    ${COREDIR}/gic_kernel_impl.c
    ${CHIPDIR}/ttc_hrt.c
    ${COREDIR}/gic_support.S
)
list(APPEND FMP3_START_FILES
    ${CHIPDIR}/chip_support.S
)

#
#  非TECS版 SIO ドライバ（Makefile.chip:34、Cadence UART。SYSSVC_DIRS）
#
list(APPEND FMP3_SYSSVC_TARGET_C_FILES
    ${CHIPDIR}/chip_serial.c
    ${CHIPDIR}/xuartps.c
)

#
#  コア依存部（Makefile.chip:39）
#
include(${COREDIR}/arch.cmake)
