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
