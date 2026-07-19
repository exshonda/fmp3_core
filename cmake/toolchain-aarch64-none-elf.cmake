#
#		ツールチェーンファイル（AArch64 ベアメタル：aarch64-none-elf）
#
#  既定は aarch64-none-elf．別のプレフィックスを使う場合は
#  -DAARCH64_NONE_ELF_TOOLCHAIN_PREFIX=... で上書きできる．
#
#  実測（2026-07-19）: /usr/local/tools/arm-gnu-toolchain-14.3.rel1-
#  x86_64-aarch64-none-elf/bin が PATH に含まれ aarch64-none-elf-gcc
#  14.3.1 が解決する．aarch64-none-elf-gcc -dumpmachine は
#  "aarch64-none-elf" を返す（実測済み）．PATH に無い環境では
#  -DAARCH64_NONE_ELF_TOOLCHAIN_PREFIX=<full-path>/aarch64-none-elf-
#  のようにフルパスを渡す．
#
#  実行ファイルに拡張子を付けない（cmake/toolchain-riscv64.cmake・
#  cmake/toolchain-arm-none-eabi.cmake と同じ理由．上流 sample/Makefile の
#  OBJNAME = fmp と同じ名前にして，Makefile ビルドとの突き合わせを容易に
#  するため）．
#
set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR aarch64)

#
#  cmake/toolchain_check.cmake が照合する「このツールチェーンファイルが
#  期待する -dumpmachine パターン」．実測: aarch64-none-elf-gcc -dumpmachine
#  は "aarch64-none-elf" を返す．
#
#  «未定義のときだけ» 既定を与える（cmake/toolchain-riscv64.cmake・
#  cmake/toolchain-arm-none-eabi.cmake と同じ理由．素の set() だと
#  コマンドラインの -D を黙って上書きしてしまい，
#  -DFMP3_EXPECTED_TOOLCHAIN_MACHINE=<pattern> という上書き手段の案内が
#  「効かない案内＝嘘」になる）．
#
if(NOT DEFINED FMP3_EXPECTED_TOOLCHAIN_MACHINE)
    set(FMP3_EXPECTED_TOOLCHAIN_MACHINE aarch64-none-elf)
endif()

if(NOT DEFINED AARCH64_NONE_ELF_TOOLCHAIN_PREFIX)
    set(AARCH64_NONE_ELF_TOOLCHAIN_PREFIX aarch64-none-elf-)
endif()

set(CMAKE_C_COMPILER   ${AARCH64_NONE_ELF_TOOLCHAIN_PREFIX}gcc)
set(CMAKE_ASM_COMPILER ${AARCH64_NONE_ELF_TOOLCHAIN_PREFIX}gcc)
set(CMAKE_OBJCOPY      ${AARCH64_NONE_ELF_TOOLCHAIN_PREFIX}objcopy CACHE FILEPATH "objcopy")
set(CMAKE_NM           ${AARCH64_NONE_ELF_TOOLCHAIN_PREFIX}nm      CACHE FILEPATH "nm")
set(CMAKE_OBJDUMP      ${AARCH64_NONE_ELF_TOOLCHAIN_PREFIX}objdump CACHE FILEPATH "objdump")

#  ベアメタルではリンクできる完全な実行ファイルを作れないため，
#  try_compile はスタティックライブラリで行う
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
