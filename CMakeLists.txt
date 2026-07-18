#
#		TOPPERS/FMP3 Core CMake ビルド
#
#  使い方（例）：
#    cmake --preset polarfire_soc_kit-qemu
#    cmake --build build/polarfire_soc_kit-qemu
#    cmake --build build/polarfire_soc_kit-qemu --target run
#
cmake_minimum_required(VERSION 3.23)

project(fmp3_core C ASM)

include(${CMAKE_CURRENT_LIST_DIR}/fmp3_core.cmake)

#
#  ライブラリ専用モード（外部 SDK アプリ向け．esp32p4 が要求する）
#
#  ON のとき libfmp3.a までを作り，fmp 実行ファイル・run ターゲット・
#  pass3 の POST_BUILD を作らない．
#
option(FMP3_LIBRARY_ONLY
    "Build only the fmp3 kernel library (no fmp executable)" OFF)

#
#  プロセッサ数（TNUM_PRCID）
#
#  上流 sample/Makefile:193-194 の PRC_NUM に相当する．指定時のみ
#  -DTNUM_PRCID=<N> をコンパイル定義に足す．未指定ならターゲットの
#  target_kernel.h の既定値（polarfire は 4）が使われる．
#
#  ★これは CMake の分岐材料ではない．ファイルリストやターゲット構成を
#    この値で分岐させてはならない（TNUM_PRCID は cfg1_out をリンクして
#    初めて cfg が知る値であり，CMake から見える保証がない）．
#
set(FMP3_PRC_NUM "" CACHE STRING
    "Number of processors (TNUM_PRCID). Empty = target default")

#
#  ターゲット依存部の情報を読み込み
#
#  target.cmake は以下の変数を積み上げ，チップ依存部の chip.cmake を
#  include する：
#    FMP3_CFG_FILES / FMP3_KERNEL_CFG_TRB_FILES / FMP3_CHECK_TRB_FILES /
#    FMP3_OFFSET_TRB_FILES / FMP3_CLASS_TRB_FILES / FMP3_SYMVAL_TABLES /
#    FMP3_API_TABLES / FMP3_INCLUDE_DIRS / FMP3_COMPILE_DEFS /
#    FMP3_COMPILE_OPTIONS / FMP3_LINK_OPTIONS / FMP3_CFG1_OUT_LINK_OPTIONS /
#    FMP3_LINK_LIBS / FMP3_LDSCRIPT / FMP3_ARCH_C_FILES / FMP3_TARGET_C_FILES /
#    FMP3_SYSSVC_TARGET_C_FILES / FMP3_START_FILES / FMP3_RUN_COMMAND
#
include(${FMP3_TARGET_DIR}/target.cmake)

if(NOT FMP3_PRC_NUM STREQUAL "")
    list(APPEND FMP3_COMPILE_DEFS TNUM_PRCID=${FMP3_PRC_NUM})
endif()

message(STATUS "fmp3_core: FMP3_ROOT_DIR   = ${FMP3_ROOT_DIR}")
message(STATUS "fmp3_core: FMP3_TARGET     = ${FMP3_TARGET}")
message(STATUS "fmp3_core: FMP3_TARGET_DIR = ${FMP3_TARGET_DIR}")
message(STATUS "fmp3_core: FMP3_BOARD      = ${FMP3_BOARD}")
message(STATUS "fmp3_core: FMP3_RISCV_MARCH= ${FMP3_RISCV_MARCH}")
message(STATUS "fmp3_core: FMP3_RISCV_SPECS= ${FMP3_RISCV_SPECS}")
message(STATUS "fmp3_core: FMP3_PRC_NUM    = '${FMP3_PRC_NUM}'")
message(STATUS "fmp3_core: FMP3_COMPILE_DEFS= ${FMP3_COMPILE_DEFS}")
message(STATUS "fmp3_core: FMP3_ARCH_C_FILES count = ")
foreach(f IN LISTS FMP3_ARCH_C_FILES)
    message(STATUS "    arch: ${f}")
endforeach()
message(STATUS "fmp3_core: FMP3_LDSCRIPT   = ${FMP3_LDSCRIPT}")
message(STATUS "fmp3_core: FMP3_CFG1_OUT_LINK_OPTIONS = ${FMP3_CFG1_OUT_LINK_OPTIONS}")
