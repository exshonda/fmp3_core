# TOPPERS/FMP3 ESP32-P4 PIE(XespV) / HWLP(Xhwlp) コプロセッサコンテキスト管理 設計メモ

最終更新: 2026-07-01
配置: `arch/riscv_gcc/esp32p4/pie_hwlp_design.md`（チップ依存部に同梱する正準版）
対象: FMP3 ツリー（3.4系）/ `arch/riscv_gcc/esp32p4` + `target/m5stamp_esp32p4_gcc`
スコープ: ESP32-P4 HPコア(RV32IMAFC + カスタム拡張)上で、PIE(XespV) と HWLP(Xhwlp)
のアーキテクチャ状態をタスクコンテキストとして正しく退避/復帰する。FMP3(SMP/AMP, ジャイアント
ロック方式)での実装方針を確定する。

> **本書の位置づけ**: 本書は設計・調査の記録であり、利用手順書ではない。利用者向けの
> 入口は `chip_user.md`（`arch/riscv_gcc/esp32p4/`）および `target_user.md`
> （`target/m5stamp_esp32p4_gcc/`）を参照。本書は上記「最終更新: 2026-07-01」時点の
> 記録であり、現在の実装と異なる場合がある。

> **用語(本書の呼称と公式名)**: 本書では SIMD/ベクタコプロセッサを **PIE(XespV)**、ハードウェア
> ループコプロセッサを **HWLP(Xhwlp)** と表記する。括弧内の `XespV`/`Xhwlp` は本書の**内部呼称で
> 公式名ではない**。公式には、機能/章名は **PIE = Processor Instruction Extensions**(ESP32-P4 TRM の
> 章名)、命令ニーモニックは **`esp.*`**(ベクタ系 `esp.v*` / ループ系 `esp.lp*`)、GCC の `-march`
> 拡張トークンは **`xesppie`**。命令セット(xesppie / TRM "PIE"章)としては HWLP の `esp.lp*` も
> PIE 拡張の一部だが、**コンテキスト管理上は IDF/HW が HWLP と PIE を別コプロセッサ**として扱う
> (別 enable CSR: HWLP=`0x7F1` / PIE=`0x7F2`、別 EXT_ILL 理由ビット、別保存域・別 coproc index)。
> 本書の HWLP/PIE の区別は後者(別コプロセッサ)に従う。

本書は実コード(`arch/riscv_gcc/common/core_support.S`, `core_kernel_impl.h`)に照らした
設計レビュー(§8)を反映済み。レビューで判明した修正は §3.4 / §4.3 / §6 に折り込み、
検証で正しいと確認できた点・未解決点は §8 に集約する。

---

## 0. 結論サマリ

- **PIE(XespV)** = 本設計の本命は遅延オーナ方式(lazy: 初回PIE命令のトラップでオンデマンド
  退避/復帰、per-PEオーナ管理、ディスパッチャ非介入)。**ただし実装は案1=eager を先行採用**
  (kernel/ 不変・共通部不変で完結するため。HWLP と同じ枠組みで全切替に 216B を退避/復帰)。
  実装・実機検証(PASS)済み。lazy(案2)は性能最適化として将来検討(§9.2)。
  ※本節の結論は §9.2.2 により更新されている(fast-trap 方式で lazy も採用可能)．
- **HWLP(Xhwlp)** = eager無条件方式(方式A)。ディスパッチャの追加退避フックで毎切替
  保存復帰。状態ビット非依存でP4 rev≤1のシリコンバグに構造的に免疫。
- **移行(`mig_tsk`)** = `p_tcb->p_pcb != p_my_pcb → E_OBJ` 制約により、移行対象の生きた
  コプロ状態は必ず**呼び出しコア上**にある。よって **PIE単独・ローカルフラッシュ**で
  完結。IPI不要、per-PEオーナへのatomics不要。HWLPはeagerゆえ移行透過(フラッシュ対象外)。
- **FPU** は本設計のスコープ外(PIE/HWLP のみ扱う)。**訂正(実装時判明)**: esp32p4 の ABI は
  実際には `-mabi=ilp32f`(ハードフロート, Makefile.chip)で `__riscv_flen` は**有効**。よって
  `core_support.S` の FP 退避フレーム(fs0-fs11 等)は**コンパイルされ動作する**(本書旧版の
  「soft-float ilp32 / FP フレーム無し」は誤り)。FP コンテキストの完全性は別課題(HANDOFF 参照)で、
  HWLP/PIE の設計・実装はこれに非依存(追加フレームは hwlp_push が最外で sp 整合)。

---

## 1. ハードウェア仕様(一次情報)

### 1.1 PIE (XespV)

- enable状態CSR: **`0x7F2`** (`CSR_PIE_STATE_REG`)。最下位2bitが `mstatus.FS` と同型:
  `00=OFF / 01=Initial / 10=Clean / 11=Dirty`。リセット時OFF。OFF状態でPIE命令を実行
  すると **illegal instruction 例外**。(ESP32-P4 TRM §1.7.2.3, p.108)
- 退避対象の完全状態 = `RvPIESaveArea` 相当 **216B (16B境界)**:

  | フィールド | サイズ | 退避命令 |
  |---|---|---|
  | q0–q7 | 8 × 16B = 128B | `esp.vst.128.ip` |
  | QACC_L(L/H), QACC_H(L/H) | 4 × 16B = 64B | `esp.st.qacc.{l,h}.{l,h}.128.ip` |
  | UA_STATE (非整列アクセス状態, 128bit) | 16B | `esp.st.ua.state.ip` |
  | XACC (40bit アキュムレータ) | 4B | `esp.st.u.xacc.ip` |
  | MISC (XACC上位byte / FFT_BIT_WIDTH[4] / SAR_BYTES[4] / SAR[6] をパック) | 4B | `esp.movx.r.{sar,sar.bytes,fft.bit.width}` |

  注: Espressifの公開PIEドキュメントにはQACC幅等の誤記が報告されている。最終確定は
  TRM + 実シリコンで裏取りすること。退避命令列はESP-IDF(Apache-2.0)の
  `pie_save_regs`/`pie_restore_regs` を流用可。

### 1.2 HWLP (Xhwlp)

- enable状態CSR: **`0x7F1`** (`CSR_HWLP_STATE_REG`)。PIEと同型の OFF/INITIAL/CLEAN/DIRTY。
- ループレジスタ = **6本のCSR (24B)**:

  | CSR | 番号 | 内容 |
  |---|---|---|
  | LOOP0_START_ADDR | `0x7C6` | ループ0 開始アドレス |
  | LOOP0_END_ADDR | `0x7C7` | ループ0 終了アドレス |
  | LOOP0_COUNT | `0x7C8` | ループ0 残回数 |
  | LOOP1_START_ADDR | `0x7C9` | ループ1 開始アドレス |
  | LOOP1_END_ADDR | `0x7CA` | ループ1 終了アドレス |
  | LOOP1_COUNT | `0x7CB` | ループ1 残回数 |

- HWLP命令の判別: **opcode `0b0101011`** (custom-1)。
- 意味論: HWLP状態が「生きている」のは **count≠0(ループ実行中)** のときだけ。
- **P4 rev≤1 シリコンバグ2件**:
  1. 最終ループ命令実行後に HWLP状態が正しく更新されない → state/dirtyビット依存の
     判定が不正確になる。
  2. illegal-instruction理由bitがHWLPを誤って立てる → トラップ判定はopcodeで行う必要。
  - **方式A(eager無条件)はどちらの状態ビットにも依存しない**ため両バグの影響を受けない。

### 1.3 コプロセッサ割当(ESP-IDF参照)

`FPU=0 / HWLP=1 / PIE=2`。本設計では FPU(0) はスコープ外(§0 訂正: 実 ABI は ilp32f で
FP 自体は使用されるが FP コンテキスト完全性は別課題)、HWLP(1)=eager、PIE(2)=lazy を扱う。

---

## 2. `mig_tsk` の制約と移行設計

### 2.1 `mig_tsk` の2パターン (kernel/task_manage.c)

ガード:
```c
if (p_tcb->p_pcb != p_my_pcb) {
    ercd = E_OBJ;   /* [NGKI1157] 対象が呼び出しPE以外に割付けられている */
}
```
→ 移行できるのは **呼び出しPE(コアM)に現在割り付いているタスクのみ**。FMP3は自動コア
割当をしないため、対象タスクが最後に実行されたのもコアM。

- **パターンA: 自分を移行** (`p_tcb == p_selftsk`)
  → `dispatch_and_migrate(p_my_pcb, p_selftsk)`。selfは実行中=Mの現コプロオーナの可能性。
- **パターンB: 同PEの他タスクを移行** (`p_tcb != p_selftsk`、ガードにより `p_pcb==M` 保証)
  → 対象は非実行。GPコンテキストはスタック上に退避済。生きたコプロ状態のみMの物理レジスタに残存しうる。
  → 休止/強制待ち/タイムアウト待ち状態は `p_pcb` 再代入のみ(コンテキストに触れない)。
    「まだスタートしていないコンテキスト」はオーナになり得ず、追加処理不要。

### 2.2 移行フラッシュは「ローカル・PIE単独」

- 遅延管理ゆえ、タスクTの生きたPIEレジスタは「T最終実行コアの物理レジスタファイル」に
  ある。移行対象は必ずコアMに割り付いている → **Tの状態は必ずコアM上 = ローカルフラッシュ
  で完結。IPI不要**。
- 帰納的不変条件: 移行のたびにローカルフラッシュすれば「Mに割り付いたタスクのPIE状態は
  常にM上か退避済み」が保たれ、リモートコアにTの残留状態は生じない。
- **per-PEオーナスロットはそのコア自身しか書かない**(M上のtrap/dispatcher/mig_tskのみ)
  → atomics不要、CPUロック下の素のポインタ代入で十分。
- HWLPはeager(2.3)で毎切替スタック退避されるため、移行はコンテキストごと運ばれる=**移行透過**
  (フラッシュ対象外)。

---

## 3. HWLP 実装(方式A: eager無条件)

### 3.1 方針

- `0x7F1` を起動時にCLEANで常時有効化(OFFにしない=ループCSRは常時アクセス可)。
- コンテキスト切替の追加退避フックで6本のループCSRをスタックに毎回保存復帰。
- **自発パスでも無条件に保存**: 通常はcount=0だが、手書きasmでループ本体に
  サービスコールを挟む病的ケースも無条件退避なら正しく保存復帰される。
- タスクは常に「中断したパスと同じパス」で再開されるためスタック格納でフレーム整合が取れる
  (自発↔`dispatch_r`、非自発↔`ret_int_r`)。
- ISRでのHWLP使用は禁止規約 → `save_additional_regs_int`(毎割込み入口)は触らない。
  退避は `*_int_disp`(割込み起因の切替時)で行う。

### 3.2 起動時初期化 (target の hardware_init_hook, 全PE) ※実装で確定

共通 `start.S` は変更しない。`start.S` が master/slave 分岐**前**に呼ぶ
`hardware_init_hook`(target_kernel_impl.c, **全PEで実行**)で HWLP を有効化する。
これでターゲット依存部のみで完結する(共通部不変)。

```c
/* target_kernel_impl.c: hardware_init_hook() 内 (TOPPERS_SUPPORT_HWLP ガード) */
Asm("csrwi 0x7f1, 2  \n\t"   /* 0x7F1=CSR_HWLP_STATE_REG を CLEAN(2) で常時有効化 */
    "csrwi 0x7c6, 0  \n\t"   /* LOOP0_START_ADDR  ┐ boot 時のループ CSR を */
    "csrwi 0x7c7, 0  \n\t"   /* LOOP0_END_ADDR    │ 0 初期化(残留 count による */
    "csrwi 0x7c8, 0  \n\t"   /* LOOP0_COUNT       │ 偽ループを防止) */
    "csrwi 0x7c9, 0  \n\t"   /* LOOP1_START_ADDR  │ */
    "csrwi 0x7ca, 0  \n\t"   /* LOOP1_END_ADDR    │ */
    "csrwi 0x7cb, 0  \n\t"   /* LOOP1_COUNT       ┘ */
    ::: "memory");
```
PIE(0x7F2)は OFF のまま(lazy)。CSR 番号は ESP-IDF `riscv/csr_hwlp.h` と一致を確認済み
(0x7F1, 0x7C6-0x7CB, CLEAN=2)。

### 3.3 退避/復帰マクロ + フック (arch/riscv_gcc/esp32p4/chip_asm.inc:82-112 を置換)

```asm
/* ループCSR 0x7C6-0x7CB (LOOP0/1 START/END/COUNT) = 6word.
 * t0 はこの時点で dead(自発:s退避前 / 非自発:既にフレーム退避済).
 * a0(p_runtsk) 等は触らない. */
.macro hwlp_push
    addi sp, sp, -32             /* 24B使用, 16B境界維持で32確保 */
    csrr t0, 0x7C6; sw t0,  0(sp)
    csrr t0, 0x7C7; sw t0,  4(sp)
    csrr t0, 0x7C8; sw t0,  8(sp)
    csrr t0, 0x7C9; sw t0, 12(sp)
    csrr t0, 0x7CA; sw t0, 16(sp)
    csrr t0, 0x7CB; sw t0, 20(sp)
    /* 退避後にループ CSR をゼロ化(§3.4). 次に走るのが restore を通らない新規タスク
     * (start_r 経由)でも count=0 を見る → 残留 count の偽ループを防止. 復帰タスクは
     * hwlp_pop で自分の値を上書きするため無害. */
    csrwi 0x7C6, 0; csrwi 0x7C7, 0; csrwi 0x7C8, 0
    csrwi 0x7C9, 0; csrwi 0x7CA, 0; csrwi 0x7CB, 0
.endm
.macro hwlp_pop
    lw t0,  0(sp); csrw 0x7C6, t0
    lw t0,  4(sp); csrw 0x7C7, t0
    lw t0,  8(sp); csrw 0x7C8, t0
    lw t0, 12(sp); csrw 0x7C9, t0
    lw t0, 16(sp); csrw 0x7CA, t0
    lw t0, 20(sp); csrw 0x7CB, t0
    addi sp, sp, 32
.endm

.macro save_additional_regs_disp
    hwlp_push
.endm
.macro restore_additional_regs_disp
    hwlp_pop
.endm
.macro save_additional_regs_int
.endm
.macro restore_additional_regs_int
.endm
.macro save_additional_regs_int_disp
    hwlp_push
.endm
.macro restore_additional_regs_int_disp
    hwlp_pop
.endm
```

### 3.4 タスク生成時(初回ディスパッチ) ※実装で確定(共通部 start_r を変更しない方式)

**問題**: `activate_context`(core_kernel_impl.h:285-289) は新規タスクの `tskctxb.sp` を
スタック先頭、`tskctxb.pc` を `start_r` に設定するだけで**追加レジスタ用フレームを作らない**。
かつ `start_r`(core_support.S:379) は `restore_additional_regs_disp`(=`hwlp_pop`) を**通らない**。
よって新規タスクは直前にそのコアで走ったタスクの HWLP CSR(`LOOP_COUNT`≠0 等)を引き継ぎ、
PC が残留 `LOOP_END` に一致すると偽のハードウェアループが暴発し得る。

**確定方針(実装済)**: 共通 `start_r` には手を入れず、**`hwlp_push`(save 側, §3.3)が退避後に
ループ CSR をゼロ化**する。これにより「ある時点でそのコアで最後に switch-out したタスク」の
直後にどのタスクが走っても(restore を通らない新規タスク=`start_r` 経由でも)`count=0` を
見るため偽ループが起きない。復帰タスクは `hwlp_pop` で自分の値を上書きするので無害。
boot 直後(まだ switch-out が無い)については §3.2 の `hardware_init_hook` が初期ゼロ化を行う。

→ **共通部(`start.S`/`start_r`)・`kernel/` を一切変更せず、チップ/ターゲット依存部のみで
設計メモ #1 を満たす**(レビュー初稿の「start_r でゼロ化」案を、実装ではこの方式に置換)。

PIE は lazy ゆえ初期化不要(`pie_enabled=false` で生成 → 初回 PIE 命令のトラップで
`pie_zero_regs()` が走る、§4.3)。

---

## 4. PIE 実装(lazy owner) ※本章は将来案。現状の実装は §9.2 の eager(案1)
※本節の結論は §9.2.2 により更新されている(fast-trap 方式で lazy も採用可能)．

ディスパッチャには一切入れない。例外トラップ + 移行フラッシュで完結。

### 4.1 per-PE オーナ (chip_kernel.h / PCB)

```c
TCB  *p_pie_owner;   /* 初期 NULL. コア固有書込みのみ = atomics不要 */
```

### 4.2 PIEコンテキスト領域 (TCB / TA_XESPV 属性タスクのみ)

216B(16B境界) + 使用フラグ `pie_enabled`。レイアウトは §1.1 の `RvPIESaveArea` 同等。
`pie_save_regs`/`pie_restore_regs` はESP-IDF(Apache-2.0)の命令列を流用し、FMP3の
ディスパッチャ規約に合わせて整える。

### 4.3 トラップフック (core_support.S / core_exc_entry, exccode=2)

```
illegal instruction かつ faulting命令が PIE opcode(※§6-1) のとき:
  非タスクコンテキスト(exncnt>0)なら abort   /* ISRでのPIE禁止. exncnt は core_exc_entry で取得済 */
  cur = p_runtsk
  own = p_pie_owner[core]
  /* ★順序が重要(レビュー#2): pie_save_regs/restore は PIE 命令なので 0x7F2==OFF では
   *   それ自体が再トラップする. まず PIE を enable し, 以後の退避/復帰命令を合法化する.
   *   旧オーナの値は物理レジスタに残存しているので enable 後に退避すればよい. */
  csrwi 0x7F2, 1                      /* PIE enable (Initial). ※以後 PIE 命令が合法 */
  if (own != NULL && own != cur) {
      pie_save_regs(own->pie_area)    /* 物理regは旧オーナの値. enable 済みなので合法 */
      own->pie_enabled = true
  }
  if (cur->pie_enabled) pie_restore_regs(cur->pie_area)
  else                  pie_zero_regs()   /* 初回: クリーン初期化 */
  p_pie_owner[core] = cur
  mret                                /* mepc は進めない. faulting命令を再実行 */
```

### 4.4 移行フラッシュ (kernel/task_manage.c, mig_tsk内, CPUロック下)

```c
/* M上で対象タスクのPIEをローカルフラッシュ */
static inline void
pie_flush_local(PCB *p_pcb, TCB *p_tcb)
{
    if (p_pcb->p_pie_owner == p_tcb) {
        T_pie_csr save = pie_csr_enable_save();  /* selfの0x7F2退避 → enable */
        pie_save_regs(&p_tcb->pie_area);         /* 物理regは対象の値 */
        p_tcb->pie_enabled = true;
        p_pcb->p_pie_owner = NULL;
        pie_csr_restore(save);                   /* selfの状態(通常OFF)へ戻す */
    }
}
```

挿入点:
- **パターンA(自分)**: `dispatch_and_migrate(...)` の直前で
  `pie_flush_local(p_my_pcb, p_selftsk)`。
- **パターンB(同PE他タスク)**: `make_non_runnable(p_my_pcb, p_tcb)` の直後で
  `pie_flush_local(p_my_pcb, p_tcb)`。
- 休止/待ち状態は `p_pie_owner` になり得ず自動的にno-op。

宛先コアNでは、対象が初めてPIE命令を実行した時点でトラップ(§4.3)→フラッシュ済み自領域
から復元、という通常経路に自然に乗る。

---

## 5. コスト

- **HWLP(A)**: 全切替で固定 12 CSRアクセス(6 csrr + 6 csrw, 24B)。WCET一定で予測容易。
  状態ビット非依存でバグ免疫。
- **PIE(lazy)**: 定常dispatchは0(ディスパッチャ非介入)。初回使用時に1回の216B退避/復帰。
  移行は **対象が実際にPIEオーナの時だけ** ローカル216Bストア。IPIゼロ。`mig_tsk` 呼出元
  にコストを局所化でき、FMP3の明示制御思想に合致。

---

## 6. 着手前に確定すべき点

1. **PIE命令のopcode**(トラップ判定用)。HWLP=`0b0101011`(custom-1)は確定。PIEの `esp.*` は
   custom-0/2/3 のいずれか → ESP-IDF同梱GCC/LLVMの逆アセンブル or TRMで確定。
   **暫定策「illegal かつ 0x7F2==OFF なら PIE」には穴があるので注意(レビュー#3)**: PIE 未使用
   タスクが本物の不正命令を踏むと(0x7F2==OFF のまま)PIE と誤認 → enable+zero+mret で同 PC を
   再実行 → また illegal → **無限トラップ**。opcode 判定を入れるか、暫定なら「直前に enable した
   PC で再トラップしたら abort」のガードを必須とする。
2. ~~`restore_additional_regs_int_disp`(726) の `t0` クロバ安全性~~ **【確認済・安全】**:
   726 直後の `core_int_entry_5` が `ret_int_prepare_unlock_cpu_asm t0`(741) と `lx t0,0(sp)`(743)
   で `t0` を再利用するため hwlp_pop の `t0` 破壊は無害(§8 参照)。
3. ~~esp32p4 は ilp32 で FPU 対象外~~ **【訂正】**: 実 ABI は `-mabi=ilp32f`(§0 訂正参照)。
   FP は別課題で HWLP/PIE は非依存。
4. ~~HWLP `*_int_disp` のネスト割込みペアリング~~ **【確認済・安全】**: `save/restore_additional_regs_int`
   を空に保つ限り plain int entry では push/pop せず、唯一の push/pop は int_disp の1ペアのみ。
   入れ子でも sp 不均衡は生じない(§8 参照)。
5. ~~start.S を per-PE で~~ **【実装で解決】**: HWLP 有効化は共通 `start.S` ではなく target の
   `hardware_init_hook`(master/slave 分岐前=全PE)に置いた(§3.2)。共通 `start.S`/`start_r` 不変。
6. **ISR/非タスクコンテキストでの HWLP 命令使用禁止**は未強制の規約。`save_additional_regs_int` を
   空にする前提が崩れると preempt されたタスクのループ状態が壊れる。コメント明記必須。
7. **PIE fast-path の asm 統合**: §4.3 は擬似コード。実際は `core_exc_entry`(core_support.S:837〜)
   の退避フレームと整合させ、ハンドラが使う GPR を退避し、`mret` で **mepc を進めない**こと。
   bare mret か通常 exc 復帰経由かを明確にする。
8. **PIE 領域(216B)の 16B 整列**: TCB 埋め込みに `__attribute__((aligned(16)))`、TCB 自体の整列も保証。

---

## 7. 推奨ビルド/テスト順

1. **HWLP(A)**: §3 のマクロ + start.S初期化のみ。hwlp命令を使うタスクで
   自発切替・割込みプリエンプト両方の保存復帰を確認(状態ビット非依存=最低リスク)。
2. **PIE**:
   1. `pie_save_regs`/`pie_restore_regs` 単体動作。
   2. トラップ遅延切替(単一タスク, 初回使用→enable→再実行)。
   3. 複数タスクのオーナ切替(別タスクが触った時の旧オーナ退避)。
   4. `mig_tsk` 両パターンのローカルフラッシュ。
3. **結合**: hwlpループ実行中タスクをPIE使用タスクが割込みプリエンプト→`mig_tsk`、の
   複合シナリオで HWLP(eager) と PIE(lazy+flush) が干渉しないことを確認。

---

## 8. 設計レビュー結果(2026-06-29, 実コード照合)

`arch/riscv_gcc/common/core_support.S` / `core_kernel_impl.h` と突き合わせた検証結果。

### 8.1 検証で正しいと確認できた点

- **sp 整合(disp 経路)**: `dispatch`(:111) が `save_additional_regs_disp` を最初に実行 →
  xreg/freg push → `sx sp,TCB_sp`(:146)。復帰 `dispatch_r`(:152) は xreg/freg pop 後に
  `restore_additional_regs_disp`(:191)。`hwlp_push`(sp-=32)↔`hwlp_pop`(sp+=32) は各タスク
  自身のスタックで完全対称。`dispatch_and_migrate`(:323/:357, resume=`dispatch_r`)も同様。
- **sp 整合(int_disp 経路)**: `save_additional_regs_int_disp`(:636) ↔
  `restore_additional_regs_int_disp`(:726, `ret_int_r` 経由)で対称。合流点
  `core_int_entry_4`(:728) は非ディスパッチ経路(:624 分岐)と sp 中立で一致。
- **`t0` クロバ安全(§6-2)**: §6-2 参照。確定で安全。
- **ネスト割込みペアリング(§6-4)**: §6-4 参照。int hook を空に保つ限り安全。
- **mig_tsk ローカルフラッシュ**: `E_OBJ` ガード(`p_tcb->p_pcb != p_my_pcb`)で対象は必ず
  呼び出しコア上 → 物理 PIE レジスタに対象値、は正しい。per-PE オーナをコア自身しか
  書かない設計も妥当で atomics 不要。

### 8.2 レビューで判明し修正した欠陥

| # | 箇所 | 内容 | 対処 |
|---|---|---|---|
| 1 | §3.4 | `start_r` は `restore_additional_regs_disp` を通らず、`activate_context` も追加フレームを作らない。初期フレーム0クリア案は消費されず、新規タスクが残留 HWLP CSR を継承し偽ループ暴発の恐れ | **実装では** `hwlp_push`(save側)が退避後にループ CSR をゼロ化＋`hardware_init_hook` で boot 時初期ゼロ化(§3.2/§3.4)。共通 `start_r` を変更せず解決 |
| 2 | §4.3 | `pie_save_regs` を PIE enable 前に置くと、PIE 命令自身が `0x7F2`==OFF で再トラップ | enable を退避/復帰より前へ(§4.3 改) |
| 3 | §6-1 | 暫定 opcode 判定だと PIE 未使用タスクの本物の不正命令で無限トラップ | opcode 判定 or 再トラップ guard 必須(§6-1) |

### 8.3 未解決・着手前確認(§6 に集約)

§6 の 5〜8(per-PE の `0x7F1` 初期化、ISR-HWLP 禁止規約、PIE fast-path の asm 統合、
216B の 16B 整列)。加えて移行時の TCB `pie_area` 可視性は giant lock のバリアに依存
(FMP3 では成立)する点を実装時に明記する。

---

## 9. 実装状況と実機検証(2026-06-29)

### 9.1 HWLP(方式A) — 実装済・実機検証 PASS

変更はチップ/ターゲット依存部のみ(共通 `start.S`/`start_r`・`kernel/` 不変):

- `arch/riscv_gcc/esp32p4/Makefile.chip` — 単一スイッチ `-DTOPPERS_SUPPORT_HWLP`。
- `arch/riscv_gcc/esp32p4/chip_asm.inc` — `hwlp_push`/`hwlp_pop` と 4 フック配線
  (`save/restore_additional_regs_disp`・`_int_disp`)。`_int`(非disp)は空(§3.1)。
  `hwlp_push` は退避後にループ CSR をゼロ化(§3.4)。負対照用に `HWLP_NO_RESTORE` ガードあり。
- `target/m5stamp_esp32p4_gcc/target_kernel_impl.c` — `hardware_init_hook` で全PE有効化＋初期ゼロ化(§3.2)。

**検証アプリ**: `target/m5stamp_esp32p4_gcc/tools/fmp_hwlp_app/`。同一コア(PRC1)に LOW/HIGH/REPORT。
LOW(連続)と HIGH(別 count・周期起床)が共に hwloop を回し、HIGH が LOW を hwloop 実行中に
プリエンプトする。結果を起動時計測の golden と毎回照合。

| ビルド | 結果 |
|---|---|
| 通常(restore 有効) | LOW 64,193 回 / HIGH 6,057 回で **errA=errB=0(PASS)**、クラッシュ無し |
| 負対照(`-DHWLP_NO_RESTORE`, CSR 復帰のみ抜く) | **errA が ~540/s 累積(FAIL)**(プリエンプトされた hwloop の約12%が破損) |

→ ループ CSR の save/restore が cross-task プリエンプト跨ぎで正しく機能することを実証。
途中で見つけた実装上の落とし穴: **`hwlp_pop`(restore 側)と `_int_disp` ペアの配線漏れ**で
dispatch 毎に sp が -32 ずれ Load access fault → 4 フックを push↔pop 対称に配線して解消。

### 9.2 PIE — 案1=eager を実装済・実機検証 PASS / 案2=lazy 設計確定(save_context フック方式・実装着手)

§4 の lazy は `kernel/task_manage.c`(mig_tsk 移行フラッシュ)に触れるため、kernel/ 不変の
方針では別途調整が要る。そこで**まず案1=eager を実装**した。HWLP と同枠組みで、全切替で
PIE 状態(216B)を chip_asm.inc の pie_push/pie_pop で退避/復帰する。共通部・kernel/ 不変。

変更(チップ/ターゲット依存部のみ):
- `Makefile.chip`: `-march=rv32imafc_zicsr_zifencei_xesppie`(esp.* 許可、IDF と同指定)
  + `-DTOPPERS_SUPPORT_PIE`。
- `chip_asm.inc`: `pie_push`/`pie_pop`(IDF portasm.S の pie_save_regs/pie_restore_regs 準拠)を
  4 フックに配線(HWLP と LIFO 整合)。負対照ガード `PIE_NO_RESTORE`(既定 off)あり。
- `target_kernel_impl.c`: `hardware_init_hook`(全PE)で 0x7F2 を enable。

実装知見:
- **`esp.*` 命令は `xesppie` 拡張が必要**。`.option arch` では有効化できず **`-march`** で指定。
- **`esp.*` の base/scratch レジスタは x8-x15 のみ**(t0/t1 不可)。全フック点で安全な
  **a2/a3/a4** を採用(a1=p_runtsk は int_disp で live、s0/s1 はフレーム退避前後で live を回避。
  a2-a5 は割込み入口でフレーム退避済 / 自発時 caller-saved)。
- **q3 は退避列から除外**(IDF/HW 仕様)。misc は SAR/SAR_BYTES/FFT を 1 ワードにパック(IDF 踏襲)。

**検証アプリ**: `tools/fmp_pie_app/`。同一コア(PRC1)で 2 つの PIE ベクタ累算タスクを相互
プリエンプトさせ golden 照合。

| ビルド | 結果 |
|---|---|
| 通常 | golden 一致(seed×iters)、LOW 555/HIGH 1854 回で **errA=errB=0(PASS)** |
| 負対照(`-DPIE_NO_RESTORE`) | LOW が毎回中断され **errA=100%(FAIL)** |

#### lazy(案2)の確定設計(save_context フック方式・完全 lazy, 2026-06-29 確定)

eager の「PIE 不使用タスクも毎切替 216B」コストを削減する lazy 化の**確定設計**。
`#ifdef TOPPERS_PIE_LAZY` で eager と切替(既定=eager)。kernel/ には mig_tsk の
**非自タスク移行分岐に依存部関数 `save_context(p_tcb)` の呼出を1点追加**する(共通部の
最小フック。これにより案2 当初の「kernel/ 不変・flush-on-switch-out」案は不要となり、
**switch-out は遅延のまま(オーナ据置)・移行点でのみ明示 flush** する素直な設計にできた)。

##### オーナシップモデル(不変条件)
- **`TCB *pie_owner[PRC_NUM]`**(PE毎): その PE の物理 PIE レジスタに生値が載るタスク。NULL=無し。
- **保存域 = タスクスタック底 224B(`tinib->stk`)**。**PE 非依存**(タスクと共に移動)→ 移行後の
  PE でトラップ復元できる肝。生成オフセット TCB_p_tinib=8, TINIB_stk=20。16B 整列(STK_T)。
  PIE 使用タスクは sp が底 224B に侵入しない規約。
- **CSR 0x7F2(PIE enable)**: 走行中タスク==`pie_owner[PE]` のときだけ enable。否なら OFF →
  `esp.*` が illegal トラップ。
- **不変条件**: *`pie_owner[PE_old]` であり得るタスクを別 PE へ再割付する前に、その物理 PIE
  状態を保存域へ flush し owner をクリアする*。全移行経路で `p_tcb->p_pcb==p_my_pcb` が保証
  されるため、**flush 対象の物理レジスタは常に自 PE**(クロスコア不要)。

##### 機構1: switch-in での 0x7F2 管理(復帰マクロ, chip)
`restore_additional_regs_disp` の PIE 部を「incoming==`pie_owner[PE]`→`csrwi 0x7F2,enable` /
否→OFF」に置換(eager の `pie_pop` を差替。**レジスタ退避復元なし**)。incoming TCB は
`dispatch_r` で a0、`ret_int_r` で a1(既存どおり live)。PE 添字=mhartid。csrwi+比較分岐のみ
ゆえ scratch は t0/t1/t2 で可(esp.* の x8-15 制約は無関係)。switch-out 側(`save_*` の PIE 部)は
**空**(オーナ据置=遅延の本体)。

##### 機構2: トラップでの退避/復元(DEF_EXC ハンドラ, chip C)
非オーナの初回 PIE 命令が illegal-instruction → ハンドラ:
1. **PIE 判別 = EXT_ILL CSR(0x7F0)**: `csrrw a0,0x7F0,zero` で読みクリア、bit2(=EXT_ILL_RSN_PIE=4)。
   **非PIE→`default_exc_handler(p_excinf, EXCNO_IINST)` へ委譲**(真の不正命令=致命扱い)。
   opcode デコード/再トラップ guard 不要(CSR が確定的)。
   理由ビット= FPU=1/HWLP=2/PIE=4(IDF vectors.S)。HWLP は本移植 eager ゆえ本経路を使わない。
2. **0x7F2 enable**(以降の `esp.*` を合法化。**旧オーナ flush の前**に必須=enable 順序。トラップ時は
   非オーナゆえ 0x7F2 は OFF なので、flush 用 esp.* も enable してからでないと再トラップする)。
3. `old=pie_owner[PE]`; `old!=NULL && old!=cur` → `pie_save_regs(old->p_tinib->stk)`(旧オーナ flush)。
4. `pie_restore_regs(cur->p_tinib->stk)`(初回は未初期化だが正常コードは使用前に PIE を初期化)。
5. `pie_owner[PE]=cur`(cur がオーナ → 0x7F2 は enable のまま復帰で正しい)。
6. **PC を進めず復帰**(faulting 命令を再実行)=`p_excinf->pc` 据置の既定挙動。
- 退避/復元は IDF portasm.S 準拠の **global asm `pie_save_regs/pie_restore_regs`(a0=frame, esp.* は
  x8-15)** を C から呼ぶ。
- 登録: **アプリ cfg の `DEF_EXC(EXCNO_IINST, {TA_NULL, pie_exc_handler})`**(PEエンコード:
  PRC1=0x10000|2, PRC2=0x20000|2 で各 PE 登録)。共通部の例外振り分け(`p_exc_tbl[exccode]`)を
  そのまま使うため**共通部 asm 変更不要**。signature `void h(void *p_excinf)`。現タスク=
  `get_my_pcb()->p_runtsk`、PE=mhartid。EXCNO_IINST=2。
- **両立制約**: lazy ビルドは illegal-instruction を PIE トラップに転用するため、**アプリ独自の
  `EXCNO_IINST` ハンドラや cpuexc テスト(`test_cpuexc1`=EXCNO_IINST 使用)とは同一ビルドで両立しない**。
  cpuexc 適合性は既定=eager ビルドで担保する。

##### 機構3: マイグレーション flush(設計の核心=正当性)
mig_tsk の4分岐と対応(§2.1 の経路解析より):

| # | 分岐 | 状態 | オーナ可 | flush 経路 |
|---|---|---|---|---|
| A | `p_tcb==p_selftsk`(自タスク) | RUN | ○ | `save_context(p_tcb)`(kernel/ C, `dispatch_and_migrate` 直前) |
| B | `else`(同PE 他 RUNNABLE) | RDY | ○(プリエンプト後もオーナ据置) | `save_context(p_tcb)`(kernel/ C) |
| C | `!TSTAT_WAITING\|\|callback==NULL`(休止/強制待ち/無timeout待ち) | 非RUN | ○(ブロック時 flush しない=lazy) | `save_context(p_tcb)`(kernel/ C) |
| D | timeout 付き待ち | 非RUN | ○ | `save_context(p_tcb)`(kernel/ C) |

- **全4分岐を `save_context(p_tcb)` に統一 → 共通部(arch/riscv_gcc/common)不変**:
  - B/C/D は純 C(kernel/)でフック無し → 各分岐に `save_context(p_tcb)` を追加。完全 lazy では
    待ち/休止タスクも owner であり得る(ブロック時 flush しないため)ので C/D も必須。
  - **A(自タスク)も C に統一**: self は mig_tsk 実行者=走行中で物理 PIE が live ゆえ、
    `dispatch_and_migrate(p_my_pcb, p_selftsk)` 呼出の**直前に `save_context(p_selftsk)`** を呼べば
    flush できる。よって当初案の「`dispatch_and_migrate` への asm flush(`pie_migrate_flush`)」や
    共通部 asm の変更は**不要**。save マクロ(`save_additional_regs_disp`)の PIE 部は lazy では空のまま。
  - `save_context` は冪等(`p_tcb!=owner` なら no-op)ゆえ全分岐で安全・安価。

##### save_context() の定義(両モードで必須)
kernel/ の呼出は無条件ゆえ eager でも定義必須:
- **eager**: 空(no-op)。eager は他タスクの PIE 状態が既に自スタックへ push 済で移動するため不要。
- **lazy**: `pe=get_my_prcidx()`(mig_tsk 実行中の自 PE。**注意**: branch B では呼出時点で既に
  `p_tcb->p_pcb=p_new_pcb`(移行先)に更新済ゆえ、添字は p_tcb->p_pcb ではなく自 PE を使う)。
  ```c
  if (pie_owner[pe] == p_tcb) {       /* p_tcb がこの PE の現オーナのときだけ flush */
      Asm("csrwi 0x7f2, 1");          /* esp.* 合法化(caller は非オーナで OFF のため) */
      pie_save_regs(p_tcb->p_tinib->stk);
      pie_owner[pe] = NULL;
      Asm("csrwi 0x7f2, 0");          /* owner=NULL ゆえ走行中 caller は非オーナ → OFF */
  }
  ```
  自タスク移行(A)では caller==p_tcb==owner で 0x7F2 は元々 enable だが、flush 後 owner=NULL に
  するため OFF で揃える(直後 dispatch_and_migrate のカーネルコードは PIE 不使用)。

##### タスク終了時の所有権解放(release_context フック, 必須)
当初「付随ハイジーン(推奨)」として `start_r` での owner NULL 化を「init-before-use ゆえ無害」と
記していたが、**実機検証で hard bug と判明したため必須対策に格上げ**(2026-06-30)。

- **不具合(hard bug)**: PIE オーナのまま自タスク終了すると所有権が残る。終了の `exit_and_dispatch`
  は退避側 `pie_push`(0x7F2 OFF 化)を**通らない**ため、終了タスクが残した `0x7F2=ON` と stale な
  `pie_owner[PE]` がそのまま残る。直後に新規タスクが `start_r`(switch-in の 0x7F2 管理=`pie_lazy_restore`
  を通らない)で起動すると **0x7F2=ON を継承して「非オーナなのに有効」な幽霊オーナ**になる。後続で
  プリエンプトされると、その生 PIE 状態が**死んだ終了タスクの保存域へ取り違えて flush** され、自分の
  保存域は未更新のまま復帰して計算が壊れる(起動時 §9.2.1 と同型)。`save_context` フックは*移行*しか
  カバーせず、*終了*はこの経路で漏れていた。
- **対策**: 共通部 `task_terminate`(全終了経路 ext_tsk/ter_tsk/ras_ter/ena_ter の合流点)の先頭で
  `release_context(p_tcb)` を呼ぶ(`save_context` の対)。終了タスクは保存不要なので、`chip_release_context`
  (旧名 `chip_pie_release_context`。HWLP のループ CSR ゼロ化も統合、後述レビュー修正)は **owner がこの PE の自分なら `pie_owner[PE]=NULL` + `0x7F2 OFF`**、加えて保存域のマジックを無効化する
  だけ(退避をしない=`save_context` より軽い)。これで終了直後の `start_r` タスクは OFF を継承して非オーナ
  となり、初回 PIE で正しくトラップしてオーナを取り直す。
- **共通部の追加**: `kernel/kernel_impl.h` に `release_context` 既定(空マクロ)、`kernel/task.c` の
  `task_terminate` で1回呼ぶ。チップは `chip_kernel_impl.{c,h}` で `chip_release_context` に解決。
  既定空ゆえ他ターゲット・eager 構成は無影響。

##### コスト
- 非PIEタスク: 切替 = csrwi 1(+数命令)。eager の 216B/切替を解消。
- PIEタスク: 初回使用時のみ 216B 退避/復元 1 回。移行時のみ追加 flush。

##### 検証
- 既存 `fmp_pie_app`(非移行, 全 PRC1)で PASS / 負対照 FAIL を維持(switch-in 0x7F2 管理+トラップの確認)。
- **移行ケースの専用アプリ**: 2 PE 間で PIE 累算タスクを mig_tsk し golden 照合(A=自移行 /
  B=他RUNNABLE移行 / C/D=待ち中移行 を各々誘発)。これが完全 lazy の移行正当性の canary。

#### 9.2.1 実機ブリングアップ結果(2026-06-30, **非移行ケース 解決・実機 PASS**)

lazy(`PIE_LAZY=1`)を実装し `fmp_pie_app`(全 PRC1, LOW/HIGH/REPORT)で実機検証。**非移行ケースは
errA=errB=0 で安定 PASS**(eager 回帰も維持)。実装は FMP3 svn(`kernel/task_manage.c`+
`kernel/kernel_impl.h` の save_context フック, `arch/riscv_gcc/esp32p4/` の
chip_asm.inc/chip_support.S/chip_kernel_impl.{c,h}, `target_kernel_impl.c`, `Makefile.chip`)。

##### 解決した根本原因 = 新規タスクの 0x7F2 継承(start_r が restore 非経由)

当初、LOW の初回 compute だけが LOW↔HIGH の PIE オーナ交替時に失敗(LOW 結果が HIGH の PIE
状態を持つ)。**実機トラップ計装(faulting PC + 命令語のリングバッファ)で確定**:
- **HIGH は初命令 esp.vld q0 でトラップするが、LOW は q0/q1 ロードでトラップせず VADD で
  トラップ**していた。→ LOW の compute 開始時に **0x7F2 が(非オーナなのに)ON**。
- 原因: **新規タスクは `start_r` 経由で起動し，switch-in の `pie_lazy_restore`(0x7F2 管理)を
  通らない**ため，**前タスク(オーナ HIGH)の 0x7F2=ON を継承**。LOW が「非オーナなのに 0x7F2 ON」で
  走り，トラップせず物理レジスタを共有して破壊 → lazy 不変条件「0x7F2 ON ⟺ 走行中==オーナ」が破綻。

**修正(共通部 unchanged)**: lazy の **`pie_push`(save 側=switch-out)で `csrwi 0x7F2, 0`**。これは
**HWLP の `hwlp_push` が save 側でループ CSR をゼロ化し，restore を通らない新規タスクにも 0 を
継承させるのと全く同じトリック**(§設計メモ #1 / chip_asm.inc)。OFF を継承した新規タスクは非オーナ
として初回 PIE 命令で必ずトラップし，正規の切替では直後の `pie_lazy_restore` がオーナには ON を
再設定する。修正後，LOW も初命令でトラップし **errA=errB=0 安定 PASS**。

> 教訓: lazy コプロセッサ管理では「新規タスクが restore を通らず enable 状態を継承する」点が
> HWLP・PIE 共通の落とし穴。**enable 状態は save 側でリセット**して継承させるのが定石。

##### IDF 正準実装との一致確認
`$HOME/tools/esp-idf` の `components/freertos/.../portasm.S` の `rtos_save_pie_coproc` と全比較し，
`pie_save_regs`/`pie_restore_regs`(命令列・q3除外・misc packing)，`pie_enable`(csrwi 0x7F2,1)，
アルゴリズム(enable→旧flush→**初回 norestore=物理継承**→owner更新→PC不進で再実行)，mepc 無調整
すべて一致。handler は IDF 準拠(enable 先頭・初回 norestore・保存域マジックで有効判定)。

##### 移行(mig_tsk)ケース 実機 PASS(pattern A=自タスク移行)
専用アプリ `tools/fmp_pie_mig_app`(WORKER=CLS_ALL_PRC1)を作成。WORKER は PIE 累算
(q0 へ seed を iters 回)を **mig_tsk(TSK_SELF, 他PE) で前半/後半に分割**し，分割点で自タスクを
他 PE へ移行する(pattern A)。q0/q1 は分割を跨いで物理レジスタに live なので，移行で lazy が
保存域へ flush→移行先 PE の後半 esp.vadd で「非オーナ→トラップ復元」されない限り結果が壊れる。
各 PE に PIE noise タスクを置きオーナを奪い合わせて到着時トラップを誘発。
- **結果: golden 一致のまま errW=0 / 2392回以上の移行で安定 PASS**。完全 lazy の移行正当性
  (pattern A: `save_context(p_selftsk)` flush + `dispatch_and_migrate` + 移行先トラップ復元)を実証。
- `chip_pie_save_context` の enable 直後 esp.* も実機で問題なし(enable ハザードは存在せず。
  非移行で疑った enable 遅延は不要だった = 真因は §上記の start_r 0x7F2 継承だった)。

##### 移行(mig_tsk) pattern B/C/D 実機 PASS
専用アプリ `tools/fmp_pie_mig2_app` を作成。**CONTROLLER(PRC1, 高優先)が WORKER(自タスク以外)を
`ref_tsk` で状態判別して移行**する:
- **B**: WORKER が busy-wait で RUNNABLE のとき `mig_tsk(WORKER, PRC2)`(同PE 他 RUNNABLE 移行)
- **C**: WORKER が `slp_tsk`(無タイムアウト待ち)のとき移行 → `wup_tsk`
- **D**: WORKER が `tslp_tsk`(タイムアウト付き待ち)のとき移行 → `wup_tsk`

WORKER は PIE 累算を前半/後半に分割し，分割点で上記状態になって CONTROLLER に移行させ，後半を
PRC2 で実行(非オーナ→トラップ復元)。各反復末に `mig_tsk(TSK_SELF, PRC1)` で PRC1 へ戻る。
- **結果: errW=0 のまま B/C/D 各 2600 回以上を誘発し安定 PASS**。これで **kernel/ の
  mig_tsk 全4分岐(A/B/C/D)で `save_context(p_tcb)` による flush + 移行先トラップ復元**の
  正当性を全数実証した。

##### まとめ(2026-06-30)
lazy PIE は **#ifdef TOPPERS_PIE_LAZY ガードで既定=eager**。実機検証は全て PASS:
| 検証 | アプリ | 結果 |
|---|---|---|
| 非移行(切替・プリエンプト) | `fmp_pie_app` | errA=errB=0 PASS |
| 移行 A(自タスク) | `fmp_pie_mig_app` | errW=0 / 2400+回 PASS |
| 移行 B/C/D(他/待ち中タスク) | `fmp_pie_mig2_app` | errW=0 / 各2600+回 PASS |
| 終了 ext_tsk(自タスク終了→start_r) | `fmp_pie_term_app` | errW=0 PASS / **負対照(release 無効)=FAIL** |
| 終了 ras_ter→ena_ter(要求による延期自終了) | `fmp_pie_raster_app` | errW=0 PASS / **負対照(release 無効)=FAIL** |

- **終了経路の `release_context` 検証**(上表下2行): PIE オーナのまま終了し直後に新規タスクが `start_r`
  起動する状況を作り、yield 中に NOISE タスクがオーナを奪って幽霊オーナの取り違え flush を誘発。
  `chip_release_context`(当時の名称 `chip_pie_release_context`)を無効化した負対照では errW>0(FAIL)で**幽霊オーナ破壊を再現**、有効版では
  errW=0(PASS)で**根治を実証**。`task_terminate` が全終了経路の合流点なので ext_tsk と ras_ter の両系統で
  確認。
- eager 回帰(`build_fmp3_lib.sh` 既定)も configuration check passed を維持。診断計装は撤去済。

#### 9.2.2 lazy の既知の制限 — 密 PIE ピンポンでの lost-wakeup(2026-06-30, JTAG 究明)

性能評価で「2タスクが**毎切換えにベクタ命令を使う密ピンポン**(slp_tsk/wup_tsk)」を lazy で走らせると，
数千反復後に**ハング(無出力)**することが判明。JTAG/OpenOCD(`board/esp32p4-builtin.cfg`)で究明した:

- **症状(JTAG 確定・無摂動 `idf.py flash` で採取)**: 両コアが `dispatcher_2`(idle)で停止。例外もスピンも
  無く，giant lock 非保持，`clic_storm_cnt=0`，tick 生存。**TASK1=TS_WAITING_SLP・wupque=0**(永久に眠る)，
  起床役 **TASK2=DORMANT**，両 PCB p_schedtsk=NULL。＝**lost-wakeup**(task2 の `wup_tsk(TASK1)` が取りこぼ
  され task1 が永久に眠る)。
- **反証された仮説**: (1) CLIC IPI ストーム/位相共振 → `clic_storm_cnt=0`・両コア idle で否定。
  (2) 例外復帰が dispatch を再評価しない → `core_exc_entry_2` は `core_int_entry_3` と**対称**(再評価あり)で否定。
- **FreeRTOS 比較で FMP3 固有と確定(2026-06-30)**: ESP-IDF FreeRTOS で**同一の密 PIE ピンポン**(TASK1/TASK2
  を core0 ピン留め, task-notify, 毎切換え esp.vadd)を実装・実機実行 → **N=20万を3回とも完走(約3秒, hang 無し)**。
  IDF も `components/riscv/vectors.S`→`rtos_save_pie_coproc`(=FMP3 lazy が模した同一アルゴリズム)を毎切換え踏むが
  lost-wakeup は起きない。**→ lazy PIE アルゴリズム本質ではなく FMP3 固有(例外復帰経路)**と確定。
- **棄却された被疑 — CLIC mil 正規化(試験A)**: 唯一の int/exc 非対称＝`irc_end_int`(chip_support.S:147-150)は
  `mcause.mpil=0` を強制するが `irc_end_exc`(199-205)はこれを欠く点。`irc_end_exc` に同等の正規化を追加して
  再試験 → **依然ハング**。∴ mil 非対称は root cause ではない(§5「mil は実質常に 0」とも整合)。修正は revert。

- **★真因確定(2026-07-01, instrument + post-hoc JTAG dump)**: `slp_tsk`/`wup_tsk` 各分岐・`pie_exc_handler`・
  `task_terminate`(ext_tsk)に RAM リングバッファ計装(`-DLWTRACE`,調査後すべて revert)を入れ，電源/RTS リセットで
  wedge 再現→**JTAG で RAM を非摂動採取**。トレースが因果連鎖を直接記録した:
  - #0–#69 規則的 ping-pong(切換え間隔 dcyc≈500 均一)。**起動直後 #15 に dcyc≈22248(≈62µs)の巨大ギャップ＝
    logtask(prio3,高優先)が main の初期 syslog "Performance" 排出で密ピンポンに割り込む**。以降 tick 周期の小ギャップ。
  - **#71 で `task_terminate` 発火(run=TASK2, arg=TASK2 の TCB)→ 以後イベント皆無＝wedge**。TASK2 の loop は
    NO_MEASURE=10000 回なのに **約17反復で ext_tsk(自己終了)**。非PIE illegal fault は記録されず。
  - 確定状態: **TASK1=TS_WAITING_SLP 固着, TASK2=DORMANT, 両 PCB p_runtsk/p_schedtsk=NULL, pie_owner[]=NULL**
    (TASK2 終了時 `release_context` がクリア)。
- **機序**: 密な毎切換え PIE **CPU 例外**(lazy のみが生む)に **logtask/tick のプリエンプトが重なると，lazy PIE の
  例外処理経路が走行タスク(TASK2)の制御文脈(復帰アドレス/ループ変数レジスタ)を破壊** → TASK2 がループを異常に抜けて
  早期 ext_tsk → 相方 TASK1 を起こす者が消え `slp_tsk` で永久固着 → ハング。**FreeRTOS が無事なのは PIE トラップを
  ベクタ内インライン処理し「プリエンプト可能な例外復帰」窓を持たないため**(FMP3 は DEF_EXC=フル例外入口＋復帰時
  ディスパッチ経路を通り，この窓が生じる)。残る未解明＝破壊される正確なレジスタ／clean-return か非IINST fault か
  (eager 採用で実害無く保留)。
- **計測上の注意**: 出力は lazy/eager 問わず "Perfor" 付近で停止する **logtask/serial 出力 wedge**(別事象)があり，
  ハング判定はシリアルでなく **JTAG(seq 不変＋TASK1=WAIT_SLP)** で行う。flash(フルリセット)は完走しやすく
  RTS リセットは wedge しやすい等，リセット種別で再現率が変動する(Heisenbug)。
- **★解決(2026-07-01, ファストトラップ方式)**: 真因は **OS の例外入口/出口を介すること**＝core_exc_entry が
  ハンドラ呼出前に `unlock_cpu_and_unlock_allint`(core_support.S:953)で**割込みを許可**し，かつ復帰時に
  core_exc_entry_2 でディスパッチするため，**グローバルな PIE 物理レジスタ＋CSR_PIE_STATE_REG(0x7F2)＋
  pie_owner(いずれもタスク文脈に入らない)を非 atomic に esp.* で退避**してしまう点(IDF FreeRTOS は同退避を
  ベクタ内・割込み禁止で atomic に行うため無事)。
  - **対処**: `pie_exc_handler` を **DEF_EXC で登録せず**，`trap_vector_table`(chip_support.S)に **PIE 不正命令だけを
    横取りするプリフィルタ**を置き，**`pie_fast_trap`**(割込み禁止のまま `chip_pie_lazy_swap` を呼び mret)で
    **atomic** に処理する。OS の重い文脈退避・割込み許可・ディスパッチ経路を一切通さない(=高速化も兼ねる)。
    caller-saved のみ task stack に退避，callee-saved/sp/gp/tp は C ABI が保つ。非PIE のトラップは従来どおり
    `core_int_entry` へ素通し。`mscratch` は task 実行中フリーゆえ判定用に流用。
  - **実機検証**: 以前は確実に wedge した RTS リセット起動を **×20 すべて完走**(task1/task2=DORMANT)。eager/
    NO_COPROC ビルドはプリフィルタが lazy ガードで無影響(回帰なし)。
  - **結論**: **eager(案1)・lazy(案2)とも採用可能**。lazy は本修正で密ピンポン×プリエンプトに耐え，かつ毎切換えの
    重い OS 例外経路が消えて高速。**correctness は元から無事**(本件は liveness の問題で，esp.* 退避列自体は不変)。
- 関連: `chip_support.S`(trap_vector_table プリフィルタ / pie_fast_trap / pie_save_regs / pie_restore_regs),
  `chip_kernel_impl.c`(chip_pie_lazy_swap / pie_exc_handler), `core_support.S`(core_exc_entry:953 割込み許可),
  `kernel/task.c`(task_terminate / release_context)。再現/検証アプリ: `tools/fmp_perf1v_app`(lazy ビルドで密ピンポン)。

---

## 付録: 設計判断の根拠(却下案)

- **PIEをeager(全切替退避)**: 216B/切替は重く、dispatch遅延を確実に悪化させるため却下。
- **HWLPをlazy owner**: 24Bのために移行設計の単純さ(PIE単独・atomics不要)を崩し、かつ
  P4 rev≤1の状態バグ・誤cause bitの直撃を受ける。費用対効果が悪く却下。
- **HWLPをcount連動(IDF方式)**: 定常は安いが rev≤1回避策が必須でdispatch遅延がデータ
  依存になる。方式Aの固定税が問題化した場合の将来最適化として保留。
- **移行をクロスコアフラッシュ(IPI)**: `mig_tsk` の `E_OBJ` 制約により対象は必ず自コアに
  あるため不要。ローカルフラッシュに退化する。
