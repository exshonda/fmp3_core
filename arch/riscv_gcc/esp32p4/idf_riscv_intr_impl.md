# ESP-IDF v5.5.0 — ESP32-P4 HPコア 割込み/タイマ/コア起動 実装リファレンス

> **本書の位置づけ**: 本書は設計・調査の記録であり，利用手順書ではない．利用者向けの
> 入口は `chip_user.md`（`arch/riscv_gcc/esp32p4/`）および `target_user.md`
> （`target/m5stamp_esp32p4_gcc/`）を参照．本文中に明示的な作成日の記載は無いため，
> 代わりに svn 上の最終コミット日時（2026-06-29，リビジョン564．`svn info` による事実）
> を記す．本書はその時点の記録であり，現在の実装と異なる場合がある．

TOPPERS/FMP3 チップ依存部 (CLIC + 割込みマトリクス + CLINT mtimer + IPI + core1 起動) 実装のための確定事実リファレンス．
参照元: `/home/honda/tools/esp-idf/` (ESP-IDF v5.5.0, 読み取りのみ)．

---

## 0. 全体アーキテクチャ (2段構成)

1. **割込みマトリクス**: ペリフェラルソース番号 (`ETS_*_INTR_SOURCE`) → コアの CLIC 割込み線番号へルーティング．
2. **CLIC**: 線ごとに IE / priority / trigger-type / vectored(SHV) を制御し CPU へ割込みを上げる．

CLIC 線番号の構造:
- 線 **0〜15**: 内部割込み (mtime=7, msip=3)．IDF は使用しない．
- 線 **16〜47**: 外部割込み (外部番号 0〜31 に `RV_EXTERNAL_INT_OFFSET=16` を加算)．
- `RV_TOTAL_INT_COUNT=48`, `RV_EXTERNAL_INT_COUNT=32`．

ソース: `components/riscv/include/esp_private/interrupt_clic.h:40-41`, `components/hal/include/hal/interrupt_clic_ll.h:22`, `components/riscv/include/riscv/encoding.h:183-188`．

---

## 1. CLIC の初期化と設定

### 1.1 関連 CSR (すべて M-mode)

ソース: `components/riscv/include/esp_private/interrupt_clic.h`

| CSR | アドレス | 説明 |
|---|---|---|
| `mtvec` | 0x305 (標準) | 例外/非ベクタ割込み入口．MODE=3 固定 (`MTVEC_MODE_CSR=3`)．64バイト境界必須． |
| `MTVT_CSR` | 0x307 | HWベクタリングのジャンプテーブル基底．64バイト境界の48エントリ表．|
| `MINTSTATUS_CSR` | 0x346 | **P4非標準** (spec は 0xFB1)．現在割込みレベル `mil` は bits[31:24] (`MINTSTATUS_MIL_S=24`, `MINTSTATUS_MIL_V=0xFF`)．|
| `MINTTHRESH_CSR` | 0x347 | **P4には存在しない** (C5/C61/H4 専用)．P4 の threshold はメモリマップド (1.5節)．|

### 1.2 起動時の mtvec / mtvt 設定

`components/esp_system/port/cpu_start.c:213-219`:
- `mtvec = &_vector_table | 3` (MODE=3)．実機値 `mtvec=0x4ff00003` (base=0x4ff00000)．
- `mtvt(0x307) = &_mtvt_table`．

### 1.3 CLIC レジスタマップ

ソース: `components/soc/esp32p4/include/soc/clic_reg.h`

```
DR_REG_CLIC_BASE      = 0x20800000   // 自コア基準 (他コアは +DUALCORE_CLIC_CTRL_OFF=0x10000)
DR_REG_CLIC_CTRL_BASE = 0x20801000
NLBITS = 3                           // 有効 priority/level ビット数 (値域 0..7)
CLIC_EXT_INTR_NUM_OFFSET = 16
```

グローバルレジスタ (DR_REG_CLIC_BASE 相対):

| レジスタ | オフセット | 主フィールド |
|---|---|---|
| `CLIC_INT_CONFIG_REG` | +0x0 | `MNLBITS`[4:1] (level bit数), `NVBITS`[0] (HWベクタ有効, default 1) |
| `CLIC_INT_INFO_REG` | +0x4 | `CTLBITS`[24:21], `NUM_INT`[12:0]=48 |
| `CLIC_INT_THRESH_REG` | +0x8 | `CLIC_CPU_INT_THRESH`[31:24] = マスク閾値 |

**Per-interrupt 32bit レジスタ** `CLIC_INT_CTRL_REG(i) = 0x20801000 + i*4` (i = CLIC線番号 0〜47，+16込み):

| フィールド | bits | 意味 |
|---|---|---|
| `CLIC_INT_CTL` | [31:24] | priority/level (上位 NLBITS=3 ビットが有効) |
| `CLIC_INT_ATTR_MODE` | [23:22] | RO=0b11 (M-mode固定) |
| `CLIC_INT_ATTR_TRIG` | [18:17] | trig[0]: 0=level/1=edge，trig[1]: 0=rising/1=falling |
| `CLIC_INT_ATTR_SHV` | [16] | 1=HWベクタ割込み，0=非ベクタ |
| `CLIC_INT_IE` | [8] | 割込みイネーブル |
| `CLIC_INT_IP` | [0] | 割込みペンディング |

バイトアクセス版: `BYTE_CLIC_INT_IP_REG(i)=base+i*4`, `IE=base+1+i*4`, `ATTR=base+2+i*4`, `CTL=base+3+i*4` (`clic_reg.h:114-160`)．

### 1.4 Per-IRQ 設定

- **priority 設定値**: `level << 5` (設定)，`(CTL field) >> 5` (取得)．
- **SHV (vectored)**: `CLIC_INT_CTRL_REG(n+16)` の `CLIC_INT_ATTR_SHV` (bit16)．
- **edge ACK**: `CLIC_INT_IP`(bit0) を対象線番号にセット．
- **trig 型設定**: ROMパッチ `esp_rom_clic.c:18-21` で `CLIC_INT_ATTR_TRIG` フィールドを書込 (ROMバグ回避のためパッチが必要)．

### 1.5 閾値によるレベルマスク (mie/mip の代替)

P4 は グローバル割込みマスクに **レベル閾値** を使う (mie/mip 非対応)．閾値は **メモリマップド** `CLIC_INT_THRESH_REG` (0x20800008) bits[31:24]．

ソース: `components/riscv/include/esp_private/interrupt_clic.h:76-256`

- レベル→reg値: `NLBITS_TO_BYTE(l) = (l<<5)|0x1F`．3ビットを上位寄せ，下位5ビットは1で埋める．格納値 = `NLBITS_TO_BYTE(level) << 24`．
- `RVHAL_INTR_ENABLE_THRESH = 0` — 全許可デフォルト．割込みのデフォルト priority は1．
- **閾値書込は即時反映されない (P4固有HW)**．書込後に `REG_READ(CLIC_INT_THRESH_REG)` 1回のread-backか約8 nop が必要．FMP3 の CPUロック/IPM変更経路で必須 (`:190-203`)．
- **CLIC マスクは inclusive**: 閾値以下の優先度を全マスク．`mask_int_level_lower_than(level)` は閾値を `level-1` に設定 (`:251-256`)．
- 初期化: `esprv_int_set_threshold(RVHAL_INTR_ENABLE_THRESH)` を起動時に1回 (`port.c:167`)．

### 1.6 ROM常駐関数

`esprv_int_enable/disable/set_priority/set_threshold/set_type` は ROM の `esprv_intc_int_*` にエイリアス (`rom.api.ld:8-12`)．ROMパッチは `set_type` のみ．
**FMP3 推奨**: ROM に依存せず 1.3節のレジスタを直接叩いて実装すること (ROM内部の+16加算等の不確実性を排除)．

---

## 2. 割込みマトリクス (intr matrix)

### 2.1 MAP レジスタ

ソース: `components/soc/esp32p4/register/soc/reg_base.h:102,198,199`, `components/hal/include/hal/interrupt_clic_ll.h:31-44`

```
DR_REG_INTERRUPT_CORE0_BASE = 0x500D6000
DR_REG_INTERRUPT_CORE1_BASE = 0x500D6800
```

- ストライド = ソース番号×4．MAP レジスタ = `base + 4*source`．
- フィールド幅 = **6ビット [5:0]**．書込値 = ルーティング先 **CLIC線番号** (= 外部割込み番号 + 16)．
- FMP3 推奨実装:
  ```c
  *(volatile uint32_t*)(0x500D6000 + 4 * source) = (ext_line + 16);  // core0
  *(volatile uint32_t*)(0x500D6800 + 4 * source) = (ext_line + 16);  // core1
  ```
- 解放時は MAP を `INT_MUX_DISABLED_INTNO=6` にルーティング (IDF慣行)．外部線6 (=CLIC線22) はIDF が切断用に予約．

### 2.2 割込みソース一覧

`components/soc/esp32p4/include/soc/interrupts.h` の enum (`ETS_*_INTR_SOURCE`)．`ETS_*_INTR_SOURCE` 値が MAP レジスタの配列インデックス (offset=source*4)．

---

## 3. タイマ

### 3.1 IDFの選択: SYSTIMER ペリフェラル (CLINT mtime は不使用)

FreeRTOS ティックは **SYSTIMER ペリフェラル** を割込みマトリクス経由で CLIC 外部線に割り当てて生成．IDF は mtime/mtimecmp (CLINT) を一切使わない．

- カウンタ `SYSTIMER_COUNTER_OS_TICK=1`，core0 アラーム=0，core1 アラーム=cpuid．
- clock = XTAL(40MHz) 固定 + 固定分周2.5 → **分解能16MHz** (ticks/16=us)．
- `DR_REG_SYSTIMER_BASE = 0x500E2000`．
- ソース: `ETS_SYSTIMER_TARGET0_INTR_SOURCE` (core0)，`ETS_SYSTIMER_TARGET1_INTR_SOURCE` (core1) (`interrupts.h:72-74`)．

### 3.2 FMP3 でのタイマ選択

FMP3 は **CLINT mtime を独自実装可** (IDFと独立)．実機既知値: mtimecmp=`0x2000_4000`，mtime=`0x2000_BFF8`，CPUクロック~360MHz，内部CLIC線7．ただし内部線7 の有効化/SHV設定の可否は **要確認** (IDFは内部線を使用しないため未検証)．

---

## 4. 割込み入口/出口 (trap entry/exit)

ファイル: `components/riscv/vectors_clic.S`, `components/riscv/vectors.S`, `components/freertos/FreeRTOS-Kernel/portable/riscv/portasm.S`

### 4.1 ベクタテーブル構成

- `_vector_table` (mtvec が指す，`.balign 0x40`): `j _panic_handler` 1命令 = 例外/非ベクタ(SHV=0)割込みの入口．
- `_mtvt_table` (mtvt(0x307) が指す，64境界，48ワードエントリ):
  - 線 0-15 (内部) → `_system_int_handler` (=`_panic_handler`)．
  - 線 16-39 / 45-47 (外部) → `_interrupt_handler`．
  - 線 40=T1WDT，41=CACHEERR → `_panic_handler`．線 42=`MEMPROT_ISR`，43=`ASTDBG_ISR`，44=`IPC_ISR_HANDLER`．

### 4.2 コンテキスト退避

`vectors.S:376-491` (`save_general_regs`):
- `CONTEXT_SIZE = 32*4 = 128` バイト確保．ra, tp, t0-t6, s0-s11, a0-a7 を退避 (gp/sp は別途)．
- 退避前sp = `sp+CONTEXT_SIZE` を `RV_STK_SP` に格納．`mepc` を `save_xepc` で退避．
- **FPレジスタは毎IRQ退避しない**．遅延退避 — 初回FPU使用時の Illegal-Instruction 例外 (`EXC_ILLEGAL_INSTRUCTION=0x2`) でオンデマンド退避．

### 4.3 アクティブ割込みIDの取得

CLICレジスタではなく **mcause** から読む (`vectors.S:410,451-456`):
- `mcause & VECTORS_MCAUSE_REASON_MASK` = 割込みID (CLIC線番号，+16込み)．
- `_global_interrupt_handler(sp, mcause)` が `s_intr_handlers[core][mcause-16]` を引く (`interrupt.c:59-69`)．
- `mcause >= 16` をアサート ("割込みソースは内部割込みにマップ禁止")．

### 4.4 ネスト割込みとレベル復元

- `SOC_INT_HW_NESTED_SUPPORTED=1` のため ソフトウェア閾値引上げブロックはコンパイル除外．
- CLIC HW が trap時に旧レベルを mcause のレベルフィールドに退避し `mintstatus.mil` を取得割込みレベルに上げる．
- 出口で `mcause` を復元 (`vectors.S:484`)，`mret` で HW が旧レベルを `mintstatus` に戻す．**手動の閾値書込は不要**．
- **FMP3 含意**: RISC-V依存 trap コードは **mcause を保存・復元** すること (ネストレベル正当性)．

### 4.5 mscratch

通常の CLIC trap 経路では未使用．割込みスタック切替は `rtos_int_enter` 内で `uxInterruptNesting` 判定で行い，mscratch swap は使わない．

---

## 5. Core1 (APP CPU) 起動

ソース: `components/esp_system/port/cpu_start.c:277-302`, `components/hal/esp32p4/include/hal/cpu_utility_ll.h`

**起動順序**:

### (1) Core1 クロック有効化 (`cpu_utility_ll.h:74-76`)

```c
REG_SET_BIT(0x500E6014, BIT(4));  // HP_SYS_CLKRST_SOC_CLK_CTRL0_REG, HP_SYS_CLKRST_REG_CORE1_CPU_CLK_EN
```

### (2) Core1 リセット解除 (`cpu_utility_ll.h:77-79`)

```c
REG_CLR_BIT(0x500E60C0, BIT(8));  // HP_SYS_CLKRST_HP_RST_EN0_REG, HP_SYS_CLKRST_REG_RST_EN_CORE1_GLOBAL
```

注: default=1 (リセット保持)．ビットのクリアでリセット解除．停止時は逆操作 (CLK_EN クリア + RST_EN セット)．

### (3) ブートアドレス設定

`ets_set_appcpu_boot_addr((uint32_t)entry)` — ROM関数 (アドレス `0x4fc000a8`, `esp32p4.rom.ld:57`)．

### (4) PMU ストール解除 (`cpu_utility_ll.h:44-53`)

```c
HAL_FORCE_MODIFY_U32_REG_FIELD(PMU.cpu_sw_stall, hpcore1_stall_code, 0xFF);  // unstall=0xFF (stall=0x86)
while (REG_GET_BIT(HP_SYSTEM_CPU_CORESTALLED_ST_REG, HP_SYSTEM_REG_CORE1_CORESTALLED_ST));
```

- PMU base: `DR_REG_PMU_BASE = 0x50115000`．
- ソフトリセット別経路: `LP_AON_CLKRST.hpcpu_reset_ctrl0.hpcore1_sw_reset`．

---

## 6. コア間割込み (IPI)

### 6.1 IDFの選択: HP_SYSTEM FROM_CPU レジスタ (CLINT msip は不使用)

IDF は CLINT msip(0x2000_0000) を IPI に使わない．**HP_SYSTEM の `CPU_INT_FROM_CPU_x_REG`** にビットを書き，`ETS_FROM_CPU_INTRx_SOURCE` をマトリクス経由で CLIC 外部線へルーティング．

### 6.2 FROM_CPU レジスタ一覧

ソース: `components/soc/esp32p4/register/soc/hp_system_reg.h`, base=`DR_REG_HP_SYS_BASE=0x500E5000`

| レジスタ | アドレス | ビット | 用途 |
|---|---|---|---|
| `HP_SYSTEM_CPU_INT_FROM_CPU_0_REG` | 0x500E5010 | BIT(0) | core0宛 yield IPI (crosscore_int) |
| `HP_SYSTEM_CPU_INT_FROM_CPU_1_REG` | 0x500E5014 | BIT(0) | core1宛 yield IPI (crosscore_int) |
| `HP_SYSTEM_CPU_INT_FROM_CPU_2_REG` | 0x500E5018 | BIT(0) | core0宛 IPC IPI (esp_ipc_isr) |
| `HP_SYSTEM_CPU_INT_FROM_CPU_3_REG` | 0x500E501C | BIT(0) | core1宛 IPC IPI (esp_ipc_isr) |

1書込でトリガ，0書込でクリア (レベル割込み，レジスタ値が pending を表す)．

### 6.3 IDF の2系統

- **crosscore_int** (yield): ソース `SYS_CPU_INTR_FROM_CPU_0/1_SOURCE`，CLIC線は `esp_intr_alloc` による動的割当．
- **esp_ipc_isr**: ソース `SYS_CPU_INTR_FROM_CPU_2/3_SOURCE` を **固定 CLIC線 ETS_IPC_ISR_INUM=28** に静的ルーティング (`soc.h:242`)．

### 6.4 FMP3 での IPI 選択

**HP_SYSTEM FROM_CPU 方式** (IDFと整合的): 外部線で動的/固定割当可能．
**CLINT msip 方式**: msip=`0x2000_0000`，他コア=`+0x1_0000`，内部CLIC線3．mtvt[3] にハンドラ設置が必要．内部線3 の有効化方法は **要確認** (IDFは内部線未使用)．

---

## 7. FMP3 移植への含意まとめ

1. **CLIC は非標準**: threshold はメモリマップド `CLIC_INT_THRESH_REG`(0x20800008, [31:24])，`mintstatus`=CSR 0x346 (0xFB1ではない)，`mintthresh` CSRなし．
2. **閾値書込は遅延反映**: 書込後 read-back か約8 nop が必要 (`interrupt_clic.h:196-201`)．FMP3 の `t_unlock_cpu`/`chg_ipm` 経路で必須．
3. **CLIC マスクは inclusive**: `NLBITS=3`，レベルは [31:24] に上位寄せ，下位5ビットは1固定．FMP3 IPM をこれに合わせる．
4. **レベル退避/復元は HW自動** (`mcause`↔`mintstatus`，`mret` 経由)．FMP3 trap コードは **mcause を保存・復元** すること．手動の閾値書込は不要．
5. **mtvec MODE=3 固定** (実機 `0x4ff00003`)，mtvt(0x307) は64境界の48エントリ表．per-IRQ HWベクタは `CLIC_INT_ATTR_SHV`(bit16)．
6. **割込みルーティング** (ROM非依存推奨): `*(uint32_t*)(0x500D6000 + 4*source) = (line + 16)` (core0)，base=0x500D6800 (core1)．
7. **CLIC per-IRQ設定**: `0x20801000 + 4*(line+16)` で IE(bit8)/priority(`level<<5`, [31:24])/trig([18:17])．他コアCLICは base+0x10000 (**自コアからのアクセス可否は要確認**)．
8. **外部線6 (=CLIC線22)** はIDFが切断用予約．FMP3 独自実装でも衝突回避のため踏襲推奨．
9. **Tick**: IDFはSYSTIMER(16MHz，周期，base=0x500E2000)使用．FMP3 は CLINT mtime(CPUクロック~360MHz，内部線7)を独自実装可 (IDFと独立)．
10. **Core1起動**: `0x500E6014 bit4 set` → `0x500E60C0 bit8 clr` → ROM `ets_set_appcpu_boot_addr` → PMU unstall(stall_code 0x86→0xFF) → ストール解除ポーリング．
11. **IPI**: IDFはHP_SYSTEM FROM_CPU(0x500E5010-0x501C, BIT0)．FMP3 は CLINT msip(内部線3)も選択可．

---

## 8. 要確認事項

- ROM `intr_matrix_set`/`esprv_intc_int_*` が内部で +16 するか (HAL版は明示+16)．
- ROM `ets_set_appcpu_boot_addr` の書込先レジスタ実体．
- 内部CLIC線 (mtime=7，msip=3) を IDF以外で使う際の有効化/SHV設定の可否 (IDFは未使用のため未検証)．
- 自コアから他コアCLIC (base+0x10000) へのアクセス可否．
