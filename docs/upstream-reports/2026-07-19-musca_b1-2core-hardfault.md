# [TOPPERS/FMP3] musca_b1_gcc の 2 コア SMP が HardFault で停止する

報告日: 2026-07-19

## 対象

- FMP3 バージョン: **3.4.0**（`UPSTREAM_VERSION`）
- ターゲット依存部: `target/musca_b1_gcc`
- 該当コミット（当方の取り込み元 pin）: `f3d29a4c220f6cd2735e24137cf50948f578b5af`

## 環境

| | |
|---|---|
| QEMU | `qemu-system-arm` **11.0.1**（`target_user.txt:51` が動作確認版として挙げているもの）／ **8.2.2** でも同一結果 |
| コンパイラ | `arm-none-eabi-gcc` 13.2.1 (15:13.2.rel1-2) |
| C ライブラリ | newlib（`arm-none-eabi-gcc -print-file-name=libc.a`） |
| アプリケーション | `sample/sample1` |
| ビルド | 当方は CMake 化した派生ビルドを使用（後述「補足」参照） |

---

## 症状

`TNUM_PRCID=2`（上流手順では `make PRC_NUM=2`）でビルドすると、起動シーケンスは進むが
直後に HardFault で停止する。

```
TOPPERS/FMP3 Kernel Release 3.4.0 for ARM Musca-B1
...
Processor 1 start.          ← 出る
Processor 2 start.          ← 出る
（prc1・prc2 の両方でタスクが走行）  ← ここまで正常
→ Unregistered Exception（HardFault）で停止
```

**QEMU 11.0.1 と 8.2.2 で同一 PC・同一箇所のクラッシュを再現する。**
したがって `target_user.txt:53-55` が言及する「古い QEMU では MHU が未実装で
クロスコア IPI が機能しないことがある」という事象とは**別の問題**である。
むしろ 8.2.2 の方が 1 行多く進行しており、IPI 自体は 8.2.2 でも機能していた。

1 コア構成（既定 `TNUM_PRCID=1`）では正常に動作する。

---

## 原因（当方の調査結果）

### 事実

**HRT ドライバの状態がプロセッサ別になっていない。**

`target/musca_b1_gcc/target_timer.c`:

```c
47: static volatile HRTCNT   hrt_base;
52: static volatile uint32_t hrt_reload;
57: static volatile HRTCNT   hrt_last;
64: static volatile uint8_t  hrt_fresh;
```

これら 4 つはいずれも単一の `static` 変数であり、配列になっていない。
また `target_timer.c` 中に **`prcid` の出現が 1 箇所も無い**（`grep` で確認）。

一方、周囲の定義は per-core を前提としている。

| 箇所 | 内容 |
|---|---|
| `target/musca_b1_gcc/target_kernel.h:59` | `TNUM_PRCID != 1` のとき `#define TOPPERS_TEPP_PRC 0x3`（**PRC1 と PRC2 の両方**が時間イベント処理プロセッサ） |
| `target/musca_b1_gcc/target_user.txt`（カーネルの使用リソース節） | 「**各コア内蔵の** SysTick（24bit ダウンカウンタ，プロセッサクロック 40MHz 駆動）を，次のタイムイベントまでの相対時間でワンショット駆動するイベント駆動 HRT として使用する（`target_timer.c`）」 |
| `target/musca_b1_gcc/target_timer.h:73,79,85` | `target_hrt_set_event(ID prcid, HRTCNT hrtcnt)` / `target_hrt_clear_event(ID prcid)` / `target_hrt_raise_event(ID prcid)` — **`prcid` を引数に取る API** |
| `target/musca_b1_gcc/target_timer.h:10-11` | 「FMP3 ではタイマ操作関数はプロセッサID（prcid）引数を取るが，**本ターゲットは単一プロセッサのため引数は無視する**」 |

すなわち、**API は `prcid` を受け取る形になっているが実装は無視しており、
そのことがヘッダのコメントに明記されている**。

### 推測

SysTick は Cortex-M のコアごとに独立したハードウェアであるため、
2 コア構成では PRC1 と PRC2 のそれぞれで SysTick 割込みが発生する。
このとき両者が同一の `hrt_base` / `hrt_reload` / `hrt_last` / `hrt_fresh` を
更新するため、2 つ目のコアの SysTick が最初に発火した時点で状態が破壊され、
以降の時刻計算が破綻して HardFault に至る、と考えている。

（当方は `-d int` による割込みトレースで、クラッシュが 2 つ目のコアの SysTick 発火直後に
起きることを確認した。ただし「状態破壊が HardFault の直接原因である」ことまでを
命令レベルで追い切ってはいない。）

---

## 修正の方向（提案）

上記 4 変数を `[TNUM_PRCID]` の配列とし、各関数が引数の `prcid`（あるいは
自プロセッサ ID）で添字を引く形にするのが素直と思われる。

SysTick はコアごとに独立したハードウェアであり、各コアは自分に対応する要素のみを
参照・更新するため、**コア間の排他制御は不要**と考えられる。

あわせて `target_timer.h:10-11` および同 `:25`（「本ターゲットは単一プロセッサ（PRC1）」）の
コメントの撤回が必要になる。

**当方ではこの修正は行っていない。** 構造変更を伴い、かつ上流の意図を確認したいため。

---

## 確認したい点

`target/musca_b1_gcc/target_user.txt:13` は

> ア構成を用いて FMP3 の 2 コア SMP（対称マルチプロセッシング）を QEMU 上で検証

と記載しており、上記の症状と矛盾する。

**当方が踏んでいない前提条件（別のアプリケーション、`TOPPERS_TEPP_PRC` の明示指定、
別の QEMU オプション等）で検証されたものではないか**、というのが最も知りたい点である。

`TOPPERS_TEPP_PRC` は `target_kernel.h:55` で `#ifndef` ガードされているため、
外から `0x1`（PRC1 のみ）を与えれば PRC2 側の SysTick は使われず、
問題が顕在化しない可能性がある。検証時にそのような指定をされていたか、
ご確認いただけると助かる。

当方の構成は以下に相当する。

```
configure.rb -T musca_b1_gcc -w -S "syslog.o banner.o serial.o serial_cfg.o logtask.o"
make PRC_NUM=2
```

---

## 付随して見つかった不足

`target/musca_b1_gcc/target_test.h` に **`INTNO2` の定義が無い**（`INTNO1` のみ、`:37-39`）。
`sample/sample1.c` の `inthd_no[]` が `TNUM_PRCID >= 2` のとき無条件に `INTNO2` を参照するため、
2 コアビルドに必須である。当方では以下を追加した。

```c
#if TNUM_PRCID >= 2
#define INTNO2			((2U << 16) | (60U + 16U))	/* PRC2, 予備NVIC IRQ60 → INTNO 76 */
#define INTNO2_INTATR	TA_ENAINT
#define INTNO2_INTPRI	(-2)
#define intno2_clear()
#endif /* TNUM_PRCID >= 2 */
```

生の IRQ 番号は `INTNO1` と同じ 60 を用い、上位 16bit のプロセッサタグのみを変えている。
NVIC はコアごとに独立したインスタンスであり、`raise_int`/`probe_int`/`clear_int` は
自コアの `NVIC_ISPR`/`NVIC_ICPR` のみを操作する（`arch/arm_m_gcc/common/core_kernel_impl.h`）
ため、未接続の予備 IRQ を PRC1 と同じ生番号のまま PRC2 にも割り当ててよい、と判断した。
1 コア構成には影響しない。**この判断が上流の意図と一致するかは未確認。**

---

## 補足: 当方のビルド環境について

当方は FMP3 を CMake 化した派生版（TECS レス・Python コンフィギュレータ）を開発しており、
本件はそのターゲット追加作業中に発見した。ただし:

- 本件は**ビルドシステムに依存しない**。生成される `kernel_cfg.c` の内容
  （`_kernel_pcb_prc1` / `_kernel_pcb_prc2` の両方が生成される）および
  コンパイル・リンクオプションは上流 Makefile と突き合わせ済みで、過不足は無い。
- 同じ CMake ビルドで `polarfire_soc_kit_gcc` の **4 コア SMP は QEMU 上で安定動作**しており
  （`Processor 1..4 start.` の 4 行、プロセッサ別初期化ルーチンも動作）、
  ビルドシステム側の一般的な不備であれば polarfire でも顕在化するはずである。
- コンフィギュレータは本件の調査時点では上流の Ruby 実装（`cfg/cfg.rb`、VERSION 1.7.1）を
  そのまま実行している。

したがって本件はターゲット依存部固有の問題と判断している。
