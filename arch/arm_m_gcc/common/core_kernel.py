# -*- coding: utf-8 -*-
#
#		パス2の生成スクリプトのコア依存部（ARM-M用）
#
#  ARM-M では，起動時ベクタテーブル・例外/割込みハンドラテーブル
#  （_kernel_exc_tbl）・割込み属性ビットパターン（_kernel_bitpat_cfgint）
#  の生成は，割込み番号の有効範囲（INHNO_VALID/INTNO_VALID/EXCNO_VALID）に
#  依存するため，ターゲット依存部（target_kernel.trb）で行う．本ファイルでは，
#  カーネル共通テンプレートのインクルードと，補助関数の定義のみを行う．
#  （このファイル自身は起動時ベクタテーブル生成コードを含まない．
#    生成は target_kernel.py 側（プロセッサ単位のテーブル）が担う．）
#
#  $Id: core_kernel.py (converted from core_kernel.trb by Claude Code Sonnet 5) $
#

#
#  配置するセクションを指定した変数定義の生成
#
def DefineVariableSection(genFile, defvar, secname):
    if secname != "":
        genFile.add(f'{defvar} __attribute__((section("{secname}"),nocommon));')
    else:
        genFile.add(f"{defvar};")


#
#  カーネルのデータ領域のセクション名
#
def SecnameKernelData(cls):
    return ""


#
#  スタック領域のセクション名
#
def SecnameStack(cls):
    return ""


#
#  ネイティブスピンロックの生成
#
#  チップ依存部が先に定義している場合は，そちらを優先する．
#  （本ファイルは末尾で kernel/kernel.py をインクルードして生成を実行するため，
#    チップ依存部が本ファイルのインクルード「後」に定義しても手遅れになる．
#    このため，チップ依存部は本ファイルのインクルード「前」に定義し，ここでは
#    未定義の場合にのみ既定の実装を与える．）
#  ★Ruby版は defined?(GenerateNativeSpn) が使えない（大文字始まりの識別子は
#    「定数」への参照と解釈され，メソッドの有無を判定できず常にnilになる）ため，
#    チップ依存部が立てる明示フラグ $generate_native_spn_defined で判定していた．
#    Python版では関数の有無は globals() で判定できるが，Ruby版と同じく
#    「チップ依存部が明示フラグを立てる」設計をそのまま踏襲する（chip_kernel.py
#    が先に本フラグをTrueにして独自定義した場合はここでの既定定義をスキップする）．
#    未定義グローバル参照（Task 4/6で踏んだ$var補間のnil問題とは別種）を避ける
#    ため，フラグ自体も未定義なら既定Falseを与える．
#
if "generate_native_spn_defined" not in globals():
    generate_native_spn_defined = False

if not generate_native_spn_defined:
    def GenerateNativeSpn(params):
        kernelCfgC.add(f"LOCK _kernel_lock_{params['spnid']};")
        return f"((intptr_t) &_kernel_lock_{params['spnid']})"


#
#  TSKINICTXBの初期化情報を生成
#
def GenerateTskinictxb(key, params):
    return (f"{{\t(void *)({params['tinib_stk']}), "
            f"\t((void *)((char *)({params['tinib_stk']}) + "
            f"({params['tinib_stksz']}))), }}")


#
#  ベクタテーブルの予約領域はデフォルトで0にする
#
def GenResVectVal(num):
    return 0


#
#  ターゲット非依存部のインクルード
#
IncludeTrb("kernel/kernel.py")
