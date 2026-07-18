#
#		TOPPERS/FMP3 Core CMake ビルド
#
#  使い方（例）：
#    cmake --preset polarfire_soc_kit-qemu
#    cmake --build build/polarfire_soc_kit-qemu
#    cmake --build build/polarfire_soc_kit-qemu --target run
#
cmake_minimum_required(VERSION 3.20)

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

message(STATUS "fmp3_core: FMP3_ROOT_DIR   = ${FMP3_ROOT_DIR}")
message(STATUS "fmp3_core: FMP3_TARGET     = ${FMP3_TARGET}")
message(STATUS "fmp3_core: FMP3_TARGET_DIR = ${FMP3_TARGET_DIR}")
