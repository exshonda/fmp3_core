#
#  nm の出力をファイルに書く（シェルの ">" を使わないための小道具）
#
#  使い方:
#    cmake -DNM=<nm> -DELF=<elf> -DOUT=<file> -P nm_to_file.cmake
#
#  add_custom_command で "COMMAND nm ... > out" と書くと，リダイレクトを
#  解釈できるジェネレータ（Ninja/Makefile）でしか動かない．
#
execute_process(
    COMMAND ${NM} -n ${ELF}
    OUTPUT_VARIABLE _syms
    RESULT_VARIABLE _rc
    ERROR_VARIABLE  _err
)
if(NOT _rc EQUAL 0)
    message(FATAL_ERROR "nm failed (${_rc}): ${_err}")
endif()
file(WRITE ${OUT} "${_syms}")
