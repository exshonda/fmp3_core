# FMP3 RISC-V 依存部 → ESP32-P4 移植 ファイル単位マッピング

> **本書の位置づけ**: 本書は設計・調査の記録であり，利用手順書ではない．利用者向けの
> 入口は `chip_user.md`（`arch/riscv_gcc/esp32p4/`）および `target_user.md`
> （`target/m5stamp_esp32p4_gcc/`）を参照．本書は下記「作成: 2026-06-27」時点の記録で
> あり，現在の実装と異なる場合がある．

作成: 2026-06-27 / 対象カーネル: FMP3 ツリー（本ドキュメントが含まれるツリー，3.4系）

方針: `arch/riscv_gcc/common/` は変更しない．チップ/ターゲット依存部（arch/riscv_gcc/esp32p4，target/m5stamp_esp32p4_gcc）を新規作成する．
CLIC/INTMTX のレジスタ詳細は `esp32p4_hw_reference.md` を参照．

### 前提差分

| 項目 | PolarFire SoC | ESP32-P4 |
|---|---|---|
| ISA | RV64GC / lp64d | RV32IMAFC / ilp32 |
| 割込みコントローラ | PLIC + CLINT | **CLIC 固定** (mtvec.MODE=0x3, mtvt使用) + INTMTX (割込みマトリクス) |
| CLINT | あり | あり (msip/mtime) |
| hartid | 1..4 (U54) | **0, 1** |
| ブート | 直接 ELF ロード | ESP-IDF 2nd ブートローダにアプリイメージとして載せる |

---

## 0. 層アーキテクチャ (呼出し契約)

`common/core_support.S` が汎用の trap 入口 `core_int_entry` / `core_exc_entry` を持ち，
**チップ依存部が提供する以下のシンボル**を呼ぶ．

| コア側が要求するシンボル | 提供元(PolarFire) | 役割 |
|---|---|---|
| `trap_vector_table` | chip_support.S | mtvec が指すベクタ表 (256B境界，各cause→`core_int_entry`/`core_exc_entry`) |
| `irc_begin_int` | chip_support.S | 入口で割込番号/前優先度/ハンドラ番地を返し，優先度マスクを上げる |
| `irc_end_int` | chip_support.S | 出口で EOI，優先度マスクを戻す |
| `irc_get_intpri` | chip_support.S | CPU例外発生前の割込優先度(外部表現)取得 |
| `irc_begin_exc` / `irc_end_exc` | chip_support.S | 例外ハンドラ前後の優先度マスク保存/復元 |
| `default_int_handler` | common(core_kernel_impl.c) | 未定義割込み |
| `t_set_ipm`/`t_get_ipm`/`enable_int`/`disable_int`/`probe_int` | chip_kernel_impl.h(→plic) | IPM/割込線操作 |
| `get_my_prcidx`/`get_my_pcb` 系 | chip_kernel_impl.h / core | hartid→prcidx 変換 |
| `MTIMER_*` / `CLINT_MSIP` / `raise_msip` | polarfire_soc.h / clint_ipi.h | CLINT タイマ/IPI |

---

## 1. `arch/riscv_gcc/common/` (コア依存部 — 原則無変更)

| ファイル | 役割 | 判定 | 備考 |
|---|---|---|---|
| core_support.S | trap入口/出口，dispatch，SMP切替 | **流用(無変更)** | `__riscv_xlen==32` 分岐あり．chip の irc_*/trap_vector_table を呼ぶだけ |
| core_kernel_impl.{c,h} | CPUロック(mstatus.MIE)，ロック，TPCB，dispatch宣言 | **流用** | CLIC でも mstatus.MIE 有効 |
| core_asm.inc | `my_pcb`/`my_istkpt` 等の共通マクロ | **流用** | chip_asm.inc から include される |
| start.S | BSS/data初期化，FPU初期化，master/slave同期，hook呼出 | **流用(注意)** | FPU初期化で `fscsr`/`mstatus.FS` を無条件実行 → `-march` に `f` を含めることが必須(実際の指定は `-march=rv32imafc_zicsr_zifencei_xesppie -mabi=ilp32f`．下記) |
| mtimer.{c,h} | CLINT mtime/mtimecmp で高分解能タイマ | **流用** | RV32 経路で chip 側マクロ `MTIMER_MTIME_L/_U`，`MTIMER_MTIMECMP_L/_U(prcid)`，`MACHINE_TIMER_MTIME_U` を要求 |
| msi_ipi.{c,h} | CLINT msip による IPI(dispatch/ext_ker/set_hrt) | **流用** | `raise_msip`/`clear_msip`(chip clint_ipi.h) を要求 |
| riscv.h / riscv_insn.h | CSR定義/特殊命令インライン | **流用** | CLIC 用 CSR(mtvt 0x307 等)は chip 側で追加 |
| core_sil.h / core_stddef.h / core_kernel.h / core_syssvc.h / core_test.h | 共通 SIL/型/定義 | **流用** | — |
| core_rename.{h,def} / core_unrename.h / core_sym.def | 名前付け | **流用** | — |
| core_*.trb | cfg生成スクリプト | **流用** | chip_kernel.trb から IncludeTrb される |
| Makefile.core | COREDIR，core_support.o/core_kernel_impl.o，start.o | **流用** | — |
| **plic_kernel_impl.{c,h}** | **PLIC ドライバ** | **不要(置換)** | ESP32-P4 に PLIC 無し．新規 `clic_kernel_impl.*` で置換 |
| **plic_kernel.trb** | `_kernel_plic_target_cidx_table` 生成 + CFG_INT チェック | **不要(置換)** | CLIC/INTMTX 用 trb を新規作成 |

**注意点:**
- start.S の FPU 初期化 (`li t1,MSTATUS_FS_INIT; fscsr t0`) は無条件実行．`-march=rv32imac` だとアセンブラが `fscsr` を拒否する．**`-march=rv32imafc_zicsr_zifencei_xesppie -mabi=ilp32f`** で開始すること(hard-float(ilp32f) ABI であり，FPU 文脈退避は RISC-V 共通部 `arch/riscv_gcc/common/core_support.S` が `__riscv_flen` 定義時に実装している)．
- mtimer.h RV32 経路の `MACHINE_TIMER_MTIME_U` は common 側で使われるため，chip ヘッダ側で同名マクロを定義して吸収する．

---

## 2. `arch/riscv_gcc/polarfire_soc/` → 新規 `arch/riscv_gcc/esp32p4/`

| ファイル | 役割 | 判定 | 具体的改変点(ESP32-P4) |
|---|---|---|---|
| chip_support.S | trap_vector_table と irc_*(PLIC前提) | **全面書換** | PLIC操作(PLIC_TH/PLIC_CC/cidx)を全廃．CLIC化: ①ベクタ表は **mtvt** へ移行(CLIC MODE=3)．②`irc_begin_int`: mcause下位で番号取得→ msip=3/mtimer=7 分岐維持，外部は INTMTX/CLIC番号(16+)から `p_inh_tbl` 引き．優先度マスクは **`CLIC_INT_THRESH_REG`(bit31:24)** に設定．③mie による MTIE/MSIE禁止は **`clicintie[3]/[7]`** 操作に置換．④`irc_end_int` の EOI は CLIC では pending 自動クリア(edge)/レベル要因クリア．⑤`irc_get/begin/end_exc` は THRESH レジスタ read/write に置換 |
| chip_kernel_impl.h | IPM変換，intno範囲，enable/disable/probe，get_my_prcidx | **要改変(大)** | `get_my_prcidx()`= `mhartid`(P4は0オリジン，`-1`不要)．`get_my_plic_cidx`削除．`TNUM_INTNO`= CLIC外部割込み数(例32) or INTMTX源数．`t_set_ipm/t_get_ipm` を CLIC THRESH + clicintie(soft/timer) に．`disable/enable/probe_int` を `clicintie[i]`/`clicintip[i]` に．`#include "plic_kernel_impl.h"`→`"clic_kernel_impl.h"` |
| chip_kernel_impl.c | chip_initialize/terminate/mprc_init | **要改変** | `riscv_write_mtvec(... MTVEC_MODE_VECTORD)` → CLIC設定(`mtvec` MODE=3 固定，`mtvt`書込，`CLIC_INT_CONFIG`(nlbits=3))．`plic_*_initialize`→`clic_*_initialize`+INTMTX初期化．`riscv_set_mie(...)`→ CLIC では mie無効，`clicintie[3]/[7]`許可へ |
| chip_kernel.h | TMIN/TMAX_INTPRI，SUPPORT_*_INT | **要改変(小)** | CLIC nlbits=3 → 8段．`TMIN_INTPRI=-7`(or -8)/`TMAX_INTPRI=-1` |
| chip_asm.inc | `my_pid`/`my_pidx`/`my_cidx`，追加コンテキスト退避マクロ | **要改変** | `my_pid`=`csrr mhartid`(そのまま)，`my_pidx`= `-1` を削除(P4は0オリジン)，`my_cidx`削除．退避マクロは空のまま |
| polarfire_soc.h | CLINT/PLIC/MMUART/SYSREG アドレス | **全面書換 → esp32p4.h** | CLINT base，`CLINT_MSIP(pid)`，`MTIMER_MTIME`/`MTIMECMP`(RV32分割: `_L/_U`)．CLIC base `0x2080_0000`/ctrl `0x2080_1000`/他コア +`0x1_0000`，THRESH/CONFIG/`clicint{ip,ie,attr,ctl}[i]`．INTMTX base とMAPレジスタ．`MACHINE_TIMER_MTIME_U` 別名も定義．UART アドレス |
| clint_ipi.h | IPINO定義，raise/clear_msip，msi_ipi.h include | **要改変(小)** | `CLINT_MSIP(prcid)` を P4 CLINT アドレスへ |
| chip_timer.h | mtimer.h を include するだけ | **流用(ほぼ無変更)** | include パスのみ |
| chip_serial.{c,h,cfg} + mmuart.{c,h} | MMUART(PolarFire UART) ドライバ | **全面書換** | ESP32-P4 UART ドライバへ．SIO API(`sio_opn_por`/`sio_snd_chr`/`sio_isr` 等，chip_serial.h の宣言は流用) を P4 UART レジスタで実装 |
| (新規) clic_kernel_impl.{c,h} | CLIC + INTMTX ドライバ(PLIC置換) | **新規作成** | `clic_set_context_priority`(THRESH)，`clic_enable/disable_int`(clicintie)，`clic_probe_pending`(clicintip)，`clic_set_priority`(clicintctl)，`clic_context/global_initialize`，INTMTX で周辺源→CLIC線マップ |
| (新規) clic_kernel.trb | cfg時の割込ターゲット表生成 + CFG_INT チェック | **新規作成** | plic_kernel.trb を雛形に，CLIC番号/コア割付へ．`pid2cidx` は P4 では hartid 直 |
| chip_rename.{h,def} / chip_unrename.h | 名前付け | **要改変(小)** | plic_→clic_ シンボル名へ |
| Makefile.chip | march/mabi，GCC_TARGET，obj一覧 | **要改変** | `GCC_TARGET=riscv32-esp-elf`．`-march=rv32imafc_zicsr_zifencei_xesppie -mabi=ilp32f -mcmodel=medany`．`KERNEL_COBJS`: `plic_kernel_impl.o`→`clic_kernel_impl.o`，`mmuart.o`→P4 uart |
| chip_stddef.h / chip_sil.h | 型/SIL chip依存 | **流用(微改変)** | include パス調整程度 |

---

## 3. `target/polarfire_soc_kit_gcc/` → 新規 `target/m5stamp_esp32p4_gcc/`

| ファイル | 役割 | 判定 | 具体的改変点 |
|---|---|---|---|
| target_kernel_impl.c | hardware_init_hook，target_initialize/exit，target_fput_log，_sbrk | **要改変** | PolarFire の UART クロック有効化(SYSREG)を削除/置換．`chip_initialize`→sio初期化の流れは維持．`_sbrk`(静的ヒープ)は流用可．clock/cache 初期化は IDF ブート委譲のため不要 |
| target_kernel_impl.h | polarfire_soc_kit.h + chip_kernel_impl.h include | **要改変(小)** | include を esp32p4_kit.h / chip(esp32p4) へ |
| polarfire_soc_kit.h → esp32p4_kit.h | TARGET_NAME，CORE_CLK_MHZ，**MTIMER_FREQ_MHZ**，USE_UARTx，SIL_DLY_* | **全面書換** | P4 値へ．`MTIMER_FREQ_MHZ`= P4 CLINT 周波数(IDF コードで要確認)．`SIL_DLY_TIM1/2` 実測調整 |
| **リンカスクリプト (mpfs-lim.ld)** | メモリ配置 | **全面書換(新規 .ld)** | Microchip SDK の .ld は廃棄．**ESP-IDF アプリイメージ形式**に合わせ，L2MEM/内蔵SRAM(または PSRAM) に `.text/.data/.bss/.stack` を配置．`__sbss_start/_end`，`__bss_start/_end`，`__idata_start/_end`，`__data_start`，`__global_pointer$`，`start_sync` 等 start.S 参照シンボルを必ず PROVIDE |
| Makefile.target | SYS/CHIP/CORE/TOOL，SDK obj，LDSCRIPT | **全面書換** | `CHIP=esp32p4 CORE=riscv TOOL=gcc SYS=m5stamp_esp32p4`．Microchip MPFS HAL(`mss_*.o`, `system_startup.o`) を全削除．`LDSCRIPT` を新規 .ld へ |
| target_serial.cfg / target_serial.h | chip_serial へ委譲 | **流用(include調整)** | — |
| target_ipi.h | clint_ipi.h include | **流用(無変更可)** | — |
| target_asm.inc | chip_asm.inc include | **流用** | — |
| target_timer.h | chip_timer.h include 相当 | **流用** | — |
| target_kernel.cfg / target_kernel.trb | カーネルcfg/生成 | **流用(微)** | chip_kernel.trb を参照する形を維持 |
| target_kernel.h / target_syssvc.h / target_sil.h / target_stddef.h / target_test.h | 型/SIL/syssvc | **流用(微改変)** | TARGET_NAME 等 |
| target_check.trb / target_class.trb / target_cfg1_out.h | cfgチェック/クラス | **流用(微)** | SMP クラス定義を 2コア向けに確認 |
| target_rename.{def} / target_rename.h / target_unrename.h | 名前付け | **流用(微)** | — |
| sdk/ , sdk_entry.c, mss_*, SoftConsole 関連 | Microchip SoftConsole/HAL | **不要** | ESP-IDF/esptool フローへ置換 |

---

## 4. ディレクトリ構成（FMP3 ツリー内）

```
arch/riscv_gcc/
  common/            RISC-Vコア依存部 + CLIC 依存部
                     （clic_kernel_impl.{c,h}, clic_kernel.trb）
  esp32p4/           ESP32-P4 チップ依存部
    chip_kernel_impl.{c,h}  chip_kernel.h  chip_asm.inc  chip_support.S
    esp32p4.h  clint_ipi.h  chip_timer.h
    chip_serial.{c,h,cfg}  chip_sil.h  chip_stddef.h
    chip_rename.{def,h}  chip_unrename.h  chip_kernel.trb  Makefile.chip
  doc/               RISC-V/CLIC のドキュメント（clic_design.md / clic_memo.md 等）
target/m5stamp_esp32p4_gcc/   ESP32-P4 ターゲット依存部
  target_kernel_impl.{c,h}  m5stamp_esp32p4_kit.h  esp32p4_fmp.ld
  target_serial.{cfg,h}  target_ipi.h  target_asm.inc  target_timer.h
  target_kernel.{cfg,h,trb}  target_*.{h,def,trb}  Makefile.target
  tools/             ビルド・実機実行・テストのツール一式（方式(a)）
```

CLIC は PLIC を置換する共通部（clic_kernel_impl）として `common/` に配置する．
チップ固有のレジスタ定義（CLIC_INT_CTRL/THRESH，CLINT 等）は esp32p4.h が提供する．
