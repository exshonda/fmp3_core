# FMP3 動的 ISR 生成（acre_isr/del_isr）設計書

**Goal:** `feature/dynamic-creation` ブランチ上で、割込みサービスルーチンの動的生成
（`acre_isr`/`del_isr` + 予約 API）を**案B: ハイブリッド方式**で実現する。
静的 ISR のみの intno は現行の平坦なインライン連鎖を無変更維持し、明示的に opt-in した
intno のみランタイムキュー方式に切り替える。

**方式裁定の経緯:**
- 案B（ハイブリッド＋API 拡張）はユーザ裁定済み（2026-08-04。「ISR のコア指定に関しては
  API の拡張ないし追加を許容する」）。
- Codex 外部レビュー（2026-08-04、3件とも独立検証で真と判定）の指摘を本 spec で解決する:
  ①走査再開位置の安定キー ②del_isr 後の実行中 ISR の寿命意味論 ③intno 検証のコア非依存化。
- 背景事実の詳細は `.superpowers/sdd/isr-policy-for-review.md`（方針書）と
  `.superpowers/sdd/stage3-isr-design-notes.md`（設計メモ）。

**参照:**
- dcre 原典: `/home/honda/TOPPERS/ASP3/asp3_3.7/extension/dcre/kernel/interrupt.{h,c,trb}`
  （ISRCB・isr_queue・call_isr・enqueue_isr・search_isr_queue の型。現行ソースが正）
- FMP3 現行: `kernel/interrupt.trb:411-477`（インライン連鎖生成）、
  `kernel/interrupt.{h,c}`、`kernel/time_event.c:709-768` / `kernel/cyclic.c:518-548`
  （割込み文脈での giant lock 取得の先例）

---

## Global Constraints

1. ブランチ `feature/dynamic-creation`。pristine 改変は DIVERGENCE_MAP 記録。
2. **既存構成への影響ゼロが本方式の存在理由**: opt-in の無い構成では、生成コード・
   実行時挙動・管理された差分がすべて不変であること（恒常出力の追加は AID 系の
   最小限に留め、許容リスト検査で固定する）。
3. acre_isr/del_isr/T_CISR は dcre 標準のシグネチャ・フィールドを維持
   （isratr, exinf, intno, isr, isrpri）。**コア引数は追加しない** — 動的 ISR の実行コアは
   対象 intno のクラス affinity に従う（コア指定は「どの intno を選ぶか」で表現される）。
4. 検証 = F-1（両エンジン同時・equivalence）+ 実コンパイル + QEMU 機能テスト。
5. FIFO・訂正C（MYSTATE）等、段階1〜3で確立した規約を踏襲。
6. rc=124 単独成功判定禁止・パイプ判定禁止・QEMU 個別実行＋pgrep。

---

## 1. API 定義

### 1.1 予約 API（案B-2 — 推奨で確定。B-1 との比較は §7）

```
AID_ISR .noisr                 ← dcre と同一（グローバル CB プール予約・クラス外専用）
ENA_DYNISR .intno              ← FMP3 拡張（intno を動的 ISR 対象として opt-in）
```

- `ENA_DYNISR(intno)` は **CFG_INT(intno) と同一クラスの囲みの中**に書く
  （CRE_ISR と CFG_INT の同一クラス規則と同じ。E_RSATR で検査）。
- ENA_DYNISR された intno は、静的 CRE_ISR の有無にかかわらずランタイムキュー方式の
  ディスパッチ（§3）に切り替わる。ENA_DYNISR の無い intno は現行のインライン連鎖のまま。
- ENA_DYNISR に対応する CFG_INT が無い場合はエラー（dcre の isr_queue 生成前提と同型）。
- DEF_INH が競合する intno への ENA_DYNISR はエラー（dcre interrupt.trb:263-270 と同じ除外則）。

### 1.2 サービスコール（dcre 標準）

```c
extern ER_ID	acre_isr(const T_CISR *pk_cisr) throw();
extern ER		del_isr(ID isrid) throw();
```

T_CISR は dcre include/kernel.h の定義そのまま（isratr, exinf, intno, isr, isrpri）。
TFN_ACRE_ISR(-204)/TFN_DEL_ISR(-220) は既存（確認済み）。

### 1.3 エラー（dcre 準拠 + Codex #3 対応）

- `acre_isr`: CHECK_VALIDATR(isratr, TARGET_ISRATR)、VALID_INTNO_CREISR(intno)、
  FUNC_ALIGN/NONNULL(isr)、VALID_ISRPRI(isrpri)。
  **intno の適格性検査は per-core ビットマップ（check_intno_cfg）を使わず**、cfg が生成する
  **グローバルな dynamic-eligible intno 表**（§2）を二分探索する — 呼出しコアに依存しない
  （Codex #3。ENA_DYNISR されていない intno は E_OBJ — dcre の search_isr_queue 失敗と同型）。
  free-list 空は E_NOID。
- `del_isr`: E_NOEXS（TA_NOEXS）→ E_OBJ（静的 = isrid <= tmax_sisrid）→ 成功（§4 の
  quiesce 意味論）。

## 2. cfg 層

- `AID_ISR(n)`: グローバル ISRCB プール（`_kernel_aisrinib_table[n]`・予約 ISRCB・
  `_kernel_tmax_sisrid` — 段階1〜3a と同じ KernelObject 枠組み。ISR には従来
  ランタイムオブジェクトが無いため、**ISRCB/ISRINIB 型の新設**（§3）とセット）。
- `ENA_DYNISR(intno)` の効果（両エンジン同時実装）:
  1. 当該 intno の生成ディスパッチを、インライン連鎖から
     `_kernel_inthdr_<intno>(){ _kernel_call_isr(&_kernel_isr_queue_table[i]); }` へ切替
  2. `_kernel_isr_queue_table[]`（QUEUE 配列）と、intno 昇順ソート済みの
     `_kernel_isr_queue_list[]`（{intno, queue*} 対 — **グローバル適格 intno 表**、
     acre_isr の二分探索対象）を生成
  3. 当該 intno の静的 CRE_ISR は `_kernel_isrorder_table[]` 経由で初期化時に
     isrpri 順挿入（dcre interrupt.trb:338-346 の型）
  - クラスの affinity が複数コアに跨る intno では、per-prcid の DEF_INH 生成
    （interrupt.trb:411-441 の既存機構）を維持 — 同一キューを複数コアが走査しうる
    （§5 の同期設計が前提）。
- opt-in 無し構成の生成物: AID_ISR 関連の恒常出力（TNUM_SISRID 等、段階1〜3a と同型）
  のみ許容リストに追加。ENA_DYNISR 無しなら isr_queue_* は一切生成しない。
- エラー回帰: AID_ISR in-class / ENA_DYNISR のクラス不一致 / CFG_INT 無し intno への
  ENA_DYNISR / DEF_INH 競合 / no-static（訂正E ガード）。

## 3. カーネル層（新設ランタイムオブジェクト）

dcre interrupt.h の ISRINIB/ISRCB を移植し、FMP3 の MP 対応を加える:

```c
typedef struct isr_initialization_block {
	ATR			isratr;			/* ISR属性 */
	EXINF		exinf;			/* ISRの拡張情報 */
	QUEUE		*p_isr_queue;	/* 登録先ISR呼出しキュー */
	ISR			isr;			/* ISRの先頭番地 */
	PRI			isrpri;			/* ISR優先度 */
} ISRINIB;

typedef struct isr_control_block {
	QUEUE		isr_queue;		/* キューエントリ（free-list と共用） */
	const ISRINIB *p_isrinib;
	uint32_t	isrseq;			/* enqueue 世代番号（Codex #1 — 走査継続キー） */
	uint_t		running;		/* 実行中コアのビットマップ（Codex #2 — quiesce 用） */
} ISRCB;
```

- キューは (isrpri, isrseq) の辞書式昇順。isrseq はキューごとの単調カウンタから
  enqueue 時に採番（del/再 acre で再利用されても新しい seq が付くため曖昧にならない）。
  **u32 wrap は 2^32 回の enqueue で発生**: キューが空になったときカウンタを 0 に
  リセットして実用上の到達不能性を保証する（空にならないまま 2^32 回は非現実的、
  spec 制約として明記）。
- `free_isrcb`（グローバル・FIFO）、initialize_isr（マスタのみ、静的 ISR の isrorder 挿入
  + 動的スロット TA_NOEXS 化）。
- ISRID マクロ: 2レンジ（段階1〜3a と同型）。

## 4. del_isr の寿命意味論（Codex #2 — quiesce 方式で確定）

**保証: del_isr が E_OK を返した時点で、対象 ISR は実行中でなく、以後実行されない**
（dcre 単一コアと同等の強い意味論）。

実装:
1. giant lock 下で E_NOEXS/E_OBJ 検査 → キューから unlink（以後、新たな走査は拾わない）。
2. `p_isrcb->running != 0`（他コアで当該 ISR 本体が実行中）なら、
   **glock と CPU ロックを解放して待機し、再取得して再確認するループ**で
   running == 0 を待つ（quiesce）。
   - デッドロック不成立の論証: running を立てるのは割込み文脈のウォーカー（§5）で、
     ISR 本体の完了により必ずクリアされる。del_isr を呼ぶタスクは待機中 CPU ロックを
     解放しているため、自コアへの割込み配送も阻害しない。同一コアで「実行中 ISR を
     del」は文脈上不可能（割込み文脈がアクティブな間、同コアのタスクは走らない）。
   - 待機時間は ISR 本体の実行時間で有界（TOPPERS の ISR 短時間規約）。
3. quiesce 完了後、TA_NOEXS → free-list 返却。
4. ユーザへの規約明記: del_isr の完了後は exinf の指す資源を安全に解放できる。

## 5. ランタイムキュー走査（call_isr の MP 版 — Codex #1 対応）

```
walk(queue):
  lock_cpu 状態確認; acquire_glock            ← signal_time/call_cyclic の先例に従う
  cur = (pri=-∞, seq=-∞)
  loop:
    next = queue 内で (isrpri,isrseq) > cur の最小要素   ← 安定キーによる再決定
    if 無し: break
    cur = (next->isrpri, next->isrseq)
    isr/exinf をローカルへコピー; next->running |= 自コア bit
    release_glock; unlock_cpu                  ← ISR 本体はロック外（現行連鎖と同じ）
    (*isr)(exinf)
    if sense_lock(): force_unlock_spin(my_pcb) ← ISR のロック放置への防御（現行連鎖踏襲）
    lock_cpu; acquire_glock
    next->running &= ~自コア bit               ← ★unlink 済みでも ISRCB は quiesce まで
                                                  free されないため安全に書ける（§4-2/3）
  release_glock; unlock_cpu（または元のロック状態へ復元）
```

- **取りこぼし・二重実行の不成立**（Codex #1 のシナリオで検証）:
  A(10,s5) 実行中に del(A)+acre(C,10→s7): 再取得後 cur=(10,s5) より大きい最小 =
  B(10,s6) → C(10,s7) → D(20,…)。A は再実行されず B も飛ばされない。
  再利用された旧 A のスロットが C になっても s7 > s5 で曖昧さなし。
- 同一 intno の複数コア並行走査: 各ウォーカーが独立に (pri,seq) を進め、
  running はビットマップなので互いに上書きしない。同一 ISR が2コアで同時実行される
  ことは**ありうる**（静的インライン連鎖でも同じ — FMP3 の既存意味論を維持）。
  del_isr の quiesce は running == 0（全コアのビットが消えること）を待つ。
- ディスパッチ経路の追加コストは opt-in した intno のみに閉じる（Global Constraint 2）。

## 6. テスト（test_dcre5、musca_b1-2core、AID_ISR + ENA_DYNISR）

1. 静的のみ intno のディスパッチが不変であること（test_int2 の非退行が兼ねる）
2. acre_isr → 割込み発生 → ISR 実行（isrpri 順、静的 ISR との混在順序）→ del_isr
3. **quiesce の実証**: 長めの ISR 実行中に別コアから del_isr し、del が ISR 完了まで
   返らないこと（時間測定または順序 checkpoint）
4. 走査中 del/acre の安定キー実証（同一 isrpri 複数 ISR で B の取りこぼし・A の
   二重実行が無いこと — 決定的に組めるかは計画時に検討、無理なら変異 control で代替）
5. E_NOID/E_NOEXS/E_OBJ（未 ENA_DYNISR intno への acre）
6. カーネル変異 negative control + 非退行全テスト

## 7. 案B-1 との比較（記録）

B-1（`AID_ISR(intno, n)` — intno 別プール）は E_NOID の粒度が intno 単位になり
診断性で勝るが、①dcre の AID_ISR 意味論（グローバルプール）から逸脱する、
②適格 intno 表と CB 容量が結合し cfg 検証が複雑化する、③Codex #3 も B-2 の分離を
仕様化しやすいと評価 — により **B-2 を採用**。B-1 が必要になったら後方互換で
追加可能（ENA_DYNISR に個数引数を足す拡張余地を残す）。

## 8. 実装前確認リスト（計画 Task 1）

1. dcre ISRINIB/ISRCB/call_isr/enqueue_isr/search_isr_queue/initialize_isr の転写元
   行番号確定と、running/isrseq フィールド追加の влияние（サイズ・初期化）
2. FMP3 の inthdr 生成（interrupt.trb:411-477）の per-prcid DEF_INH 機構と
   キュー方式切替の結合点（isr_flag ガードとの整合）
3. 割込み文脈での acquire_glock の作法（signal_time の実装詳細 — ネスト・
   すでに glock 保持中の場合の扱い）
4. TARGET_ISRATR・VALID_ISRPRI・TMIN/TMAX_ISRPRI の FMP3 既存有無
5. ENA_DYNISR の静的 API 文法（kernel_api.def 記法・クラス内 API の先例 = CFG_INT）
6. quiesce ループの待機プリミティブ（busy-wait + glock 再取得の既存先例 —
   spin_lock.c の型）と、待機中の sil_dly_nse 等の要否
7. u32 isrseq のキュー空リセットの実装点（queue_empty 検査の挿入箇所）
8. 走査の「元のロック状態へ復元」が必要か（inthdr 入口のロック状態 — 現行連鎖の
   sense_lock 分岐と同じ扱いでよいか）

## 9. 統治

- 8タスク構成想定（実装前確認 → cfg 両エンジン〔切替・表生成〕→ ISRCB/初期化 →
  walk/call_isr → acre/del（quiesce）→ test_dcre5 → 最終回帰。分割は計画で確定）。
- 着手は段階3b 完了後（裁定済みの順序: 3a → 3b → ISR）。
- pristine 編集は DIVERGENCE_MAP。上流報告候補は従来4件。
