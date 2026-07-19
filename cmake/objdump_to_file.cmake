#
#  objdump -s <opts> <elf> の出力をファイルに書く（">" を使わないため，
#  nm_to_file.cmake と同じ理由）．
#
#  使い方:
#    cmake -DOBJDUMP=<objdump> -DELF=<elf> -DDUMPOPTS="-j .text -j .rodata"
#          -DOUT=<file> -P objdump_to_file.cmake
#
#  DUMPOPTS はスペース区切りの1本の文字列として渡す（CMakeのリストは
#  ";"区切りでコマンドライン上のクォートが煩雑になるため）．
#
separate_arguments(_fmp3_dumpopts_list UNIX_COMMAND "${DUMPOPTS}")
execute_process(
    COMMAND ${OBJDUMP} -s ${_fmp3_dumpopts_list} ${ELF}
    OUTPUT_VARIABLE _dump
    RESULT_VARIABLE _rc
    ERROR_VARIABLE  _err
)
if(NOT _rc EQUAL 0)
    message(FATAL_ERROR "objdump failed (${_rc}): ${_err}")
endif()
file(WRITE ${OUT} "${_dump}")
