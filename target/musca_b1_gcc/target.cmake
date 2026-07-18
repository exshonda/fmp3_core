#
#		ターゲット依存部の CMake 定義（ARM Musca-B1 用）
#
#  上流 target/musca_b1_gcc/Makefile.target の CMake 版．Task 3 で中身を
#  入れる．
#
set(TARGETDIR ${FMP3_TARGET_DIR})

list(APPEND FMP3_INCLUDE_DIRS ${TARGETDIR})
