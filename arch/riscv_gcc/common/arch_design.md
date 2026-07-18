# RISC-V（riscv_gcc）アーキ共通部の設計知見（FMP3）

- 対象: `arch/riscv_gcc/common`（PLIC/CLINT・M-mode software IPI 対応の RISC-V マルチコア共通部）
- 適用チップ/ターゲット: polarfire_soc（polarfire_soc_kit / -hw）
- 作成: 2026-06-14（Step7 = PolarFire SoC QEMU 移植、Step14 = 実機で確立）
- 上位の中立な契約は `doc/porting_notes_fmp3.md`、チップ固有は `arch/riscv_gcc/polarfire_soc/chip_user.md` を参照。

## 1. PLIC レジスタの MMIO アクセス幅（CPU例外復帰でfault）

**症状**: QEMU（`microchip-icicle-kit`）で、通常の割込みは正常なのに **CPU例外（cpuexc1-9）からの復帰
だけが無限ループ**に陥る。

**真因**: `arch/riscv_gcc/polarfire_soc/chip_support.S` の `irc_get_intpri` が PLIC の threshold
レジスタ（32bit, `0x0c20_1000`〜）を **`ld`（64bit ロード）** で読んでいた。QEMU の sifive_plic は
8バイトアクセスを fault させるため、CPU例外復帰経路（この関数を通る唯一の経路）だけが落ちる。
通常割込みは `irc_begin/end_int` の `lw/sw`（32bit）で読むため正常だった。

**修正**: `ld` → **`lw`**（32bit、asp3_core の同ファイルと一致）。r492 でコミット済。

**教訓**: RISC-V で「CPU例外復帰だけ fault」を見たら、まず MMIO レジスタのアクセス幅（`lw` か `ld` か）を
疑う。PLIC/CLINT のレジスタは基本 32bit 幅。

## 2. マルチコア（SMP）構成

- セカンダリコア起床は M-mode software interrupt（`msi_ipi.c` / CLINT MSIP）で行う。ディスパッチ通知も
  同経路。新規移植では両ハートで IPI ハンドラ到達カウンタを確認すること
  （`doc/porting_notes_fmp3.md` 契約2）。
- バリアを使うMPテスト（mtskman/mmutex）は `PRC_NUM=2` で実行する（契約3）。既定の全コア SMP では
  テスト不参加ハートが `test_barrier` に来ず永久待ちになる。
- PolarFire SoC の 5 ハート構成（E51 監視ハート + U54×4）でのブートモデルは
  `arch/riscv_gcc/polarfire_soc/chip_user.md` を参照。

## 3. ツールチェーン（picolibc + A拡張）

- RISC-V マルチコアは atomic（A拡張, `rv64gc`）が必須。**Vitis 同梱の riscv64 gcc は A拡張つき
  multilib を持たない**ため使えない。
- **Ubuntu の `riscv64-unknown-elf-gcc` + picolibc** を使う（`QEMU=1 make`）。newlib-nano ではなく
  picolibc を選ぶ点に注意。実機（Step14）では `-T mpfs-lim.ld` で LIM にリンクする
  （`target/polarfire_soc_kit_hw_gcc/Makefile.target`）。

## 4. 予備割込み（ras_int）非サポート

- `int1` は `ras_int`（ソフトで pending を設定する予備割込み）を要求するが、**QEMU の sifive_plic は
  pending 書込みを無視する**（`hw/intc/sifive_plic.c`）。PLIC の割込みは実デバイスの IRQ ライン経由でしか
  上がらない。asp3_core も polarfire では int1 非対応。
- `TOPPERS_SUPPORT_RAS_INT` 未定義で `E_NOSPT` になるため、`arm_*`/`simt`/`perf` と同様にスイートから
  **除外**する（リグレッションではない）。
