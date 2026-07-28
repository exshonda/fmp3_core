# CLAUDE.md — FMP3 OS Awareness (gdb) 開発ガイド

`os_awareness.py`（gdb Python による TOPPERS/FMP3 OS Awareness, Step2）の内部構造と
拡張方法をまとめる。利用方法は同ディレクトリの `README.md` を参照。

## 構成

- `os_awareness.py` — gdb Python スクリプト。`source` するとコマンド `stask` / `atask` / `sem`
  を登録する。FMP3（AArch64）のカーネルシンボル（`_kernel_*`）と `kernel_cfg.h` に依存。
- 実行は **Python 対応 gdb（`gdb-multiarch`）** が前提（`aarch64-none-elf-gdb` は Python 非対応）。

## 参照するカーネル構造（fmp3_3.3_svn/kernel）

- タスク: `_kernel_tinib_table[]`(TINIB, 静的) / `_kernel_p_tcb_table[]`(TCB*, 動的, index=tskid-1) /
  `_kernel_tmax_tskid`。状態 `tstat` は `task.h` の `TS_*`（DORMANT0/RUNNABLE1/SUSPENDED2/
  WAITING_* は bit2〜, `TS_WAITING_MASK`）。RUNNING は PCB の `p_runtsk` と一致で判定。
- プロセッサ: `_kernel_p_pcb_table[]`(PCB*)。PCB に `prcid`/`p_runtsk`/`p_schedtsk`/
  `ready_queue[TNUM_TPRI]`/`ready_primap`。レディキューは **per-processor**（SMP）。
- 同期/通信: 各 `_kernel_<obj>inib_table[]`(静的) と `_kernel_p_<obj>cb_table[]`(動的, CB* 配列)。
  CB 先頭は `wait_queue`（WOBJCB 共通, offset0）。**DTQ/PDQ は `swait_queue`/`rwait_queue` の2本**。
- 定数: `TMIN_TSKID=1`, `TMIN_SEMID=1`, `TMIN_TPRI=1`, `TMAX_TPRI=16`(→`TNUM_TPRI=16`)。
  **外部優先度 = 内部優先度 + TMIN_TPRI**。`TA_ACT=0x02`, `TA_TPRI=0x01`。

## 重要な依存・注意

- **優先度ビットマップ**: arm64 は `PRIMAP_BIT(pri)=0x8000>>pri`（MSB 詰め, CLZ サーチ用,
  `arch/arm64_gcc/common/core_kernel_impl.h`）。`ready_primap` のビット b ↔ 内部優先度 `15-b`
  （外部 `16-b`）。`ready_queue[]` は内部優先度で添字付け。
- **SMP 一貫スナップショット**: 両コアを halt してから読む（README 参照）。a35_0 のみ halt だと
  Core1 走行中で PRC2 のキューが不整合。
- **キュー walk の NULL ガード必須**: 正常な空キューは `p_next==head`（自己ループ）だが，未初期化
  （静的 ELF や生成前 CB）は `p_next==0`。`node==0` でも停止しないとクラッシュする。

## 名前解決（kernel_cfg.h）

`-g`（`-g3` でない）ビルドでは CRE_TSK/CRE_SEM 名はマクロで ELF に無いため，`kernel_cfg.h` を
解析して ID→名前を得る（`_load_obj_names`）。kernel_cfg.h は種別ごとに `#define TNUM_<X>ID n`
の後に `#define <名前> <id>` が並ぶので，`TNUM_<X>ID` 〜 次の `TNUM_` を種別 X の名前とする。
探索順は ELF と同じディレクトリ → cwd → `./gen`。無ければ数字のみ表示。

## 共通ヘルパ

- `_eval(expr)` … `gdb.parse_and_eval`。
- `_tid_label(id, names)` … `名前(ID)` または `ID`。
- `_tcb_to_tid(p_tinib, base, sz)` … `p_tinib` から TID 逆算（TSKID マクロ相当）。
- `_walk_taskqueue(head, tcb_ptr_t, ...)` … QUEUE を辿り TID ラベル list。`task_queue`/`wait_queue`
  が TCB 先頭なので node を TCB* にキャストするだけ。NULL/自己ループで停止。
- `_resolve_wobj(tstat, p_wobjcb, objnames)` … 待ち種別から CB 表を選び `p_wobjcb` を逆引きして
  オブジェクト名(ID)へ。`_WOBJ_TABLE` が種別→表の対応。

## オブジェクトコマンドの実装（汎用フレームワーク）

同期・通信オブジェクト（SEM/DTQ/PDQ/FLG/MTX/MPF）は **spec 駆動の汎用ダンプ** に統合済み:
- `_OBJ_SPECS[name]` … 種別ごとの仕様（`typekey`/`label`/`tmax`/`inib`/`cb`/`cols`/`wq`）。
  - `cols`: `(header, src['inib'|'cb'], field, fmt|None, width)`。`fmt` は `f(gdb.Value, ctx)->str`
    （`_fmt_attr`=TPRI/TFIFO, `_fmt_hex`=ビットパターン, `_fmt_tcb`=TCB*→タスク名, None=int）。
  - `wq`: `[(wait_queue field, ラベル)]`。単一 `wait_queue` か DTQ/PDQ の `swait/rwait`(snd/rcv)。
- `_show_objects(spec, arg)` … tmax を読み（0 なら `no <X> objects`），全/指定 ID を静的(INIB)＋
  動的(CB)＋待ちキューで表示。ID/名前解決は `_load_obj_names().get(typekey)`。
- `_make_objcmd(name)` … spec から gdb.Command を生成し help(docstring)を設定。末尾の
  `_OBJ_CMD_NAMES` に名前を足すだけで新コマンドが増える。

### 新しい同期・通信オブジェクトを足す
1. `ptype <OBJ>INIB` / `<OBJ>CB` でフィールドを確認。
2. `_OBJ_SPECS` にエントリを追加（テーブル名 `_kernel_<obj>inib_table` / `_kernel_p_<obj>cb_table`
   / `_kernel_tmax_<obj>id`，cols，wq）。
3. `_OBJ_CMD_NAMES` に名前を追加。**情報が少ないオブジェクトは静的＋動的を 1 コマンドに**（既定）。

### 割込み（intr）・スピンロック（spn）— 個別コマンド
- `intr`（`IntrCmd`）: `_kernel_intinib_table`（INTINIB: intno/intatr/intpri/iprcid/affinity, 件数
  `_kernel_tnum_cfg_intno`）を列挙。**INTNO は `(prcid<<16)|INTID` エンコード**なので下位 16bit を
  INTID として表示。INH(`inhinib_table`)/ISR 専用表は本ビルド非搭載（`_kernel_tnum_def_inhno` 無し）。
  **handler 列**: プロセッサ毎ハンドラ表 `_kernel_p_inh_table[prcidx][intid]`（arm64 共通,
  `core_kernel_impl.h`）の FP をターゲット依存部 `inh_handler()` 経由で読み，`_sym_for` で関数名へ。
  プロセッサは INTNO 上位 16bit を優先（無ければ iprcid）。ATT_ISR はコンフィギュレータ生成の
  ラッパ `_kernel_inthdr_<n>` 経由なので，**kernel_cfg.c を解析**（`_load_isr_map`,
  `((ISR)(関数))((EXINF)(値))` を正規表現抽出, 複数 ISR 対応）して `実ISR名(exinf=値) [ISR]` に解決。
  ハンドラ表は const なので ELF 単体でも表示可（ena/pend のみ実機要）。
  **ディスパッチ IPI の注記**: `USE_BYPASS_IPI_DISPATCH_HANDER` 定義時は DEF_INH されず表は
  default のまま asm（core_support.S の IRQ 入口）が直接ディスパッチャへ分岐するため，
  `(dispatch IPI: bypassed, handled in asm)` と表示。判定は core 層 `is_dispatch_ipi_bypassed()`
  （①INTID がディスパッチ IPI 番号〔TZ_S=12/非TZ=0, gic_ipi.h〕②表が default
  ③`_kernel_dispatch_handler` シンボルが ELF に無い〔=OMIT_DISPATCH_HANDLER〕）。
- `spn`（`SpnCmd`）: `_kernel_spninib_table`（SPNINIB: spnatr/lock, 件数 `_kernel_tmax_spnid`）を列挙し，
  保持プロセッサは各 `PCB.p_locspn` が `&spninib_table[k]` を指すかで判定（`PRCn`/`free`）。

### ターゲット依存の割込み状態（GIC）・ハンドラ表 — レイヤ構造
割込みの許可/禁止・ペンディングは GIC レジスタ依存，ハンドラ表は arm64 共通部の実装依存なので，
**FMP3 ソース階層に対応したレイヤ**でターゲット依存部に置く（`os_awareness.py` 本体はボード非依存を維持）:
- `target/stm32mp257f_dk_arm64_gcc/target_os_awareness.py` … ボード依存（今回は追加なし→chip 再エクスポート）。
- `arch/arm64_gcc/stm32mp2/chip_os_awareness.py` … SoC 依存（`GICD_BASE=0x4AC10000`，core を呼ぶ。
  `inh_handler` はチップ固有の知識が無いので core のものを再エクスポート）。
- `arch/arm64_gcc/common/core_os_awareness.py` … arm64 共通（GICv2: `GICD_ISENABLER`=base+0x100, `ISPENDR`=base+0x200，
  `*(unsigned int*)addr` を `gdb.parse_and_eval` で読む。`inh_handler(prcidx,intid)` は
  プロセッサ毎ハンドラ表 `_kernel_p_inh_table[prcidx][intid]`（`core_kernel_impl.h`）の FP 番地を返す。
  `is_dispatch_ipi_bypassed(intid,addr)` はディスパッチ IPI の asm バイパス判定）。
- 連鎖は `import`：各層が `__file__` から次層ディレクトリを `sys.path.insert` して `import`。
  `os_awareness.py` は冒頭で **任意 import**（`try: import target_os_awareness as _TGT`）。`intr` は
  `_TGT` があれば ena/pend/handler 列を出す。無ければ列を省略。
- `make osdebug` が gdb 起動時に `python sys.path.insert(0,'<TARGETDIR>')` を実行してから
  `os_awareness.py` を source するため，`import target_os_awareness` が解決する。手動起動時も同様に
  ターゲット依存部を sys.path に足せばよい。
- 別ボードへ移す場合：`chip_os_awareness.py`（GICD ベース）と必要なら `target_os_awareness.py` を差し替える。

### スタック使用量（atask の列）— `_stack_use`
`tskctxb.sp`（保存スタックポインタ, `core_kernel_impl.h` の TSKCTXB{sp,pc}）と
`p_tinib`(stk/stksz) から `used=(stk+stksz)-sp`。非実行タスクは正確，実行中は最後の切替時の値（概算）。
休止/範囲外は `-`。高水位マーク方式（パターン埋め走査）は未採用（スタックのパターン初期化が無いため）。

### 時間イベント（CYC/ALM）— 実装済み（別フレームワーク）
CYC/ALM は待ちキューを持たず形が異なるため，専用の `_TEVT_SPECS` / `_show_timeevt` /
`_make_tevtcmd` で実装（コマンド `cyc` / `alm`）。静的=ハンドラ(`nfyhdr`→symbol)・周期/位相(cyc の
`cyctim`/`cycphs`)・割付プロセッサ・affinity，動的=動作状態(`cycsta`/`almsta`→STA/STP)・次回起動
時刻(`tmevtb.evttim`, STA 時のみ)。新しい時間イベント種別は `_TEVT_SPECS` ＋ `_TEVT_CMD_NAMES` に追加。

## 規約

- **`.pyc`（`__pycache__`）をソースツリーに作らない**: 各 `.py` は import を行う前に
  `sys.dont_write_bytecode = True` を設定する（`os_awareness.py` と target/chip 層に設定済み。
  どの層から import されても効くよう、import する側すべてに置く）。
- **gdb 内に表示される文字列（print と docstring）は英語**。ソースコメント（`#`）は日本語可
  （gdb には表示されない）。
- まず ELF 単体（静的）で動作確認し，次に実機（`make osdebug` + 両コア halt）で検証する。
- 検証は `gdb-multiarch`。`obj/obj_stm32mp2`(PRC_NUM=2) でビルドした `fmp` を用いる。
