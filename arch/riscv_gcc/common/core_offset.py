# -*- coding: utf-8 -*-
#
#		オフセットファイル生成用テンプレートファイル（RISC-V用）
#
#  $Id: core_offset.py (converted from core_offset.trb by Claude Code Sonnet 5) $
#

#
#  ターゲット非依存部のインクルード
#
IncludeTrb("kernel/genoffset.py")

#
#  フィールドのオフセットの定義の生成
#
#  RISC-Vでは T_EXCINF 構造体に cpsr フィールドが無く，PCB にも例外ネスト
#  カウンタ（PCB_exncnt）が無いため，$offsetof_T_EXCINF_cpsr / $offsetof_PCB_exncnt
#  はどこにも代入されない．Ruby版では未代入の $グローバル変数を参照すると
#  nilになり，文字列展開（#{...}）では空文字列として出力される
#  （実測: `#define T_EXCINF_cpsr\t`／`#define PCB_exncnt\t\t` のように
#  値部分が空のまま出力される）。Pythonでは未定義グローバル参照はNameError
#  になるため，Ruby のnil展開結果と同じ空文字列をここで明示的に補う
#  （core_kernel.pyのUSE_INTCFG_TABLEガードと同種の対策だが，あちらは
#  真偽値として使うのでFalseを補っている．こちらは文字列展開に使うため
#  空文字列""を補う必要があり，Falseを補うと出力が"False"になってしまう）．
#
if "offsetof_T_EXCINF_cpsr" not in globals():
    offsetof_T_EXCINF_cpsr = ""
if "offsetof_PCB_exncnt" not in globals():
    offsetof_PCB_exncnt = ""

offsetH.append(f"""\
#define TCB_p_tinib\t\t{offsetof_TCB_p_tinib}
#define TCB_sp\t\t\t{offsetof_TCB_sp}
#define TCB_pc\t\t\t{offsetof_TCB_pc}
#define TINIB_tskatr\t{offsetof_TINIB_tskatr}
#define TINIB_exinf\t\t{offsetof_TINIB_exinf}
#define TINIB_task\t\t{offsetof_TINIB_task}
#define TINIB_stksz\t\t{offsetof_TINIB_stksz}
#define TINIB_stk\t\t{offsetof_TINIB_stk}
#define T_EXCINF_cpsr\t{offsetof_T_EXCINF_cpsr}
#define PCB_p_runtsk\t{offsetof_PCB_p_runtsk}
#define PCB_p_schedtsk\t{offsetof_PCB_p_schedtsk}
#define PCB_exncnt\t\t{offsetof_PCB_exncnt}
#define PCB_istkpt\t\t{offsetof_PCB_istkpt}
#define PCB_idstkpt\t\t{offsetof_PCB_idstkpt}
#define PCB_p_exc_tbl\t{offsetof_PCB_p_exc_tbl}
#define PCB_p_inh_tbl\t{offsetof_PCB_p_inh_tbl}
""")
