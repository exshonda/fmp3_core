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

#
#  cmake/toolchain_check.cmake が照合する「このツールチェーンファイルが
#  期待する -dumpmachine パターン」．ここでは RV64 なので riscv64．
#
#  «未定義のときだけ» 既定を与える（素の set() だとコマンドラインの -D を
#  黙って上書きしてしまい，-DFMP3_EXPECTED_TOOLCHAIN_MACHINE=<pattern> という
#  上書き手段の案内が「効かない案内＝嘘」になる。実際に asp3_esp_idf の
#  C5 で踏んだ罠：asp3/target/esp32c6_espidf/target.cmake:44-47 参照）。
#
#  申し送り（将来 ARM 系 = Cortex-M/A/R のツールチェーンファイルを足す人へ）：
#  同じ要領で自分のツールチェーンファイルにも FMP3_EXPECTED_TOOLCHAIN_MACHINE を
#  宣言すること（例 arm-none-eabi）。宣言を忘れても FATAL_ERROR にはならない
#  （toolchain_check.cmake は「期待値が未定義なら検査を行わない」設計）が，
#  その代わりツールチェーン誤りを検出できなくなる。STATUS ログに
#  「検査を行わなかった」旨が出るので，見落としていないか確認すること。
#
if(NOT DEFINED FMP3_EXPECTED_TOOLCHAIN_MACHINE)
    set(FMP3_EXPECTED_TOOLCHAIN_MACHINE riscv64)
endif()

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
