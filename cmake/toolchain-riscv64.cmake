#
#		ツールチェーンファイル（RISC-V ベアメタル：RV64 / lp64d）
#
#  既定は riscv64-unknown-elf．別のプレフィックスを使う場合は
#  -DRISCV64_TOOLCHAIN_PREFIX=riscv64-elf- 等で上書きできる．
#
#  実行ファイルに拡張子を付けない（上流 sample/Makefile の OBJNAME = fmp と
#  同じ名前にして，Makefile ビルドとの突き合わせを容易にするため）．
#
set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR riscv64)

if(NOT DEFINED RISCV64_TOOLCHAIN_PREFIX)
    set(RISCV64_TOOLCHAIN_PREFIX riscv64-unknown-elf-)
endif()

set(CMAKE_C_COMPILER   ${RISCV64_TOOLCHAIN_PREFIX}gcc)
set(CMAKE_ASM_COMPILER ${RISCV64_TOOLCHAIN_PREFIX}gcc)
set(CMAKE_OBJCOPY      ${RISCV64_TOOLCHAIN_PREFIX}objcopy CACHE FILEPATH "objcopy")
set(CMAKE_NM           ${RISCV64_TOOLCHAIN_PREFIX}nm      CACHE FILEPATH "nm")
set(CMAKE_OBJDUMP      ${RISCV64_TOOLCHAIN_PREFIX}objdump CACHE FILEPATH "objdump")

#  ベアメタルではリンクできる完全な実行ファイルを作れないため，
#  try_compile はスタティックライブラリで行う
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
