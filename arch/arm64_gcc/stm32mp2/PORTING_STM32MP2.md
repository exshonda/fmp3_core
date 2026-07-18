# TOPPERS/FMP3 → STM32MP257F-DK 移植メモ

最終更新: 2026-06-03

## 目的
TOPPERS/FMP3 (Release 3.3.1) を i.MX8MM EVK 版を基に **STM32MP257F-DK**
(Cortex-A35 ×2 / GICv2(GIC-400) / ARM generic timer) へ移植する。
- StepS : シングルコア実行
- StepM : マルチコア実行

## 成果サマリ (StepS) — **完了 (2026-06-03)**

**ビルド成功 + 実機でシングルコア完全動作**。FSBL→EL3→セキュアEL1→MMU/cache→
カーネル起動→LOGTASK/タスク/タイマtick/周期ハンドラが連続動作することを確認
(sample1 の TASK1 出力が 197 行連続・カウンタ増加)。

実機 UART(USART2/ttyACM0) 出力:
```
MP2
TOPPERS/FMP3 Kernel Release 3.3.1 for STM32MP257F-DK CA35(AArch64 Secure) ...
Processor 1 start.
local_inirtn exinf = 1, counter = 1
System logging task is started on port 1.
Server task 1 starts.
Sample program starts (exinf = 1).
TASK1_1 is running on prc1 (001) .   |
TASK1_1 is running on prc1 (002) .   |
  ... (連続)
```

## 成果サマリ (StepM = マルチコア SMP) — **完了 (2026-06-03)**

`PRC_NUM=2`(obj Makefile)で 2 コア SMP 動作を実機確認。Core1(a35_1)は STM32MP2 に
BL31/PSCI が無いため **EL3 直接起動**で立ち上げる:
- `chip_kernel_impl.c` の `chip_mprc_initialize`(マスタのみ・bss クリア後に呼ばれる)で、
  `CA35SYSCFG.VBAR_CR`(0x48802084, bits[31:2]=リセットベクタ)に start.S のエントリ
  (0x88001000)を設定し、`RCC.C1P1RSTCSETR`(0x44200408)の C1P1RST(bit1)で Core1 の
  リセットを解除。Core1 は EL3 から start.S 実行→start_el1 の my_core_index 判定で
  slave_wait→マスタの start_sync を待って sta_ker へ合流。
- レジスタ定義は `stm32mp2.h`。両レジスタは MMU 周辺マップ(0x40000000-0x50000000)内で
  セキュア EL1 からアクセス可(chip_mprc_initialize は EL1 動作)。
- OpenOCD は **`-c "set EN_CA35_1 0"`** で a35_1 を examine させずに起動すること。

実機出力(抜粋):
```
Processor 1 start.
Processor 2 start.
TASK1_1 is running on prc1 (048) .   |
TASK2_1 is running on prc2 (048) .   |   (両コア並行・カウンタ同期で連続動作)
```

## 追加・変更したファイル

### chip 依存部 `arch/arm64_gcc/stm32mp2/` (imx8mm を複製して改変)
- `stm32mp2.h` — SoC 定義。GICv2(`TOPPERS_GIC_VER=2`), GICD=0x4AC10000,
  GICC=0x4AC20000, タイマ PPI(SPHY=29/NSPHY=30), USART2=0x400E0000(IRQ147),
  USART6=0x40220000(IRQ168), RCC=0x44200000, GIC_TNUM_INTNO=416。
- `stm32usart.c/.h` — STM32 USART 用 SIO ドライバ（imx8uart を置換）。
  **TF-A が 115200 8N1 で初期化済みのため BRR 等は再設定せず TE/RE/UE のみ有効化**。
- `chip_kernel_impl.c` — EL3/EL2/mprc 初期化。GICv2 では ICC_SRE を触らない
  (`#if TOPPERS_GIC_VER>=3` でガード)。**CPTR_EL3 のトラップ解除を追加**(後述)。
  `chip_mprc_initialize` のサブコア起動は単核では消える(`#if TNUM_PRCID>=2`)。
- `chip_serial.c/.cfg/.h`, `chip_kernel.h/_impl.h`, `chip_timer.*`,
  `chip_stddef.h`, `chip_rename*`, `chip_kernel.trb`(global_intno を 32..415 に拡張),
  `Makefile.chip`(`-mcpu=cortex-a35`, `-DTOPPERS_CORTEX_A35`), `MANIFEST`。

### target 依存部 `target/stm32mp257f_dk_arm64_gcc/` (imx8mm_evk を複製して改変)
- `Makefile.target` — SYS=stm32mp257f_dk, CHIP=stm32mp2。`TOPPERS_TZ_S`(ATF不使用、
  `TOPPERS_WITH_ATF`/`SYSMON` は未定義)。**TEXT=0x88001000 / DATA=0x90000000**(後述)。
  GCC_TARGET=aarch64-none-elf。
- `target_kernel_impl.c` — `target_mmu_init` を STM32MP2 用に書換え:
  周辺 0x40000000-0x50000000(Device, USART/RCC/GIC 含む) + DDR 0x80000000-0xC0000000
  (Normal cacheable, text/data 含む)。UART pinmux(`sio_port_init`) は USE_UART2/6 では空。
- `stm32mp257f_dk.h`(DDR_ADDR=0x80000000/DDR_SIZE=0x40000000),
  `target_syssvc.h`(SIO base/INTNO を USART2/6 に, TARGET_NAME),
  `target_kernel_impl.h`(board ヘッダ include 修正), `stm32mp257f_dk.ld`(imx8mm.ld 流用),
  `target_stddef.h`, `MANIFEST` 等。

### 共通部 `arch/arm64_gcc/common/` の変更（★shared コードの改変。upstream 取り込み時に要注意）

本移植では、特定チップに依存しない共通部(`arch/arm64_gcc/common/`)にも最小限の変更を加えた。
いずれも **既存ターゲット(imx8mm 等)の挙動を壊さない**ように条件付き／GICv2 パス限定にしてある。

1. **`core_asm.inc` (L72) / `core_sil.h` (L126) / `arm64.h` (L645)** — MPIDR からコア index を
   取り出す `#if defined(TOPPERS_CORTEX_A53) || defined(TOPPERS_CORTEX_A57)` の条件に
   **`|| defined(TOPPERS_CORTEX_A35)`** を追加。Cortex-A35 も A53 同様 AFF0 マスクでコア index を
   得るため(追加のみ、A53/A57 の挙動は不変)。`-DTOPPERS_CORTEX_A35` は `Makefile.chip` で定義。

2. **`chip_kernel.h` の `DAIF_CPULOCK`** — TZ_S 時に **`DAIF_F_BIT`(FIQ マスク)** にする(zynqmp と同じ)。
   セキュア(Group0)割込みは **FIQ で配送**(`gicc_init` は `GICC_CTLR=FIQEN|ENABLEGRP0` のまま)され、
   CPU ロックを FIQ マスクにすることで割込みハンドラ前半の `daifclr` が FIQ を保持し、暴走再入を防ぐ。
   これがネイティブな GICv2+Secure 構成。
   （※2026/06/14 訂正前は、CPU ロックを IRQ マスクのままにして `gicc_init` を `ENABLEGRP0`(FIQEN無し=IRQ配送)
   に変える `GIC_NO_FIQ_IN_SECURE` 方式だった。配送とロックの不整合が真因だったため、CPUロックを FIQ に
   揃える方式へ統一し、`GIC_NO_FIQ_IN_SECURE` は不要として削除。実機 STM32MP257F-DK で実証。下記「★解決済み」参照。）

### ビルド構成 `obj/obj_stm32mp2/`
- `obj_imx8mm/Makefile` を複製し `TARGET=stm32mp257f_dk_arm64_gcc`, `PRC_NUM=1`,
  `imx8uart.o`→`stm32usart.o` に変更。`objs/` `gen/` を作成して `make`。
- 生成物: `fmp`(ELF), `main.bin`(~174KB)。警告 2 件(`.bss type PROGBITS` /
  `RWX segment`)は上流と同じで無害。

## 実機での確立事項 / ハマりどころ

### ビルド
```
cd fmp3_3.3/obj/obj_stm32mp2 && mkdir -p objs gen && make
```
ツールチェイン aarch64-none-elf-gcc 14.3 で警告なくビルド可。

### SWD ロード手順 (debug.md の方式)
1. openocd 起動(別ディレクトリ): `cd stm32mp2-baremetal; openocd -c "set EN_CA35_1 0" -f openocd.cfg`
2. **クリーンな minimal_boot(MP2) 状態に戻すのが必須**。前アプリ(multicore_smp 等)が
   キャッシュ/MMU 有効で走っている状態でロードするとコヒーレンシが崩れる。
   openocd telnet(4444) で `reset run` → FSBL が走り LPDDR4 初期化 → minimal_boot が
   "MP2" 表示でハング(**キャッシュ OFF** の状態)。
3. telnet で:
   `stm32mp25x.a35_0 arp_examine; arp_halt; load_image <…>/fmp; reg pc 0x88001000; resume`
   - gdb の `monitor` 経由は gdb-attach の "FSBL wrapper" ハンドシェイク
     (cfg L131-160)が当該 FSBL に無く失敗するため、**telnet 直叩きが確実**。

### ★重要な発見 1: 先頭ページは SWD 実行不可
- FSBL は FIP ペイロードを 0x88000000 にロードし **0x88000000 へ entry**。
  minimal_boot 実体は 0x88000000-0x88000040 にあり、ここで spin。
- 0x88000040(先頭ページ内)に pc をセットして resume すると **即例外(EC=0)**。
  一方 0x88001000(2ページ目)は正常実行。baremetal の従来アプリも _Reset を
  0x88001xxx に置いて先頭ページからの実行を避けていた。
- 対策: **TEXT_START_ADDRESS=0x88001000**(start を 2 ページ目に配置)。

### ★重要な発見 2: CPTR_EL3 トラップ解除が必須
- TF-A は CPTR_EL3 の TCPAC/TAM/TTA/TFP をセットした状態で BL31 に制御を渡す。
  この状態で EL1 へドロップすると start.S の `mrs/msr cpacr_el1`(FPU 有効化)が
  **EL3 へトラップ(EC=0x18)** する。
- 対策: `chip_el3_initialize` で CPTR_EL3 の該当ビットをクリア(実装済み)。
  これで EL1 まで降りてカーネルが起動しバナー表示まで到達するようになった。

## ★解決済み: タイマ FIQ ライブロック (StepS の最後の壁)

**根本原因**: GICv2 セキュア構成で**セキュア(Group0)割込みが FIQ で配送**されていた
(`gicc_init` が `GICC_CTLR=FIQEN|ENABLEGRP0`)。一方カーネルの CPU ロックは
**`DAIF_I_BIT`(IRQ)**で実現されており、割込みハンドラ(core_support.S:400)は
ハンドラ中で `daifclr` により**FIQ を再許可しつつ IRQ のみマスク継続**する。
そのためタイマ FIQ を **GIC で ack(GICC_IAR読み)する前に**ハンドラが FIQ を再許可し、
まだ pending の同一タイマ FIQ が即座に再入 → 文脈退避を繰り返し SP が DDR 下限を
割り込んで暴走、というライブロック/再帰になっていた。
(imx8mm は GICv3 で管理割込みを IRQ で受けるため顕在化しない。)

実機 GIC 状態で確定: `GICC_CTLR=0x9`(FIQEN+Grp0), `IGROUPR0` bit29=0(PPI29=Group0),
`ISPENDR0` bit29=1(pending) かつ `ISACTIVER0=0`(未ack), `GICC_RPR=0xFF`。

**修正（2026/06/14 訂正・確定版）**: 根本原因は「配送(FIQ)と CPUロック(IRQ)の不整合」なので、
不整合を解消する方法は2つある。**ネイティブな Secure 構成に揃える方を採用**した：

- **採用＝FIQ 方式**: `chip_kernel.h` の TZ_S 時 `DAIF_CPULOCK=DAIF_F_BIT`(FIQ マスク)にする(zynqmp と同じ)。
  `gicc_init` は `GICC_CTLR=FIQEN|ENABLEGRP0`(FIQ 配送)のまま。割込みハンドラ前半の `daifclr` は
  CPUロック(=FIQ)を保持するので、ack 前の同一 FIQ 再入は起きない。
  → セキュア物理タイマ(PPI29)もディスパッチ要求SGIも FIQ ハンドラ(F マスク状態)で正しく処理される。
- 不採用＝IRQ 方式(旧): `gicc_init` を `ENABLEGRP0`(FIQEN無し=IRQ配送)にし `GIC_NO_FIQ_IN_SECURE` を定義、
  CPUロックは IRQ マスクのまま。これでも動くが、Secure で本来 FIQ のものをわざわざ IRQ に落とす回避策。

→ **FIQ 方式に統一**し `GIC_NO_FIQ_IN_SECURE` は削除。実機 STM32MP257F-DK で cpuexc/MP/機能テストが
全通過(mtrans2 のみ既知のディスパッチ飽和でHANG=方式に依らない)を確認。zynqmp(zcu102) と設計統一。
※ 当初は IRQ 方式で StepS を通したが、zynqmp の検証(2026/06/14)で FIQ 方式が正しく動くと分かり訂正した。

### 補足(初回の混乱の記録)
- バナー後に「出力が止まる/ハング」に見えたのは、デバッガの `monitor halt` 多用と
  tty キャプチャの競合による観測アーティファクト。非干渉のクリーン実行では連続動作する。
- 当初観測した SP=0x7fffffa0 / 各種 DFSC(alignment/translation) は、放置して暴走再帰が
  二次的にベクタ/コードを破壊した結果でありルート原因ではない(真因は上記 FIQ 再入)。

**確定した事実(クリーンに切り分け済み)**:
- TZ_S 構成では**セキュア割込みは FIQ で配送**される設計(`gicc_init` で
  `GICC_CTLR=FIQEN|ENABLEGRP0`)。`cur_spx_fiq_handler` は TZ_S では
  `b cur_spx_irq_handler`(core_support.S L804-805)で IRQ ハンドラへ合流する。
  → セキュア物理タイマ(PPI29)は **FIQ ベクタ(VBAR+0x300=0x88001b00)** に入る(正常)。
- 例外ベクタを `b .` にパッチして切り分けた結果、**初回非同期例外は FIQ**(SYNC/IRQ ではない)。
  発生時 **SP=0x900105a0(有効)・ELR_EL1=main_task のループ**で、スタック破壊や
  再帰はこの時点では無い。
- `timer_clock_mhz = 64`(= CNTFRQ_EL0 ≈ 64MHz, 0x90001ac8)。**0 ではない**ので
  tick 間隔のゼロ除算ではない。
- 全ベクタを正規に戻して resume すると、pc は**常に FIQ ベクタ 0x88001b00**に居て
  SP は有効のまま。main_task はほとんど進まず、コンソール無出力。
  → **タイマ割込み条件がクリアされず連続再発火**していると推測。
- (注: 以前観測した SP=0x7fffffa0 までの再帰・各種 DFSC は、放置して二次的に
  ベクタ/コードまで破壊された結果。ルート原因ではない。)

**ルート原因の仮説(次回最優先で確認)**:
1. **GICv2 の Group0/FIQ の EOI/Deactivate**。`GICC_EOIR` 書き込みのみで
   deactivate されない(`GICC_CTLR.EOImodeS`)等で、レベルtrigのタイマが
   pending のまま再発火。GICv2 のセキュアビュー(GICC_CTLR/AEOIR/DIR)を確認。
2. **タイマ ISR の再プログラム**。`core_timer` の ISR が次回 compare(CNTPS_CVAL)を
   設定して割込み条件をクリアしているか。FIQ 経路で ISR まで到達しているか
   (FIQ→IRQ 合流後の dispatch がタイマハンドラを正しく呼ぶか)。
3. ハンドラが FIQ ベクタから先へ進めない様子(単一ステップで pc が 0x88001b00 から
   動かない)も観測。openocd ステップの癖か、ハンドラ即再入かの切り分けが必要。

**有効だったデバッグ手法(再帰でメモリ破壊させずに初回例外を捕捉)**:
- ロード後に EL1 SPx ベクタを `b .`(0x14000000)へパッチ:
  `mww 0x88001a00 0x14000000`(sync) / `0x88001a80`(IRQ) / `0x88001b00`(FIQ) / `0x88001b80`(SError)。
  resume すると初回例外でそのベクタに spin して停止 → `ELR_EL1/ESR_EL1/FAR_EL1/SP` を
  破壊前に読める。1つだけ正規へ戻せば、そのハンドラの挙動だけを試験できる。
- 正規エンコーディング: sync=0x140011a9, IRQ=0x1400104b, FIQ=0x14001195, SError=0x14001176
  (要 `make` のたびに `objdump -d fmp` で再確認)。
- HW ブレークポイント(`bp <addr> 4 hw`)は例外処理中(PSTATE.D=1)は発火しない。
  ハンドラ内 daifclr 以降(IRQ=0x88005c40 / SYNC=0x880067e8)なら有効。
  BRP が枯渇したら **ボード電源 OFF/ON**(openocd 再起動・reset run では CPU debug
  レジスタの古い BP が残ることがある)。

## メモリマップ(現状)
- TEXT 0x88001000 / DATA(VMA) 0x90000000 / DATA LMA は text 直後(ROM)。
- start.S が .data を LMA→0x90000000 へコピー、.bss(0x90001000-)をクリア。
- MMU: 周辺 0x40000000-0x50000000(Device), DDR 0x80000000-0xC0000000(Normal cacheable)。

---

## fmp3_3.3_svn（arm64 共通部リファクタ版）への適用記録

本移植は元々 `fmp3_3.3` 向け。`fmp3_3.3_svn` は `arch/arm64_gcc/common` がリファクタ
され，プロセッサID取得などが**ターゲット依存部へ移設**されている。`stm32mp2`(チップ)と
`stm32mp257f_dk_arm64_gcc`(ターゲット)は _svn に存在しないため**新規追加**で衝突なし。
共通部は上書きせず，以下の差分のみをマージした。

### 共通部 `arch/arm64_gcc/common` への最小変更（2 ファイル）
1. **`arm64.h`** — `CPUECTLR_EL1_SMPEN` マクロと `enable_smp()` のガードに
   `|| defined(TOPPERS_CORTEX_A35)` を追加。_svn は CPUECTLR を A53/A57 限定にしていたが，
   Cortex-A35 も同一(SMPEN=bit6)。`chip_el3_initialize` が CPUECTLR を使うため必須。
2. **`arm64_tool.h`** — `CPUECTLR_EL1_READ/WRITE` マクロのガードに同様に A35 を追加。
3. **`gic_kernel_impl.c`** — `gicc_init`(GICv2) の TZ_S 経路を **`GIC_NO_FIQ_IN_SECURE`**
   マクロで分岐。定義時は `GICC_CTLR=ENABLEGRP0`(FIQEN を立てない＝IRQ 配送)，未定義時は
   従来どおり `FIQEN|ENABLEGRP0`。**stm32mp2 のみ** `Makefile.chip` で
   `-DGIC_NO_FIQ_IN_SECURE` を定義するので，同じ GICv2+TZ_S の zynqmp 系には影響しない。
   （`fmp3_3.3` では無条件だったが，_svn では zynqmp も GICv2+TZ_S のためマクロでスコープ化。）

### ターゲット依存部で新規提供（_svn が移設先として要求。imx8mm_evk と同一スキーム）
4. **`target_sil.h`** — `sil_get_pid()`（MPIDR の AFF0 でコア番号）。
5. **`target_kernel_impl.h`** — `get_my_prcidx()` / `conv_prcid_to_mpidr()` /
   `conv_prcid_to_gicdtarget()`。チップ依存部(→`gic_kernel_impl.h`)が使うため
   `#include "chip_kernel_impl.h"` より**前**で定義する。
6. **`target_asm.inc`（新規）** — アセンブラ版 `my_prcidx`。_svn の `start.S` /
   `gic_support.S` / `core_support.S` が `#include "target_asm.inc"` するため必須。
7. ~~**`chip_kernel_impl.h`** — `#define SMP_CACHE_BYTES 64`（_svn のロック変数の
   キャッシュライン整列に必要。A35 のラインは 64B）。~~
   → **2026/7/17 の変更で不要**。ロック変数をキャッシュライン長の配列として確保する
   方式に変更し，`SMP_CACHE_BYTES` は arm64 から削除した（キャッシュライン長は共通部の
   `ARM64_CACHELINE_SIZE`，既定 64）。新規移植では定義しないこと。
8. ~~**`stm32mp257f_dk.ld`** — `.bss` に `*.o(.bss..cacheline_aligned)`（64B 整列）を追加。~~
   → **2026/7/17 の変更で不要**。上記により型自身がサイズとアラインメントを保証する
   ため，リンカスクリプトでのセクション集約は不要（当該指定は削除済み）。

### 検証
- `aarch64-none-elf-gcc 14.3` / `PRC_NUM=2`(SMP) でビルド成功（`fmp` 生成，Entry=0x88001000）。
- ロック変数(`giant_lock`/`_kernel_start_sync`/`TOPPERS_sil_spn_var`)が 64B 境界に配置。
- `.bss type changed to PROGBITS` 警告は _svn の既存挙動（imx8mm でも同様）で無害。

---

## MMU 設定の静的テーブル化（2026-06-03）

arm 依存部（`arch/arm_gcc/common`）の `arm_memory_area[]` 方式と同等の I/F に揃えるため，
MMU 設定の渡し方を「`target_mmu_init()` が実行時に `mmu_mmap_add()` を呼ぶ」方式から
「静的テーブルを core 依存部が走査する」方式へ変更した。
ステアリング記録: `baremetal/.steering/20260603-arm64-mmu-static-config/`。

### 切替マクロ `USE_ARM64_MMU_CONFIG_TABLE`

- `stm32mp257f_dk_arm64_gcc` の `Makefile.target` で `-DUSE_ARM64_MMU_CONFIG_TABLE` を定義。
- **未定義時は従来どおり `target_mmu_init()` を呼ぶ**ため，他の arm64 ターゲット
  （de25 / imx8mm_evk / kr260 / sulfur / zcu102）には影響しない
  （zcu102 でビルド回帰確認済み。`_kernel_target_mmu_init` が従来どおりリンクされる）。

### 共通部 `arch/arm64_gcc/common` への変更（マクロガード付き差分）

1. **`core_kernel_impl.h`** — `USE_ARM64_MMU_CONFIG_TABLE` 定義時のみ有効な宣言を追加:
   `typedef mmap_t ARM64_MMU_CONFIG;` / `extern ARM64_MMU_CONFIG arm64_memory_area[];` /
   `extern const uint_t arm64_tnum_memory_area;`（arm の `core_kernel_impl.h` の
   `ARM_MMU_CONFIG`/`arm_memory_area`/`arm_tnum_memory_area` に対応）。
2. **`arm64.c`** — `mmu_init()` の「ターゲット依存部での変換テーブルの初期化」を分岐。
   マクロ定義時は `arm64_memory_area[]` を先頭から `mmu_mmap_add()` で取り込む。
   `mmu_mmap_add()` はソート挿入のためテーブルの記述順は任意。

### ターゲット依存部 `stm32mp257f_dk_arm64_gcc` の変更

3. **`target_kernel_impl.c`** — `target_mmu_init()`（`TOPPERS_32BIT_ABOVE_ADDR` 有無の
   2 変種）と `user_mmu_init()`（weak フック）を削除し，weak の
   `arm64_memory_area[]` / `arm64_tnum_memory_area` テーブルへ置換。
   - メモリマップの内容（領域・属性・`TOPPERS_TZ_S`/`INITMMU_ALL_MEM` の変種構造）は
     旧コードを忠実に踏襲（実質変更なし）。
   - アプリ側でのカスタマイズは weak テーブルの差し替えで行う（arm と同じ流儀）。
     `user_mmu_init()` を使っていたコードは移行が必要（リポジトリ内に該当なしを確認済み）。
   - 注: 非 `TOPPERS_32BIT_ABOVE_ADDR` の旧変種は `mm.ap` 未設定（未初期化）だったため，
     テーブルでは既定値相当の `MEM_AP_RW_EL1` を明示した（潜在バグの修正）。
4. **`target_kernel_impl.h`** — `target_mmu_init()` の宣言を削除。
5. **`target_rename.def`** — `target_mmu_init` を削除し `arm64_tnum_memory_area` /
   `arm64_memory_area` を追加（arm の zybo_z7 と同じパターン）。
   `target_rename.h` / `target_unrename.h` は `utils/genrename.rb target` で再生成。
6. **`Makefile.target`** — `-DUSE_ARM64_MMU_CONFIG_TABLE` を追加。

### 検証（2026-06-03）

- `obj/obj_stm32mp2`（`PRC_NUM=2`，gcc 14.3）で警告なしビルド
  （リンカの `.bss`/RWX 警告は変更前からの既存挙動）。
- `_kernel_arm64_memory_area`（weak, 0x40B=2 エントリ）/ `_kernel_arm64_tnum_memory_area`
  （=2）がリンクされ，`target_mmu_init` シンボルは消滅。
- SWD 実機（`make swd-run`）で変更前後の UART 出力が一致
  （バナー，Processor 1/2 start，両コアのタスク交互実行）。
  ログ: `.steering/20260603-arm64-mmu-static-config/{baseline,after}-uart.log`。

### 追記: 全 arm64 ターゲットへの展開（2026-06-03）

当初は stm32mp257f_dk のみの適用としていたが，ユーザー指示により残りの 5 ターゲット
（de25 / imx8mm_evk / kr260 / sulfur / zcu102）も静的テーブル方式へ移行した
（各 `target_kernel_impl.c` のテーブル化，`target_mmu_init()`/`user_mmu_init()` 廃止，
rename 再生成，`Makefile.target` への `-DUSE_ARM64_MMU_CONFIG_TABLE` 追加）。
**全 5 ターゲットでビルド成功・コンパイラ警告ゼロを確認**（実機が無いため
ビルド確認まで．実機検証済みは stm32mp257f_dk のみ）。
共通部の新旧分岐（マクロ未定義時は `target_mmu_init()` を呼ぶ）は，ツリー外の
ターゲットとの互換のため維持している。

## HRT 変換マクロの括弧不足の修正（2026-06-04）

`target/stm32mp257f_dk_arm64_gcc/target_timer.h` の `HRT_CNT_TO_HRTCNT(cnt)`／
`HRT_HRTCNT_TO_CNT(hrtcnt)` マクロの引数が無括弧で，式を渡すと演算子優先順位に
より誤展開する潜在バグを修正（`(cnt)`／`(hrtcnt)` に）．FMP3 の既存の呼出しは
単一変数渡しのため動作への影響はない（HRP3 移植（hrp3_3.4）でタイムウィンドウ
タイマの残り時間計算 `HRT_CNT_TO_HRTCNT(cval - now)` が `cval - now/mhz` に
誤展開して顕在化したもの．ASP3（asp3_3.7.2）も同日修正済み）．
修正後にビルド確認済み（コンパイラ警告ゼロ）．

## ENTRY_PC の自動取得化（2026-06-04）

`Makefile.target` の swd-run/swd-debug 用 `ENTRY_PC` を，`TEXT_START_ADDRESS`
固定から **ELF のエントリポイントの自動取得**（readelf）に変更（HRP3 移植からの
バックポート．ASP3 も同日変更）．本ターゲットは固定リンカスクリプトの
`ENTRY(start)` により両者が一致するため動作は不変だが，ELF を正とすることで
配置変更に頑健になる．

## ランタイムテストの実施（2026-06-05）

`test/testexec.rb` により，標準のランタイムテスト（機能テスト 36 件＋マルチプロセッサ
固有テスト 10 件 = 46 件）を DK 実機（PRC_NUM=2, SMP）で実施した．方式は ASP3 側
（`PORTING_ASP3_STM32MP2.md`「ランタイムテストの実施」）と同じ．

### テスト環境（`<FMP3>/TEST-EXEC/`）

- `TARGET_OPTIONS`（非 TECS 構成）:
  ```
  -T stm32mp257f_dk_arm64_gcc -w -S 'syslog.o banner.o serial.o serial_cfg.o
  stm32usart.o chip_serial.o test_svc.o' -b '-lc -lnosys'
  ```
  - テストは `test_common1.cfg`（serial/syslog/banner）＋ `test_svc.o` で動く
    （logtask は使わない）．
  - **`-b '-lc -lnosys'`**: mtrans2/mtrans5 が `rand()` を使い，newlib の reent 経由で
    `_sbrk` 等のスタブを要求するため．`-lc` の後に `-lnosys` が来る並びを作る
    （`-nostdlib` リンクのため `--specs=nosys.specs` は効かない）．
- `TARGET_RUN`: `bash ../run_test.sh`（ASP3 版と同じ方式．完了マーカーに
  mtrans2/4/5 の `Test finished.`，mtrans3 の最終統計行を追加）．
- PRC_NUM は Makefile.target の既定（2）が適用される．

### 結果（46 テスト中 45 PASS / mtrans2 は既知の制限）

| テスト | 結果 |
|---|---|
| task1〜tmevt1（機能テスト 36 件，ASP3 と同一セット） | PASS（36件） |
| cpuexc1〜cpuexc10 | PASS（arm64 は同期例外のため arm_m のようなマスク中エスカレートの制限なし．cpuexc10 は「not necessary」正常終了） |
| dlynse | PASS（SIL_DLY_TIM 修正後，17 測定全て OK） |
| mtskman1〜3 / mtrans3〜5 / mmutex1 / malarm1 / spinlock1 | PASS（9件） |
| **mtrans2** | **TIMEOUT（既定パラメータ．下記「既知の制限」）／PASS（`TEST_DELAY_TIME_NSE=1000`）** |

### テストにより発見・修正した問題

1. **swd-run の halt レース**（`Makefile.target`）: ASP3 側と同じ修正
   （起動待ち 6000ms＋examine/halt 間に `sleep 500`＋`arp_poll`）を適用．
2. **`SIL_DLY_TIM1/2` が実機と不一致**（`target_kernel_impl.h`）:
   70/44（移植元由来）→ **12/10**（dlynse の fitting 出力から逆算．ASP3 側と同一値）．

### 既知の制限: mtrans2（IPI 飽和によるスタベーション）

**現象**: 既定パラメータ（`TEST_DELAY_TIME_NSE=10`）では進行が事実上停止する
（25 分でフェーズ A の 1000 回ループすら完了しないことがある．進行レートは
ロック競合の運に依存し，まれに 1 分程度でフェーズ A を抜けることもある）．

**機構**（両コア halt スナップショット＋PC サンプリングで確認）:
- TASK2(PRC2) が `sus_tsk(TASK1)`/`rsm_tsk(TASK1)` をほぼ無待機で連射し，
  1 回の sus/rsm 毎にディスパッチ要求 IPI（SGI）が PRC1 へ飛ぶ
  （PC サンプル: PRC2 は `update_schedtsk_dsp`→`gicd_raise_sgi` 内）．
- PRC1 は IRQ 入口（`cur_spx_irq_handler` のレジスタ保存プロローグ）を周回し，
  タスクコンテキスト（TASK1）がほぼ実行されない（IRQ 到着レート ≒ 処理レート）．
  IRQ 毎の全 FPU コンテキスト保存（USE_ARM64_FPU）とジャイアントロック競合
  （TAS+WFE，公平性なし）がスタベーションを増幅する．
- カーネルの状態自体は正常（TASK1 は SUSPENDED↔RUNNABLE を正しく遷移．
  デッドロックではない）．SIL_DLY_TIM を実機値（12/10）に修正したことで
  テスト内の delay がほぼ 0ns になり，IPI レートが上がって顕在化した．

**回避策**: テストの調整ノブ `TEST_DELAY_TIME_NSE`（`#ifndef` ガード付き，既定 10）
を 1000 にすると数秒で全フェーズ完走する（`configure.rb` に
`-O "-DTEST_DELAY_TIME_NSE=1000"` を追加して再ビルド）．統計も健全
（TTS_RUN/TTS_SUS・E_OK/E_QOVR とも両側を観測）．

**本質対策の候補**（未実施）: ジャイアントロックの公平化（チケットロック等．
共通部の変更になる）／IRQ 経路の FPU 保存の遅延化．mtrans3〜5・spinlock1 が
PASS しており，カーネルロジックの正しさはテストで確認できているため，
本移植では既知の制限として記録するに留める．

### 残課題

- 性能評価（perf0〜5）・タイマドライバシミュレータ系・拡張パッケージ系
  （subprio 等）のテストは未実施．
- mtrans2 の既定パラメータでの完走（上記「本質対策の候補」参照）．
