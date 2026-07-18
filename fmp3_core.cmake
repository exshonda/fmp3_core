#
#		TOPPERS/FMP3 Core CMake エントリ
#
#  アプリケーション（または本リポジトリのルート CMakeLists.txt）から
#  include して使用する．
#
#  - FMP3_ROOT_DIR：FMP3 カーネルソースのルート（本ファイルの場所）
#  - FMP3_TARGET：ターゲット名（target/ 配下のディレクトリ名）
#  - FMP3_TARGET_DIR：ターゲット依存部（target.cmake）のディレクトリ．
#    未指定なら ${FMP3_ROOT_DIR}/target/${FMP3_TARGET}．
#    外部 SDK が target/ をリポジトリ外に置く場合は
#    -DFMP3_TARGET_DIR=<絶対パス> で供給する．
#
#  ★外部 SDK 契約（asp3_core / asp3_pico_sdk と同型。fmp3_add_syssvc() /
#  fmp3_cfg_check() を呼ぶ側が守るべきこと）：
#
#  CMake の function() は「呼び出し側のスコープ」で変数を解決する（定義時の
#  スコープではない）。本ファイルが定義する fmp3_add_syssvc() 等のヘルパ関数は
#  ${FMP3_ROOT_DIR} と ${FMP3_SYSSVC_TARGET_C_FILES} を参照するが，これらは
#  add_subdirectory(fmp3_core) の**子スコープ**の変数であり，親（呼び出し側）
#  スコープには自動で伝播しない（本ファイルに PARENT_SCOPE export は無い．
#  asp3_core も同じで PARENT_SCOPE を使っていない）。
#
#  外部 SDK が FMP3_LIBRARY_ONLY=ON ＋ add_subdirectory(fmp3_core) で本リポジトリを
#  取り込み，その後 add_subdirectory() の外（＝呼び出し側スコープ）で
#  fmp3_add_syssvc(<target>) を呼ぶ場合，呼び出し側の CMakeLists.txt（または
#  パス解決層 fmp3_<sdk>.cmake）が **add_subdirectory() より前に**
#      set(FMP3_ROOT_DIR <fmp3_core への絶対パス>)
#  を自分のスコープで明示的に設定しておく必要がある．前例：
#  asp3_pico_sdk/asp3/asp3_pico_sdk.cmake:27-30 が
#  `set(ASP3_ROOT_DIR ${ASP3_CORE_DIR})` を同じ理由でコメント付きで行っている．
#
#  ★FMP3_SYSSVC_TARGET_C_FILES は同じ手が使えない：この変数は
#  ${FMP3_TARGET_DIR}/target.cmake（および chip.cmake）が **fmp3_core 自身の
#  ディレクトリスコープの中で** list(APPEND ...) / list(REMOVE_ITEM ...) して
#  組み立てる値であり，呼び出し側が事前に set() できる性質のものではない．
#  現状これが問題化しないのは，唯一の前例（asp3_pico_sdk の pico2_*_sdk_gcc）が
#  ターゲット依存 SIO ドライバを APPEND した直後に REMOVE_ITEM で同じファイルを
#  取り除いており，子スコープ内で実質空リストになるため，呼び出し側スコープで
#  未定義（＝空扱い）と一致してたまたま辻褄が合っているに過ぎない（詳細は
#  docs/superpowers/specs/2026-07-18-fmp3-cmake-design.md §9.2 の追記参照）．
#  target.cmake が非空の FMP3_SYSSVC_TARGET_C_FILES を必要とする外部ターゲットを
#  add_subdirectory 経由（SDK-as-top-level）で使う場合，本ファイルは対応しない
#  （未対応。対応するなら target.cmake 側から PARENT_SCOPE export する設計変更が要る）．
#
set(FMP3_ROOT_DIR ${CMAKE_CURRENT_LIST_DIR})

#
#  コンパイラが本当に RISC-V ベアメタル向けかを検査する（project() の後で
#  ないと CMAKE_C_COMPILER が確定しないため，project() の後で include される
#  本ファイルの先頭で行う）．
#
include(${FMP3_ROOT_DIR}/cmake/toolchain_check.cmake)

if(NOT DEFINED FMP3_TARGET)
    message(FATAL_ERROR
        "FMP3_TARGET is not defined. "
        "Use a preset (e.g. --preset polarfire_soc_kit-qemu) or -DFMP3_TARGET=<target>.")
endif()

if(NOT DEFINED FMP3_TARGET_DIR)
    set(FMP3_TARGET_DIR ${FMP3_ROOT_DIR}/target/${FMP3_TARGET})
endif()

if(NOT EXISTS ${FMP3_TARGET_DIR}/target.cmake)
    message(FATAL_ERROR
        "${FMP3_TARGET_DIR}/target.cmake not found. "
        "FMP3_TARGET='${FMP3_TARGET}' is not supported by the CMake build "
        "(set -DFMP3_TARGET_DIR=<dir> for an external/SDK target).")
endif()

#
#  非TECS版システムサービスと library のソースを TARGET に追加するヘルパ．
#  上流の configure.rb 引数
#    -S "syslog.o banner.o serial.o serial_cfg.o logtask.o mmuart.o chip_serial.o"
#  に対応する（mmuart.o / chip_serial.o は chip.cmake が
#  FMP3_SYSSVC_TARGET_C_FILES に積む）．
#
function(fmp3_add_syssvc TARGET)
    if(NOT FMP3_ROOT_DIR)
        message(FATAL_ERROR
            "fmp3_add_syssvc(${TARGET}): FMP3_ROOT_DIR is empty in the caller's scope. "
            "fmp3_add_syssvc() resolves \${FMP3_ROOT_DIR} in the scope of whoever calls "
            "it, not fmp3_core's own directory scope (CMake function() semantics). "
            "If you add_subdirectory(fmp3_core)'d this with FMP3_LIBRARY_ONLY=ON and are "
            "calling fmp3_add_syssvc() from your own CMakeLists.txt, set(FMP3_ROOT_DIR "
            "<path-to-fmp3_core>) in your own scope BEFORE the add_subdirectory() call "
            "(see fmp3_core.cmake header comment; asp3_pico_sdk.cmake:27-30 is the "
            "precedent for this pattern in the asp3 family).")
    endif()
    target_sources(${TARGET} PRIVATE
        ${FMP3_ROOT_DIR}/syssvc/syslog.c
        ${FMP3_ROOT_DIR}/syssvc/banner.c
        ${FMP3_ROOT_DIR}/syssvc/serial.c
        ${FMP3_ROOT_DIR}/syssvc/serial_cfg.c
        ${FMP3_ROOT_DIR}/syssvc/logtask.c
        ${FMP3_SYSSVC_TARGET_C_FILES}
    )
    target_sources(${TARGET} PRIVATE
        ${FMP3_ROOT_DIR}/library/log_output.c
        ${FMP3_ROOT_DIR}/library/vasyslog.c
        ${FMP3_ROOT_DIR}/library/t_perror.c
        ${FMP3_ROOT_DIR}/library/strerror.c
    )
endfunction()
