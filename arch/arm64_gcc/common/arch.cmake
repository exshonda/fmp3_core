#
#		アーキテクチャ依存部の CMake 定義（ARM64 コア共通）
#
#  chip.cmake から include される（上流 Makefile.core に相当）．
#
#  本ファイルは常に fmp3_core（このリポジトリ）側にある共通コア層なので，
#  ${FMP3_ROOT_DIR} 基準で自己解決する（arch/riscv_gcc/common/arch.cmake・
#  arch/arm_m_gcc/common/arch.cmake と同じ作法）．
#
#  ★上流 Makefile.core（本ファイルの翻訳元）は START_OBJS / -nostdlib を
#    持たない（riscv_gcc・arm_m_gcc の Makefile.core とはこの点が異なる。
#    現物 arch/arm64_gcc/common/Makefile.core を確認済み）．start.S と
#    -nostdlib は arch/arm64_gcc/zynqmp/Makefile.chip 側にあり，チップ層
#    （chip.cmake）が積む．本ファイルでは扱わない．
#
set(COREDIR ${FMP3_ROOT_DIR}/arch/arm64_gcc/common)
set(TOOLDIR ${FMP3_ROOT_DIR}/arch/gcc)

#
#  ROM イメージ形式（Makefile.core:34  DUMP = dump）．
#
#  ★現物確認：この代入は arm64_gcc の Makefile.core（コア共通層）に
#    あり，kria_arm64_gcc の target/chip 層ではない（sample/Makefile:133-134
#    の `ifndef DUMP: DUMP = srec` を，include 順で後に読まれる本行が
#    無条件代入で上書きする．GNU make で実測して確認済み）．したがって
#    このフックは Makefile.core の翻訳先である本ファイルに置くのが，
#    将来 arm64_gcc に他チップ（imx8mm 等）を追加したときも含めて最も
#    上流に忠実．汎用層（CMakeLists.txt Task 3）の FMP3_DUMP_FORMAT
#    フックを使う（既定は srec のままなので，ここで宣言しないと srec に
#    なる）．
#
#  ★注意（懸念として報告する）：兄弟の arch/arm_m_gcc/common/Makefile.core:31
#    にも同じ無条件代入 `DUMP = dump` があり，同じ include 順の理屈で
#    musca_b1_gcc / rp2350_pico2_gcc も本来は dump 形式になるはずだが，
#    それらの target.cmake は FMP3_DUMP_FORMAT を宣言しておらず現状 srec
#    のままになっている（本 Task の範囲外のため変更しない．Task 6 の
#    報告に記載）．
#
set(FMP3_DUMP_FORMAT dump)

#  Makefile.core:44  CFG_TABS
list(APPEND FMP3_SYMVAL_TABLES
    ${COREDIR}/core_sym.def
)

#  Makefile.core:47  TARGET_OFFSET_TRB
list(APPEND FMP3_OFFSET_TRB_FILES
    ${COREDIR}/core_offset.py
)

#  Makefile.core:19-20
list(APPEND FMP3_INCLUDE_DIRS
    ${COREDIR}
    ${TOOLDIR}
)

#  Makefile.core:30-40  KERNEL_ASMOBJS core_support.o gic_support.o /
#  KERNEL_COBJS core_kernel_impl.o core_timer.o arm64.o gic_kernel_impl.o
#  （本ターゲットは OMIT_CORE_TIMER 未設定のため core_timer.o を含める．
#  SYSMON=ATF_S 時のみの atf_support.o は本ターゲット既定の単独動作
#  （非SYSMON）では不要なので含めない）
list(APPEND FMP3_ARCH_C_FILES
    ${COREDIR}/core_kernel_impl.c
    ${COREDIR}/core_timer.c
    ${COREDIR}/arm64.c
    ${COREDIR}/gic_kernel_impl.c
    ${COREDIR}/core_support.S
    ${COREDIR}/gic_support.S
)

#  Makefile.core:22 の文言は「-lgcc のみ」だが，これだけを移植すると
#  リンクエラーになる（実測：syslog.c の memcpy 参照が undefined
#  reference になり fmp のリンクが失敗することを確認した）．
#  上流は sample/Makefile:62-63（`ifeq ($(SRCLANG),c): LIBS := $(LIBS)
#  -lc`）で全ターゲット共通に -lc を無条件付加しており，この汎用層の
#  付加分を CMake 側は arch.cmake ごとに手動で再現する規約になっている
#  （arch/riscv_gcc/common/arch.cmake:47・arch/arm_m_gcc/common/arch.cmake:39
#  も同じく `c gcc` を積んでおり，本ファイルもそれに倣う．汎用層
#  CMakeLists.txt には -lc を足す仕組みが無いため，個々の arch.cmake が
#  補う既存規約であり，本ファイルはそれに合わせただけで汎用層は変更
#  していない）．
list(APPEND FMP3_LINK_LIBS c gcc)

#  Makefile.core:23
list(APPEND FMP3_COMPILE_OPTIONS -mstrict-align)
