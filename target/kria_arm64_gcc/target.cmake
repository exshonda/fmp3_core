#
#		ターゲット依存部の CMake 定義（KRIA SOM Cortex-A53(AArch64) / QEMU 用）
#
#  上流 target/kria_arm64_gcc/Makefile.target の CMake 版．
#
set(ARCHDIR ${FMP3_ROOT_DIR}/arch/arm64_gcc)
get_filename_component(CHIPDIR ${CMAKE_CURRENT_LIST_DIR}/../../arch/arm64_gcc/zynqmp ABSOLUTE)
set(TARGETDIR ${CMAKE_CURRENT_LIST_DIR})

#
#  ダンプするセクションの指定（Makefile.target:97  DUMPOPTS）．
#
#  ★ROM イメージ「形式」自体（FMP3_DUMP_FORMAT = dump）は，現物確認の
#    結果 arch/arm64_gcc/common/Makefile.core:34 側（コア共通層）の定義
#    なので arch.cmake に置いた（このファイルではなく）．DUMPOPTS の方は
#    Makefile.target 自身が定義しているので，ここに置く．
#
set(FMP3_DUMPOPTS "-j .text -j .rodata")

#
#  対象 Kria ボード（バナー表示名のみに影響．PS は全 Kria で同一．
#  Makefile.target:16-36）
#
set(FMP3_BOARD "kr260" CACHE STRING "Kria board name (kr260 / kv260 / kd240)")
set_property(CACHE FMP3_BOARD PROPERTY STRINGS kr260 kv260 kd240)
if(FMP3_BOARD STREQUAL "kr260")
    list(APPEND FMP3_COMPILE_DEFS TOPPERS_KRIA_KR260)
elseif(FMP3_BOARD STREQUAL "kv260")
    list(APPEND FMP3_COMPILE_DEFS TOPPERS_KRIA_KV260)
elseif(FMP3_BOARD STREQUAL "kd240")
    list(APPEND FMP3_COMPILE_DEFS TOPPERS_KRIA_KD240)
else()
    message(FATAL_ERROR "FMP3_BOARD must be kr260, kv260 or kd240 (got: ${FMP3_BOARD})")
endif()

#  Makefile.target:41-43  FPUサポート
list(APPEND FMP3_COMPILE_DEFS USE_ARM64_FPU)

#
#  単独動作（SYSMON 未定義）のメモリ配置（Makefile.target:70-83）．
#  ATF_S/ATF_NS（外部ATF連携）は本計画のスコープ外（QEMUにATFを用意する
#  手段が無いため）．
#
if(NOT DEFINED FMP3_MEM_BASE)
    set(FMP3_MEM_BASE 0x00000000)
endif()
if(NOT DEFINED FMP3_MEM_SIZE)
    set(FMP3_MEM_SIZE 0x40000000)
endif()
list(APPEND FMP3_COMPILE_DEFS
    TOPPERS_TZ_S
    TOPPERS_MEM_BASE=${FMP3_MEM_BASE}
    TOPPERS_MEM_SIZE=${FMP3_MEM_SIZE}
    TOPPERS_32BIT_ABOVE_ADDR
    USE_ARM64_MMU_CONFIG_TABLE
)

#
#  Makefile.target:83  TEXT_START_ADDRESS=$(TOPPERS_MEM_BASE) と
#  sample/Makefile:364-365 の `ifdef TEXT_START_ADDRESS: LDFLAGS +=
#  -Wl,-Ttext,$(TEXT_START_ADDRESS)` に相当．
#
#  ★現物確認で判明した移し漏れ（brief には無かった）：kria.ld には
#    MEMORY ブロックも先頭の `. = <addr>;` も無く，.text の配置は
#    もっぱら -Ttext（相当）に委ねられている．実測（aarch64-none-elf-gcc
#    14.3.1・-nostdlib）で -Ttext 無指定だと .text は既定の 0x400000 に
#    置かれ，付けると指定アドレスに置かれることを readelf -l で確認済み。
#    これを積まないと FMP3_MEM_BASE=0x0 前提のコード（リセットベクタ等）
#    と実際のロードアドレスがずれる．cfg1_out・fmp の両方のリンクに
#    掛かる必要があるため（上流も無印 LDFLAGS＝両方に効く変数に足して
#    いる），FMP3_CFG1_OUT_LINK_OPTIONS ではなく汎用の FMP3_LINK_OPTIONS
#    に積む．
#
list(APPEND FMP3_LINK_OPTIONS -Wl,-Ttext,${FMP3_MEM_BASE})

#  Makefile.target:88-92
list(APPEND FMP3_INCLUDE_DIRS ${TARGETDIR})
list(APPEND FMP3_COMPILE_OPTIONS -mlittle-endian -gdwarf-4 -gstrict-dwarf)
list(APPEND FMP3_LINK_OPTIONS -mlittle-endian -Wl,--build-id=none)
list(APPEND FMP3_CFG1_OUT_LINK_OPTIONS -nostdlib)

#  Makefile.target:100-115  SIOP：SYSMON 未定義（単独動作）は XUART1
list(APPEND FMP3_COMPILE_DEFS USE_XUART1)

#  Makefile.target:130-132  TECS を使用しない
list(APPEND FMP3_COMPILE_DEFS TOPPERS_OMIT_TECS)

#
#  カーネルに含めるターゲット依存ソース（Makefile.target:120-122）
#
list(APPEND FMP3_TARGET_C_FILES
    ${TARGETDIR}/target_kernel_impl.c
)
list(APPEND FMP3_ARCH_C_FILES
    ${ARCHDIR}/common/psci_support.S
)

#  Makefile.target:127
set(FMP3_LDSCRIPT ${TARGETDIR}/kria.ld)

#
#  cfg に渡すファイル
#
list(APPEND FMP3_CFG_FILES            ${TARGETDIR}/target_kernel.cfg)
list(APPEND FMP3_CLASS_TRB_FILES      ${TARGETDIR}/target_class.py)
list(APPEND FMP3_KERNEL_CFG_TRB_FILES ${TARGETDIR}/target_kernel.py)
list(APPEND FMP3_CHECK_TRB_FILES      ${TARGETDIR}/target_check.py)

#
#  ★--gc-sections は使わない：上流 Makefile.chip / Makefile.core /
#    Makefile.target / sample/Makefile のいずれにも --gc-sections /
#    -ffunction-sections / -fdata-sections が無いことを実測で確認済み
#    （musca_b1_gcc と同じ理由．target.cmake:83-90 参照）．したがって
#    polarfire 固有の cfg1_out --no-gc-sections ワークアラウンドも不要．
#

#
#  チップ依存部
#
include(${CHIPDIR}/chip.cmake)

#
#  QEMU（xlnx-zcu102）．v11.0.1 以降が必要（APU RVBAR／CRF リセット制御・
#  RPU クラスタ実装のバージョン帯。/usr/bin/qemu-system-aarch64 は 8.2.2
#  のため既定では使わない。musca_b1/target.cmake と同じバージョン
#  チェック方式）。
#
#  ★実測（Task 7）：/home/honda/qemu-build/install/bin/ には
#    qemu-system-arm（musca_b1用）しかインストールされておらず，
#    qemu-system-aarch64 は存在しない。11.0.1 の aarch64 バイナリは
#    ビルドツリー配下 qemu-11.0.1/build-a64/ に置かれたまま（未
#    `make install`）だったため，そちらもフォールバック候補に加える。
#    将来 install/bin に aarch64 が入れば，そちらが優先される。
set(_fmp3_kria_arm64_qemu_candidates
    /home/honda/qemu-build/install/bin/qemu-system-aarch64
    /home/honda/qemu-build/qemu-11.0.1/build-a64/qemu-system-aarch64
)
set(_fmp3_kria_arm64_qemu_default qemu-system-aarch64)
foreach(_fmp3_kria_arm64_qemu_cand ${_fmp3_kria_arm64_qemu_candidates})
    if(EXISTS ${_fmp3_kria_arm64_qemu_cand})
        set(_fmp3_kria_arm64_qemu_default ${_fmp3_kria_arm64_qemu_cand})
        break()
    endif()
endforeach()
set(QEMU_SYSTEM_AARCH64_KRIA ${_fmp3_kria_arm64_qemu_default} CACHE STRING
    "Path to qemu-system-aarch64 for the xlnx-zcu102 machine (needs >= 11.0.1)")
unset(_fmp3_kria_arm64_qemu_candidates)
unset(_fmp3_kria_arm64_qemu_cand)
unset(_fmp3_kria_arm64_qemu_default)

execute_process(
    COMMAND ${QEMU_SYSTEM_AARCH64_KRIA} --version
    OUTPUT_VARIABLE _fmp3_qemu_version_output
    RESULT_VARIABLE _fmp3_qemu_version_result
    OUTPUT_STRIP_TRAILING_WHITESPACE
)
if(_fmp3_qemu_version_result EQUAL 0 AND _fmp3_qemu_version_output MATCHES "version ([0-9]+)\\.")
    set(_fmp3_qemu_major ${CMAKE_MATCH_1})
    message(STATUS "fmp3_core: QEMU_SYSTEM_AARCH64_KRIA version = ${_fmp3_qemu_version_output}")
    if(_fmp3_qemu_major LESS 11)
        message(WARNING
            "fmp3_core: QEMU_SYSTEM_AARCH64_KRIA='${QEMU_SYSTEM_AARCH64_KRIA}' reports "
            "major version ${_fmp3_qemu_major} (< 11). The xlnx-zynqmp RPU cluster / "
            "APU RVBAR-CRF reset control this target relies on may be missing. "
            "Override with -DQEMU_SYSTEM_AARCH64_KRIA=<path to qemu-system-aarch64 >= 11.0.1>.")
    endif()
    unset(_fmp3_qemu_major)
else()
    message(WARNING
        "fmp3_core: could not determine the version of "
        "QEMU_SYSTEM_AARCH64_KRIA='${QEMU_SYSTEM_AARCH64_KRIA}' (is it installed / on PATH?).")
endif()
unset(_fmp3_qemu_version_output)
unset(_fmp3_qemu_version_result)

#
#  QEMU による実行（cmake --build <dir> --target run）
#
#  ★target_exit()（target_kernel_impl.c）はセミホスティング終了を持たず
#    while(true) の無限ループのため，QEMU は自然終了しない。実行検証は
#    timeout 併用が前提（musca_b1/polarfireと同様）．
#
#  ★secure=on で EL3 起動（TOPPERS_TZ_S の単独動作前提と一致）．
#  ★-smp の数はビルドの TNUM_PRCID（FMP3_PRC_NUM，既定4）と一致させる
#    こと（一致しないと Task 7 の判定に影響する）。既定はターゲットの
#    target_kernel.h の既定値である4に合わせる．
#
if(NOT FMP3_PRC_NUM STREQUAL "")
    set(_fmp3_kria_arm64_smp ${FMP3_PRC_NUM})
else()
    set(_fmp3_kria_arm64_smp 4)
endif()
#  実測（Task 7）で判明: 本ターゲットは USE_XUART1（上記, Makefile.target:
#  100-115 由来）でコンソールを UART1 (0xFF010000) に出す。QEMU の
#  hw/arm/xlnx-zynqmp.c は uart[i] に serial_hd(i) を割り当てる
#  (uart_addr = {0xFF000000, 0xFF010000})。-serial を1個しか渡さないと
#  index0=UART0 に繋がり、カーネルが書き込む UART1 にはバックエンドが無く
#  コンソール出力がエラーも出さず黙って消える（QEMUの-serial割当ての罠。
#  実機やゲスト側からは検知できない）。-serial を2個
#  (UART0=null, UART1=mon:stdio) 渡す必要がある。
set(FMP3_RUN_COMMAND
    ${QEMU_SYSTEM_AARCH64_KRIA} -M xlnx-zcu102,secure=on -smp ${_fmp3_kria_arm64_smp} -m 2G
    -nographic -serial null -serial mon:stdio
    -kernel $<TARGET_FILE:fmp>
)
unset(_fmp3_kria_arm64_smp)
