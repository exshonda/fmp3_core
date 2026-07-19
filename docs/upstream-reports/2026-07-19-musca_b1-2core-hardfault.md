# [TOPPERS/FMP3] musca_b1_gcc の 2 コア SMP が HardFault で停止する

報告日: 2026-07-19

## ステータス: 上流 20260719 で修正済み（2026-07-19 追記）

`rp2350_pico2_gcc-20260719`（release/3.4、取り込み pin
`b59797f14dedcb07020f96895903ca7fcd14a4af`。musca_b1 の simple パッケージは
`musca_b1_gcc-20260719`）で、下記「原因」節が指摘した
`target/musca_b1_gcc/target_timer.c` の HRT 状態（`hrt_base`/`hrt_reload`/
`hrt_last`/`hrt_fresh`）が、単一の `static volatile` 変数から
**`[TNUM_PRCID]` の配列**（それぞれ `target_timer.c:58,63,68,75`）に修正された。
各アクセス箇所（9箇所）が `get_my_prcidx()` でプロセッサ別に添字を引く形に
改められ、`target_timer.h` の「本ターゲットは単一プロセッサ」という
コメントも削除された。これは本報告の「修正の方向（提案）」節の内容と一致する。

あわせて、本報告の「付随して見つかった不足」節で報告した
`target/musca_b1_gcc/target_test.h` の `INTNO2` 未定義も、
上流版で（当方が追加したものと等価な定義として）解消された。

当方側で QEMU（`qemu-system-arm -machine musca-b1`）による2コア再検証を行い、
`Processor 1 start.` / `Processor 2 start.` の2行が出力され、以前のように
起動直後で HardFault 停止することなく走行を継続することを確認した。
以下、報告時点の本文はそのまま残す。

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

---

## 上流 20260719 修正後に残る事象（2026-07-19 追記）

上記「ステータス」節の通り、本報告が指摘した HRT 状態未分離による HardFault は
`rp2350_pico2_gcc-20260719`（pin `b59797f14dedcb07020f96895903ca7fcd14a4af`）で解消された。
**しかし、同じ再検証の過程で別の異常が見つかったので、同一ターゲット・同一コミットに
関するものとしてここに追記する。**（本件を別ファイルに分けるか一体にするかについては、
当方は「一体化」を選んだ。理由は文末の「本追記の位置づけについて」を参照。）

### 症状（事実）

QEMU（`musca-b1`、`qemu-system-arm` 11.0.1）で `sample/sample1` を2コア構成
（`TNUM_PRCID=2`）でビルドし、既定状態（シリアル入力を一切与えない）のまま実行すると、
以下のログが**周期的に**出力され続ける。

```
no time event is processed in hrt interrupt on PRC2.
```

- 出所: `kernel/time_event.c:756-758`（`signal_time()` 内、`nocall == 0` のとき出る `LOG_NOTICE`）
- 20秒の実行で **47回**（全出力160行の約30%）出力される
- **全て `on PRC2`。`on PRC1` は1回も出ない**
- musca_b1 の1コア構成（`TNUM_PRCID=1`）、および polarfire の4コア構成では **0回**

カーネルは HardFault 等では停止せず、`set_hrt_event()` により毎回タイマを再設定して
走行を継続する。**致命的ではないが、正常でもない。**

#### 当方による再現・実測

`build/musca_b1-2core/fmp`（本タスク時点で既にビルド済みの、pristine 未改変の成果物）を、
リポジトリを一切変更せずにそのまま実行して確認した。

```
timeout 22 qemu-system-arm -machine musca-b1 -cpu cortex-m33 \
    -kernel build/musca_b1-2core/fmp -nographic \
    -semihosting-config enable=on,target=native
```

- 22秒のタイムアウト内で本ログが **52回**出力され、全て `on PRC2`（`on PRC1` は0回）。
  別の25秒タイムアウトの試行では**59回**（同じく全て PRC2）。回数はタイムアウトの
  切り方（何周期目で `timeout` が SIGTERM を送るか）に依存するため、報告記載の
  「20秒で47回」と厳密には一致しないが、同じ現象・同じオーダーである。
- 各行に発生時刻（ホスト側 `date +%s.%N`、1行ずつパイプで付与）を記録し、
  連続する通知の間隔を計測したところ、52回中51個の間隔は
  **平均 0.41947秒（最小 0.4115秒、最大 0.4264秒）** と、極めて狭い分散で一定だった
  （ばらつきはホスト側タイムスタンプ付与のシェルパイプのジッタで説明できる範囲）。
  この周期性が、後述する機序の直接的な裏付けである。

### 機序（当方の調査結果）

#### 事実（コード読解）

`kernel/time_event.c` の `set_hrt_event()`（`:420-481`）は、対象プロセッサのタイムイベント
ヒープが空（`LAST_INDEX(...) == 0`）のとき、musca_b1 が使う非64bit HRTCNT の場合分けで
次のように分岐する（`:436-457`）。

```c
if (p_pcb->prcid == TOPPERS_TMASTER_PRCID) {
    /* タイムマスタプロセッサでは，HRTCNT_BOUND後に割込みを発生させる．*/
    target_hrt_set_event(p_pcb->prcid, HRTCNT_BOUND);
}
else {
    /* 他のプロセッサでは，割込みを発生させないようにする．*/
    target_hrt_clear_event(p_pcb->prcid);
}
```

`target/musca_b1_gcc/target_kernel.h:37` により `TOPPERS_TMASTER_PRCID = PRC1`。
2コア構成では `TOPPERS_TEPP_PRC = 0x3`（`target_kernel.h:59`）のため PRC1・PRC2 とも
タイムイベント処理プロセッサであり、`set_hrt_event()` は両方に対して呼ばれる。

PRC2（非マスタ）で呼ばれる `target_hrt_clear_event(prcid)` は
`target/musca_b1_gcc/target_timer.h:88-92` の薄い転送を経て
`hrt_clear_event_body()`（`target_timer.c:228-233`）に到達する。

```c
void
hrt_clear_event_body(void)
{
    hrt_base[get_my_prcidx()] += hrt_offset_ticks() / HRT_CLOCKS_PER_US;
    hrt_program(HRT_MAX_TICKS);
}
```

`hrt_program()`（`target_timer.c:123-144`）は SysTick を「リロード値 `ticks`、
`SYSTIC_TICINT` 割込み有効」で（再）起動する関数であり、`hrt_clear_event_body()` は
これを **`HRT_MAX_TICKS`（`target_timer.h:52`、`0x00FFFFFF` = SysTick の24bit最大値）**
という値で呼んでいる。すなわち「クリア」＝「割込みを止める」ではなく、
「**表現できる最大区間で SysTick を再起動する（割込みは有効なまま）**」である。

musca_b1 の SysTick はプロセッサクロック 40MHz（`musca_b1.h:57`、
`HRT_CLOCKS_PER_US = CPU_CLOCK_HZ/1000000 = 40`）で駆動するダウンカウンタなので、
この「最大区間」は

```
HRT_MAX_TICKS / (CPU_CLOCK_HZ / 1e6) = 16777215 / 40 = 419430.375 us ≒ 0.41943 秒
```

にしかならない。したがって PRC2 は「クリアした」つもりでも **約0.4194秒後に必ず
SysTick 割込みが発生し**、`target_hrt_handler()` → `signal_time()` が呼ばれる。
このとき PRC2 のタイムイベントヒープが依然として空であれば（後述）、
`nocall == 0` となり本 NOTICE が出て、`signal_time()` の末尾で `set_hrt_event()` が
再度呼ばれて同じ「クリア」（＝0.4194秒後の再割込み）が繰り返される。
これが無限に続く。

上で実測した平均間隔 **0.41947秒**は、この計算値 **0.419430秒**と一致する
（差は計測パイプのジッタの範囲内）。したがって**この機序で、実測された周期性まで
説明できる**。

#### なぜ PRC2 だけか（事実：ヒープが空のまま）

`sample/sample1.cfg` は PRC1・PRC2 それぞれに `CRE_CYC`/`CRE_ALM`
（`CYCHDR1_1`/`ALMHDR1_1` は PRC1、`CYCHDR2_1`/`ALMHDR2_1` は PRC2）を登録しているが、
いずれも `TA_STA`（自動起動）属性ではない（`sample1.cfg:24-25,55-56`）。
`sample/sample1.c` を読むと、これらはシリアルからのキー入力コマンド
（`sta_cyc`/`msta_cyc`/`sta_alm`/`msta_alm`、`sample1.c:1062-1083`）でのみ起動される。
本検証ではシリアル入力を一切与えていないため、**PRC1・PRC2 とも、周期ハンドラ／
アラームハンドラは既定では1つも起動していない**。

一方、`syssvc/logtask.c` の `logtask_main()` は `LOGTASK` タスクとして起動時に
自動実行され（`syssvc/logtask.cfg:12-13`、`TA_ACT`）、`dly_tsk(LOGTASK_INTERVAL)`
（既定 `10000`us = 10ms、`logtask.c:65`）や `dly_tsk(LOGTASK_FLUSH_WAIT)`
（既定 `1000`us = 1ms、`logtask.c:72`）でポーリングし続ける。`LOGTASK` の所属クラスは
`CLS_SERIAL`（`syssvc/logtask.cfg:12`）で、musca_b1 では
`target/musca_b1_gcc/target_syssvc.h:33` により **`CLS_SERIAL == CLS_PRC1`**。

すなわち、**PRC1 には常時 10ms 未満の間隔でタイムイベントを積み続ける LOGTASK が
存在し、PRC2 には既定でそのような発生源が1つも無い**。これが PRC1 の
ヒープが（少なくともこの実行時間内は）空にならず、PRC2 のヒープだけが恒常的に
空になる理由である。

`set_hrt_event()` のヒープ空判定（`kernel/time_event.c:436`）に、
PRC1 は（LOGTASK の10ms/1ms周期に押されて）ほぼ到達せず、PRC2 は常に到達する。
これが「全て PRC2、PRC1 は0回」を説明する。

#### 結論（機序は特定できた）

以上をまとめると:

1. `TOPPERS_TEPP_PRC = 0x3` により PRC2 も独立したタイムイベント処理プロセッサとなる
   （`target_kernel.h:59`。この値は今回の 20260719 取り込みより前から変わっていない
   ことを `git log upstream -- target/musca_b1_gcc/target_kernel.h` で確認済み）。
2. 既定の `sample1` 構成では PRC2 側に周期的なタイムイベント発生源が無いため、
   PRC2 のタイムイベントヒープは恒常的に空になる。
3. ヒープが空の非マスタプロセッサに対し、カーネル共通部は
   `target_hrt_clear_event()`（＝「当面割込みを発生させない」という契約）を呼ぶ
   （`kernel/time_event.c:454`）。
4. musca_b1 の `hrt_clear_event_body()` はこの契約を**「SysTick が表現できる最大区間
   （24bit / 40MHz ≒ 0.4194秒）で再武装する」**という形で実装している
   （`target_timer.c:228-233`）。SysTick はダウンカウンタでありハードウェア的に
   「無限に鳴らさない」設定ができないため、この実装は**必ず約0.42秒ごとに
   割込みを発生させてしまう**。
5. 20260719 の修正**前**は PRC2 がこの状態（安定して走り続けること自体）に
   到達できなかった（起動直後に HardFault）ため、この既存の（修正で変わっていない）
   `hrt_clear_event_body()` の挙動は**観測されたことが無かった**。修正により PRC2 が
   安定して走るようになったことで、この潜在していた挙動が初めて表面化した。

**機序は特定できたと考える**（コード読解に加え、実測した周期 0.41947秒が計算値
0.419430秒と一致することで裏付けた）。「状態破壊」のような未解明の要素は残っていない。

`target_hrt_set_event`/`clear_event`/`raise_event`（`target_timer.h:82-98`）はいずれも
引数 `prcid` を無視し `get_my_prcidx()` のみを使うが、これは
`TOPPERS_SUPPORT_CONTROL_OTHER_HRT` が未定義（他プロセッサの HRT 操作は
`request_set_hrt_event()` の IPI 経由）である限り `prcid` は常に自プロセッサと
一致するため安全であり、本件の原因ではない（一度疑ったが、コード読解で否定した）。

### 影響評価

- **致命的ではない。** カーネルは `set_hrt_event()` により毎回タイマを再設定し、
  走行を継続する。データや状態の破壊は起きていない。
- **周期タスクの精度への影響は無いと考えられる。** 実際に周期ハンドラ／アラームを
  PRC2 に起動した場合、`tmevtb_enqueue`/`tmevtb_enqueue_reltim` はヒープへの登録直後に
  `set_hrt_event()` を呼び直し、`hrt_program()` は毎回 SysTick の
  `CONTROL_STATUS`/`RELOAD_VALUE`/`CURRENT_VALUE` を書き直すため、直前の「クリア」
  状態（最大区間で回っていたこと）は実イベント登録時に完全に上書きされる。
  また `signal_time()`／`set_hrt_event()` は `lock_cpu()`+`acquire_glock()` で
  保護されているため、スプリアス割込みとイベント登録のレースで実イベントが
  失われる経路も見当たらない。ただし**この「精度に影響しない」は静的なコード読解に
  よる判断であり、PRC2 で実際に `CRE_CYC`/`sta_cyc` を使った周期タスクを走らせて
  精度を実測する検証までは行っていない**（本タスクの時間内では未実施）。
- **CPU 時間への影響は僅少。** 割込みハンドラ（`target_hrt_handler`）とヒープ空判定は
  いずれも短い処理であり、0.42秒に1回程度の頻度でこれが起きても実行時間への影響は
  無視できる範囲と考えられる。
- **消費電力への影響は「不明・本リポジトリでは評価不能」。** 本カーネル（`kernel/` 配下、
  `arch/arm_m_gcc/common/` 配下）に `WFI`／低消費電力アイドルの仕組みが実装されている
  形跡を確認できなかった（`grep -rn "WFI\|wfi\|idle"` で該当なし）。したがって
  「アイドル中の PRC2 を不要に起こしてしまう」という一般的な低消費電力設計上の懸念は
  理屈としては成立するが、**本ターゲットの現状の実装ではそもそも低消費電力アイドルを
  行っていない可能性が高く、電力への実害があるかどうかは判断できない**。
- **ログの実用上の影響。** `LOG_NOTICE` レベルで20秒に47回（≒全出力の3割）出力されるため、
  実機デバッグ時にシリアルログの可読性を大きく損なう。これは軽微ではあるが実害である。

### 1コア構成／polarfire に影響が無いことの確認（理屈）

- **musca_b1 1コア構成（`TNUM_PRCID=1`）**: `TOPPERS_TEPP_PRC = 0x1`
  （`target_kernel.h:57`）で PRC2 自体が存在せず、`target_timer.cfg` の PRC2 用
  `CLASS(CLS_PRC2){...}` ブロックも `#if TNUM_PRCID >= 2` でコンパイルされない
  （`target_timer.cfg:26`）。加えて PRC1 は前述の通り LOGTASK が常時押しているため、
  そもそも「ヒープが空の非マスタプロセッサ」という状況が発生し得ない。
- **polarfire（RISC-V、4コア）**: `TOPPERS_TEPP_PRC = 0xf`
  （`target/polarfire_soc_kit_gcc/target_kernel.h:42`）で PRC2〜4 も musca_b1 と同様に
  タイムイベント処理プロセッサであり、既定構成でも周期ハンドラを起動していなければ
  同じ理屈で「ヒープが空の非マスタプロセッサ」は起こり得る。しかし
  `arch/riscv_gcc/common/mtimer.h:164-168` の `target_hrt_clear_event()` は

  ```c
  Inline void
  target_hrt_clear_event(ID prcid)
  {
      mtimer_set_mtimecmp(prcid, 0xFFFFFFFFFFFFFFFFULL);
  }
  ```

  と、**真に64bitのコンペアレジスタ**（RISC-V `mtimecmp`）を最大値に設定する。
  この値に実際に到達するまでの時間は現実的な実行時間を大きく超えるため、
  「クリア＝事実上二度と鳴らない」が成立する。musca_b1 の SysTick が
  24bit・40MHzで最大区間が0.42秒しか取れないのとは対照的に、polarfire の
  HRT ハードウェアはカーネル共通部の「クリア」契約を額面通り実現できる。
  **したがって、両ターゲットとも `TOPPERS_TEPP_PRC` の設計は同型だが、
  HRT ハードウェアの表現力の違いにより musca_b1 だけで問題が顕在化する**、
  という理屈になる（polarfire での実機・QEMU 再検証は本タスクでは行っていない。
  既知の「4コアで0回」という観測結果と、上記コード読解が整合することのみ確認した）。

### 確認したい点（上流への質問）

1. **`hrt_clear_event_body()` の実装意図について。** 「クリア」を
   「SysTick が表現できる最大区間（≒0.42秒）で再武装し、割込みは有効なまま」
   として実装したのは意図的（SysTick には真の「無期限停止」の手段が無い前提での
   次善策）か、それとも見落とし（本来は `SYSTIC_TICINT` または `SYSTIC_ENABLE` を
   落として真にマスクすべきだった）か。仮に意図的だとしても、`kernel/time_event.c`
   側の「クリア＝割込みを発生させない」というコメント（`:452`）との齟齬をどう
   解決すべきとお考えか。
2. **`TOPPERS_TEPP_PRC = 0x3`（PRC1・PRC2 とも時間イベント処理プロセッサ）は
   musca_b1 の2コア構成にとって必須の設計か。** `target_timer.cfg` が
   `TNUM_PRCID >= 2` で無条件に PRC2 の SysTick 初期化・割込みハンドラ登録を行う
   （`target_timer.cfg:26-33`）ため、PRC2 が時間イベント処理プロセッサでない
   （`TOPPERS_TEPP_PRC = 0x1`）構成にすると `signal_time()` が
   `p_my_pcb->p_tevtcb`（NULL）を参照してクラッシュする恐れがあると当方は
   読んでいるが、この理解で合っているか。合っているなら、PRC2 の SysTick を
   「HRT として持つが時間イベント処理はしない」という構成は本ターゲットでは
   選択できない、ということでよいか。
3. **`kernel/time_event.c:756-758` の `LOG_NOTICE` の位置づけについて。**
   これはタイムマスタプロセッサの `HRTCNT_BOUND` 経由の周期的な安全弁ウェイクアップ
   （`:443-449`、ラップアラウンド対策）でも同様に `nocall == 0` となり得るため、
   「本当に想定外の異常」と「設計上意図された安全弁／ターゲット依存の副作用」を
   ログ上で区別できない。後者（本件のような、ターゲット依存の理由による
   周期的な空振り）を `LOG_NOTICE`（デフォルトで有効なレベル）ではなく、
   より低いレベルにする、または区別するフラグを設けるご予定はあるか。

いずれも「意図された動作か、修正すべき不具合か」を確認したい、というのが主旨である。
当方側での修正（`hrt_clear_event_body()` を `SYSTIC_ENABLE`/`SYSTIC_TICINT` を落として
真にマスクする形に変える等）は、HRT の他の呼び出し規約（`hrt_get_current_body()` が
割込み保留ビットを見て経過を判定している点など）への影響を精査する必要があるため、
**当方では実施していない**。

### 本追記の位置づけについて

本件は「別ファイルにするか、本ファイルに追記するか」を判断する必要があった。
当方は**本ファイルへの追記**を選んだ。理由:

- 本件は 20260719 の同一コミット（`target/musca_b1_gcc/target_timer.c` の同一改修）を
  再検証する過程で見つかったものであり、対象ファイル・対象コミットが本報告と完全に
  一致する。上流側が「HardFault 修正の確認」として本ファイルを読む際、同じ文脈のまま
  「直したら別の副作用が見えた」と読めることに価値があると判断した。
- タイトルが `-hardfault.md` で内容が「解消済み」であるため、無関係の新規事象を
  ここに足すと誤解を招く懸念はあったが、本件は`HardFault` そのものではないものの
  「同じ修正の影響範囲の続報」であり、独立した新規不具合ではないため、
  分離するメリットより文脈の連続性を保つメリットが上回ると判断した。
- ステータス節（冒頭）は指示通り変更していない。**本追記は HardFault の状況を
  何も変えない**（HardFault は解消済みのまま）。
