#
#		アーキテクチャ依存部の CMake 定義（ARM-M コア共通）
#
#  chip.cmake から include される（上流 Makefile.core に相当）．
#
#  本ファイルは常に fmp3_core（このリポジトリ）側にある共通コア層なので，
#  ${FMP3_ROOT_DIR} 基準で自己解決する（arch/riscv_gcc/common/arch.cmake と
#  同じ作法。chip 依存部・target 依存部がリポジトリ外に置かれる場合でも，
#  この層だけは動かない）．
#
set(COREDIR ${FMP3_ROOT_DIR}/arch/arm_m_gcc/common)
set(TOOLDIR ${FMP3_ROOT_DIR}/arch/gcc)

#
#  ROM イメージ形式（Makefile.core:31  DUMP = dump）．
#
#  ★Task 6/7（kria_arm64_gcc・arch/arm64_gcc/common/arch.cmake:20-37）で
#    「本ファイル（arm_m_gcc/common/Makefile.core:31）にも同じ無条件代入
#    `DUMP = dump` があり，musca_b1_gcc / rp2350_pico2_gcc も本来は dump
#    形式になるはずだが，それらの target.cmake は FMP3_DUMP_FORMAT を
#    宣言しておらず現状 srec のまま」という懸念が報告されたまま未対応
#    だった（DIVERGENCE_MAP.md 参照）。Task 13 で現物（本ファイル31行目）
#    を再確認し，arm64_gcc と同じ理由（sample/Makefile:133-134 の
#    `ifndef DUMP: DUMP = srec` を，include 順で後に読まれる本行が無条件
#    代入で上書きする）で同一の乖離だと判断し，ここで解消する。
#
#  ★DUMPOPTS: musca_b1_gcc / rp2350_pico2_gcc の Makefile.target は
#    DUMPOPTS を定義しない（kria_arm64_gcc とは異なる。現物確認済み）ため，
#    上流でも `objdump -s`（セクション指定なし＝全セクション）になる。
#    CMake 汎用層は FMP3_DUMPOPTS 未定義時に既定で空文字列
#    （CMakeLists.txt:96-97）にするため，ここで何も宣言しなければ上流と
#    同じ「フィルタ無し」の dump になる。
#
set(FMP3_DUMP_FORMAT dump)

#  Makefile.core:25
list(APPEND FMP3_INCLUDE_DIRS
    ${COREDIR}
    ${TOOLDIR}
)

#  Makefile.core:43  CFG_TABS
list(APPEND FMP3_SYMVAL_TABLES
    ${COREDIR}/core_sym.def
)

#  Makefile.core:48  TARGET_OFFSET_TRB
list(APPEND FMP3_OFFSET_TRB_FILES
    ${COREDIR}/core_offset.py
)

#  Makefile.core:37-38  KERNEL_ASMOBJS core_support.o / KERNEL_COBJS
#  core_kernel_impl.o（どちらも libfmp3.a に入る．riscv_gcc/common/arch.cmake
#  と同じ扱いで FMP3_ARCH_C_FILES に .c と .S を両方積む）
list(APPEND FMP3_ARCH_C_FILES
    ${COREDIR}/core_kernel_impl.c
    ${COREDIR}/core_support.S
)

#  Makefile.core:26（-lgcc）と sample/Makefile:63（SRCLANG=c のとき -lc）
list(APPEND FMP3_LINK_LIBS c gcc)

#
#  ★offset.h への C ソース依存関係についての注記（Makefile.core:51-64）
#
#  ARM-M コア依存部では core_kernel_impl.h が（CFG1_OUT 以外で）offset.h を
#  取り込み，kernel_impl.h → target_kernel_impl.h → chip_kernel_impl.h →
#  core_kernel_impl.h の連鎖でほぼ全 C オブジェクトに波及する（他アーキ
#  では offset.h をアセンブラからのみ取り込む）．上流 Make ビルドは
#  OFFSET_COBJS を使ってこれに順序専用依存を付けているが，CMake 側では
#  add_dependencies(fmp3 generate_cfg_gen_files)（libfmp3.a 分。既存）と
#  add_dependencies(fmp generate_cfg_gen_files)（fmp 自身が直接コンパイル
#  するソース分。計画A2 Task 1 で追加済み）でカバーする．本ファイルでの
#  対応は不要（汎用層で完結する）．
#
