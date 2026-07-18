#
#  .trb（cfg pass2 生成スクリプト）が IncludeTrb("...") で再帰的に
#  取り込む先を configure 時に解決し，依存ファイルの閉包を求める．
#
#  背景（Task 5 レビュー Critical）：add_custom_command(DEPENDS ...) には
#  トップレベルの .trb（-T/-C で渡す target_kernel.trb 等）しか入って
#  いなかった．しかし .trb は IncludeTrb("...") で他の .trb を再帰的に
#  load する（例：target_kernel.trb → chip_kernel.trb → core_kernel.trb
#  → kernel/kernel.trb → kernel/task.trb ...）．間接的に取り込まれる
#  .trb を編集しても DEPENDS に無いため Ninja が「変更なし」と誤判定し，
#  古い生成物が無警告で使われ続ける．これを塞ぐため，configure 時に
#  IncludeTrb(...) の連鎖をテキスト走査で辿り，閉包を DEPENDS に足す．
#
#  パス解決規則は cfg/cfg.rb の IncludeTrb / SearchFilePath
#  （cfg/cfg.rb:406-437，2026-07-19 時点の行番号）を忠実に再現する：
#    1. fileName をそのまま存在チェック．cfg 実行時の cwd は
#       add_custom_command の WORKING_DIRECTORY（= CFG_GEN_DIR）なので，
#       ここでは CWD_DIR 起点で存在チェックする．fileName が絶対パスの
#       場合は Ruby の File.exist? 同様 cwd を無視してそのまま見る
#       （トップレベルの .trb は CMake 側から絶対パスで -T/-C 渡される
#       ため，実質ここで解決する）．
#    2. "." で始まる相対パスは，1. で見つからなければ「存在しない」と
#       断定する（cfg 側が意図しない相対参照を防ぐための仕様．
#       cfg.rb 406-413 のコメント参照）．
#    3. それ以外は $includeDirectories（-I の列，渡された順）を順に
#       includeDirectory + "/" + fileName で探し，最初に見つかったもの
#       を使う．CMake 側では INCLUDE_DIRS 引数（FMP3_INCLUDE_DIRS と
#       同じ変数・同じ並び順であることが呼び出し側の責務）で再現する．
#
#  ★解決できない IncludeTrb("...") は黙って飛ばさず FATAL_ERROR で
#    止める．黙って飛ばすと，まさに今直そうとしている「無警告で古い
#    生成物が使われる」状態に逆戻りするため．
#
#  ★条件付き IncludeTrb（if/unless の中で実行時にしか呼ばれるかどうか
#    決まらないもの）について：本関数は Ruby を実行しない単純な
#    テキスト走査であり，条件の真偽を判定できない．2026-07-19 時点で
#    リポジトリ内の全 .trb（`grep -rn -B3 'IncludeTrb(' **/*.trb`）を
#    目視確認した限り，IncludeTrb(...) の呼び出しはすべてトップレベル
#    文であり，条件分岐の中にあるものは無い．仮に将来そのような
#    条件付き呼び出しが追加されても，本関数は分岐の真偽に関わらず
#    テキスト上に現れる呼び出し先をすべて依存に含める（＝実行されない
#    かもしれない分岐先も安全側に依存へ入れる）．これは「不要な
#    再生成が起きることがある」側の誤りであり，「陳腐化した生成物が
#    無警告で使われる」側の誤りより無害なので，意図的にこちらへ倒す．
#
#  ★循環参照（A が B を include し，B が A を include する等）に対して
#    無限ループしない：一度閉包に入れたファイルは worklist に積み直さ
#    ない．
#
#  ★configure 時トリガについて：本関数が返す閉包は，呼び出し側が
#    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ...)
#    に足すこと．閉包に入っている既存の .trb の中身が変わって
#    IncludeTrb(...) の集合が変化した場合（新しい取り込みの追加／削除），
#    CMAKE_CONFIGURE_DEPENDS がその .trb の変更を検知して configure を
#    やり直し，このマクロが再実行されて閉包を計算し直す．
#    ルート側（FMP3_*_TRB_FILES に積む .trb 自体を増やす／減らす）は
#    target.cmake / arch.cmake / chip.cmake の編集で行うため，これらの
#    .cmake ファイルは CMake が自動的に configure 依存として追跡する
#    （明示指定不要）．
#
#    唯一カバーできないのは「閉包にまだ一度も入ったことのない，
#    トップレベルからは辿れない新規 .trb を，どの既存 .trb からも
#    IncludeTrb されない形でいきなり追加する」ケースだが，そのような
#    ファイルは定義上どの生成物にも取り込まれないので依存追跡の対象外
#    で問題ない．
#
#  使い方：
#    fmp3_trb_closure(<出力変数> <CWD_DIR> "<INCLUDE_DIRS(セミコロン区切り)>"
#        <ルートの.trbファイル(絶対パス)...>)
#
function(fmp3_trb_closure OUT_VAR CWD_DIR INCLUDE_DIRS)
    set(_worklist ${ARGN})
    set(_result "")

    while(_worklist)
        list(POP_FRONT _worklist _current)

        if(_current IN_LIST _result)
            continue()
        endif()
        if(NOT EXISTS "${_current}")
            message(FATAL_ERROR
                "fmp3_trb_closure: trb file not found: ${_current}")
        endif()
        list(APPEND _result "${_current}")

        #  コメント行（行頭が # のみ．cfg の .trb は Ruby なので # が
        #  コメント）を除いてから走査する．IncludeTrb(...) の呼び出しは
        #  本リポジトリの現状すべて非コメント行だが，将来コメントアウト
        #  された例示コード等で誤検出しないための防御．
        file(STRINGS "${_current}" _lines)
        set(_code_lines "")
        foreach(_line IN LISTS _lines)
            if(NOT _line MATCHES "^[ \t]*#")
                list(APPEND _code_lines "${_line}")
            endif()
        endforeach()
        string(REPLACE ";" "\n" _content "${_code_lines}")

        string(REGEX MATCHALL "IncludeTrb\\(\"[^\"]+\"\\)" _matches "${_content}")
        foreach(_m IN LISTS _matches)
            string(REGEX REPLACE "IncludeTrb\\(\"([^\"]+)\"\\)" "\\1" _name "${_m}")

            set(_resolved "")

            #  規則1: そのまま存在チェック（絶対パスなら cwd 無視，
            #  相対パスなら CWD_DIR 起点）．
            if(IS_ABSOLUTE "${_name}")
                if(EXISTS "${_name}")
                    set(_resolved "${_name}")
                endif()
            else()
                if(EXISTS "${CWD_DIR}/${_name}")
                    set(_resolved "${CWD_DIR}/${_name}")
                endif()
            endif()

            if(_resolved STREQUAL "")
                if(_name MATCHES "^\\.")
                    #  規則2: "." 始まりで規則1に無ければ「存在しない」
                    #  ものとして扱う（cfg.rb と同じ）．_resolved は
                    #  空のままにして，下の FATAL_ERROR で検出させる．
                else()
                    #  規則3: インクルードディレクトリを順に探す．
                    foreach(_dir IN LISTS INCLUDE_DIRS)
                        if(EXISTS "${_dir}/${_name}")
                            set(_resolved "${_dir}/${_name}")
                            break()
                        endif()
                    endforeach()
                endif()
            endif()

            if(_resolved STREQUAL "")
                message(FATAL_ERROR
                    "fmp3_trb_closure: IncludeTrb(\"${_name}\") in "
                    "${_current} を解決できない（cfg/cfg.rb の "
                    "SearchFilePath と同じ規則で CWD=${CWD_DIR} と "
                    "INCLUDE_DIRS=${INCLUDE_DIRS} を探索したが見つから"
                    "ない）．黙って無視すると陳腐化した生成物が無警告で"
                    "使われる状態に戻るため停止する．cfg 側が実際に "
                    "load できているなら，このマクロへ渡す INCLUDE_DIRS "
                    "が cfg へ渡す -I の並びとずれている可能性がある．")
            endif()

            get_filename_component(_resolved_abs "${_resolved}" ABSOLUTE)
            if(NOT _resolved_abs IN_LIST _result AND NOT _resolved_abs IN_LIST _worklist)
                list(APPEND _worklist "${_resolved_abs}")
            endif()
        endforeach()
    endwhile()

    set(${OUT_VAR} ${_result} PARENT_SCOPE)
endfunction()
