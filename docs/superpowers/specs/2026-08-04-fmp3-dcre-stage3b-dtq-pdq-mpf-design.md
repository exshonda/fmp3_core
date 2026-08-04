# FMP3 動的生成API 段階3b（dtq/pdq/mpf）設計書

**Goal:** `feature/dynamic-creation` ブランチ上で、データキュー（`acre_dtq`/`del_dtq` +
`AID_DTQ`）・優先度データキュー（`acre_pdq`/`del_pdq` + `AID_PDQ`）・固定長メモリプール
（`acre_mpf`/`del_mpf` + `AID_MPF`）の動的生成を dcre 忠実移植で実現する。
これで dcre 標準の動的生成オブジェクトのうち ISR を除く全てが揃う。

**参照:**
- 段階1 spec（共通基盤・§2.3 残余ウィンドウの受容判断）・段階2 spec・段階3a spec
  （実行済み前例。特に 3a の per-object 型当てはめの型）
- dcre 原典: `/home/honda/TOPPERS/ASP3/asp3_3.7/extension/dcre/kernel/`
  `dataqueue.{h,c}`・`pridataq.{h,c}`・`mempfix.{h,c}`（現行ソースが正、DIFF は参考。
  DIFF:1022,2658 が管理領域確保の型）
- 調査記録: 6オブジェクト構造調査（2026-08-04）

**スコープ外:** ISR（別計画・案B ハイブリッドで裁定済み、Codex レビュー指摘3件の解決を含む
専用 spec を起草する）。

---

## Global Constraints（段階1/2/3a から継承。実装中に再解釈しない）

1. ブランチ `feature/dynamic-creation`。main へマージしない。pristine 改変は
   `DIVERGENCE_MAP.md` に記録。
2. 段階3b = dtq/pdq/mpf の acre/del + AID 3種 + TA_MBALLOC 定義のみ。ISR を含めない。
3. API 面は dcre 標準のみ：`T_CDTQ`/`T_CPDQ`/`T_CMPF`（dcre include/kernel.h の該当定義と
   同一）。独自 API なし。`AID_*` はクラス外専用（E_RSATR）。
4. **プロセッサ親和なし**: 3オブジェクトの INIB に iprcid/affinity は存在しない
   （現物確認済み）。充填コードを書かないこと。
5. 検証 = F-1：両エンジン同時変更、cfg_equivalence exit 0 のみ合格。
6. CB はヒープ確保しない。free-list リンクは dtq/pdq = `swait_queue`、mpf = `wait_queue`
   の直接流用（dcre と同一）。FIFO（裁定済み・再議しない）。
7. **管理領域はメモリプール（malloc_mpk）から確保**し `TA_MBALLOC` を立てる（dcre 忠実）。
   mpf のブロック領域は mpf==NULL のとき自動確保で `TA_MEMALLOC`。
8. 汎用層不変。KERNEL_FCSRCS 不変。訂正C（del_* は CHECK_TSKCTX_UNL_MYSTATE）を踏襲。
9. rc=124 単独成功判定禁止・パイプ判定禁止・QEMU 個別実行＋pgrep。

---

## 1. プール裁定（段階1最終レビュー Important #1 の宿題 — ユーザ承認済み 2026-08-04）

**裁定: 受容継続。** 根拠:

1. **3b の管理領域は構造的にウィンドウ・フリー**である。dtq/pdq/mpf の管理領域
   （p_dtqmb/p_pdqmb/p_mpfmb・mpf ブロック領域）はカーネルが **giant lock 下でのみ**
   CB 経由で参照する。del_* は待ちタスクを E_DLT で全解放（init_wait_queue、glock 下）
   してから TA_NOEXS 化し free_mpk するため、del 完了後は E_NOEXS ゲートが全 API を
   遮断し、「解放済み管理領域への残余参照」の経路が存在しない。
   段階1 §2.3 の自終了スタック窓（スタックは glock **外**で exiting コアが使う）とは
   質的に異なる。**この不存在の論証は 3b 最終レビューで反証を試みること。**
2. 段階1で受容した自終了スタック窓そのものは 3b で変質しない。ただし 3b により
   プールの alloc/free 往復が増え、count==0（全割当解放 → brk リセット → 先頭から再利用）
   状態の出現頻度が変わるため、窓の成立機会の確率分布は変わる。これは事実として
   記録する（定量評価はしない）。
3. hardening（プール再利用の遅延・del_tsk の quiesce 等）は複雑さに見合わない。
   上流 FMP3 自身が ext_tsk の release_glock 後実行という同型の窓を持ち受容している。

## 2. API 定義

### 2.1 パケット型（dcre include/kernel.h の該当定義と同一。T_CMTX の後に置く）

```c
typedef struct t_cdtq {
	ATR			dtqatr;		/* データキュー属性 */
	uint_t		dtqcnt;		/* データキューの容量 */
	void		*dtqmb;		/* データキュー管理領域の先頭番地 */
} T_CDTQ;

typedef struct t_cpdq {
	ATR			pdqatr;		/* 優先度データキュー属性 */
	uint_t		pdqcnt;		/* 優先度データキューの容量 */
	PRI			maxdpri;	/* データ優先度の最大値 */
	void		*pdqmb;		/* 優先度データキュー管理領域の先頭番地 */
} T_CPDQ;

typedef struct t_cmpf {
	ATR			mpfatr;		/* 固定長メモリプール属性 */
	uint_t		blkcnt;		/* 獲得できる固定長メモリブロックの数 */
	uint_t		blksz;		/* 固定長メモリブロックのサイズ */
	MPF_T		*mpf;		/* 固定長メモリプール領域の先頭番地 */
	void		*mpfmb;		/* 固定長メモリプール管理領域の先頭番地 */
}T_CMPF;
```

（正確なフィールド・コメントは dcre 現物から転写。上記は調査時点の要旨 —
実装前確認§8-1 で dcre とバイト照合すること。）

### 2.2 サービスコール

```c
extern ER_ID	acre_dtq(const T_CDTQ *pk_cdtq) throw();
extern ER		del_dtq(ID dtqid) throw();
extern ER_ID	acre_pdq(const T_CPDQ *pk_cpdq) throw();
extern ER		del_pdq(ID pdqid) throw();
extern ER_ID	acre_mpf(const T_CMPF *pk_cmpf) throw();
extern ER		del_mpf(ID mpfid) throw();
```

TFN コードの既存有無は実装前確認（3a では6個とも既存だった）。

### 2.3 確保とエラー（dcre 準拠）

- `acre_dtq`: 検査（dtqatr・dtqcnt 範囲は dcre の式）→ E_NOID（free-list 空を先に）→
  dtqcnt > 0 なら `p_dtqmb = malloc_mpk(sizeof(DTQMB) * dtqcnt)`、NULL なら E_NOMEM、
  成功なら `dtqatr |= TA_MBALLOC`。dtqcnt==0 の扱い（管理領域不要のはず）と
  ユーザ供給 dtqmb（T_CDTQ.dtqmb != NULL）の dcre での扱いは実装前確認§8-2。
- `acre_pdq`: 同型（`sizeof(PDQMB) * pdqcnt` + maxdpri 検査）。
- `acre_mpf`: **2段確保**。①mpf==NULL なら `malloc_mpk(ROUND_MPF_T(blksz) * blkcnt)` +
  `TA_MEMALLOC`（NULL なら E_NOMEM）②管理領域 `malloc_mpk(sizeof(MPFMB) * blkcnt)` +
  `TA_MBALLOC`（NULL なら **①で確保した分を free_mpk してから** E_NOMEM — 巻き戻しの
  有無と順序は dcre 現物照合、実装前確認§8-3）。
- `del_*`: E_NOEXS → E_OBJ（静的）→ 両待ちキュー（dtq/pdq は swait+rwait、mpf は
  wait_queue のみ）を init_wait_queue で E_DLT 解放 → TA_MBALLOC なら管理領域 free_mpk、
  mpf は TA_MEMALLOC ならブロック領域も free_mpk → TA_NOEXS → free-list。
  滞留データ（dtq の未受信データ）は破棄される（dcre 意味論 — テストで実証）。

## 3. cfg 層

- `kernel_api.def` に AID 3行（`AID_DTQ .nodtq` 等、dcre と同一）。
- per-object テンプレート変更ゼロ（3a で3家族目まで実証済みの汎用枠組み）。
- 訂正E ガード・クラス外専用検査は自動適用。エラー回帰 cfg 6件
  （in-class E_RSATR ×3 + no-static E_OBJ ×3。no-static の include 構成は
  3a の serial.cfg 教訓を踏まえ現物確認して組む）。
- 管理された差分・positive control・実コンパイル検査（test_dcre_mix の cfg 拡張が媒体）・
  negative control は 3a と同型。

## 4. カーネル層

- `TA_MBALLOC` を kernel_impl.h へ（`#ifndef` ガード付き `UINT_C(0x4000)`、
  TA_MEMALLOC の隣）。
- free_dtqcb/free_pdqcb/free_mpfcb、initialize_* の動的スロット節（master-only 内・
  非親和）、DTQID/PDQID/MPFID の2レンジ置換、E_NOEXS 23関数
  （dataqueue.c 9: snd/psnd/tsnd/fsnd/rcv/prcv/trcv/ini/ref_dtq、
  pridataq.c 8: snd/psnd/tsnd/rcv/prcv/trcv/ini/ref_pdq、
  mempfix.c 6: get/pget/tget/rel/ini/ref_mpf）— すべて 3a の型どおり。
- 配線: allfunc.h 6行・Makefile.kernel・kernel_rename.def + 再生成。

## 5. MP 安全性

- §1 の裁定どおり: 管理領域はウィンドウ・フリー（glock 下参照のみ + E_NOEXS ゲート）。
  spec として明示し、最終レビューで反証を試みる。
- E_DLT 解放・ディスパッチ尻尾は 3a と同じ既存機構（init_wait_queue / ini_* の型）。
- acre_* の E_NOMEM 巻き戻し経路で free-list の一貫性が保たれること（TCB 相当の
  CB は E_NOMEM 時に pop しない — acre_tsk と同じ「確保成功後に pop」順序。
  dcre 現物の順序を実装前確認§8-4 で確定）。

## 6. テスト（test_dcre4、musca_b1-2core、AID_DTQ(2)/AID_PDQ(1)/AID_MPF(1) + DEF_MPK）

1. acre_dtq → snd/rcv の実通信（データ整合）→ del → E_NOEXS
2. **E_NOMEM 実証**: プールに入らない dtqcnt で acre_dtq → E_NOMEM
   （スロットは残っている状態で — E_NOID との順序の実証）
3. **送信待ち E_DLT**（満杯 dtq に snd_dtq で待たせて del）と
   **受信待ち E_DLT**（空 dtq に rcv_dtq で待たせて del）の両方
4. **滞留データ破棄**: データを入れたまま del → 再 acre → 空であることを確認
5. pdq: acre → 優先度順配送の確認 → del
6. mpf: acre（mpf=NULL 自動確保）→ get/rel → ブロック内容の書込み読出し → del、
   mpf の E_NOMEM（ブロック領域が入らない blksz*blkcnt）
7. 枯渇 E_NOID・静的 E_OBJ・決定形の同一 ID 再 acre
8. カーネル変異 negative control（del_dtq の free-list 返却）
9. 非退行: test_dcre1/2/3・test_dcre_mix・test_int2

## 7. 統治

- 8タスク構成（3a と同型: 実装前確認 → cfg → dtq → pdq → mpf → test_dcre4 →
  最終回帰。hardening 枠は無いので7タスク構成でも可 — 計画で確定）。
- 全 pristine 編集は DIVERGENCE_MAP。上流報告候補は従来4件のまま。

## 8. 実装前確認リスト（計画 Task 1 で現物確認）

1. T_CDTQ/T_CPDQ/T_CMPF の dcre 現物とのバイト照合（§2.1 は要旨）
2. acre_dtq の dtqcnt==0 分岐とユーザ供給 dtqmb/pdqmb/mpfmb の dcre での扱い
   （NULL 必須か受理か。受理なら TA_MBALLOC を立てない分岐がある）
3. acre_mpf の2段確保の巻き戻し（②失敗時に①を free するか）の dcre 現物確認
4. acre_* の「CB pop と確保の順序」（E_NOMEM 時に free-list 不変か）
5. TFN 6コードの既存有無
6. DTQID/PDQID/MPFID マクロの現行実装（3a と同じく既存 inib 方式の置換のはず）
7. DTQMB/PDQMB/MPFMB 型と COUNT/ROUND 系マクロの FMP3 既存有無
   （段階1 COUNT_MB_T の教訓 — cfg が出力するなら定義側も要確認）
8. no-static 回帰 cfg の include 構成（隠れ静的インスタンスの有無を全 syssvc/target で確認）
