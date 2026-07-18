# ESP32-P4 HW リファレンス（FMP3 移植のための割込み/タイマ/CPU 調査）

> **本書の位置づけ**: 本書は設計・調査の記録であり，利用手順書ではない．利用者向けの
> 入口は `chip_user.md`（`arch/riscv_gcc/esp32p4/`）および `target_user.md`
> （`target/m5stamp_esp32p4_gcc/`）を参照．本書は下記「調査日: 2026-06-27」時点の記録
> であり，現在の実装と異なる場合がある．

対象: ESP32-P4 HP コア（RV32IMAFC デュアルコア, mhartid 0/1）
調査日: 2026-06-27

## 出典
- ESP32-P4 Technical Reference Manual, Chip Revision v1.3（PDF, Espressif 公式）
  https://documentation.espressif.com/esp32-p4-chip-revision-v1.3_technical_reference_manual_en.pdf
  （HTML 版: https://documentation.espressif.com/esp32-p4-chip-revision-v1.3_technical_reference_manual_en.html ）
- ESP-IDF master ソース（レジスタ定義・起動手順の裏取り）
  - `components/soc/esp32p4/include/soc/clic_reg.h`
  - `components/soc/esp32p4/include/soc/interrupt_reg.h`
  - `components/soc/esp32p4/register/hw_ver1/soc/reg_base.h`
  - `components/soc/esp32p4/register/hw_ver1/soc/interrupt_core0_reg.h`
  - `components/soc/esp32p4/register/hw_ver1/soc/uart_reg.h`
  - `components/soc/esp32p4/include/soc/interrupts.h`, `system_intr.h`
  - `components/hal/esp32p4/include/hal/cpu_utility_ll.h`
  - `components/esp_system/port/cpu_start.c`
- ESP-IDF Interrupt Allocation (ESP32-P4):
  https://docs.espressif.com/projects/esp-idf/en/stable/esp32p4/api-reference/system/intr_alloc.html

## 重要な結論（FMP3 設計への影響）
- **ESP32-P4 は CLIC モード専用**．`mtvec.MODE` は 0x3 にハードワイヤされており，
  CLINT 割込みモード（標準 RISC-V のベクタ/非ベクタモード）は使えない．
  `mie`/`mip`/`mideleg`/`uie`/`uip` 等の CSR は無効（0 ハードワイヤ）．マシンモード割込みのみ．
  （TRM §1.9.2.1, §1.9.1）
- **標準 PLIC は無い**．外部割込みは「Espressif 割込みマトリクス(Interrupt Matrix)」で
  各周辺ソースをコアの CLIC 割込み線へルーティングする（TRM Ch.12）．PLIC 相当の役割はこれ．
- **CLINT(mtime/mtimecmp/msip) は存在する**（TRM §1.8, §1.9.3）．FMP3 のタイマ(mtime)と
  IPI(msip) はこの CLINT で実現可能．外部割込みは CLIC＋割込みマトリクスに置換が必要．

---

## 1. CLINT（mtime / mtimecmp / msip）

CLINT/CLIC レジスタ領域は CPU サブシステム `0x2000_0000–0x2FFF_FFFF`（TRM Table 1.4-1）．
アドレスデコード（TRM Table 1.8-1, 1.8-2, §1.8.3）:

| ブロック | 開始アドレス | 終了アドレス |
|---|---|---|
| CLINT (自コア) | `0x2000_0000` | `0x2000_FFFF` |
| CLINT (他コア) | `0x2001_0000` | `0x2001_FFFF` |
| CLIC  (自コア) | `0x2080_0000` | `0x2080_FFFF` |
| CLIC  (他コア) | `0x2081_0000` | `0x2081_FFFF` |
| CLIC user      | `0x20B0_0000` | （submodule 0xB） |

- アドレスビット 27:20 = サブモジュール（0x0=CLINT, 0x08=CLIC, 0xB=CLIC user）
- アドレスビット 19:16 = アクセス種別（0x0=自コア, 0x1=他コア）→ 他コアは +0x1_0000

### CLINT レジスタ（自コア基準．offset は CLINT 自コアベース `0x2000_0000` からの相対）
出典: TRM §1.9.3.5 / §1.9.3.6（Reg 1.89–1.96）

| レジスタ | オフセット | 自コア絶対アドレス | アクセス | 説明 |
|---|---|---|---|---|
| msip       | `0x0000` | `0x2000_0000` | R/W | マシンソフトウェア割込みペンディング(IPI) |
| mtimecmplo | `0x4000` | `0x2000_4000` | R/W | mtimecmp 下位32bit |
| mtimecmphi | `0x4004` | `0x2000_4004` | R/W | mtimecmp 上位32bit |
| mtimeloadlo| `0x4008` | `0x2000_4008` | R/W | mtime ロード値 下位32bit |
| mtimeloadhi| `0x400C` | `0x2000_400C` | R/W | mtime ロード値 上位32bit |
| mtimectl   | `0x4010` | `0x2000_4010` | R/W | タイマ制御(MTIME_EN/OVF/SAM) |
| mtimelo    | `0xBFF8` | `0x2000_BFF8` | RO  | mtime カウンタ 下位32bit |
| mtimehi    | `0xBFFC` | `0x2000_BFFC` | RO  | mtime カウンタ 上位32bit |

- 他コアの同レジスタは上記 + `0x1_0000`（例: 他コア msip = `0x2001_0000`）．
- **mtime は 64bit でコア間共有**．タイマの有効化(MTIME_EN)/値変更は **Core0 の mtimectl のみ有効**．
  Core1 は Core0 の CLINT 空間(`0x2001_xxxx`)経由で操作可（TRM §1.8.2, Reg 1.94 注記, §1.9.3）．
- **割込み配線**: msip → CLIC 割込み番号 **3**（ソフト割込み），mtime 一致 → CLIC 割込み番号 **7**（タイマ割込み）．
  有効化は `clicintie[3]` / `clicintie[7]`，ペンディングは `clicintip[3]` / `clicintip[7]`（TRM §1.9.3.3, §1.9.3.4）．
- **mtime クロックソース・周波数: 未確認**（要実機計測）．
  参考: SYSTIMER は CNT_CLK ≈ 16 MHz（XTAL 40MHz / 2.5，1/16µs 刻み, TRM §15.5）．

---

## 2. 割込み方式（CLIC）

- **CLIC モード固定**（CLINT モード非対応）．`mtvec.MODE` = 0x3 ハードワイヤ（TRM §1.9.2.1）．
- 各 HP コアの CLIC は **外部割込み 32 本 + CLINT 割込み 2 本**（timer/soft）をサポート（TRM §1.9.1）．
- RISC-V Privileged CLIC 提案 Version 0.8 準拠（TRM §1.9.2.1）．

### 関連 CSR（TRM §1.9.2.2）
| CSR | アドレス | 備考 |
|---|---|---|
| mtvec | `0x305` | 例外/割込みベースアドレス（MODE は 0x3 固定） |
| mtvt  | `0x307` | CLIC ベクタテーブルベースアドレス |
| mintstatus | `0x346` | 現在の割込みレベル（rev<3 では非標準位置 0x346，標準は 0xFB1） |
| mclicbase  | `0x350` | CLIC ベースアドレス（カスタム CSR） |
| mie/mip/mideleg | `0x304`/`0x344`/`0x303` | **CLIC モードでは無効（0 ハードワイヤ）** |

### CLIC メモリマップレジスタ（自コア．base `0x2080_0000`，control `0x2080_1000`）
出典: TRM §1.9.2.6/§1.9.2.7, `clic_reg.h`
- `DR_REG_CLIC_BASE = 0x2080_0000`, `DR_REG_CLIC_CTRL_BASE = 0x2080_1000`
- 他コアオフセット `DUALCORE_CLIC_CTRL_OFF = 0x1_0000`
- グローバル: `CLIC_INT_CONFIG_REG = 0x2080_0000`（nlbits 設定），`CLIC_INT_INFO_REG = 0x2080_0004`，
  `CLIC_INT_THRESH_REG = 0x2080_0008`（マシンモード割込み閾値，ビット31:24）
- 各割込み i の制御（32bit，`0x2080_1000 + 4*i`）:
  - `clicintip[i]`  = `0x2080_1000 + 4*i`（bit0，ペンディング）
  - `clicintie[i]`  = `0x2080_1001 + 4*i`（bit0，イネーブル）
  - `clicintattr[i]`= `0x2080_1002 + 4*i`（SHV=bit0 ベクタ化，TRIG=bit2:1 level/edge，MODE=bit7:6）
  - `clicintctl[i]` = `0x2080_1003 + 4*i`（優先度，上位ビット）
- 優先度: `NLBITS = 3`（レベル/優先度エンコード，TRM Fig.1.9-2）．実効 8 段．
- `CLIC_EXT_INTR_NUM_OFFSET = 16`（外部割込みは CLIC 番号 16 以降．0–15 はコアローカル等で予約）．

---

## 3. 割込みマトリクス（Interrupt Matrix / INTMTX）— PLIC 相当

出典: TRM Ch.12「Interrupt Matrix」（§12.3, §12.5.1/§12.5.2, §12.6.1/§12.6.2），
`interrupt_core0_reg.h`, `reg_base.h`

- 役割: 任意の周辺割込みソースを各コアの CLIC 外部割込み線(スロット)へマップする（PLIC 相当）．
  複数ソースを 1 スロットへまとめることも可能．
- ベースアドレス:
  - `DR_REG_INTR_BASE`(INTERRUPT_CORE0) = `0x500D_6000`（HPPERIPH1 `0x500C_0000` + `0x16000`）
  - `DR_REG_INTERRUPT_CORE1_BASE` = `0x500D_6800`（CORE0 + 0x800）
- 構成: **ソースごとに 1 本の MAP レジスタ**（CORE0/CORE1 それぞれに同じ並びを持つ）．
  4 バイトごとに各ソースが並ぶ（例: `INTERRUPT_CORE0_LP_RTC_INT_MAP_REG = 0x500D_6000 + 0x0`）．
- MAP レジスタの値: フィールド `[5:0]`（6bit）に **割り付け先 CPU(CLIC) 割込み番号** を書く．
  0 を書くと無効（`ETS_INVALID_INUM = 0`）．有効な外部割込みは CLIC 番号 16 以降．
- ソース定義: `soc/interrupts.h` の `ETS_*_INTR_SOURCE`（番号 0 = LP_RTC，計 130+ 本，
  `ETS_MAX_INTR_SOURCE` 参照）．
- IPI 用ソース: `ETS_FROM_CPU_INTR0..3_SOURCE`（`system_intr.h` の `SYS_CPU_INTR_FROM_CPU_0..3`）．
  ESP-IDF が使うコア間割込みの代替経路（CLINT msip を使わない）．

---

## 4. 2 つ目の HP コア(Core1)の起動

出典: `cpu_utility_ll.h`, `cpu_start.c`, TRM（HP_SYS_CLKRST / LP_AON_CLKRST / PMU 章）

- **mhartid**: Core0 = `0x0`，Core1 = `0x1`．
- 起動シーケンス（`call_start_cpu1` が Core1 の C エントリ）:
  1. **クロック有効化**: `HP_SYS_CLKRST_SOC_CLK_CTRL0_REG` の `CORE1_CPU_CLK_EN` を 1．
     （HP_SYS_CLKRST ベース = `0x500E_6000` = HPPERIPH1 + 0x26000）
  2. **リセット解除（グローバル）**: `HP_SYS_CLKRST_HP_RST_EN0_REG` の
     `RST_EN_CORE1_GLOBAL` を 0 にクリア．
  3. **SW リセット**: `LP_AON_CLKRST.hpcpu_reset_ctrl0.hpcore1_sw_reset`（必要時）．
  4. **エントリアドレス設定**: ROM API **`ets_set_appcpu_boot_addr((uint32_t)entry)`** で
     Core1 の開始アドレスを設定．HW レジスタ名は ROM 内部実装のため未確認．
  5. **アンストール**: `PMU.cpu_sw_stall` の `hpcore1_stall_code` に **0xFF**（=unstall．0x86=stall）．
     完了確認は `HP_SYSTEM_CPU_CORESTALLED_ST_REG` の `CORE1_CORESTALLED_ST` ビット．
- 結論: Core1 起動には ROM API `ets_set_appcpu_boot_addr` 呼び出しが確実．

---

## 5. UART（コンソール用最小構成）

出典: `reg_base.h`, `uart_reg.h`, TRM Ch.（UART, GPIO Matrix, Boot）

- ベースアドレス（stride 0x1000）:
  - `UART0 = 0x500C_A000`，`UART1 = 0x500C_B000`，`UART2 = 0x500C_C000`（HPPERIPH1 + 0xA000〜）
  - LP_UART = `0x5012_1000`
- 主要レジスタ（オフセットは各 UART ベース基準，`uart_reg.h`）:

| レジスタ | オフセット | 用途 |
|---|---|---|
| UART_FIFO_REG | `0x0` | TX/RX FIFO データ（書込=送信） |
| UART_INT_RAW / ST / ENA / CLR | `0x4/0x8/0xC/0x10` | 割込み |
| UART_CLKDIV_SYNC_REG | `0x14` | ボーレート分周（整数12bit + 小数 `UART_CLKDIV`=12bit幅） |
| UART_STATUS_REG | `0x1C` | `UART_TXFIFO_CNT`(bit23:16)=TX FIFO 残量，RXFIFO_CNT 等 |
| UART_CONF0_SYNC_REG | `0x20` | 基本設定（データ長/パリティ/ストップ） |
| UART_CONF1_REG | `0x24` | FIFO しきい値等 |
| UART_CLK_CONF_REG | `0x88` | クロック源選択/分周イネーブル |

- ボーレート: `baud = source_clk / (CLKDIV + CLKDIV_FRAG/16)`．クロック源は `UART_CLK_CONF_REG` で選択
  （XTAL 40MHz / PLL 等）．送信は FIFO に書く前に `UART_TXFIFO_CNT < FIFO 段数` を確認．
- **デフォルトコンソール**: **UART0**．デフォルトピンは **U0TXD=GPIO37，U0RXD=GPIO38**
  （TRM IO MUX 表: GPIO37=UART0_TXD_PAD，GPIO38=UART0_RXD_PAD）．
- 注意: UART 使用前に対象 UART のクロック有効化・リセット解除（HP_SYS_CLKRST 系）と
  GPIO Matrix での信号アサインが必要（GPIO37/38 はデフォルトで UART0 にマップ済み）．

---

## 付録: 主なベースアドレス早見表
| 名称 | アドレス | 由来 |
|---|---|---|
| HPPERIPH0 | `0x5000_0000` | reg_base.h |
| HPPERIPH1 | `0x500C_0000` | reg_base.h |
| LPPERIPH  | `0x5012_0000` | reg_base.h |
| CLINT 自コア | `0x2000_0000` | TRM Table 1.8-2 |
| CLINT 他コア | `0x2001_0000` | TRM Table 1.8-2 |
| CLIC 自コア  | `0x2080_0000` | TRM / clic_reg.h |
| CLIC ctrl 自コア | `0x2080_1000` | clic_reg.h |
| CLIC 他コア  | `0x2081_0000` | TRM Table 1.8-2 |
| INTMTX CORE0 | `0x500D_6000` | reg_base.h |
| INTMTX CORE1 | `0x500D_6800` | reg_base.h |
| UART0 | `0x500C_A000` | reg_base.h |
| SYSTIMER | `0x500E_2000` | reg_base.h |
| HP_SYS_CLKRST | `0x500E_6000` | reg_base.h |

## 未確認事項（要追加調査）
1. CLINT mtime のクロックソースと周波数（SYSTIMER の 16MHz と同一カウンタか否か）．
2. `ets_set_appcpu_boot_addr` が書き込む実 HW レジスタ（Core1 ブートベクタ）名/アドレス．
3. UART のデフォルトボーレート（ブートログ）と CLK_CONF の既定値．
