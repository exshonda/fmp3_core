# STM32MP2依存部マニュアル

- 最終更新: 2026年06月03日

# 概要

STM32MP2依存部は，STMicroelectronics社の STM32MP2xx（Cortex-A35）をターゲットとしている．動作確認は STM32MP257F-DK（Cortex-A35 ×2 / GICv2(GIC-400) / ARMジェネリックタイマ / LPDDR4）で行っている．

割込みコントローラは GICv2(GIC-400)，カーネルタイマはコア内蔵の Secure Physical Timer，コンソールは STM32 の USART（SIOドライバは stm32usart.c）を使用する．

# チップ依存の EL3／EL2 初期化ルーチン

## chip_el3_initialize()

EL3 で実行する初期化．SCR_EL3 の設定，CPUECTLR_EL1 の SMPEN（SMPコヒーレンシ）の有効化，CPTR_EL3 のトラップ解除を行う．TF-A(FSBL) が CPTR_EL3 のトラップを残すため，解除しないと cpacr_el1 アクセスで例外（EC=0x18）になる．

## chip_el2_initialize()

EL2 で実行する初期化（EL2 を経由する構成の場合）．

# チップ依存のマスタプロセッサ用初期化ルーチン

カーネル起動時に str_ker() の実行前に呼び出されるルーチンとして，chip_mprc_initialize() があり，以下の初期化を行う．

## サブコアの起動

サブコア（Core1, a35_1）を起動する．BL31/PSCI が無いため，PSCI ではなく EL3 直接起動を用いる．CA35SYSCFG の VBAR_CR にスタート番地を設定し，RCC の C1P1RSTCSETR でリセットを解除する．

## ARM64依存部のマスタプロセッサ初期化ルーチンの呼び出し

ARM64依存部のマスタプロセッサ初期化ルーチンである，core_mprc_initialize()を呼び出す．

# チップ依存の初期化ルーチン

チップ依存の初期化ルーチンとして，chip_initialize()があり，以下の初期化を行う．

## ARM64依存部の初期化ルーチンの呼び出し

ARM64依存部の初期化ルーチンである，core_initialize()を呼び出す．

# チップ依存の終了処理ルーチン

チップ依存の終了処理ルーチンである chip_terminate() では，以下の終了処理を行う．

## ARM64依存部の終了処理ルーチンの呼び出し

ARM64依存部の終了処理ルーチンである，core_terminate()を呼び出す．

# ターゲット依存部開発者向けの情報

## chip_mprc_initialize()の呼び出し

target_mprc_initialize()から呼び出すこと．

## chip_initialize()の呼び出し

target_initialize()から呼び出すこと．

## 割込みの配送（GICv2, セキュア状態 TOPPERS_TZ_S）

セキュア(Group0)割込みは **FIQ で配送**し（gicc_init で `GICC_CTLR=FIQEN|ENABLEGRP0`），カーネルの CPUロックを **FIQ マスク（DAIF F ビット, chip_kernel.h の TOPPERS_TZ_S 分岐）** にする．これがネイティブな GICv2+Secure 構成で，zynqmp(zcu102/kr260) と同一．割込みハンドラ前半(core_support.S)の `daifclr` は CPUロック(=FIQ)を保持するので，GIC で ack(GICC_IAR 読み)する前に同一割込みが再入することはない．

**重要（2026/06/14 訂正）**: 当初は CPUロックを IRQ マスク(DAIF I)のままにしていたため，FIQ 配送だと「ack 前に FIQ 再許可 → 暴走再入」が起きると考え `GIC_NO_FIQ_IN_SECURE`（FIQEN を立てず IRQ 配送に切替える回避策）を導入していた．しかし真因は **配送方式(FIQ)と CPUロック(IRQ)の不整合**であり，CPUロックを FIQ に揃えれば回避策は不要．実機 STM32MP257F-DK で cpuexc/MP/機能テストが FIQ 方式で全通過（mtrans2 のみ既知のディスパッチ飽和でHANG＝IRQ方式でも同じ）することを確認し，`GIC_NO_FIQ_IN_SECURE` は削除した．

## OS-awareness のチップ依存部

gdb の OS-awareness（utils/gdb_os_aware/os_awareness.py）から参照される chip_os_awareness.py を同梱する．STM32MP2 の GICD ベース(0x4AC10000)を持ち，指定 INTID の GIC 許可/禁止・ペンディング状態を返す（core_os_awareness.py を呼ぶ）．割込みハンドラ番地の取得（inh_handler，arm64 共通部の _kernel_p_inh_table を読む）は core 層の実装をそのまま再エクスポートする．

# 変更履歴

- 2026/06/03
  - STM32MP2（STM32MP257F-DK）向けに新規作成．i.MX8MM 依存部を基に移植．GICv2(GIC-400)対応，STM32 USARTドライバ，CPTR_EL3 トラップ解除，Core1 の EL3 直接起動（SMP），OS-awareness のチップ依存部 chip_os_awareness.py を追加．
    （当初はセキュア割込みを IRQ 配送 `GIC_NO_FIQ_IN_SECURE` にしていたが，2026/06/14 に FIQ 配送へ訂正．下記参照．）

- 2026/06/14
  - セキュア割込みの配送を **FIQ 方式**（CPUロック=FIQ マスク, `GICC_CTLR=FIQEN|ENABLEGRP0`）に統一し，`GIC_NO_FIQ_IN_SECURE` を削除（zynqmp と同じネイティブ Secure 構成）．当初の「FIQ だと暴走再入」は CPUロックを IRQ マスクのままにしていた不整合が原因で，CPUロックを FIQ に揃えれば解消する．実機 STM32MP257F-DK で cpuexc/MP/機能テストが全通過することを確認．

以上．
