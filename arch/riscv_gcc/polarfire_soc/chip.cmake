#
#		チップ依存部の CMake 定義（PolarFire SoC 用）
#
#  target.cmake から include される（上流 Makefile.chip に相当）．
#
#  外部（SDK）ターゲットのパス解決規約（fmp3_esp_idf / fmp3_pico_sdk 等の
#  統合リポジトリから submodule として使われる場合に備える）：
#   - 共通 arch（arch/riscv_gcc/common）は fmp3_core 側＝ARCHDIR
#   - チップ依存部（polarfire_soc）・target 依存部は，これを include する
#     target.cmake 側で解決される＝CHIPDIR／TARGETDIR
#  ARCHDIR／CHIPDIR／TARGETDIR は呼び出し元の target.cmake が設定済み
#  （本ファイルではハードコードしない）．今の polarfire_soc_kit_gcc は
#  chip も target も fmp3_core リポジトリ内にあるため CHIPDIR は
#  ${FMP3_ROOT_DIR}/arch/riscv_gcc/polarfire_soc と同じ値になるが，
#  それは target.cmake が CMAKE_CURRENT_LIST_DIR 相対で計算した結果であり，
#  本ファイルが決め打ちしているわけではない．
#
set(COREDIR ${ARCHDIR}/common)

#
#  ISA と ABI
#
#  上流 Makefile.chip:25 は -march=rv64gc．ISA としては rv64imafdc と同一
#  だが，綴りで multilib の解決先が変わる．rv64gc は
#  rv64imafdc/lp64d に解決され，Ubuntu の picolibc-riscv64-unknown-elf
#  1.8.6-2 にはそのディレクトリが実在しないため crt0.o が見つからない．
#  rv64imafdc と綴ると既定ディレクトリ . に解決され，picolibc はそこに
#  crt0.o/libc.a を置いているのでリンクできる（ツールチェーンの -march
#  既定が rv64imafdc_zicsr であるため）．よって rv64imafdc を既定とする．
#  ABI は lp64d のまま変わらず，圧縮命令(C)も維持されるのでコードサイズが
#  小さくなる．
#
set(FMP3_RISCV_MARCH "rv64imafdc" CACHE STRING
    "RISC-V ISA string passed to -march (same ISA as upstream rv64gc; this spelling resolves to picolibc's default multilib)")

#
#  C ライブラリの specs
#
#  上流 Makefile.chip:24 は nano.specs（SoftConsole 同梱の newlib-nano）
#  を既定とし，QEMU ビルドでは Makefile.target:20 が picolibc.specs へ
#  差し替える．同じ切り分けを POLARFIRE_QEMU で行う（target.cmake が設定）．
#
if(NOT DEFINED FMP3_RISCV_SPECS)
    set(FMP3_RISCV_SPECS "--specs=nano.specs")
endif()

list(APPEND FMP3_INCLUDE_DIRS
    ${CHIPDIR}
)

#  Makefile.chip:25-27
list(APPEND FMP3_COMPILE_OPTIONS
    -march=${FMP3_RISCV_MARCH}
    -mabi=lp64d
    -mcmodel=medany
    -msmall-data-limit=8
    -mstrict-align
    -mno-save-restore
    -fsigned-char
    -ffunction-sections
    -fdata-sections
    ${FMP3_RISCV_SPECS}
)

#  リンク時にも ISA/ABI/specs を渡す（gcc をリンカドライバとして使うため）
list(APPEND FMP3_LINK_OPTIONS
    -march=${FMP3_RISCV_MARCH}
    -mabi=lp64d
    -mcmodel=medany
    ${FMP3_RISCV_SPECS}
    -nostartfiles          #  Makefile.chip:28
)

#
#  カーネルに含めるチップ依存ソース（Makefile.chip:36,42,44）
#
#  plic_kernel_impl.c / msi_ipi.c / mtimer.c は COREDIR にあるが，
#  「PLIC と Machine Timer と MSI-IPI を使う」というのはチップの決定なので，
#  上流 Makefile.chip と同じくここで選ぶ．
#
list(APPEND FMP3_ARCH_C_FILES
    ${CHIPDIR}/chip_kernel_impl.c
    ${CHIPDIR}/chip_support.S
    ${COREDIR}/plic_kernel_impl.c
    ${COREDIR}/msi_ipi.c
    ${COREDIR}/mtimer.c
)

#
#  非TECS版 SIO ドライバ（MMUART）
#  上流の configure.rb -S "... mmuart.o chip_serial.o" に対応
#
list(APPEND FMP3_SYSSVC_TARGET_C_FILES
    ${CHIPDIR}/chip_serial.c
    ${CHIPDIR}/mmuart.c
)

#
#  コア依存部（Makefile.chip:54）
#
include(${COREDIR}/arch.cmake)
