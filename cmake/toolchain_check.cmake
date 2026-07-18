#
#		ツールチェーン同定の検査
#
#  cmake/toolchain-riscv64.cmake は CMAKE_TRY_COMPILE_TARGET_TYPE を
#  STATIC_LIBRARY にしている（ベアメタルは完全なリンクができないため）．
#  この設定のせいで，-DCMAKE_TOOLCHAIN_FILE を渡し忘れてもホストの gcc で
#  try_compile が通ってしまい，「ビルドは通るのに間違ったコンパイラ」という
#  事故が起きる（実測：兄弟プロジェクト asp3_esp_idf/asp3/target/esp32c6_espidf/
#  target.cmake:18-33，build/ 配下 320 構成のうち 164 構成がホストの
#  Ubuntu 汎用 GCC でビルドされていた）．
#
#  configure 時に `${CMAKE_C_COMPILER} -dumpmachine` の出力を見て，
#  RISC-V ベアメタル向けでなければ即座に FATAL_ERROR で止める。
#  コンパイラは project() が実行されるまで確定しないため，本ファイルは
#  project() の後（fmp3_core.cmake の先頭）から include すること．
#
execute_process(
    COMMAND ${CMAKE_C_COMPILER} -dumpmachine
    OUTPUT_VARIABLE FMP3_C_COMPILER_MACHINE
    OUTPUT_STRIP_TRAILING_WHITESPACE
    RESULT_VARIABLE FMP3_DUMPMACHINE_RESULT
)

if(NOT FMP3_DUMPMACHINE_RESULT EQUAL 0)
    message(FATAL_ERROR
        "Failed to run '${CMAKE_C_COMPILER} -dumpmachine' (exit ${FMP3_DUMPMACHINE_RESULT}). "
        "CMAKE_C_COMPILER='${CMAKE_C_COMPILER}' does not look like a working compiler. "
        "Did you forget to pass a toolchain file "
        "(-DCMAKE_TOOLCHAIN_FILE=${FMP3_ROOT_DIR}/cmake/toolchain-riscv64.cmake), "
        "or use a CMake preset that sets it (e.g. --preset polarfire_soc_kit-qemu)?")
endif()

if(NOT FMP3_C_COMPILER_MACHINE MATCHES "riscv64")
    message(FATAL_ERROR
        "CMAKE_C_COMPILER ('${CMAKE_C_COMPILER}') reports target machine "
        "'${FMP3_C_COMPILER_MACHINE}' (via -dumpmachine), which is not a riscv64 "
        "toolchain. fmp3_core targets RISC-V bare metal only; configuring with a "
        "host compiler silently produces a binary for the HOST, not the target "
        "firmware, because CMAKE_TRY_COMPILE_TARGET_TYPE is STATIC_LIBRARY here and "
        "try_compile does not fail against a host gcc "
        "(this exact mistake caused 164/320 misbuilt configurations in the sibling "
        "asp3_esp_idf project: asp3/target/esp32c6_espidf/target.cmake:18-33). "
        "Fix: pass -DCMAKE_TOOLCHAIN_FILE=${FMP3_ROOT_DIR}/cmake/toolchain-riscv64.cmake "
        "(or use a CMake preset that sets it, e.g. --preset polarfire_soc_kit-qemu), and "
        "check that RISCV64_TOOLCHAIN_PREFIX (default 'riscv64-unknown-elf-') matches an "
        "installed RISC-V cross toolchain.")
endif()
