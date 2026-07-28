# TOPPERS/FMP3 OS-awareness for gdb  （STM32MP257F-DK / AArch64）
#
# gdb の Python 機構で FMP3 カーネルの状態を可視化する（Step2: OS Awareness）。
#
# 使い方:
#   (gdb) source <この .py>
#   (gdb) stask              … タスクの静的情報 (_kernel_tinib_table) を表示
#
#   const な _kernel_tinib_table は ELF に初期値が載るため，実行中ターゲットに
#   接続していなくても `aarch64-none-elf-gdb fmp -ex "source os_awareness.py" -ex stask`
#   で表示できる（試作・確認に便利）。動的情報(atask)は実機/コアダンプが要る。
#
# 対応カーネル定数（fmp3_3.3_svn/kernel, include/kernel.h より）:
#   TMIN_TSKID=1, TMIN_TPRI=1, EXT_TSKPRI(x)=x+TMIN_TPRI
#   タスク属性 TA_ACT=0x02（生成時に起動）

import gdb
import os
import re
import sys

# ターゲット依存部モジュール(target/chip/core_os_awareness.py)の import で .pyc を生成させない
# （ソースツリーに __pycache__ を残さないため）。
sys.dont_write_bytecode = True

# ターゲット依存部の awareness（割込み許可/禁止状態など）。
# launcher（make osdebug 等）がターゲット依存部のディレクトリを sys.path に追加していれば
# import できる。無ければ該当機能（intr の ena/pend 列）を省略する。
try:
    import target_os_awareness as _TGT
except Exception:
    _TGT = None

TMIN_TSKID = 1
TMIN_SEMID = 1
TMIN_TPRI  = 1
TA_ACT     = 0x02
TA_TPRI    = 0x01   # 待ち行列を優先度順に（未設定なら TA_TFIFO）


def _eval(expr):
    return gdb.parse_and_eval(expr)


def _kernel_cfg_path():
    """kernel_cfg.h を探す（ELF と同じディレクトリ → cwd → ./gen）。無ければ None。"""
    try:
        for o in gdb.objfiles():
            if o.filename:
                p = os.path.join(os.path.dirname(o.filename), "kernel_cfg.h")
                if os.path.exists(p):
                    return p
    except Exception:
        pass
    for p in ("kernel_cfg.h", os.path.join("gen", "kernel_cfg.h")):
        if os.path.exists(p):
            return p
    return None


def _load_obj_names():
    """kernel_cfg.h からオブジェクト種別ごとの ID→名前辞書を作る。

    kernel_cfg.h は種別ごとに `#define TNUM_<種別>ID n` の後に `#define <名前> <id>` が
    並ぶ。`TNUM_<X>ID` 〜 次の `TNUM_...` の区間を種別 X の名前定義として採る。
    返り値: {"TSK": {id:name}, "SEM": {...}, "DTQ": {...}, ...}。
    ファイルが無ければ空辞書（→ 数字のみ表示）。
    """
    path = _kernel_cfg_path()
    out = {}
    if not path:
        return out
    cur = None
    try:
        with open(path) as f:
            for line in f:
                m = re.match(r'\s*#define\s+(\w+)\s+(\d+)\b', line)
                if not m:
                    continue
                name, num = m.group(1), int(m.group(2))
                tm = re.match(r'TNUM_(\w+)ID$', name)
                if tm:
                    cur = tm.group(1)
                    out.setdefault(cur, {})
                elif name.startswith("TNUM_"):
                    cur = None
                elif cur is not None:
                    out[cur][num] = name
    except OSError:
        return {}
    return out


def _load_task_names():
    """タスクID→名前の辞書（_load_obj_names の TSK 部分）。"""
    return _load_obj_names().get("TSK", {})


def _load_isr_map():
    """kernel_cfg.c から ISR ラッパ関数名→[(ISR関数名, exinf), ...] の辞書を作る。

    ATT_ISR で登録した ISR は，コンフィギュレータが生成するラッパ関数
    _kernel_inthdr_<intno>（kernel_cfg.c）経由で呼ばれ，ハンドラテーブルには
    ラッパの番地が入る。ラッパ本体の `((ISR)(関数))((EXINF)(値))` を解析して
    実際の ISR 関数名と exinf を得る（同一割込みに複数 ISR も可）。
    kernel_cfg.c は kernel_cfg.h と同じディレクトリを見る。無ければ空辞書。
    """
    path = _kernel_cfg_path()
    out = {}
    if not path:
        return out
    cpath = os.path.join(os.path.dirname(path), "kernel_cfg.c")
    try:
        with open(cpath) as f:
            src = f.read()
    except OSError:
        return out
    for m in re.finditer(r'(_kernel_inthdr_\d+)\(void\)\s*\{(.*?)\}', src, re.S):
        calls = re.findall(r'\(\(ISR\)\((\w+)\)\)\s*\(\(EXINF\)\(([^)]*)\)\)',
                           m.group(2))
        if calls:
            out[m.group(1)] = calls
    return out


# 待ち種別(tstat & TS_WAITING_MASK) → (種別キー, CB ポインタ表シンボル)
_WOBJ_TABLE = {
    0x10 << 2: ("SEM", "_kernel_p_semcb_table"),   # TS_WAITING_SEM
    0x11 << 2: ("FLG", "_kernel_p_flgcb_table"),   # TS_WAITING_FLG
    0x08 << 2: ("DTQ", "_kernel_p_dtqcb_table"),   # TS_WAITING_RDTQ
    0x12 << 2: ("DTQ", "_kernel_p_dtqcb_table"),   # TS_WAITING_SDTQ
    0x09 << 2: ("PDQ", "_kernel_p_pdqcb_table"),   # TS_WAITING_RPDQ
    0x13 << 2: ("PDQ", "_kernel_p_pdqcb_table"),   # TS_WAITING_SPDQ
    0x14 << 2: ("MTX", "_kernel_p_mtxcb_table"),   # TS_WAITING_MTX
    0x15 << 2: ("MPF", "_kernel_p_mpfcb_table"),   # TS_WAITING_MPF
}


def _resolve_wobj(tstat, p_wobjcb, objnames):
    """待ちオブジェクトの管理ブロックポインタ(p_wobjcb)を名前(ID)へ解決する。

    各 CB はポインタ表 _kernel_p_<obj>cb_table[] で管理されるので，表を線形探索して
    一致する添字 k から ID = k + TMIN_<obj>ID(=1) を得る。名前は kernel_cfg.h 由来。
    """
    w = tstat & TS_WAITING_MASK
    info = _WOBJ_TABLE.get(w)
    addr = int(p_wobjcb) & 0xFFFFFFFFFFFFFFFF
    if info is None or addr == 0:
        return "-"
    typekey, tbl = info
    try:
        arr = _eval(tbl)
        cnt = int(_eval("sizeof(%s)/sizeof(void*)" % tbl))
    except gdb.error:
        return "%s?0x%x" % (typekey, addr)
    oid = None
    for k in range(cnt):
        if int(arr[k]) == addr:
            oid = k + 1
            break
    if oid is None:
        return "%s?0x%x" % (typekey, addr)
    nm = objnames.get(typekey, {}).get(oid)
    return ("%s(%d)" % (nm, oid)) if nm else ("%s#%d" % (typekey, oid))


def _sym_for(addr):
    """アドレスに対応する関数/シンボル名を返す（無ければ空文字）。"""
    addr = int(addr) & 0xFFFFFFFFFFFFFFFF
    if addr == 0:
        return ""
    try:
        s = gdb.execute("info symbol 0x%x" % addr, to_string=True).strip()
    except gdb.error:
        return ""
    if not s or s.startswith("No symbol"):
        return ""
    # 例: "task_main in section .text" / "task1 + 4 in section .text"
    return s.split(" in section")[0].split(" + ")[0].strip()


# 優先度段階数（TMAX_TPRI - TMIN_TPRI + 1 = 16）。ready_primap は uint16_t。
TNUM_TPRI = 16

# --- タスク状態(tstat, 内部表現)のデコード（kernel/task.h より） ---
TS_RUNNABLE     = 0x01
TS_SUSPENDED    = 0x02
TS_WAITING_MASK = 0x1f << 2
_WAIT_NAME = {
    0x01 << 2: "SLP",  0x02 << 2: "DLY",  0x08 << 2: "RDTQ", 0x09 << 2: "RPDQ",
    0x10 << 2: "SEM",  0x11 << 2: "FLG",  0x12 << 2: "SDTQ", 0x13 << 2: "SPDQ",
    0x14 << 2: "MTX",  0x15 << 2: "MPF",
}


def _state_str(tstat, running):
    if tstat == 0:
        return "DORMANT"
    parts = []
    if tstat & TS_RUNNABLE:
        parts.append("RUNNING" if running else "READY")
    w = tstat & TS_WAITING_MASK
    if w:
        parts.append("WAIT-" + _WAIT_NAME.get(w, "0x%02x" % (w >> 2)))
    if tstat & TS_SUSPENDED:
        parts.append("SUSPENDED")
    return "|".join(parts) if parts else ("0x%02x" % tstat)


def _tid_label(tid, names):
    nm = names.get(tid)
    return ("%s(%d)" % (nm, tid)) if nm else str(tid)


def _tcb_to_tid(p_tinib_addr, tinib_base, tinib_sz):
    """TCB の p_tinib から TID を逆算（TSKID マクロ相当）。"""
    return (p_tinib_addr - tinib_base) // tinib_sz + TMIN_TSKID


def _stack_use(tcb):
    """保存スタックポインタ(tskctxb.sp)から現スタック使用量 "used/size" を概算する。

    スタックは [stk, stk+stksz)。used = (stk+stksz) - sp。非実行タスクでは正確，実行中
    タスクは最後の切替時の値（概算）。休止/範囲外は "-"。
    """
    try:
        inib = tcb['p_tinib']
        stk  = int(inib['stk'])
        sz   = int(inib['stksz'])
        sp   = int(tcb['tskctxb']['sp'])
    except gdb.error:
        return "-"
    top = stk + sz
    if sp == 0 or sp < stk or sp > top:
        return "-"
    return "%d/%d" % (top - sp, sz)


def _walk_taskqueue(head, tcb_ptr_t, tinib0, tinib_sz, names):
    """QUEUE 先頭(head: QUEUE 値)から TCB を辿り TID ラベルの list を返す。

    task_queue / wait_queue は TCB の先頭(offset 0)なので，キュー node を TCB* に
    キャストするだけで TCB が得られる。
    """
    head_addr = int(head.address)
    node = head['p_next']
    out = []
    guard = 0
    # 正常な空キューは p_next==head(自己ループ)。未初期化(p_next==0)は空扱いで抜ける。
    while int(node) != head_addr and int(node) != 0 and guard < 256:
        p_tcb = node.cast(tcb_ptr_t)
        t = _tcb_to_tid(int(p_tcb.dereference()['p_tinib']), tinib0, tinib_sz)
        out.append(_tid_label(t, names))
        node = node.dereference()['p_next']
        guard += 1
    return out


# 待ちキューを持つオブジェクト種別: (種別キー, CB ポインタ表, [(待ちキュー field, ラベル)])
# SEM/FLG/MTX/MPF は単一 wait_queue。DTQ/PDQ は送信(swait)・受信(rwait)の2本。
_WQ_KINDS = [
    ("SEM", "_kernel_p_semcb_table", [("wait_queue",  "")]),
    ("FLG", "_kernel_p_flgcb_table", [("wait_queue",  "")]),
    ("DTQ", "_kernel_p_dtqcb_table", [("swait_queue", "snd"), ("rwait_queue", "rcv")]),
    ("PDQ", "_kernel_p_pdqcb_table", [("swait_queue", "snd"), ("rwait_queue", "rcv")]),
    ("MTX", "_kernel_p_mtxcb_table", [("wait_queue",  "")]),
    ("MPF", "_kernel_p_mpfcb_table", [("wait_queue",  "")]),
]


class StaticTaskCmd(gdb.Command):
    """stask : show FMP3 task static info (_kernel_tinib_table)."""

    def __init__(self):
        super(StaticTaskCmd, self).__init__("stask", gdb.COMMAND_USER)

    def invoke(self, arg, from_tty):
        try:
            tmax  = int(_eval("_kernel_tmax_tskid"))
            tinib = _eval("_kernel_tinib_table")
        except gdb.error as e:
            print("stask: cannot read symbols (%s). Is the fmp ELF loaded?" % e)
            return

        names = _load_task_names()
        n = tmax - TMIN_TSKID + 1
        src = "named" if names else "numeric only (kernel_cfg.h not found)"
        print("FMP3 task static info  (tnum_tsk = %d, TID=%s)" % (n, src))
        print("%-22s %-16s %-10s %-4s %-4s %-9s %-8s %-12s %s"
              % ("TID", "entry(task)", "exinf", "pri", "prc", "affinity",
                 "stksz", "stk", "attr"))
        print("-" * 100)
        for tid in range(TMIN_TSKID, tmax + 1):
            t = tinib[tid - TMIN_TSKID]
            nm    = names.get(tid)
            tidst = ("%s(%d)" % (nm, tid)) if nm else str(tid)
            entry = _sym_for(t['task']) or ("0x%x" % int(t['task']))
            exinf = int(t['exinf'])
            pri   = int(t['ipriority']) + TMIN_TPRI
            prc   = int(t['iprcid'])
            aff   = int(t['affinity'])
            stksz = int(t['stksz'])
            stk   = int(t['stk'])
            attr  = int(t['tskatr'])
            act   = "TA_ACT" if (attr & TA_ACT) else "-"
            print("%-22s %-16s 0x%-8x %-4d %-4d 0x%-7x %-8d 0x%-10x %s"
                  % (tidst, entry, exinf & 0xFFFFFFFFFFFFFFFF, pri, prc, aff,
                     stksz, stk, act))


class DynTaskCmd(gdb.Command):
    """atask [tid|name] : show FMP3 task dynamic info (TCB), ready queues and wait queues.

    No argument   : dynamic info of all tasks + each processor's ready queue + wait queues.
    With argument : only the specified task (number or CRE_TSK name).
    Dynamic info lives in RAM, so attach to a running target / core dump
    (with the ELF alone, .bss reads as its initial value = all DORMANT).
    """

    def __init__(self):
        super(DynTaskCmd, self).__init__("atask", gdb.COMMAND_USER)

    def invoke(self, arg, from_tty):
        try:
            tmax    = int(_eval("_kernel_tmax_tskid"))
            tcbtab  = _eval("_kernel_p_tcb_table")
            pcbtab  = _eval("_kernel_p_pcb_table")
            nprc    = int(_eval("sizeof(_kernel_p_pcb_table)/sizeof(void*)"))
            tinib0  = int(_eval("_kernel_tinib_table").address)
            tinib_sz = gdb.lookup_type("TINIB").sizeof
        except gdb.error as e:
            print("atask: cannot read symbols (%s). Is the ELF loaded?" % e)
            return

        objnames = _load_obj_names()
        names    = objnames.get("TSK", {})
        name2id  = {v: k for k, v in names.items()}

        arg = (arg or "").strip()
        if arg:
            if arg.isdigit():
                tid = int(arg)
            elif arg in name2id:
                tid = name2id[arg]
            else:
                print("atask: task '%s' not found" % arg)
                return
            if tid < TMIN_TSKID or tid > tmax:
                print("atask: TID %d out of range (1..%d)" % (tid, tmax))
                return
            ids = [tid]
        else:
            ids = list(range(TMIN_TSKID, tmax + 1))

        # 各タスクの動的情報
        print("FMP3 task dynamic info")
        print("%-22s %-20s %-8s %-4s %-12s %-4s %-5s %-5s %s"
              % ("TID", "state", "cur/base", "prc", "stk(use/sz)",
                 "actq", "wupq", "rast", "wobj"))
        print("-" * 104)
        for tid in ids:
            p_tcb = tcbtab[tid - TMIN_TSKID]
            tcb   = p_tcb.dereference()
            tstat = int(tcb['tstat'])
            p_pcb = tcb['p_pcb']
            prc   = int(p_pcb['prcid']) if int(p_pcb) != 0 else 0
            running = (int(p_pcb) != 0 and int(p_pcb['p_runtsk']) == int(p_tcb))
            state = _state_str(tstat, running)
            if tstat == 0:
                prib = "-"
            else:
                prib = "%d/%d" % (int(tcb['priority']) + TMIN_TPRI,
                                  int(tcb['bpriority']) + TMIN_TPRI)
            stk   = _stack_use(tcb)
            wobjs = _resolve_wobj(tstat, tcb['p_wobjcb'], objnames)
            print("%-22s %-20s %-8s %-4s %-12s %-4d %-5d %-5d %s"
                  % (_tid_label(tid, names), state, prib,
                     (str(prc) if prc else "-"), stk,
                     int(tcb['actque']), int(tcb['wupque']), int(tcb['raster']),
                     wobjs))

        if arg:
            return

        # 各プロセッサのレディキュー状態
        tcb_ptr_t = gdb.lookup_type("TCB").pointer()
        for i in range(nprc):
            pcb = pcbtab[i].dereference()
            prc_no = i + 1          # p_pcb_table[i] はプロセッサ i+1 に対応
            primap = int(pcb['ready_primap']) & 0xFFFF
            run    = pcb['p_runtsk']
            sched  = pcb['p_schedtsk']

            def _tcbptr_label(p):
                if int(p) == 0:
                    return "(none)"
                t = _tcb_to_tid(int(p.dereference()['p_tinib']), tinib0, tinib_sz)
                return _tid_label(t, names)

            print("\n[PRC%d] runtsk=%s schedtsk=%s  ready_primap=0x%04x"
                  % (prc_no, _tcbptr_label(run), _tcbptr_label(sched), primap))
            if primap == 0:
                print("  ready queue: (empty)")
                continue
            rq = pcb['ready_queue']
            # arm64 は PRIMAP_BIT(pri)=(0x8000>>pri)（MSB 詰め, CLZ サーチ用）。
            # ready_queue は内部優先度 pri(0=最高)で添字付け。外部優先度 = pri + TMIN_TPRI。
            for ipri in range(TNUM_TPRI):
                if not (primap & (0x8000 >> ipri)):
                    continue
                members = _walk_taskqueue(rq[ipri], tcb_ptr_t, tinib0,
                                          tinib_sz, names)
                print("  pri %2d: %s" % (ipri + TMIN_TPRI, ", ".join(members)))

        # 同期・通信オブジェクトの待ちキュー（待ち中のタスク一覧）
        print("\nWait queues (tasks blocked on synchronization/communication objects)")
        any_wq = False
        for typekey, tbl, fields in _WQ_KINDS:
            try:
                arr = _eval(tbl)
                cnt = int(_eval("sizeof(%s)/sizeof(void*)" % tbl))
            except gdb.error:
                continue            # その種別のオブジェクトが無い（表が未定義）
            onames = objnames.get(typekey, {})
            for k in range(cnt):
                p_cb = arr[k]
                if int(p_cb) == 0:
                    continue
                cb = p_cb.dereference()
                oid = k + 1
                onm = onames.get(oid)
                olabel = ("%s(%d)" % (onm, oid)) if onm else ("#%d" % oid)
                for field, flabel in fields:
                    try:
                        head = cb[field]
                    except gdb.error:
                        continue
                    members = _walk_taskqueue(head, tcb_ptr_t, tinib0,
                                              tinib_sz, names)
                    if members:
                        any_wq = True
                        tag = (" %s" % flabel) if flabel else ""
                        print("  %-3s %s%s: %s"
                              % (typekey, olabel, tag, ", ".join(members)))
        if not any_wq:
            print("  (no blocked tasks)")


# ===========================================================================
# Generic dump for synchronization/communication objects
#   (static INIB + dynamic CB + wait queue), one command per object kind.
# ===========================================================================
# Column formatter: f(gdb.Value, ctx) -> str
def _fmt_attr(v, ctx):
    return "TPRI" if (int(v) & TA_TPRI) else "TFIFO"


def _fmt_hex(v, ctx):
    return "0x%x" % (int(v) & 0xFFFFFFFF)


def _fmt_tcb(v, ctx):
    """TCB* -> task "name(id)" or "(none)" (e.g. mutex locking task)."""
    if int(v) == 0:
        return "(none)"
    t = _tcb_to_tid(int(v.dereference()['p_tinib']), ctx['tinib0'], ctx['tinib_sz'])
    return _tid_label(t, ctx['tasknames'])


# Per-kind spec.  cols: (header, src['inib'|'cb'], field, fmt|None, width)
#                 wq  : [(wait_queue field, label)]
_OBJ_SPECS = {
    "sem": {"typekey": "SEM", "label": "semaphore",
            "tmax": "_kernel_tmax_semid", "inib": "_kernel_seminib_table",
            "cb": "_kernel_p_semcb_table",
            "cols": [("attr", "inib", "sematr",  _fmt_attr, 6),
                     ("cur",  "cb",   "semcnt",  None, 5),
                     ("init", "inib", "isemcnt", None, 5),
                     ("max",  "inib", "maxsem",  None, 5)],
            "wq": [("wait_queue", "")]},
    "dtq": {"typekey": "DTQ", "label": "dataqueue",
            "tmax": "_kernel_tmax_dtqid", "inib": "_kernel_dtqinib_table",
            "cb": "_kernel_p_dtqcb_table",
            "cols": [("attr",  "inib", "dtqatr", _fmt_attr, 6),
                     ("cap",   "inib", "dtqcnt", None, 5),
                     ("count", "cb",   "count",  None, 6)],
            "wq": [("swait_queue", "snd"), ("rwait_queue", "rcv")]},
    "pdq": {"typekey": "PDQ", "label": "pri-dataqueue",
            "tmax": "_kernel_tmax_pdqid", "inib": "_kernel_pdqinib_table",
            "cb": "_kernel_p_pdqcb_table",
            "cols": [("attr",    "inib", "pdqatr",  _fmt_attr, 6),
                     ("cap",     "inib", "pdqcnt",  None, 5),
                     ("maxdpri", "inib", "maxdpri", None, 8),
                     ("count",   "cb",   "count",   None, 6)],
            "wq": [("swait_queue", "snd"), ("rwait_queue", "rcv")]},
    "flg": {"typekey": "FLG", "label": "eventflag",
            "tmax": "_kernel_tmax_flgid", "inib": "_kernel_flginib_table",
            "cb": "_kernel_p_flgcb_table",
            "cols": [("attr", "inib", "flgatr",  _fmt_attr, 6),
                     ("iptn", "inib", "iflgptn", _fmt_hex, 12),
                     ("ptn",  "cb",   "flgptn",  _fmt_hex, 12)],
            "wq": [("wait_queue", "")]},
    "mtx": {"typekey": "MTX", "label": "mutex",
            "tmax": "_kernel_tmax_mtxid", "inib": "_kernel_mtxinib_table",
            "cb": "_kernel_p_mtxcb_table",
            "cols": [("attr",    "inib", "mtxatr",   _fmt_attr, 6),
                     ("ceilpri", "inib", "ceilpri",  None, 8),
                     ("owner",   "cb",   "p_loctsk", _fmt_tcb, 20)],
            "wq": [("wait_queue", "")]},
    "mpf": {"typekey": "MPF", "label": "fixed-mempool",
            "tmax": "_kernel_tmax_mpfid", "inib": "_kernel_mpfinib_table",
            "cb": "_kernel_p_mpfcb_table",
            "cols": [("attr",   "inib", "mpfatr",  _fmt_attr, 6),
                     ("blksz",  "inib", "blksz",   None, 6),
                     ("blkcnt", "inib", "blkcnt",  None, 6),
                     ("free",   "cb",   "fblkcnt", None, 6)],
            "wq": [("wait_queue", "")]},
}


def _show_objects(spec, arg):
    cmd = spec["cmd"]
    try:
        tmax = int(_eval(spec["tmax"]))
    except gdb.error as e:
        print("%s: cannot read symbols (%s). Is the ELF loaded?" % (cmd, e))
        return
    if tmax < 1:
        print("%s: no %s objects" % (cmd, spec["typekey"]))
        return
    try:
        inibt     = _eval(spec["inib"])
        cbt       = _eval(spec["cb"])
        tinib0    = int(_eval("_kernel_tinib_table").address)
        tinib_sz  = gdb.lookup_type("TINIB").sizeof
        tcb_ptr_t = gdb.lookup_type("TCB").pointer()
    except gdb.error as e:
        print("%s: cannot read symbols (%s)." % (cmd, e))
        return

    objnames  = _load_obj_names()
    onames    = objnames.get(spec["typekey"], {})
    tasknames = objnames.get("TSK", {})
    name2id   = {v: k for k, v in onames.items()}
    ctx = {"tinib0": tinib0, "tinib_sz": tinib_sz, "tasknames": tasknames}

    arg = (arg or "").strip()
    if arg:
        if arg.isdigit():
            oid = int(arg)
        elif arg in name2id:
            oid = name2id[arg]
        else:
            print("%s: '%s' not found" % (cmd, arg))
            return
        if oid < 1 or oid > tmax:
            print("%s: ID %d out of range (1..%d)" % (cmd, oid, tmax))
            return
        ids = [oid]
    else:
        ids = list(range(1, tmax + 1))

    cols = spec["cols"]
    header = "%-22s " % "ID" + " ".join("%-*s" % (w, h) for (h, _, _, _, w) in cols)
    header += "  waiters"
    print("FMP3 %s info  (tnum = %d)" % (spec["label"], tmax))
    print(header)
    print("-" * (len(header) + 4))
    for oid in ids:
        inib = inibt[oid - 1]
        p_cb = cbt[oid - 1]
        cb = p_cb.dereference() if int(p_cb) != 0 else None
        cells = []
        for (h, src, field, fmt, w) in cols:
            if src == "cb" and cb is None:
                cells.append("%-*s" % (w, "?"))
                continue
            val = (inib if src == "inib" else cb)[field]
            s = fmt(val, ctx) if fmt else str(int(val))
            cells.append("%-*s" % (w, s))
        wparts = []
        if cb is not None:
            for (wf, wl) in spec["wq"]:
                try:
                    qhead = cb[wf]
                except gdb.error:
                    continue
                m = _walk_taskqueue(qhead, tcb_ptr_t, tinib0, tinib_sz, tasknames)
                if m:
                    wparts.append((("%s:" % wl) if wl else "") + ",".join(m))
        wstr = "; ".join(wparts) if wparts else "-"
        print("%-22s %s  %s" % (_tid_label(oid, onames), " ".join(cells), wstr))


def _make_objcmd(name):
    spec = dict(_OBJ_SPECS[name])
    spec["cmd"] = name

    class _Cmd(gdb.Command):
        def __init__(self):
            super(_Cmd, self).__init__(name, gdb.COMMAND_USER)

        def invoke(self, arg, from_tty):
            _show_objects(spec, arg)

    _Cmd.__doc__ = ("%s [id|name] : show FMP3 %s info (static + dynamic). "
                    "No arg = all objects; arg = number or CRE name."
                    % (name, spec["label"]))
    return _Cmd()


_OBJ_CMD_NAMES = ("sem", "dtq", "pdq", "flg", "mtx", "mpf")


# ===========================================================================
# Time-event handlers (cyclic / alarm): no wait queue; have a handler,
# an operational flag and a time-event block (next fire time).
# ===========================================================================
def _fmt_sym(v, ctx):
    return _sym_for(v) or ("0x%x" % int(v))


# scols: (header, INIB field, fmt|None, width)
_TEVT_SPECS = {
    "cyc": {"typekey": "CYC", "label": "cyclic",
            "tmax": "_kernel_tmax_cycid", "inib": "_kernel_cycinib_table",
            "cb": "_kernel_p_cyccb_table", "sta": "cycsta",
            "scols": [("handler",  "nfyhdr",   _fmt_sym, 16),
                      ("period",   "cyctim",   None, 7),
                      ("phase",    "cycphs",   None, 7),
                      ("prc",      "iprcid",   None, 4),
                      ("affinity", "affinity", _fmt_hex, 9)]},
    "alm": {"typekey": "ALM", "label": "alarm",
            "tmax": "_kernel_tmax_almid", "inib": "_kernel_alminib_table",
            "cb": "_kernel_p_almcb_table", "sta": "almsta",
            "scols": [("handler",  "nfyhdr",   _fmt_sym, 16),
                      ("prc",      "iprcid",   None, 4),
                      ("affinity", "affinity", _fmt_hex, 9)]},
}


def _show_timeevt(spec, arg):
    cmd = spec["cmd"]
    try:
        tmax = int(_eval(spec["tmax"]))
    except gdb.error as e:
        print("%s: cannot read symbols (%s). Is the ELF loaded?" % (cmd, e))
        return
    if tmax < 1:
        print("%s: no %s objects" % (cmd, spec["typekey"]))
        return
    try:
        inibt = _eval(spec["inib"])
        cbt   = _eval(spec["cb"])
    except gdb.error as e:
        print("%s: cannot read symbols (%s)." % (cmd, e))
        return

    onames  = _load_obj_names().get(spec["typekey"], {})
    name2id = {v: k for k, v in onames.items()}

    arg = (arg or "").strip()
    if arg:
        if arg.isdigit():
            oid = int(arg)
        elif arg in name2id:
            oid = name2id[arg]
        else:
            print("%s: '%s' not found" % (cmd, arg))
            return
        if oid < 1 or oid > tmax:
            print("%s: ID %d out of range (1..%d)" % (cmd, oid, tmax))
            return
        ids = [oid]
    else:
        ids = list(range(1, tmax + 1))

    scols = spec["scols"]
    header = "%-18s " % "ID" + " ".join("%-*s" % (w, h) for (h, _, _, w) in scols)
    header += " %-5s %s" % ("state", "next")
    print("FMP3 %s info  (tnum = %d)" % (spec["label"], tmax))
    print(header)
    print("-" * (len(header) + 4))
    for oid in ids:
        inib = inibt[oid - 1]
        p_cb = cbt[oid - 1]
        cb = p_cb.dereference() if int(p_cb) != 0 else None
        cells = []
        for (h, field, fmt, w) in scols:
            val = inib[field]
            s = fmt(val, None) if fmt else str(int(val))
            cells.append("%-*s" % (w, s))
        if cb is None:
            state, nxt = "?", "-"
        else:
            sta = int(cb[spec["sta"]])
            state = "STA" if sta else "STP"
            nxt = str(int(cb['tmevtb']['evttim'])) if sta else "-"
        print("%-18s %s %-5s %s" % (_tid_label(oid, onames), " ".join(cells),
                                    state, nxt))


def _make_tevtcmd(name):
    spec = dict(_TEVT_SPECS[name])
    spec["cmd"] = name

    class _Cmd(gdb.Command):
        def __init__(self):
            super(_Cmd, self).__init__(name, gdb.COMMAND_USER)

        def invoke(self, arg, from_tty):
            _show_timeevt(spec, arg)

    _Cmd.__doc__ = ("%s [id|name] : show FMP3 %s handler info (static + dynamic). "
                    "No arg = all; arg = number or CRE name."
                    % (name, spec["label"]))
    return _Cmd()


_TEVT_CMD_NAMES = ("cyc", "alm")


class IntrCmd(gdb.Command):
    """intr : show FMP3 configured interrupt request lines (INTINIB).

    INTID / priority / attribute / assigned processor / affinity / handler, and
    (when the target-dependent helper target_os_awareness is importable and a
    live target is attached) the GIC enable state (ena/dis) and pending state.
    handler is resolved from the per-processor handler table (_kernel_p_inh_table,
    via target_os_awareness); a configurator-generated ISR wrapper
    (_kernel_inthdr_<n>) is further resolved to the actual ISR function and its
    exinf by parsing kernel_cfg.c.
    """

    def __init__(self):
        super(IntrCmd, self).__init__("intr", gdb.COMMAND_USER)

    def _handler_label(self, intid, addr, isrmap):
        """ハンドラ番地を表示名へ。ISR ラッパなら実 ISR 名(exinf=値) [ISR] に解決。

        ディスパッチ要求 IPI が asm 直行のバイパス処理（USE_BYPASS_IPI_DISPATCH_HANDER）
        の場合は，表上 default のままなので注記を付ける（判定はターゲット依存部）。
        """
        nm = _sym_for(addr) if addr else ""
        if not nm:
            return "-"
        if nm in isrmap:
            return (", ".join("%s(exinf=%s)" % (f, x) for f, x in isrmap[nm])
                    + " [ISR]")
        try:
            if (hasattr(_TGT, "is_dispatch_ipi_bypassed")
                    and _TGT.is_dispatch_ipi_bypassed(intid, addr)):
                return "(dispatch IPI: bypassed, handled in asm)"
        except Exception:
            pass
        return nm

    def invoke(self, arg, from_tty):
        try:
            n   = int(_eval("_kernel_tnum_cfg_intno"))
            tab = _eval("_kernel_intinib_table")
        except gdb.error as e:
            print("intr: cannot read symbols (%s). Is the ELF loaded?" % e)
            return
        if n < 1:
            print("intr: no configured interrupts")
            return
        use_tgt = _TGT is not None and hasattr(_TGT, "int_enabled")
        use_inh = _TGT is not None and hasattr(_TGT, "inh_handler")
        isrmap  = _load_isr_map() if use_inh else {}
        print("FMP3 configured interrupts  (tnum_cfg_intno = %d)" % n)
        hdr = "%-7s %-6s %-10s %-4s %-9s" % ("INTID", "pri", "attr", "prc", "affinity")
        if use_tgt:
            hdr += " %-4s %-5s" % ("ena", "pend")
        if use_inh:
            hdr += " %s" % "handler"
        if not (use_tgt and use_inh):
            hdr += "  (ena/pend/handler: target_os_awareness not loaded)"
        print(hdr)
        print("-" * (72 if use_inh else 56))
        for i in range(n):
            e = tab[i]
            # INTNO は (prcid<<16)|INTID でエンコードされる（core_timer.h 参照）。
            intno = int(e['intno']) & 0xFFFFFFFF
            intid = intno & 0xFFFF
            iprcid = int(e['iprcid'])
            line = ("%-7d %-6d 0x%-8x %-4d 0x%-7x"
                    % (intid, int(e['intpri']), int(e['intatr']) & 0xFFFFFFFF,
                       iprcid, int(e['affinity']) & 0xFFFFFFFF))
            if use_tgt:
                try:
                    ena = "ena" if _TGT.int_enabled(intid) else "dis"
                except Exception:
                    ena = "?"
                try:
                    pend = "pend" if _TGT.int_pending(intid) else "-"
                except Exception:
                    pend = "?"
                line += " %-4s %-5s" % (ena, pend)
            if use_inh:
                # ハンドラテーブルはプロセッサ毎。INTNO 上位 16bit のプロセッサ番号を
                # 優先し，無ければ割付プロセッサ(iprcid)を使う。
                penc = (intno >> 16) & 0xFFFF
                prcidx = (penc - 1) if penc >= 1 else (iprcid - 1)
                try:
                    addr = _TGT.inh_handler(prcidx, intid)
                except Exception:
                    addr = None
                line += " %s" % self._handler_label(intid, addr, isrmap)
            print(line)


class SpnCmd(gdb.Command):
    """spn [id|name] : show FMP3 spinlocks (static attribute + current holder).

    holder is the processor whose PCB.p_locspn points to this spinlock, else 'free'.
    """

    def __init__(self):
        super(SpnCmd, self).__init__("spn", gdb.COMMAND_USER)

    def invoke(self, arg, from_tty):
        try:
            tmax   = int(_eval("_kernel_tmax_spnid"))
            tab    = _eval("_kernel_spninib_table")
            base   = int(tab.address)
            sz     = gdb.lookup_type("SPNINIB").sizeof
            pcbtab = _eval("_kernel_p_pcb_table")
            nprc   = int(_eval("sizeof(_kernel_p_pcb_table)/sizeof(void*)"))
        except gdb.error as e:
            print("spn: cannot read symbols (%s). Is the ELF loaded?" % e)
            return
        if tmax < 1:
            print("spn: no spinlocks")
            return

        names   = _load_obj_names().get("SPN", {})
        name2id = {v: k for k, v in names.items()}
        # processor holding each spinlock: PCB.p_locspn -> spninib_table[k]
        holders = {}
        for i in range(nprc):
            try:
                pl = int(pcbtab[i].dereference()['p_locspn'])
            except gdb.error:
                continue
            if pl != 0:
                holders[pl] = i + 1

        arg = (arg or "").strip()
        if arg:
            if arg.isdigit():
                sid = int(arg)
            elif arg in name2id:
                sid = name2id[arg]
            else:
                print("spn: '%s' not found" % arg)
                return
            if sid < 1 or sid > tmax:
                print("spn: ID %d out of range (1..%d)" % (sid, tmax))
                return
            ids = [sid]
        else:
            ids = list(range(1, tmax + 1))

        print("FMP3 spinlock info  (tnum_spn = %d)" % tmax)
        print("%-18s %-10s %s" % ("ID", "attr", "holder"))
        print("-" * 40)
        for sid in ids:
            e = tab[sid - 1]
            holder = holders.get(base + (sid - 1) * sz)
            print("%-18s 0x%-8x %s"
                  % (_tid_label(sid, names), int(e['spnatr']) & 0xFFFFFFFF,
                     ("PRC%d" % holder) if holder else "free"))


StaticTaskCmd()
DynTaskCmd()
IntrCmd()
SpnCmd()
_OBJ_CMDS  = [_make_objcmd(n) for n in _OBJ_CMD_NAMES]
_TEVT_CMDS = [_make_tevtcmd(n) for n in _TEVT_CMD_NAMES]
print("[os_awareness] commands registered: stask, atask, "
      + ", ".join(_OBJ_CMD_NAMES + _TEVT_CMD_NAMES + ("intr", "spn")))
