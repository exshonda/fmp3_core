# -*- coding: utf-8 -*-
#
#		オフセットファイル生成用テンプレートファイル（ARM-M用）
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
offsetH.append(f"""\
#define TCB_p_tinib\t\t{offsetof_TCB_p_tinib}
#define TCB_pc\t\t\t{offsetof_TCB_pc}
#define TCB_sp\t\t\t{offsetof_TCB_sp}
#define TCB_stk_top\t\t{offsetof_TCB_stk_top}
#define TCB_fpu_flag\t\t{offsetof_TCB_fpu_flag}
#define TINIB_exinf\t\t{offsetof_TINIB_exinf}
#define TINIB_task\t\t{offsetof_TINIB_task}
#define TINIB_stk_bottom\t{offsetof_TINIB_TSKINICTXB_stk_bottom}
#define PCB_p_runtsk\t\t{offsetof_PCB_p_runtsk}
#define PCB_p_schedtsk\t\t{offsetof_PCB_p_schedtsk}
#define PCB_idstkpt\t\t{offsetof_PCB_idstkpt}
#define PCB_idstktop\t\t{offsetof_PCB_idstktop}
#define PCB_lock_flag\t\t{offsetof_PCB_lock_flag}
#define PCB_target_pcb\t\t{offsetof_PCB_target_pcb}
""")
