# TOPPERS/FMP3 OS Awareness for gdb

gdb の Python 機構で TOPPERS/FMP3 カーネルの状態（タスク・同期/通信オブジェクト）を
可視化するスクリプト集（Step2: OS Awareness）。本体は `os_awareness.py`。

## 必要なもの

- **Python 対応の gdb**。Arm GNU Toolchain 同梱の `aarch64-none-elf-gdb` は **Python 非対応**
  （`Python scripting is not supported`）なので使えない。Ubuntu の **`gdb-multiarch`**
  （Python 対応・AArch64 対応）を使う:
  ```bash
  sudo apt-get install -y gdb-multiarch
  ```
- 動的情報（タスク状態・カウント・待ちキュー）は RAM 上にあるため，**実行中ターゲット**
  （OpenOCD 経由の SWD など）または**コアダンプ**への接続が要る。静的情報（`const`）は
  ELF 単体でも読める。

## コマンド

| コマンド | 内容 |
|---|---|
| `stask` | タスクの静的情報（`_kernel_tinib_table`）: entry/exinf/優先度/初期プロセッサ/affinity/スタック/属性 |
| `atask [tid|name]` | タスクの動的情報（TCB）: 状態(DORMANT/READY/RUNNING/WAIT-xxx/SUSPENDED)・現/基底優先度・**スタック使用量(use/size)**・待ちオブジェクト名。引数なしで全タスク＋各プロセッサのレディキュー＋待ちキュー |
| `sem` `dtq` `pdq` `flg` `mtx` `mpf` `[id|name]` | 同期・通信オブジェクトの静的＋動的を 1 コマンドで（属性・容量/初期値・現在値・待ちキュー）。`dtq`/`pdq` は送信 snd / 受信 rcv 別。`mtx` はロック中タスク(owner)。該当オブジェクトが無ければ `no <X> objects` |
| `cyc` `alm` `[id|name]` | 時間イベントハンドラ（周期・アラーム）の静的＋動的: ハンドラ(symbol)・周期/位相(cyc)・割付プロセッサ・動作状態(STA/STP)・次回起動時刻(evttim) |
| `intr` | 設定済み割込み要求ライン（INTINIB）: INTID・優先度・属性・割付プロセッサ・affinity・**ハンドラ関数名**。さらに **GIC の許可/禁止(ena/dis)・ペンディング(pend)** 状態（ena/pend は実機接続時のみ）。ハンドラはプロセッサ毎のハンドラ表（`_kernel_p_inh_table`, ターゲット依存部経由）から解決し，ATT_ISR のラッパ（`_kernel_inthdr_<n>`）は kernel_cfg.c を解析して **実 ISR 関数名と exinf** に解決（例 `sio_isr(exinf=1) [ISR]`）。ディスパッチ要求 IPI が asm 直行バイパス（`USE_BYPASS_IPI_DISPATCH_HANDER`）の場合は `(dispatch IPI: bypassed, handled in asm)` と注記。ハンドラ表は const なので ELF 単体でも表示可 |
| `spn [id|name]` | スピンロック（SPNINIB）: 属性・保持プロセッサ（PCB.p_locspn から `PRCn`/`free`） |

> `atask` のスタック使用量は **保存スタックポインタ(`tskctxb.sp`)** から `(stk+stksz)-sp` で概算する。
> 非実行タスクでは正確，実行中タスクは最後の切替時の値（概算）。休止/範囲外は `-`。

- 引数は **数字（ID）または名前**（`CRE_TSK`/`CRE_SEM` で付けた名前）。
- ID 表記は `名前(ID)`（`kernel_cfg.h` を解析。見つからなければ数字のみ）。
- gdb 内の表示は全て英語。

## 使い方

### 静的情報のみ（実機不要）
```bash
gdb-multiarch -q -nx <OBJ>/fmp \
  -ex 'source <FMP3>/utils/gdb_os_aware/os_awareness.py' -ex stask
```
`<OBJ>/fmp` と同じディレクトリ（または cwd / `./gen`）に `kernel_cfg.h` があれば名前解決される。

### 実機（STM32MP257F-DK ターゲット）
ビルドディレクトリ `<OBJ>` で:
```bash
make osdebug        # OpenOCD 自動起動 + gdb-multiarch + os_awareness.py 読込
# gdb 内: continue → (FMP3 実行) → Ctrl-C で halt → atask / stask / sem / dtq / ...
```
`make osdebug` は本ターゲット依存部（`target/stm32mp257f_dk_arm64_gcc/Makefile.target`）が提供する。

### SMP（PRC_NUM=2）で一貫スナップショットを取る
Core1 起動のため OpenOCD は `EN_CA35_1 0` で a35_1 を未 examine にしている。a35_0 だけ halt だと
Core1 が走行中で PRC2 のキューが不整合になるので，FMP3 実行後に **両コアを halt** してから読む:
```
(gdb) monitor stm32mp25x.a35_0 arp_halt
(gdb) monitor stm32mp25x.a35_1 arp_examine
(gdb) monitor stm32mp25x.a35_1 arp_halt
(gdb) atask
```

## ターゲット依存の割込み状態（GIC）・ハンドラ表

割込みの許可/禁止・ペンディング状態は GIC レジスタに，ハンドラ表（`_kernel_p_inh_table`）は
arm64 共通部の実装に依存するため，**ターゲット依存部**にレイヤ構造で実装している
（FMP3 のソース階層 target→chip→core に対応）:

| ファイル | 場所 | 役割 |
|---|---|---|
| `target_os_awareness.py` | `target/stm32mp257f_dk_arm64_gcc/` | ボード依存。今回は追加なしで chip を再エクスポート |
| `chip_os_awareness.py` | `arch/arm64_gcc/stm32mp2/` | SoC 依存。GICD ベース(0x4AC10000)を持ち core を呼ぶ |
| `core_os_awareness.py` | `arch/arm64_gcc/common/` | arm64 共通。GICv2 の `GICD_ISENABLER`/`ISPENDR` と，プロセッサ毎ハンドラ表 `_kernel_p_inh_table`（`core_kernel_impl.h`）を読む |

- `target_os_awareness.py` → `chip_os_awareness.py` → `core_os_awareness.py` と `import` で連鎖（各層が
  `__file__` から次層の相対パスを `sys.path` に追加）。
- `os_awareness.py` は **任意 import**（`import target_os_awareness`）。読めれば `intr` に
  ena/pend/handler 列を出し，読めなければその列を省く。`make osdebug` はターゲット依存部のパスを
  `sys.path` に追加してから読み込む。
- GIC レジスタ読み出しのため実機/コアダンプ接続（halt）が必要。MMU 有効でも周辺(0x4ac1xxxx)は
  device 領域として読める。ハンドラ表は const（.rodata）なので **ELF 単体（静的）でも読める**。

## 適用範囲

- `os_awareness.py` は **FMP3（AArch64）** のカーネルシンボル（`_kernel_*`）と `kernel_cfg.h`
  にのみ依存し，ボード非依存。AArch64 の FMP3 ターゲット一般で使える。
- 一方 `make osdebug` や `monitor stm32mp25x.a35_1 ...` は **STM32MP257F-DK ターゲット固有**。
- arm64 の優先度ビットマップ（`PRIMAP_BIT(pri)=0x8000>>pri`, MSB 詰め）に依存する。

## 設計・拡張

開発者向けの内部構造・拡張方法は同ディレクトリの `CLAUDE.md` を参照。
