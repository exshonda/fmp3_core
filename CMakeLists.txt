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
#  include する（現時点で実在する変数のみ列挙。FMP3_API_TABLES は
#  CMakeLists.txt 側で，FMP3_RUN_COMMAND は target.cmake 側で，
#  ともに将来 Task で追加予定）：
#    FMP3_CFG_FILES / FMP3_KERNEL_CFG_TRB_FILES / FMP3_CHECK_TRB_FILES /
#    FMP3_OFFSET_TRB_FILES / FMP3_CLASS_TRB_FILES / FMP3_SYMVAL_TABLES /
#    FMP3_INCLUDE_DIRS / FMP3_COMPILE_DEFS /
#    FMP3_COMPILE_OPTIONS / FMP3_LINK_OPTIONS / FMP3_CFG1_OUT_LINK_OPTIONS /
#    FMP3_LINK_LIBS / FMP3_LDSCRIPT / FMP3_ARCH_C_FILES / FMP3_TARGET_C_FILES /
#    FMP3_SYSSVC_TARGET_C_FILES / FMP3_START_FILES
#
include(${FMP3_TARGET_DIR}/target.cmake)

#
#  リンカスクリプトの適用（-T）はここ1箇所に集約する．
#
#  target.cmake は FMP3_LDSCRIPT を「設定するだけ」で -T 自体は積まない
#  （target.cmake 側のコメント参照）．-T をここでしか足さないことで，
#  将来 fmp 実行ファイルを組む Task が asp3_core 由来の
#    list(APPEND FMP3_LINK_OPTIONS -Wl,-T,${FMP3_LDSCRIPT})
#  をそのまま流用しても二重指定（ld の "linker script file '...' appears
#  multiple times" fatal error）にならない．
#
#  ★asp3_core と違い，QEMU（picolibc）と実機（newlib-nano）で書式を
#    変える必要がある（実測で確認済み）：
#    picolibc.specs は `%{!T:-Tpicolibc.ld}` を持ち，gcc ドライバの -T
#    スイッチが立っているかどうかだけで picolibc 既定のリンカスクリプトを
#    追加するか判定する．-Wl,-T,<file> は -Wl 経由でリンカへそのまま
#    転送されるだけで gcc の -T スイッチを立てないため，QEMU 側で
#    asp3_core と同じ -Wl,-T, を使うと picolibc.ld が「も」-T されて
#    しまう．実測（riscv64-unknown-elf-gcc 13.2.0 + picolibc 1.8.6-2，
#    mpfs-lim.ld）：
#      -T mpfs-lim.ld          → _start = 0x08000000（l2lim．意図通り）
#      -Wl,-T,mpfs-lim.ld のみ → _start = 0x80000000（picolibc.ld 既定）
#    ld はエラーにせず両方の SECTIONS を受理し，後着の picolibc.ld が
#    勝つ．**ビルドは通るのに実機では起動しない**という，fatal error
#    より発見しにくい壊れ方をする．上流 Makefile.target:106-107 が
#    QEMU=1 のとき素の -T を COPTS に混ぜているのも同じ理由．実機
#    （nano.specs）にはこの罠が無いので通常どおり -Wl,-T, を使う．
#
if(DEFINED FMP3_LDSCRIPT)
    if(POLARFIRE_QEMU)
        list(APPEND FMP3_LINK_OPTIONS -T ${FMP3_LDSCRIPT})
    else()
        list(APPEND FMP3_LINK_OPTIONS -Wl,-T,${FMP3_LDSCRIPT})
    endif()
endif()

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
message(STATUS "fmp3_core: FMP3_LINK_OPTIONS = ${FMP3_LINK_OPTIONS}")
