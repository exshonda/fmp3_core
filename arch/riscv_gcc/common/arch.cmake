#
#		アーキテクチャ依存部の CMake 定義（RISC-V コア共通）
#
#  chip.cmake から include される（上流 Makefile.core に相当）．
#  start.S はライブラリ外で先頭にリンクする（FMP3_START_FILES）．
#
#  本ファイルは常に fmp3_core（このリポジトリ）側にある共通コア層なので，
#  ${FMP3_ROOT_DIR} 基準で自己解決する（chip 依存部・target 依存部が
#  リポジトリ外に置かれる場合でも，この層だけは動かない）．
#
set(COREDIR ${FMP3_ROOT_DIR}/arch/riscv_gcc/common)
set(TOOLDIR ${FMP3_ROOT_DIR}/arch/gcc)

#  Makefile.core:33
list(APPEND FMP3_SYMVAL_TABLES
    ${COREDIR}/core_sym.def
)

#  Makefile.core:38  TARGET_OFFSET_TRB
list(APPEND FMP3_OFFSET_TRB_FILES
    ${COREDIR}/core_offset.trb
)

#  Makefile.core:20
list(APPEND FMP3_INCLUDE_DIRS
    ${COREDIR}
    ${TOOLDIR}
)

#  Makefile.core:27-28
list(APPEND FMP3_ARCH_C_FILES
    ${COREDIR}/core_kernel_impl.c
    ${COREDIR}/core_support.S
)

#  Makefile.core:45  START_OBJS
list(APPEND FMP3_START_FILES
    ${COREDIR}/start.S
)

#  Makefile.core:51
list(APPEND FMP3_LINK_OPTIONS
    -nostdlib
)

#  Makefile.core:21（-lgcc）と sample/Makefile:63（-lc）
list(APPEND FMP3_LINK_LIBS c gcc)
