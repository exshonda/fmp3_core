#
#  cfg1_out.syms に TOPPERS_magic_number が含まれることを確認する
#  （cfg1_out.syms を生成した直後に呼ばれるガード）．
#
#  使い方:
#    cmake -DSYMS=<cfg1_out.syms> -P check_magic_number.cmake
#
#  背景: cfg1_out のリンクは
#    target_link_options(cfg1_out PRIVATE
#        ${FMP3_LINK_OPTIONS} ${FMP3_CFG1_OUT_LINK_OPTIONS})
#  の順で ld にオプションを渡す．picolibc.specs 等が末尾に足す
#  --gc-sections（FMP3_LINK_OPTIONS 側）より，FMP3_CFG1_OUT_LINK_OPTIONS の
#  -Wl,--no-gc-sections が「後着」で ld に渡るために TOPPERS_magic_number
#  （--gc-sections だけなら除去されるシンボル）が生き残っている．
#  これは ld が同種オプションの最後着を勝たせるという暗黙の順序保証に
#  依存しており，target_link_options の間に別のオプションが挿し込まれる，
#  あるいは順序が入れ替わると，ビルドはエラー無く成功したまま
#  TOPPERS_magic_number だけが静かに消える．その場合，pass2 が
#  cfg1_out.syms からこのシンボルを探せずに error_exit するまで
#  気づけない（Task 5）．そこで生成直後にここで検査し，無ければここで
#  明確に止める．
#
if(NOT DEFINED SYMS)
    message(FATAL_ERROR "check_magic_number.cmake: SYMS が指定されていない")
endif()
if(NOT EXISTS ${SYMS})
    message(FATAL_ERROR "check_magic_number.cmake: ${SYMS} が存在しない")
endif()

file(STRINGS ${SYMS} _magic_lines REGEX "TOPPERS_magic_number")

if(_magic_lines STREQUAL "")
    message(FATAL_ERROR
"cfg1_out.syms に TOPPERS_magic_number が見つからない．
  何が起きたか: --gc-sections によって TOPPERS_magic_number が
    リンク時に除去された（cfg1_out は _start を参照しないため，
    未使用として GC される）．このまま進むと pass2 が
    cfg1_out.syms からこのシンボルを見つけられず error_exit する．
  どこを見るべきか: target_link_options(cfg1_out PRIVATE
    \${FMP3_LINK_OPTIONS} \${FMP3_CFG1_OUT_LINK_OPTIONS}) の引数列で，
    FMP3_CFG1_OUT_LINK_OPTIONS の -Wl,--no-gc-sections が，
    specs 由来／FMP3_LINK_OPTIONS の --gc-sections より
    ★後ろ★に来ているか確認すること（ld は同種オプションの
    最後着を勝たせるため，順序が入れ替わるとこの安全網は無効になる）．
  検査対象: ${SYMS}")
endif()
