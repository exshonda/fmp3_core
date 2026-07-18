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
#  fmp3_core は RISC-V 専用ではない（musca_b1_gcc/rp2350_pico2_gcc は
#  Cortex-M，kria_arm64_gcc は AArch64，kria_r5_gcc は Cortex-R；ESP32-P4/C6
#  の RV32 も将来 CMake 化される）ので，「riscv64 でなければ弾く」という
#  固定判定は書けない．そこで判定基準はツールチェーンファイル自身に宣言
#  させる：各ツールチェーンファイルが FMP3_EXPECTED_TOOLCHAIN_MACHINE に
#  自分の -dumpmachine 期待パターンを設定し（例 cmake/toolchain-riscv64.cmake
#  は riscv64），本ファイルはそれと照合するだけにする．
#
#    - 期待値が宣言されていれば，-dumpmachine の出力と MATCHES で照合し，
#      一致しなければ FATAL_ERROR で止める．
#    - 期待値が宣言されておらず，かつ CMAKE_TOOLCHAIN_FILE も渡されていない
#      （＝ホスト gcc へのフォールバックが疑われる）場合は，FATAL_ERROR で止める．
#    - 期待値が宣言されていないが，何らかの CMAKE_TOOLCHAIN_FILE は渡されている
#      場合（＝期待値をまだ宣言していないツールチェーンファイル．将来の ARM 系
#      など）は，無関係な FATAL で止めない．ただし黙って素通りは最悪なので，
#      検査を行わなかった旨を message(STATUS ...) で必ず出す．
#
#  -DFMP3_EXPECTED_TOOLCHAIN_MACHINE=<pattern> で上書きできる．ツールチェーン
#  ファイル側は「未定義のときだけ既定を与える」ようにしてあるので（asp3_esp_idf
#  で実際に踏んだ「素の set() がコマンドラインの -D を黙って上書きする」罠は
#  ここでは起きない），この -D は常に効く．
#
#  configure 時に `${CMAKE_C_COMPILER} -dumpmachine` の出力を見る．
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

if(DEFINED FMP3_EXPECTED_TOOLCHAIN_MACHINE)
    if(NOT FMP3_C_COMPILER_MACHINE MATCHES "${FMP3_EXPECTED_TOOLCHAIN_MACHINE}")
        message(FATAL_ERROR
            "CMAKE_C_COMPILER ('${CMAKE_C_COMPILER}') reports target machine "
            "'${FMP3_C_COMPILER_MACHINE}' (via -dumpmachine), which does not match "
            "the expected pattern '${FMP3_EXPECTED_TOOLCHAIN_MACHINE}' "
            "(FMP3_EXPECTED_TOOLCHAIN_MACHINE, declared by the toolchain file and/or "
            "overridden with -DFMP3_EXPECTED_TOOLCHAIN_MACHINE=<pattern>). Configuring "
            "with a mismatched compiler silently produces a binary for the WRONG "
            "target, because CMAKE_TRY_COMPILE_TARGET_TYPE is STATIC_LIBRARY here and "
            "try_compile does not fail against a mismatched (even host) gcc "
            "(this exact mistake caused 164/320 misbuilt configurations in the sibling "
            "asp3_esp_idf project: asp3/target/esp32c6_espidf/target.cmake:18-33). "
            "Fix: pass the matching -DCMAKE_TOOLCHAIN_FILE (or use a CMake preset that "
            "sets it, e.g. --preset polarfire_soc_kit-qemu), check that the toolchain "
            "prefix variable (e.g. RISCV64_TOOLCHAIN_PREFIX) matches an installed cross "
            "toolchain, or override -DFMP3_EXPECTED_TOOLCHAIN_MACHINE=<pattern> if this "
            "toolchain file's default expectation genuinely does not apply here.")
    endif()
elseif(CMAKE_TOOLCHAIN_FILE)
    message(STATUS
        "fmp3_core: toolchain identity check skipped -- CMAKE_TOOLCHAIN_FILE="
        "'${CMAKE_TOOLCHAIN_FILE}' does not declare FMP3_EXPECTED_TOOLCHAIN_MACHINE "
        "(compiler '${CMAKE_C_COMPILER}' reports '${FMP3_C_COMPILER_MACHINE}' via "
        "-dumpmachine, unverified). This is expected for toolchain files that have "
        "not yet adopted the check; it is NOT verified against FMP3_TARGET. Add "
        "'set(FMP3_EXPECTED_TOOLCHAIN_MACHINE <pattern>)' to the toolchain file "
        "(see cmake/toolchain-riscv64.cmake) or pass "
        "-DFMP3_EXPECTED_TOOLCHAIN_MACHINE=<pattern> to enable it.")
else()
    message(FATAL_ERROR
        "No CMAKE_TOOLCHAIN_FILE was given, so CMAKE_C_COMPILER='${CMAKE_C_COMPILER}' "
        "is presumed to be the HOST compiler (it reports target machine "
        "'${FMP3_C_COMPILER_MACHINE}' via -dumpmachine, and no "
        "FMP3_EXPECTED_TOOLCHAIN_MACHINE was declared to check it against). fmp3_core "
        "is a bare-metal cross-build; configuring with the host compiler silently "
        "produces a binary for the HOST, not the target firmware, because "
        "CMAKE_TRY_COMPILE_TARGET_TYPE is STATIC_LIBRARY here and try_compile does "
        "not fail against a host gcc "
        "(this exact mistake caused 164/320 misbuilt configurations in the sibling "
        "asp3_esp_idf project: asp3/target/esp32c6_espidf/target.cmake:18-33). "
        "Fix: pass -DCMAKE_TOOLCHAIN_FILE=${FMP3_ROOT_DIR}/cmake/toolchain-riscv64.cmake "
        "(or use a CMake preset that sets it, e.g. --preset polarfire_soc_kit-qemu).")
endif()
