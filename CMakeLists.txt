#
#		TOPPERS/FMP3 Core CMake ビルド
#
#  使い方（例）：
#    cmake --preset polarfire_soc_kit-qemu
#    cmake --build build/polarfire_soc_kit-qemu
#    cmake --build build/polarfire_soc_kit-qemu --target run
#
cmake_minimum_required(VERSION 3.23)

project(fmp3_core C ASM)

include(${CMAKE_CURRENT_LIST_DIR}/fmp3_core.cmake)

#
#  .trb（cfg pass2 生成スクリプト）の IncludeTrb("...") 連鎖を configure
#  時に解決する fmp3_trb_closure() をここで定義する（cfg パス2の
#  add_custom_command から使う．Task 5 レビュー Critical 対応）．
#  詳細は cmake/trb_depends.cmake のコメント参照．
#
include(${FMP3_ROOT_DIR}/cmake/trb_depends.cmake)

#
#  ライブラリ専用モード（外部 SDK アプリ向け．esp32p4 が要求する）
#
#  ON のとき libfmp3.a までを作り，fmp 実行ファイル・run ターゲット・
#  pass3 の POST_BUILD を作らない．
#
option(FMP3_LIBRARY_ONLY
    "Build only the fmp3 kernel library (no fmp executable)" OFF)

#
#  プロセッサ数（TNUM_PRCID）
#
#  上流 sample/Makefile:193-194 の PRC_NUM に相当する．指定時のみ
#  -DTNUM_PRCID=<N> をコンパイル定義に足す．未指定ならターゲットの
#  target_kernel.h の既定値（polarfire は 4）が使われる．
#
#  ★これは CMake の分岐材料ではない．ファイルリストやターゲット構成を
#    この値で分岐させてはならない（TNUM_PRCID は cfg1_out をリンクして
#    初めて cfg が知る値であり，CMake から見える保証がない）．
#
set(FMP3_PRC_NUM "" CACHE STRING
    "Number of processors (TNUM_PRCID). Empty = target default")

#
#  ターゲット依存部の情報を読み込み
#
#  target.cmake は以下の変数を積み上げ，チップ依存部の chip.cmake を
#  include する（現時点で実在する変数のみ列挙。FMP3_API_TABLES は
#  CMakeLists.txt 側で，FMP3_RUN_COMMAND は target.cmake 側で，
#  ともに将来 Task で追加予定）：
#    FMP3_CFG_FILES / FMP3_KERNEL_CFG_TRB_FILES / FMP3_CHECK_TRB_FILES /
#    FMP3_OFFSET_TRB_FILES / FMP3_CLASS_TRB_FILES / FMP3_SYMVAL_TABLES /
#    FMP3_INCLUDE_DIRS / FMP3_COMPILE_DEFS /
#    FMP3_COMPILE_OPTIONS / FMP3_LINK_OPTIONS / FMP3_CFG1_OUT_LINK_OPTIONS /
#    FMP3_LINK_LIBS / FMP3_LDSCRIPT / FMP3_ARCH_C_FILES / FMP3_TARGET_C_FILES /
#    FMP3_SYSSVC_TARGET_C_FILES / FMP3_START_FILES
#
#  リンカスクリプト指定の書式（-T か -Wl,-T, か）は，ターゲット固有の C
#  ライブラリ specs が「-Wl,-T,<file> だけでは自前のリンカスクリプトを
#  検出できず，specs 側の既定リンカスクリプトを追加注入してしまう」場合に
#  だけ ON にするトグルである．picolibc.specs の %{!T:-Tpicolibc.ld} が
#  これに該当する（gcc ドライバの -T スイッチが立っているかどうかだけを
#  見るため，-Wl 経由の -T では検出できない．詳細は下の -T 適用箇所の
#  コメントと target/polarfire_soc_kit_gcc/target.cmake 参照）．
#
#  ほとんどのツールチェーン（arm-none-eabi の newlib 既定 specs を含む）は
#  この種の自動注入を持たないため，既定は OFF（-Wl,-T, を使う）．
#  «未定義のときだけ» 既定を与える（cmake/toolchain-riscv64.cmake の
#  FMP3_EXPECTED_TOOLCHAIN_MACHINE と同じ理由．素の set() だと
#  target.cmake 側の意図的な ON 宣言があってもここで上書きしてしまう
#  ……というのは誤りで，実際には逆に「target.cmake がこの後 include
#  されて ON を set() する」ので問題は起きないが，将来 -D 経由で
#  上書きしたい場合に備えて同じ書き方に揃えておく）．
#
if(NOT DEFINED FMP3_LDSCRIPT_VIA_DRIVER_T)
    set(FMP3_LDSCRIPT_VIA_DRIVER_T OFF)
endif()

include(${FMP3_TARGET_DIR}/target.cmake)

#
#  リンカスクリプトの適用（-T）はここ1箇所に集約する．
#
#  target.cmake は FMP3_LDSCRIPT を「設定するだけ」で -T 自体は積まない
#  （target.cmake 側のコメント参照）．-T をここでしか足さないことで，
#  将来 fmp 実行ファイルを組む Task が asp3_core 由来の
#    list(APPEND FMP3_LINK_OPTIONS -Wl,-T,${FMP3_LDSCRIPT})
#  をそのまま流用しても二重指定（ld の "linker script file '...' appears
#  multiple times" fatal error）にならない．
#
#  ★書式（-T か -Wl,-T, か）はターゲットが宣言する FMP3_LDSCRIPT_VIA_DRIVER_T
#    で決める（既定 OFF＝-Wl,-T,）．汎用層（本ファイル）がターゲット固有の
#    変数名（旧: POLARFIRE_QEMU）で直接分岐していたのは設計上の誤りだった：
#    「picolibc の specs が gcc の -T ドライバスイッチを要求する」という
#    知識は picolibc を使うターゲット（polarfire の QEMU/picolibc ビルド）
#    が持つべきものであり，汎用層はターゲットが宣言した結果だけを読む．
#    実測（riscv64-unknown-elf-gcc 13.2.0 + picolibc 1.8.6-2，mpfs-lim.ld）：
#      -T mpfs-lim.ld          → _start = 0x08000000（l2lim．意図通り）
#      -Wl,-T,mpfs-lim.ld のみ → _start = 0x80000000（picolibc.ld 既定）
#    ld はエラーにせず両方の SECTIONS を受理し，後着の picolibc.ld が
#    勝つ．**ビルドは通るのに実機では起動しない**という，fatal error
#    より発見しにくい壊れ方をする．musca_b1（newlib，picolibc 不使用）は
#    FMP3_LDSCRIPT_VIA_DRIVER_T を一切設定しないため既定の OFF のままとなり
#    -Wl,-T, が使われる（これが正しい）．
#
if(DEFINED FMP3_LDSCRIPT)
    if(FMP3_LDSCRIPT_VIA_DRIVER_T)
        list(APPEND FMP3_LINK_OPTIONS -T ${FMP3_LDSCRIPT})
    else()
        list(APPEND FMP3_LINK_OPTIONS -Wl,-T,${FMP3_LDSCRIPT})
    endif()
endif()

if(NOT FMP3_PRC_NUM STREQUAL "")
    list(APPEND FMP3_COMPILE_DEFS TNUM_PRCID=${FMP3_PRC_NUM})
endif()

#
#  共通コンパイルオプション（target/chip 非依存）
#
#  上流 sample/Makefile:182-188 の既定値に相当する（OMIT_WARNING_ALL /
#  OMIT_OPTIMIZATION いずれも未定義のときの既定）：
#    COPTS := -g @(COPTS) $(COPTS)
#    COPTS := -Wall $(COPTS)        # ifndef OMIT_WARNING_ALL
#    COPTS := -O2 $(COPTS)          # ifndef OMIT_OPTIMIZATION
#  target.cmake / chip.cmake はこれを積まない（arch/target 非依存の既定値
#  なので，Makefile と同じ層＝ここに置く）．
#
#  ★-O2 を省略すると単に「遅い」では済まない．PolarFire SoC Kit QEMU の
#    既定リンカスクリプト（lim-debug/mpfs-lim.ld）は l2lim を 256KiB しか
#    割り当てておらず，-O0（gcc 既定）でビルドした fmp は実測で
#    ".stack section will not fit in region l2lim"（16KiB オーバー）で
#    リンクに失敗する．-O2 で通常サイズに戻ると収まる．
#
list(APPEND FMP3_COMPILE_OPTIONS -g -Wall -O2)

message(STATUS "fmp3_core: FMP3_ROOT_DIR   = ${FMP3_ROOT_DIR}")
message(STATUS "fmp3_core: FMP3_TARGET     = ${FMP3_TARGET}")
message(STATUS "fmp3_core: FMP3_TARGET_DIR = ${FMP3_TARGET_DIR}")
message(STATUS "fmp3_core: FMP3_BOARD      = ${FMP3_BOARD}")
message(STATUS "fmp3_core: FMP3_RISCV_MARCH= ${FMP3_RISCV_MARCH}")
message(STATUS "fmp3_core: FMP3_RISCV_SPECS= ${FMP3_RISCV_SPECS}")
message(STATUS "fmp3_core: FMP3_PRC_NUM    = '${FMP3_PRC_NUM}'")
message(STATUS "fmp3_core: FMP3_COMPILE_DEFS= ${FMP3_COMPILE_DEFS}")
message(STATUS "fmp3_core: FMP3_ARCH_C_FILES count = ")
foreach(f IN LISTS FMP3_ARCH_C_FILES)
    message(STATUS "    arch: ${f}")
endforeach()
message(STATUS "fmp3_core: FMP3_LDSCRIPT   = ${FMP3_LDSCRIPT}")
message(STATUS "fmp3_core: FMP3_CFG1_OUT_LINK_OPTIONS = ${FMP3_CFG1_OUT_LINK_OPTIONS}")
message(STATUS "fmp3_core: FMP3_LINK_OPTIONS = ${FMP3_LINK_OPTIONS}")

#
#  コンフィギュレータの起動コマンド
#
#  CMake が呼ぶのは常に cfg_py/cfg.py だけ（AGENTS.md §2 規則3）。
#  ★計画B Task 11（cutover）で，cfg_py/ の中身は asp3_core 1.7.1 の
#    本物の Python cfg エンジン（旧 cfg_py/engine_next/）へ昇格した。
#    計画Aの間の「pristine cfg/cfg.rb への委譲シム」はここでは終わり，
#    以後 CMake からは pristine の cfg/*.rb を一切参照しない。
#    Ruby（cfg/cfg.rb 一式）は tools/cfg_equivalence.sh がオラクルとして
#    呼ぶ以外に用途は無い。DIVERGENCE_MAP.md の「期限付きの逸脱」は
#    Task 12 で解消記録を追記する。
#
find_package(Python3 REQUIRED COMPONENTS Interpreter)
set(CFG_COMMAND ${Python3_EXECUTABLE} -B ${FMP3_ROOT_DIR}/cfg_py/cfg.py)
set(CFG_SCRIPT_DEPS
    ${FMP3_ROOT_DIR}/cfg_py/cfg.py
    ${FMP3_ROOT_DIR}/cfg_py/pass1.py
    ${FMP3_ROOT_DIR}/cfg_py/pass2.py
    ${FMP3_ROOT_DIR}/cfg_py/gen_file.py
    ${FMP3_ROOT_DIR}/cfg_py/srecord.py
)
message(STATUS "fmp3_core: cfg = cfg_py/cfg.py (asp3_core 1.7.1 engine, Plan-B)")

#
#  cfg 共通の定義
#
#  sample/Makefile:263-264 の CFG_TABS に相当（core_sym.def は arch.cmake が積む）
#
list(APPEND FMP3_API_TABLES    ${FMP3_ROOT_DIR}/kernel/kernel_api.def)
list(APPEND FMP3_SYMVAL_TABLES ${FMP3_ROOT_DIR}/kernel/kernel_sym.def)

#
#  アプリケーションの選択
#
if(NOT DEFINED FMP3_APPLNAME)
    set(FMP3_APPLNAME sample1)
endif()
if(NOT DEFINED FMP3_APPLDIR)
    set(FMP3_APPLDIR ${FMP3_ROOT_DIR}/sample)
endif()
if(NOT IS_ABSOLUTE ${FMP3_APPLDIR})
    set(FMP3_APPLDIR ${FMP3_ROOT_DIR}/${FMP3_APPLDIR})
endif()
set(FMP3_APP_CFG_FILE ${FMP3_APPLDIR}/${FMP3_APPLNAME}.cfg)

#  アプリの .cfg は最後に置く（sample/Makefile:393 の $< に相当）
list(APPEND FMP3_CFG_FILES ${FMP3_APP_CFG_FILE})

#  sample/Makefile:197 の INCLUDES := -I. -I$(SRCDIR)/include ... -I$(SRCDIR)
list(APPEND FMP3_INCLUDE_DIRS
    ${FMP3_ROOT_DIR}/include
    ${FMP3_ROOT_DIR}
    ${FMP3_APPLDIR}
)

#
#  生成物の置き場所
#
#  cfg は cfg1_out.db / cfg1_out.syms / cfg1_out.srec / cfg2_out.db を
#  ★裸の相対名で読み書きする★ため，全パスで WORKING_DIRECTORY を
#  ここに固定することが必須である．
#
set(CFG_GEN_DIR ${CMAKE_BINARY_DIR}/generated)
file(MAKE_DIRECTORY ${CFG_GEN_DIR})

set(CFG1_OUT_FILE       ${CFG_GEN_DIR}/cfg1_out.c)
set(CFG1_OUT_TIMESTAMP  ${CFG_GEN_DIR}/cfg1_out.timestamp)
set(CFG1_OUT_DEPFILE    ${CFG_GEN_DIR}/cfg1_out_c.d)
set(CFG1_OUT_SYMS_FILE  ${CFG_GEN_DIR}/cfg1_out.syms)
set(CFG1_OUT_SREC_FILE  ${CFG_GEN_DIR}/cfg1_out.srec)
set(OFFSET_H_FILE       ${CFG_GEN_DIR}/offset.h)
set(OFFSET_TIMESTAMP    ${CFG_GEN_DIR}/offset.timestamp)
set(KERNEL_CFG_TIMESTAMP ${CFG_GEN_DIR}/kernel_cfg.timestamp)
set(KERNEL_CFG_C_FILE   ${CFG_GEN_DIR}/kernel_cfg.c)
set(KERNEL_CFG_H_FILE   ${CFG_GEN_DIR}/kernel_cfg.h)

#  生成した kernel_cfg.h / offset.h を #include できるようにする
list(APPEND FMP3_INCLUDE_DIRS ${CFG_GEN_DIR})

#
#  cfg コマンドライン引数の組み立て
#
foreach(path IN LISTS FMP3_SYMVAL_TABLES)
    list(APPEND CFG_SYMVAL_TABLES "--symval-table" ${path})
endforeach()
foreach(path IN LISTS FMP3_API_TABLES)
    list(APPEND CFG_API_TABLES "--api-table" ${path})
endforeach()
foreach(path IN LISTS FMP3_CLASS_TRB_FILES)
    list(APPEND CFG_CLASS_TRB_FILES "-C" ${path})
endforeach()
foreach(path IN LISTS FMP3_OFFSET_TRB_FILES)
    list(APPEND CFG_OFFSET_TRB_FILES "-T" ${path})
endforeach()
foreach(path IN LISTS FMP3_KERNEL_CFG_TRB_FILES)
    list(APPEND CFG_KERNEL_CFG_TRB_FILES "-T" ${path})
endforeach()
foreach(path IN LISTS FMP3_CHECK_TRB_FILES)
    list(APPEND CFG_CHECK_TRB_FILES "-T" ${path})
endforeach()
foreach(path IN LISTS FMP3_INCLUDE_DIRS)
    list(APPEND CFG_INCLUDE_DIRS "-I${path}")
endforeach()

#
#  cfg パス1：.cfg ＋ api-table → cfg1_out.c
#
#  ★OUTPUT に宣言するのは cfg1_out.timestamp であって cfg1_out.c ではない．
#    cfg の GenFile は内容が変わらないときファイルを書き直さない
#    （タイムスタンプを更新しない）ため，.c を OUTPUT に宣言すると
#    毎ビルド pass1 が再実行される．上流 Makefile も timestamp 方式である．
#
#  ★DEPFILE が追跡するのは「.cfg が INCLUDE() する別の .cfg」だけである．
#    C ヘッダの #include は追跡しない．pristine cfg/pass1.rb は
#    #include/#ifdef 等のプリプロセッサ指示子をパースせず（pass1.rb:624
#    付近，$cfgFileInfo に不透明トークンとして積むだけでマクロ展開しない），
#    cfg1_out.c へそのまま転記する（cfg1_out.c 中に文字通り #include が
#    現れる）．-M が書く depfile へ依存が積まれるのは cfg 独自の
#    INCLUDE("....cfg") 構文だけ（pass1.rb:705 の $dependencyFiles.push）．
#    実測でも generated/cfg1_out_c.d に sample1.h は現れない．
#
#    ヘッダ変更の検出が結果的に効いて見えるのは，生成された cfg1_out.c を
#    C コンパイラが実際にコンパイルする際の通常のヘッダ依存追跡
#    （.c.obj.d → ninja の暗黙依存）が拾うためであり，DEPFILE とは
#    無関係な副次効果である．したがって「pass1 だけを再実行して
#    cfg1_out.c を作るが，それをコンパイルしない」経路（例えば
#    pass2 の kernel_cfg.c 生成）では，この安全網は保証されない．
#    Task 5 で kernel_cfg.c 系の依存を設計する際は，ここに同じ安全網が
#    自動的にあると仮定しないこと．
#
#    depfile のターゲットは裸の "cfg1_out.timestamp" だが，CMake 3.23+ の
#    Ninja ジェネレータが変換するため OUTPUT と一致しなくてよい．
#
add_custom_command(
    OUTPUT ${CFG1_OUT_TIMESTAMP}
    BYPRODUCTS ${CFG1_OUT_FILE} ${CFG_GEN_DIR}/cfg1_out.db
    WORKING_DIRECTORY ${CFG_GEN_DIR}
    COMMAND ${CFG_COMMAND} --pass 1 --kernel fmp
            ${CFG_INCLUDE_DIRS} ${CFG_API_TABLES} ${CFG_SYMVAL_TABLES}
            -M ${CFG1_OUT_DEPFILE} ${FMP3_CFG_FILES}
    DEPENDS ${CFG_SCRIPT_DEPS} ${FMP3_API_TABLES} ${FMP3_SYMVAL_TABLES}
            ${FMP3_CFG_FILES}
    DEPFILE ${CFG1_OUT_DEPFILE}
    COMMENT "Running cfg pass 1 to generate cfg1_out.c"
)

#
#  ★cfg1_out.c を「別の add_custom_command の OUTPUT」にしてはならない．
#    同じファイルを生成する規則が2つできて Ninja が
#    "multiple rules generate ..." で失敗する．
#    BYPRODUCTS で GENERATED 扱いにし，順序は add_dependencies で付ける．
#
add_custom_target(cfg_pass1 DEPENDS ${CFG1_OUT_TIMESTAMP})
set_source_files_properties(${CFG1_OUT_FILE} PROPERTIES GENERATED TRUE)

#
#  cfg1_out のビルド（静的 API パラメータの値の取り出し用）
#
#  ★この実行ファイルは絶対に実行しない．nm と objcopy でシンボルの値を
#    静的に読み出すだけである．これがクロスコンパイルで成立する理由．
#
add_executable(cfg1_out ${FMP3_START_FILES} ${CFG1_OUT_FILE})
add_dependencies(cfg1_out cfg_pass1)
target_include_directories(cfg1_out
    PRIVATE ${FMP3_INCLUDE_DIRS}
    PRIVATE ${FMP3_ROOT_DIR}/kernel
)
target_compile_definitions(cfg1_out PRIVATE ${FMP3_COMPILE_DEFS})
target_compile_options(cfg1_out PRIVATE ${FMP3_COMPILE_OPTIONS})
#  ★最終 ELF 用の --gc-sections が cfg1_out に効くと TOPPERS_magic_number が
#    消え，pass2 が cfg1_out.syms から見つけられずに停止する．
#    上流 Makefile.target:110 と同じく --no-gc-sections を明示的に足す．
target_link_options(cfg1_out PRIVATE
    ${FMP3_LINK_OPTIONS} ${FMP3_CFG1_OUT_LINK_OPTIONS})
target_link_libraries(cfg1_out PRIVATE ${FMP3_LINK_LIBS})

if(DEFINED FMP3_LDSCRIPT)
    set_property(TARGET cfg1_out APPEND PROPERTY LINK_DEPENDS ${FMP3_LDSCRIPT})
endif()

#
#  シンボルテーブルとイメージの取り出し
#
#  ★">" のシェルリダイレクトは使わない（ジェネレータ依存になるため）．
#    cmake -E env の代わりに nm の出力を CMake 経由でファイルへ書く．
#
#  ★nm_to_file.cmake の直後に check_magic_number.cmake を続けて走らせる．
#    --gc-sections/--no-gc-sections は「最後着が勝つ」暗黙の順序保証に
#    依存しており（cfg1_out のリンクオプション参照），順序が崩れると
#    ビルドはエラー無く成功したまま TOPPERS_magic_number だけが消える．
#    それを cfg1_out.syms 生成の直後にここで検査し，無ければ止める。
add_custom_command(
    OUTPUT ${CFG1_OUT_SYMS_FILE}
    COMMAND ${CMAKE_COMMAND}
            -DNM=${CMAKE_NM}
            -DELF=$<TARGET_FILE:cfg1_out>
            -DOUT=${CFG1_OUT_SYMS_FILE}
            -P ${FMP3_ROOT_DIR}/cmake/nm_to_file.cmake
    COMMAND ${CMAKE_COMMAND}
            -DSYMS=${CFG1_OUT_SYMS_FILE}
            -P ${FMP3_ROOT_DIR}/cmake/check_magic_number.cmake
    DEPENDS cfg1_out ${FMP3_ROOT_DIR}/cmake/nm_to_file.cmake
            ${FMP3_ROOT_DIR}/cmake/check_magic_number.cmake
    COMMENT "Generating cfg1_out.syms"
)

add_custom_command(
    OUTPUT ${CFG1_OUT_SREC_FILE}
    COMMAND ${CMAKE_OBJCOPY} -O srec -S $<TARGET_FILE:cfg1_out> ${CFG1_OUT_SREC_FILE}
    DEPENDS cfg1_out
    COMMENT "Generating cfg1_out.srec"
)

#
#  ★上の2つの add_custom_command は，それ単体では Ninja ジェネレータの
#    build.ninja に書き出されない（実測で確認: 何にも依存されない
#    add_custom_command(OUTPUT ...) は，ジェネレータが到達不能と判断して
#    ビルドグラフから丸ごと落とす。Makefiles/Ninja 双方の CMake 一般論
#    ではなく，このジェネレータ・バージョンでの実測結果）。
#    そのままでは `ninja generated/cfg1_out.syms` が
#    "unknown target" になる。DEPENDS で拾う target を用意して
#    到達可能にする（ALL 化はしない．既定ビルドに含めるかは pass2 を
#    書く Task 5 の判断に委ねる）。
#
add_custom_target(cfg1_out_syms DEPENDS ${CFG1_OUT_SYMS_FILE})
add_custom_target(cfg1_out_srec DEPENDS ${CFG1_OUT_SREC_FILE})

#
#  .trb は IncludeTrb("...") で他の .trb を再帰的に取り込む（例：
#  target_kernel.trb → chip_kernel.trb → core_kernel.trb →
#  kernel/kernel.trb → kernel/task.trb ...）．トップレベルの .trb
#  （FMP3_CLASS_TRB_FILES / FMP3_OFFSET_TRB_FILES / FMP3_KERNEL_CFG_TRB_FILES）
#  だけを DEPENDS に積むと，間接的に取り込まれる .trb を編集しても
#  再生成されない（Task 5 レビュー Critical）．fmp3_trb_closure()
#  （cmake/trb_depends.cmake）で IncludeTrb 連鎖を configure 時に辿り，
#  閉包を DEPENDS に足す．
#
#  ★INCLUDE_DIRS 引数には FMP3_INCLUDE_DIRS をそのまま渡す．これは
#    CFG_INCLUDE_DIRS（"-I<dir>" 形式）の元になった同じ変数・同じ並び順
#    であり，cfg/cfg.rb の $includeDirectories と一致する．
#
#  ★閉包の結果を CMAKE_CONFIGURE_DEPENDS に足し，閉包に入っている
#    .trb の中身が変わって IncludeTrb(...) の集合が変化したときに
#    configure をやり直させる（cmake/trb_depends.cmake 冒頭コメント
#    参照）．
#
fmp3_trb_closure(FMP3_OFFSET_TRB_CLOSURE ${CFG_GEN_DIR} "${FMP3_INCLUDE_DIRS}"
    ${FMP3_CLASS_TRB_FILES} ${FMP3_OFFSET_TRB_FILES})
fmp3_trb_closure(FMP3_KERNEL_CFG_TRB_CLOSURE ${CFG_GEN_DIR} "${FMP3_INCLUDE_DIRS}"
    ${FMP3_CLASS_TRB_FILES} ${FMP3_KERNEL_CFG_TRB_FILES})
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    ${FMP3_OFFSET_TRB_CLOSURE} ${FMP3_KERNEL_CFG_TRB_CLOSURE})

#
#  cfg パス2（-O）：offset.h の生成
#
#  上流 sample/Makefile:417-421 に相当．-O は cfg2_out.db を出力しない指定で，
#  次の kernel_cfg 生成側が書く db を潰さないためにある．
#
add_custom_command(
    OUTPUT ${OFFSET_TIMESTAMP}
    BYPRODUCTS ${OFFSET_H_FILE}
    WORKING_DIRECTORY ${CFG_GEN_DIR}
    COMMAND ${CFG_COMMAND} --pass 2 -O --kernel fmp
            ${CFG_INCLUDE_DIRS} ${CFG_CLASS_TRB_FILES} ${CFG_OFFSET_TRB_FILES}
            --rom-symbol ${CFG1_OUT_SYMS_FILE} --rom-image ${CFG1_OUT_SREC_FILE}
    DEPENDS ${CFG_SCRIPT_DEPS} ${FMP3_CLASS_TRB_FILES} ${FMP3_OFFSET_TRB_FILES}
            ${CFG1_OUT_SYMS_FILE} ${CFG1_OUT_SREC_FILE} ${CFG1_OUT_TIMESTAMP}
            ${FMP3_OFFSET_TRB_CLOSURE}
    COMMENT "Running cfg pass 2 to generate offset.h"
)

#
#  cfg パス2：kernel_cfg.c / kernel_cfg.h の生成
#
#  上流 sample/Makefile:408-411 に相当．こちらは --rom-symbol / --rom-image を
#  渡さない（cfg が cfg1_out.syms / cfg1_out.srec を裸の相対名で開くため，
#  WORKING_DIRECTORY が効いている必要がある）．
#
#  offset.h の生成と順序を付けるため OFFSET_TIMESTAMP に依存させる．
#  ★上流 sample/Makefile:405-421 に同じ依存辺は無い（kernel_cfg.timestamp・
#    offset.timestamp はどちらも独立に cfg1_out.db/.syms/.$(DUMP) にのみ
#    依存し，make -j では並列に走り得る．-O 側は $omitOutputDb により
#    cfg2_out.db を書かないため衝突しない）．ここで順序を付けているのは
#    上流を再現するためではなく，CMake 側の安全側の判断（両方が同じ
#    cfg1_out.db を pass1 出力として共有する；実害は無いが依存を単純化
#    できる）であり，無害だが上流にはこの順序制約は無い．
#
add_custom_command(
    OUTPUT ${KERNEL_CFG_TIMESTAMP}
    BYPRODUCTS ${KERNEL_CFG_C_FILE} ${KERNEL_CFG_H_FILE} ${CFG_GEN_DIR}/cfg2_out.db
    WORKING_DIRECTORY ${CFG_GEN_DIR}
    COMMAND ${CFG_COMMAND} --pass 2 --kernel fmp
            ${CFG_INCLUDE_DIRS} ${CFG_CLASS_TRB_FILES} ${CFG_KERNEL_CFG_TRB_FILES}
    DEPENDS ${CFG_SCRIPT_DEPS} ${FMP3_CLASS_TRB_FILES} ${FMP3_KERNEL_CFG_TRB_FILES}
            ${CFG1_OUT_SYMS_FILE} ${CFG1_OUT_SREC_FILE} ${OFFSET_TIMESTAMP}
            ${FMP3_KERNEL_CFG_TRB_CLOSURE}
    COMMENT "Running cfg pass 2 to generate kernel_cfg.c/h"
)

#
#  ★cfg1_out.c と同じ理由で，kernel_cfg.c を別規則の OUTPUT にしない．
#    BYPRODUCTS で GENERATED 扱いにし，順序は add_dependencies で付ける
#    （Task 6 の add_library が ${KERNEL_CFG_C_FILE} をソースに取る）．
#
add_custom_target(generate_cfg_gen_files
    DEPENDS ${OFFSET_TIMESTAMP} ${KERNEL_CFG_TIMESTAMP}
)
set_source_files_properties(${KERNEL_CFG_C_FILE} PROPERTIES GENERATED TRUE)

#
#  カーネルライブラリ（libfmp3.a）
#
#  カーネルの .c は上流 kernel/Makefile.kernel の KERNEL_FCSRCS（22個）に
#  対応する．上流は1つの .c から複数の .o を作る（ALLFUNC を定義しない）
#  ビルドもできるが，CMake 側は asp3_core と同じく .c を1回ずつコンパイルし，
#  ALLFUNC を定義して全関数を1つの .o に入れる．
#
add_library(fmp3 STATIC
    ${FMP3_ARCH_C_FILES}
    ${FMP3_TARGET_C_FILES}
    ${FMP3_ROOT_DIR}/kernel/startup.c
    ${FMP3_ROOT_DIR}/kernel/task.c
    ${FMP3_ROOT_DIR}/kernel/taskhook.c
    ${FMP3_ROOT_DIR}/kernel/wait.c
    ${FMP3_ROOT_DIR}/kernel/time_event.c
    ${FMP3_ROOT_DIR}/kernel/task_manage.c
    ${FMP3_ROOT_DIR}/kernel/task_refer.c
    ${FMP3_ROOT_DIR}/kernel/task_sync.c
    ${FMP3_ROOT_DIR}/kernel/task_term.c
    ${FMP3_ROOT_DIR}/kernel/semaphore.c
    ${FMP3_ROOT_DIR}/kernel/eventflag.c
    ${FMP3_ROOT_DIR}/kernel/dataqueue.c
    ${FMP3_ROOT_DIR}/kernel/pridataq.c
    ${FMP3_ROOT_DIR}/kernel/mutex.c
    ${FMP3_ROOT_DIR}/kernel/mempfix.c
    ${FMP3_ROOT_DIR}/kernel/time_manage.c
    ${FMP3_ROOT_DIR}/kernel/cyclic.c
    ${FMP3_ROOT_DIR}/kernel/alarm.c
    ${FMP3_ROOT_DIR}/kernel/spin_lock.c
    ${FMP3_ROOT_DIR}/kernel/sys_manage.c
    ${FMP3_ROOT_DIR}/kernel/interrupt.c
    ${FMP3_ROOT_DIR}/kernel/exception.c
    ${KERNEL_CFG_C_FILE}
)
add_dependencies(fmp3 generate_cfg_gen_files)

target_include_directories(fmp3
    PUBLIC  ${FMP3_INCLUDE_DIRS}
    PRIVATE ${FMP3_ROOT_DIR}/kernel
)
#  ALLFUNC: 上流 sample/Makefile:302 の KERNEL_CFLAGS := -DALLFUNC ...
target_compile_definitions(fmp3
    PRIVATE ALLFUNC
    PUBLIC  ${FMP3_COMPILE_DEFS}
)
target_compile_options(fmp3 PUBLIC ${FMP3_COMPILE_OPTIONS})

#
#  cfg パス3：構成チェック（最終 ELF に対する POST_BUILD）
#
#  上流 sample/Makefile:483-487 に相当．
#  ライブラリ専用モードでは最終 ELF が無いため呼ばない．引数一式は
#  親スコープへ export し，外部 SDK 側が自前の ELF に対して呼べるようにする．
#
set(PASS3_ARGS ${CFG_COMMAND} --pass 3 --kernel fmp -O
    ${CFG_INCLUDE_DIRS} ${CFG_CHECK_TRB_FILES})

function(fmp3_cfg_check TARGET)
    if(NOT FMP3_ROOT_DIR)
        message(FATAL_ERROR
            "fmp3_cfg_check(${TARGET}): FMP3_ROOT_DIR is empty in the caller's scope. "
            "Same caller-scope contract as fmp3_add_syssvc() -- see fmp3_core.cmake "
            "header comment. fmp3_cfg_check() itself is currently only called from "
            "within fmp3_core's own CMakeLists.txt (guarded by NOT FMP3_LIBRARY_ONLY); "
            "this guard exists in case that changes.")
    endif()
    #  ★外部レビューで指摘・現物で裏取り済み：FMP3_ROOT_DIR のガードだけでは
    #  不十分。CFG_GEN_DIR / PASS3_ARGS は fmp3_core 自身の CMakeLists.txt
    #  の「このファイルのディレクトリスコープ」で set() された変数であり
    #  （function() は呼び出し側スコープで変数解決する — fmp3_add_syssvc()
    #  の FMP3_ROOT_DIR と同じ CMake 変数スコープの話だが，こちらは
    #  「呼び出し側が事前に set() すれば直る」性質のものではない：値の中身
    #  （CFG_COMMAND や PASS3_ARGS の各引数）は fmp3_core 自身のディレクトリ
    #  スコープでしか組み立てられない）。外部 SDK が add_subdirectory() の
    #  外（呼び出し側スコープ）から fmp3_cfg_check(app) を直接呼ぶと，
    #  FMP3_ROOT_DIR は（呼び出し側が事前に set() していれば）空でなくガード
    #  を通過するのに，CFG_GEN_DIR / PASS3_ARGS は呼び出し側スコープに存在
    #  せず空文字列に展開され，WORKING_DIRECTORY と COMMAND が空になり
    #  「--rom-symbol がコマンド名として扱われる」ような分かりにくい失敗に
    #  なる（計画A2 Task 9 で fmp3_add_syssvc() 側は対処したが，同じ関数群の
    #  fmp3_cfg_check() 側を見落としていた）。
    #
    #  正しい使い方：fmp3_cfg_check() は fmp3_core 自身の CMakeLists.txt の
    #  中（このファイル）からのみ呼ぶこと。外部 SDK が自前の ELF に対して
    #  pass3 相当を実行したい場合は，この関数を呼ぶのではなく，
    #  FMP3_LIBRARY_ONLY=ON 時に PARENT_SCOPE export される
    #  ${FMP3_PASS3_ARGS} / ${FMP3_CFG_GEN_DIR}（:560-561）を使って
    #  自前の add_custom_command を組み立てること。
    if(NOT CFG_GEN_DIR OR NOT PASS3_ARGS)
        message(FATAL_ERROR
            "fmp3_cfg_check(${TARGET}): CFG_GEN_DIR and/or PASS3_ARGS are empty in the "
            "caller's scope. These are directory-scope variables local to fmp3_core's "
            "own CMakeLists.txt, not exported under these names to a caller's scope "
            "(unlike FMP3_ROOT_DIR, whose FMP3_ROOT_DIR guard above can pass even when "
            "this one fails, if the caller pre-set FMP3_ROOT_DIR of its own accord -- "
            "that combination is exactly what makes this failure confusing). "
            "fmp3_cfg_check() is only meant to be called from within fmp3_core's own "
            "CMakeLists.txt. If you are an external SDK that add_subdirectory()'d this "
            "repository with FMP3_LIBRARY_ONLY=ON and want a pass-3 style check against "
            "your own ELF, do NOT call fmp3_cfg_check() from your own scope; instead use "
            "the PARENT_SCOPE-exported \${FMP3_PASS3_ARGS} / \${FMP3_CFG_GEN_DIR} "
            "variables to build your own add_custom_command(), the same way this "
            "function does internally.")
    endif()
    add_custom_command(TARGET ${TARGET} POST_BUILD
        WORKING_DIRECTORY ${CFG_GEN_DIR}
        COMMAND ${CMAKE_COMMAND}
                -DNM=${CMAKE_NM}
                -DELF=$<TARGET_FILE:${TARGET}>
                -DOUT=${CFG_GEN_DIR}/${TARGET}.syms
                -P ${FMP3_ROOT_DIR}/cmake/nm_to_file.cmake
        COMMAND ${CMAKE_OBJCOPY} -O srec -S $<TARGET_FILE:${TARGET}>
                ${CFG_GEN_DIR}/${TARGET}.srec
        COMMAND ${PASS3_ARGS}
                --rom-symbol ${CFG_GEN_DIR}/${TARGET}.syms
                --rom-image  ${CFG_GEN_DIR}/${TARGET}.srec
        COMMENT "Running cfg pass 3 to check configuration"
    )
endfunction()

if(FMP3_LIBRARY_ONLY)
    #  外部 SDK 側が最終 ELF を作るため，pass3 の材料を渡す．
    #
    #  本リポジトリ自身をトップレベル project として configure した場合
    #  （このリポジトリ単体で FMP3_LIBRARY_ONLY=ON を試すとき）は親スコープが
    #  存在しないため，PARENT_SCOPE の set() は CMake の dev warning
    #  （"current scope has no parent"）を出す。想定される使い方は
    #  add_subdirectory() 経由（fmp3_esp_idf / fmp3_pico_sdk のような外部
    #  SDK リポジトリから）であり，そのときだけ親スコープへの export に
    #  意味があるので，ここでガードする（Task 9 検証：add_subdirectory()
    #  経由なら警告は出ず，FMP3_PASS3_ARGS / FMP3_CFG_GEN_DIR は親から見える）。
    if(NOT CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
        set(FMP3_PASS3_ARGS   ${PASS3_ARGS}  PARENT_SCOPE)
        set(FMP3_CFG_GEN_DIR  ${CFG_GEN_DIR} PARENT_SCOPE)
    endif()
endif()

if(NOT FMP3_LIBRARY_ONLY)

#
#  アプリケーション実行ファイル（fmp）
#
#  上流 sample/Makefile:453-456 の $(OBJFILE) に相当．
#  START_OBJS（start.S）はライブラリ外で先頭にリンクする．
#  Microchip SDK のソースは上流が SYSSVC_COBJS/SYSSVC_ASMOBJS として
#  最終リンクに加えているのと同じ扱いにする．
#
add_executable(fmp
    ${FMP3_START_FILES}
    ${FMP3_APPLDIR}/${FMP3_APPLNAME}.c
    ${FMP3_SDK_C_FILES}
    ${FMP3_SDK_ASM_FILES}
)

#
#  fmp 自身が直接コンパイルする C/ASM ソース（sample1.c・非TECS版システム
#  サービス・SDK ソース等）を，offset.h / kernel_cfg.h の生成完了より前に
#  コンパイルしてしまう競合を防ぐ．
#
#  libfmp3.a 側（add_library(fmp3 ...)）は既に
#  add_dependencies(fmp3 generate_cfg_gen_files) を持つが，fmp 自身が直接
#  コンパイルするソース（sample1.c 等，fmp3 ライブラリの外）には同じ順序
#  依存が無かった．target_link_libraries(fmp PRIVATE fmp3 ...) はリンク時
#  の依存しか作らず，fmp 自身のコンパイルステップの順序は保証しない．
#
#  この漏れは，RISC-V（polarfire）では offset.h をアセンブラ（start.S）
#  からしか取り込まないため気付かれなかった（arch/riscv_gcc の start.S は
#  offset.h を含まない）．一方 ARM-M では core_kernel_impl.h が
#  （CFG1_OUT 以外で）offset.h を取り込み（arch/arm_m_gcc/common/
#  Makefile.core:51-64、arch/arm_m_gcc/doc/arm_m_design.md:287-319），
#  これが kernel_impl.h → target_kernel_impl.h → chip_kernel_impl.h →
#  core_kernel_impl.h の連鎖でほぼ全 C オブジェクトに波及するため，並列
#  ビルドでレース不具合（offset.h 不在によるコンパイル失敗）になりうる．
#  また kernel_cfg.h（CRE_TSK 等が払い出すオブジェクトIDの #define）は
#  arch を問わず sample1.c 自身が必要とする．
#
#  cfg1_out には付けない：cfg1_out.c は自身の中で TOPPERS_CFG1_OUT を
#  #define しており（cfg/pass1.rb:785），offset.h の生成自体が cfg1_out の
#  実行結果（.syms/.srec）を必要とするため，ここで依存させると循環になる．
#
add_dependencies(fmp generate_cfg_gen_files)

fmp3_add_syssvc(fmp)

target_include_directories(fmp
    PRIVATE ${FMP3_ROOT_DIR}/kernel
)
target_link_libraries(fmp PRIVATE fmp3 ${FMP3_LINK_LIBS})
target_link_options(fmp PRIVATE ${FMP3_LINK_OPTIONS})

#  リンカスクリプトを変更したら再リンクされるようにする
#  （-T / -Wl,-T は自動では依存にならない）
if(DEFINED FMP3_LDSCRIPT)
    set_property(TARGET fmp APPEND PROPERTY LINK_DEPENDS ${FMP3_LDSCRIPT})
endif()

fmp3_cfg_check(fmp)

#
#  実行ターゲット（target.cmake が FMP3_RUN_COMMAND を定義した場合）
#
if(DEFINED FMP3_RUN_COMMAND)
    add_custom_target(run
        COMMAND ${FMP3_RUN_COMMAND}
        DEPENDS fmp
        USES_TERMINAL
        COMMENT "Running on QEMU (Ctrl-A X to quit)"
    )
endif()

endif()  # NOT FMP3_LIBRARY_ONLY
