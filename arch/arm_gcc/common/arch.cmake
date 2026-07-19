#
#		アーキテクチャ依存部の CMake 定義（ARM(Cortex-R/A32) コア共通）
#
#  chip.cmake から include される（上流 Makefile.core に相当）．
#
#  本ファイルは常に fmp3_core（このリポジトリ）側にある共通コア層なので，
#  ${FMP3_ROOT_DIR} 基準で自己解決する．
#
#  ★上流 Makefile.core（本ファイルの翻訳元）は DUMP を設定しない（arm64_gcc
#    ・arm_m_gcc の Makefile.core とはこの点が異なる。現物
#    arch/arm_gcc/common/Makefile.core を確認済み。sample/Makefile:134 の
#    既定 `DUMP = srec` がそのまま生きるため，本ファイルは FMP3_DUMP_FORMAT
#    を宣言しない（汎用層 CMakeLists.txt の既定 srec のままになる）．
#
set(COREDIR ${FMP3_ROOT_DIR}/arch/arm_gcc/common)
set(TOOLDIR ${FMP3_ROOT_DIR}/arch/gcc)

#  Makefile.core:33  CFG_TABS
list(APPEND FMP3_SYMVAL_TABLES
    ${COREDIR}/core_sym.def
)

#  Makefile.core:38  TARGET_OFFSET_TRB
list(APPEND FMP3_OFFSET_TRB_FILES
    ${COREDIR}/core_offset.py
)

#  Makefile.core:19-20
list(APPEND FMP3_INCLUDE_DIRS
    ${COREDIR}
    ${TOOLDIR}
)

#  Makefile.core:27-28  KERNEL_ASMOBJS core_support.o / KERNEL_COBJS core_kernel_impl.o
list(APPEND FMP3_ARCH_C_FILES
    ${COREDIR}/core_kernel_impl.c
    ${COREDIR}/core_support.S
)

#  Makefile.core:45  START_OBJS := start.o $(START_OBJS)
#  ★kria_r5_gcc では zynqmp_r5/Makefile.chip が START_OBJS の先頭に
#    chip_support.o を積んでから本ファイルが include される（上流Make
#    の評価順）ため，最終的な START_OBJS は「chip_support.o start.o」
#    に相当する．CMakeのリストは追加順のみが意味を持ち，実際にどちらが
#    ENTRYになるかはリンカスクリプトのENTRY()ディレクティブ／既定の
#    _start解決で決まるため，ここでは順序を上流と厳密一致させることに
#    こだわらず両方をFMP3_START_FILESに含める．
list(APPEND FMP3_START_FILES
    ${COREDIR}/start.S
)

#  Makefile.core:51  LDFLAGS := -nostdlib $(LDFLAGS)
list(APPEND FMP3_LINK_OPTIONS -nostdlib)

#  Makefile.core:22（-lgcc のみ．libc は非リンク）
list(APPEND FMP3_LINK_LIBS gcc)
