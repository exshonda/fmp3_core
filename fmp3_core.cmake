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
set(FMP3_ROOT_DIR ${CMAKE_CURRENT_LIST_DIR})

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
