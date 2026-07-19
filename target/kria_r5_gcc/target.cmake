#
#		ターゲット依存部の CMake 定義（KRIA SOM Cortex-R5F / QEMU 用）
#
#  上流 target/kria_r5_gcc/Makefile.target の CMake 版．
#
set(ARCHDIR ${FMP3_ROOT_DIR}/arch/arm_gcc)
get_filename_component(CHIPDIR ${CMAKE_CURRENT_LIST_DIR}/../../arch/arm_gcc/zynqmp_r5 ABSOLUTE)
set(TARGETDIR ${CMAKE_CURRENT_LIST_DIR})

#
#  対象 Kria ボード（Makefile.target:16-30）
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

#
#  FPUサポートとABI（Makefile.target:32-38）
#
#  Cortex-R5F は VFPv3-D16 を持つ．-mfloat-abi=hard を使う．
#
#  ★Makefile.target の COPTS は -mfpu/-mfloat-abi を持つが LDFLAGS には
#    無い（現物確認）。しかし本 CMake 層では target_link_options(fmp ...)
#    が FMP3_LINK_OPTIONS のみを使い FMP3_COMPILE_OPTIONS を継承しない
#    （CMakeLists.txt:722）。実測（arm-none-eabi-gcc 13.2.1）で
#    -mfpu=vfpv3-d16 -mfloat-abi=hard の有無により
#    -print-libgcc-file-name が thumb/v7/nofp（無指定）→
#    thumb/v7+fp/hard（指定あり）と異なる multilib を指すことを確認した
#    （-lgcc の解決先が変わる）。したがって上流の綴りに反してでも
#    FMP3_LINK_OPTIONS 側にも積む（Makefile.chip の -mcpu 重複と同じ
#    理由・同じパターン）。
#
list(APPEND FMP3_COMPILE_DEFS USE_ARM_FPU_ALWAYS)
list(APPEND FMP3_COMPILE_OPTIONS -mfpu=vfpv3-d16 -mfloat-abi=hard)
list(APPEND FMP3_LINK_OPTIONS -mfpu=vfpv3-d16 -mfloat-abi=hard)

#
#  QEMU に関する定義（Makefile.target:40-51）
#
#  QEMU（xlnx-zcu102）のTTCモデルは133MHz固定（実機は100MHz）．既定は
#  QEMU向け（上流 Makefile.target 自身の既定 QEMU ?= true と同じ）．
#  実機向けには -DFMP3_KRIA_R5_QEMU=OFF でconfigureする．
#
option(FMP3_KRIA_R5_QEMU "Build for QEMU xlnx-zcu102 TTC clock (OFF: real Kria board, 100MHz TTC)" ON)
if(FMP3_KRIA_R5_QEMU)
    list(APPEND FMP3_COMPILE_DEFS TOPPERS_USE_QEMU TTC_CLK_HZ=133000000)
endif()

#  Makefile.target:56-59
list(APPEND FMP3_INCLUDE_DIRS ${TARGETDIR})
list(APPEND FMP3_COMPILE_OPTIONS -mlittle-endian)
list(APPEND FMP3_LINK_OPTIONS -mlittle-endian)
list(APPEND FMP3_COMPILE_DEFS
    USE_BYPASS_IPI_DISPATCH_HANDER
    TOPPERS_OMIT_USE_WFE
)

#  ★ブリーフの Step 3 コード例に無かった追加。sample/Makefile:191
#    （生成物には含まれない除外済みインフラ）が全ターゲット共通で
#    -DTOPPERS_OMIT_TECS を無条件付加しており，他の全 target.cmake
#    （musca_b1_gcc/rp2350_pico2_gcc/polarfire_soc_kit_gcc/kria_arm64_gcc）
#    がこの層でそれを再現している既存規約に倣う。実測：これが無いと
#    xuartps.h の SIOPCB 型定義部（#ifdef TOPPERS_OMIT_TECS で囲われている）
#    が丸ごと消え，chip_serial.h が「unknown type name 'SIOPCB'」で
#    コンパイルエラーになることを確認した。
list(APPEND FMP3_COMPILE_DEFS TOPPERS_OMIT_TECS)

#
#  カーネルに含めるターゲット依存ソース（Makefile.target:64-65）
#
list(APPEND FMP3_TARGET_C_FILES
    ${TARGETDIR}/target_kernel_impl.c
)

#  Makefile.target:70
set(FMP3_LDSCRIPT ${TARGETDIR}/kria_r5.ld)

#
#  cfg に渡すファイル
#
list(APPEND FMP3_CFG_FILES            ${TARGETDIR}/target_kernel.cfg)
list(APPEND FMP3_CLASS_TRB_FILES      ${TARGETDIR}/target_class.py)
list(APPEND FMP3_KERNEL_CFG_TRB_FILES ${TARGETDIR}/target_kernel.py)
list(APPEND FMP3_CHECK_TRB_FILES      ${TARGETDIR}/target_check.py)

#
#  チップ依存部
#
include(${CHIPDIR}/chip.cmake)

#
#  QEMU（xlnx-zcu102, RPUクラスタ）．上流Makefile.target:87-102の
#  runqu/runqugターゲットをそのまま翻訳する．v11.0以降が必要（上流
#  コメント自身が明記）．
#
set(_fmp3_kria_r5_qemu_builtin /home/honda/qemu-build/qemu-11.0.1/build-a64/qemu-system-aarch64)
if(EXISTS ${_fmp3_kria_r5_qemu_builtin})
    set(_fmp3_kria_r5_qemu_default ${_fmp3_kria_r5_qemu_builtin})
else()
    set(_fmp3_kria_r5_qemu_default qemu-system-aarch64)
endif()
set(QEMU_SYSTEM_AARCH64_KRIA_R5 ${_fmp3_kria_r5_qemu_default} CACHE STRING
    "Path to qemu-system-aarch64 for the xlnx-zcu102 RPU cluster (needs >= 11.0.1)")
unset(_fmp3_kria_r5_qemu_builtin)
unset(_fmp3_kria_r5_qemu_default)

#
#  Makefile.target:89-94 (runqu) の翻訳．
#  ★-smp 6 必須（R5Fが2個生成されるのは smp が A53(4個)を超えたときのみ，
#    xlnx-zynqmp.c:210-217 の xlnx_zynqmp_get_rpu_number()）．
#  ★boot-cpu=rpu-cpu[0] でRPU0を電源ON，mp-affinity=0で実機同様Aff0=0．
#  ★cpu-num=4：グローバルCPU index（A53が0-3、RPUが4,5）でRPU0を指す．
#
#  ★2コア（split mode, FMP3_PRC_NUM=2）は上記の1コア用コマンドでは動かない
#    （DIVERGENCE_MAP.md「解消済み事項」kria_r5_gcc QEMU実行検証の節を参照）。
#    QEMU の xlnx-zynqmp.c は boot-cpu に指定されなかった RPU を既定で
#    start-powered-off=true にするため，RPU1（PRC2）が起動せず，
#    barrier_sync() で PRC1 が無期限に停止する（バナー0行の無反応ハング）。
#    `-global xlnx-zynqmp.rpu-secondary-start=true` を立てて RPU1 を
#    reset から同時起動させ，cpu-num=5 の -device loader で RPU1 に
#    同じイメージを積む必要がある（kria_arm64/target.cmake の
#    FMP3_PRC_NUM 分岐と同じ形）。
#
if(FMP3_PRC_NUM STREQUAL "2")
    set(FMP3_RUN_COMMAND
        ${QEMU_SYSTEM_AARCH64_KRIA_R5} -M xlnx-zcu102 -smp 6 -m 2G -nographic
        -global xlnx-zynqmp.boot-cpu=rpu-cpu[0]
        -global xlnx-zynqmp.rpu-secondary-start=true
        -global cortex-r5f-arm-cpu.mp-affinity=0
        -device loader,file=$<TARGET_FILE:fmp>,cpu-num=4
        -device loader,file=$<TARGET_FILE:fmp>,cpu-num=5
        -serial null -serial mon:stdio
    )
else()
    set(FMP3_RUN_COMMAND
        ${QEMU_SYSTEM_AARCH64_KRIA_R5} -M xlnx-zcu102 -smp 6 -m 2G -nographic
        -global xlnx-zynqmp.boot-cpu=rpu-cpu[0]
        -global cortex-r5f-arm-cpu.mp-affinity=0
        -device loader,file=$<TARGET_FILE:fmp>,cpu-num=4
        -serial null -serial mon:stdio
    )
endif()
