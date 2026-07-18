#
#		ツールチェーンファイル（ARM Cortex-M ベアメタル：arm-none-eabi）
#
#  既定は arm-none-eabi．別のプレフィックスを使う場合は
#  -DARM_NONE_EABI_TOOLCHAIN_PREFIX=... で上書きできる．
#
#  実行ファイルに拡張子を付けない（cmake/toolchain-riscv64.cmake と同じ
#  理由．上流 sample/Makefile の OBJNAME = fmp と同じ名前にして，Makefile
#  ビルドとの突き合わせを容易にするため）．参考実装 asp3_core の
#  cmake/toolchain-arm-none-eabi.cmake は CMAKE_EXECUTABLE_SUFFIX を .elf に
#  しているが，fmp3_core では riscv64 と作法を揃えて拡張子を付けない．
#
set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR arm)

#
#  cmake/toolchain_check.cmake が照合する「このツールチェーンファイルが
#  期待する -dumpmachine パターン」．arm-none-eabi-gcc -dumpmachine は
#  そのまま "arm-none-eabi" を返す（riscv64-unknown-elf-gcc の
#  "riscv64-unknown-elf" と同型．実測済み）．
#
#  «未定義のときだけ» 既定を与える（cmake/toolchain-riscv64.cmake と同じ
#  理由．素の set() だとコマンドラインの -D を黙って上書きしてしまい，
#  -DFMP3_EXPECTED_TOOLCHAIN_MACHINE=<pattern> という上書き手段の案内が
#  「効かない案内＝嘘」になる）．
#
if(NOT DEFINED FMP3_EXPECTED_TOOLCHAIN_MACHINE)
    set(FMP3_EXPECTED_TOOLCHAIN_MACHINE arm-none-eabi)
endif()

if(NOT DEFINED ARM_NONE_EABI_TOOLCHAIN_PREFIX)
    set(ARM_NONE_EABI_TOOLCHAIN_PREFIX arm-none-eabi-)
endif()

set(CMAKE_C_COMPILER   ${ARM_NONE_EABI_TOOLCHAIN_PREFIX}gcc)
set(CMAKE_ASM_COMPILER ${ARM_NONE_EABI_TOOLCHAIN_PREFIX}gcc)
set(CMAKE_OBJCOPY      ${ARM_NONE_EABI_TOOLCHAIN_PREFIX}objcopy CACHE FILEPATH "objcopy")
set(CMAKE_NM           ${ARM_NONE_EABI_TOOLCHAIN_PREFIX}nm      CACHE FILEPATH "nm")
set(CMAKE_OBJDUMP      ${ARM_NONE_EABI_TOOLCHAIN_PREFIX}objdump CACHE FILEPATH "objdump")

#  ベアメタルではリンクできる完全な実行ファイルを作れないため，
#  try_compile はスタティックライブラリで行う
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
