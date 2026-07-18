#
#		ターゲット依存部の CMake 定義（PolarFire SoC Kit 用）
#
#  上流 target/polarfire_soc_kit_gcc/Makefile.target の CMake 版．
#  Task 2 で中身を入れる．
#
set(TARGETDIR ${FMP3_TARGET_DIR})

list(APPEND FMP3_INCLUDE_DIRS ${TARGETDIR})
