# FMP3 動的生成API 段階2（cyc/alm）設計書

**Goal:** `feature/dynamic-creation` ブランチ上で、周期通知（`acre_cyc`/`del_cyc` +
`AID_CYC`）とアラーム通知（`acre_alm`/`del_alm` + `AID_ALM`）の動的生成を、
段階1（タスク + DEF_MPK、spec `2026-08-03-fmp3-dynamic-creation-design.md`）が
構築した共通基盤の上に dcre 忠実移植で実現する。あわせて arm_m の
`core_rename.def` 欠落（sense_lock/unlock_cpu）を修正する。

**参照:**
- 段階1 spec: `docs/superpowers/specs/2026-08-03-fmp3-dynamic-creation-design.md`
  （§4.5 の共通基盤・Global Constraints・§2.3 の MP 安全性論証の形式を継承）
- dcre 原典: `/home/honda/TOPPERS/ASP3/asp3_3.7/extension/dcre/`
  （`kernel/cyclic.{h,c,trb}`・`alarm.{h,c,trb}`・`include/kernel.h`。
  DIFF ファイルより**現行ソースを正**とする — 段階1 Task 3 で確立した規約）

**スコープ外:** `acre_isr`/`del_isr`。FMP3 にはランタイム ISR オブジェクト
（ISRINIB/ISRCB/isr_queue）が存在せず（ISR は cfg 生成時に intno ごとの平坦な
呼出し列 `_kernel_inthdr_<intno>` へ消し込まれる）、移植は新規サブシステム構築に
相当するため別計画とする。旧 spec §7 の「ISR は interrupt.h:82-86 の inib 同型」は
誤り（実体は INTINIB＝割込み線の管理ブロック）であり、本設計と同時に訂正する。

---

## Global Constraints（段階1から継承。実装中に再解釈しない）

1. ブランチ `feature/dynamic-creation`（段階1の続き）。**main へはマージしない。**
   pristine への改変は `DIVERGENCE_MAP.md` に記録（種別 `mod (dcre-port)`、上流報告欄 `-`）。
2. 段階2 = `acre_cyc`/`del_cyc`・`acre_alm`/`del_alm` + `AID_CYC`/`AID_ALM` +
   ランタイム通知機構 + arm_m rename 修正のみ。ISR・段階3のタスクを含めない。
3. API 面は dcre 標準のみ：`T_CCYC`/`T_CALM` は **`T_NFYINFO` を含む dcre 定義そのまま**
   （全通知モードをサポート — A案）。独自 API なし。`acre_*` にクラス引数なし。
   `AID_CYC`/`AID_ALM` はクラス外専用（クラス内は E_RSATR）。
4. 動的生成 cyc/alm は **`iprcid = 1`（PRC1）、`affinity = (1U << TNUM_PRCID) - 1`
   （全プロセッサ）** をカーネルが固定で埋める。`msta_cyc`/`msta_alm` により任意の
   （p_tevtcb を持つ）プロセッサへ移動可能。
5. 検証 = F-1：Ruby `.trb` にも同時移植し `tools/cfg_equivalence.sh`
   （exit 0=一致/1=不一致/2=前提未充足であり合格ではない）を主検査に維持。
6. CB はヒープ確保しない。予約 CB（named static + ポインタ表末尾）+ RAM inib 配列。
   free-list のリンクには **tmevtb 領域を転用**する（dcre cyclic.c:118-121 の技法）。
7. 汎用層 `CMakeLists.txt`・`fmp3_core.cmake`・`cfg_py/pass1.py`・`cfg_py/pass2.py` は変更しない。
8. `rc=124` 単独を成功判定に使わない。`cmd | tail`/`cmd | grep` で成否判定しない。

---

## 1. API 定義

### 1.1 パケット型（dcre include/kernel.h:300-315 と同一。include/kernel.h の
T_CTSK（段階1追加）の後に置く）

```c
typedef struct t_ccyc {
	ATR			cycatr;		/* 周期通知属性 */
	T_NFYINFO	nfyinfo;	/* 通知方法 */
	RELTIM		cyctim;		/* 周期通知の通知周期 */
	RELTIM		cycphs;		/* 周期通知の通知位相 */
} T_CCYC;

typedef struct t_calm {
	ATR			almatr;		/* アラーム通知属性 */
	T_NFYINFO	nfyinfo;	/* 通知方法 */
} T_CALM;
```

`T_NFYINFO` は FMP3 の include/kernel.h に既存（静的 API 用に定義済み）である
ことを実装前に確認する。無ければ dcre 定義を移植する。

### 1.2 サービスコール

```c
extern ER_ID	acre_cyc(const T_CCYC *pk_ccyc) throw();
extern ER		del_cyc(ID cycid) throw();
extern ER_ID	acre_alm(const T_CALM *pk_calm) throw();
extern ER		del_alm(ID almid) throw();
```

機能コード `TFN_ACRE_CYC`/`TFN_DEL_CYC`/`TFN_ACRE_ALM`/`TFN_DEL_ALM` が
`include/kernel_fncode.h` に既存かを実装前に確認（段階1では TFN_ACRE_TSK/-193 が
既存だった。無ければ dcre の値で追加し台帳記録）。

### 1.3 エラーコード（dcre 準拠）

- `acre_*`: E_PAR（属性・nfyinfo・cyctim/cycphs 検査）、E_NOID（free-list 空）。
  スタック確保が無いため **E_NOMEM 経路は無い**（タスクとの相違点）。
- `del_*`: E_NOEXS（TA_NOEXS）、E_OBJ（静的生成 = id <= tmax_s*id）。
  **DORMANT 相当の前提条件なし** — 動作中でも削除可（動作中なら
  tmevtb_dequeue で停止してから free-list へ返却。dcre cyclic.c:262-264 と同一）。

---

## 2. cfg 層（両エンジン同時変更）

### 2.1 kernel_api.def

dcre と同一の2行を追加：

```
AID_CYC .nocyc
AID_ALM .noalm
```

### 2.2 KernelObject 共通枠組み

段階1で一般化済み（`@aidapi` 登録有無によるガード、`kernel.trb:131-137` /
`kernel.py` 同等箇所）のため、**共通枠組みの変更は不要**。AID_CYC/AID_ALM の
登録により `TNUM_CYCID`（総数）・`TNUM_SCYCID`・`_kernel_tmax_scycid`・
RAM `_kernel_acycinib_table[]`（または `TOPPERS_EMPTY_LABEL`）・予約
`_kernel_acyccb_<i>` + `_kernel_p_cyccb_table` 末尾追加が自動で出る。alm も同型。
クラス外専用検査（E_RSATR）も共通枠組みが担う（段階1で DEF_MPK に施したのと
同じ規約が AID_* には既に入っている — 実装前に cyclic/alarm で発火することを確認）。

### 2.3 cyclic.trb/.py・alarm.trb/.py の個別変更

- 静的 inib 出力のサイズトークンを `TNUM_CYCID` → `TNUM_SCYCID`（alm 同様）へ。
- 動的分の nfyinfo テーブル出力：`AID_CYC` の個数 N > 0 のとき
  `T_NFYINFO _kernel_acyc_nfyinfo_table[N];`（RAM・非const）、N=0 のとき
  EMPTY_LABEL。alm 同型（dcre cyclic.trb/alarm.trb の該当ハンクを両エンジンへ）。
- `torder` 相当は cyc/alm に無い（タスク固有）ため対象外。

### 2.4 既存構成への影響（管理された差分）

AID 無し構成の生成物差分は、段階1 Task 2 Step 7 と同じ方式で**許容リストとの
完全一致**を検査する。許容されるのは cyc/alm ブロックへの
TNUM_S*ID/tmax_s*id/EMPTY_LABEL 追加とサイズトークン変更のみ（タスクのときと同型）。

---

## 3. カーネル層

### 3.1 free-list（kernel/cyclic.c・alarm.c）

```c
#ifdef TOPPERS_cycini
QUEUE	free_cyccb;		/* 使用していないCYCCBのリスト（リンクは tmevtb 領域を転用） */
#endif
```

CYCCB/ALMCB には専用のキュー領域が無いため、dcre cyclic.c:118-121 の技法どおり
**tmevtb 領域を QUEUE として転用**する（`(QUEUE *) &(p_cyccb->tmevtb)`）。
FMP3 の CYCCB/ALMCB にも tmevtb はあり（cyclic.h:75、alarm.h:70）、QUEUE より
大きいことを実装前に static assert 相当（コンパイル時 or 目視）で確認する。

### 3.2 initialize_cyclic / initialize_alarm

- 静的ループの境界を `tnum_cyc` → `tnum_scyc`（alm 同様）。既存の
  「`iprcid == p_my_pcb->prcid` のものだけ各プロセッサが初期化」という
  per-processor フィルタは静的分についてそのまま維持。
- 動的スロット（`tnum_scyc..tnum_cyc`）は**マスタプロセッサのみ**が初期化：
  `acycinib_table[j].cycatr = TA_NOEXS;`、`p_cyccb->p_cycinib` を RAM inib へ、
  `p_cyccb->p_pcb = get_pcb(1);`、free_cyccb へ挿入（FIFO。段階1と同じ
  queue_insert_prev/queue_delete_next 規律 — **LIFO 化しない**）。
  可視性は barrier_sync が保証（段階1 task 港と同じ論証）。

### 3.3 acre_cyc / acre_alm（kernel/cyclic.c・alarm.c に追加）

dcre cyclic.c:171-233 / alarm.c:160-207 を FMP3 のロック規約
（lock_cpu + acquire_glock、段階1 acre_tsk と同じ）へ適応：

- 検査（ロック外）: CHECK_TSKCTX_UNL、CHECK_VALIDATR(cycatr, TA_STA)（alm は
  TA_NULL）、`check_nfyinfo(&(pk_ccyc->nfyinfo))`、cyctim/cycphs の範囲検査
  （dcre と同一式）。
- ロック内: free-list 空なら E_NOID。空きスロットを pop し、inib へ
  cycatr/exinf/nfyhdr/cyctim/cycphs を充填。**Constraint 4**: `iprcid = 1`、
  `affinity = (1U << TNUM_PRCID) - 1`。`p_cyccb->p_pcb = get_pcb(1)`、
  `cycsta = false`。
- 通知: `nfymode == TNFY_HANDLER` なら exinf/nfyhdr を直接格納。それ以外は
  `acyc_nfyinfo_table[スロット番号]` に `T_NFYINFO` をコピーし、
  `exinf = &acyc_nfyinfo_table[i]`、`nfyhdr = notify_handler`（§4）。
- 返値は CYCID(p_cyccb)（§3.5 の 2レンジ対応後のマクロ）。

### 3.4 del_cyc / del_alm

dcre cyclic.c:242-277 / alarm.c:216-251 の FMP3 適応：
E_NOEXS（TA_NOEXS）→ E_OBJ（`cycid <= tmax_scycid`）→ 動作中なら
`tmevtb_dequeue`（FMP3 の版は p_pcb 引数等の差異あり — 現物の stp_cyc の
呼び方に合わせる）→ `cycatr = TA_NOEXS` → free-list へ queue_insert_prev。

### 3.5 ID マクロの2レンジ化

段階1の TSKID と同じ問題が CYCID/ALMID にもある。FMP3 の CYCID/ALMID マクロの
現行実装（inib ベースか CB ポインタ表ベースか）を実装前に確認し、
**inib ベースなら段階1 TSKID と同型の2レンジ版**へ置換する
（`p_cycinib` が `acycinib_table` 範囲内なら動的 ID 式、でなければ静的 ID 式）。
CB ポインタ表の線形位置から引く実装なら変更不要の可能性がある — 現物で判断し、
判断根拠を計画に記録する。

### 3.6 配線

`allfunc.h` へ `TOPPERS_acre_cyc`/`TOPPERS_del_cyc`/`TOPPERS_acre_alm`/
`TOPPERS_del_alm`（TOPPERS_kermem は段階1 Task 3 で追加済み）。
`Makefile.kernel` の cyclic/alarm 行へ .o 追記（KERNEL_FCSRCS は不変）。
`kernel_rename.def` へ free_cyccb/free_almcb/acycinib_table/aalminib_table/
tmax_scycid/tmax_salmid/acyc_nfyinfo_table/aalm_nfyinfo_table/notify_handler 等を
追加し genrename.rb で再生成。

---

## 4. ランタイム通知機構（A案の本体）

FMP3 は静的生成時に通知を cfg が関数合成（`_kernel_cychdr_<id>` 等）して
消し込むため、ランタイムの `T_NFYINFO` 処理機構が存在しない。dcre の機構を移植する：

- `check_nfyinfo(const T_NFYINFO *)` — nfymode・パラメータの妥当性検査
  （E_PAR）。dcre の実装（dcre で notify 系がどのファイルにあるか —
  time_manage.c / kernel_impl.h 近傍 — を実装時に特定し、そのまま転写）。
- `notify_handler(EXINF exinf)` — exinf を `T_NFYINFO *` とみなし、
  nfymode に応じて変数設定／フラグセット／dtq 送信等を行うトランポリン。
  dcre 実装をそのまま転写（内部で呼ぶサービスコールは FMP3 に全て存在する）。
- 格納テーブル `acyc_nfyinfo_table[]`/`aalm_nfyinfo_table[]` は cfg が出力（§2.3）。

**MP 注意**: notify_handler はタイムイベントハンドラ文脈（当該 cyc/alm の
p_pcb プロセッサ）で走る。テーブルスロットの再利用（del → acre）は giant lock
下で行われ、ハンドラ実行との競合は §5 の論証でカバーする。

---

## 5. E_NOEXS 検査と MP 安全性

### 5.1 E_NOEXS 挿入（8関数）

`sta_cyc`・`msta_cyc`・`stp_cyc`・`ref_cyc`・`sta_alm`・`msta_alm`・`stp_alm`・
`ref_alm`。acquire_glock 直後の**最初の分岐**として
`if (p_*cb->p_*inib->*atr == TA_NOEXS) { ercd = E_NOEXS; } else ...`
（段階1と同じ existence-before-state 規約）。
**msta_cyc/msta_alm は dcre に存在しない FMP3 固有関数**であり先例が無い —
sta_cyc/sta_alm と同位置への類推適用であることを計画・レビューで明示する
（段階1の mig_tsk/ras_ter/ter_tsk と同じ扱い）。

### 5.2 MP 安全性の論証（本 spec が段階1 spec §2.3 に相当する記録を持つ）

**del_* vs 実行中ハンドラ**: `call_cyclic`/`call_alarm`（FMP3 版）はハンドラ
呼出しの前後で giant lock を解放/再取得する。ハンドラ実行中（glock 解放窓）に
別コアが del_* を完了し得るが、call_* はハンドラ復帰後に **glock を取り直してから**
CB を再参照するため、TA_NOEXS 化・tmevtb の free-list 転用と衝突しない
（call_* 側の再参照が「削除済み」をどう扱うかは dcre の call_cyclic の再判定
ロジックを FMP3 現物と突き合わせ、必要な分岐だけ dcre から補う —
実装前確認項目）。段階1 spec §2.3 の「自終了タスクのスタック残余ウィンドウ」の
ような未防御窓が cyc/alm に存在するか否かを、計画の中で明示的に結論づける。

**del_* vs 発火済み tmevtb**: 動作中の削除は tmevtb_dequeue で timer キューから
外してから free-list 転用するため、タイマ側から見た dangling は生じない
（dequeue は当該オブジェクトの p_pcb が指すプロセッサのイベントキューに
対して行う — FMP3 の stp_cyc と同じ手順を踏む）。

**前提条件の現物確認**: `initialize_cyclic`/`initialize_alarm` は
`p_my_pcb->p_tevtcb == NULL` のプロセッサでは何もしない構造がある
（cyclic.c:121-123）。**PRC1（マスタ）が p_tevtcb を持たない構成が
5ターゲット・8プリセットに存在しないこと**を実装前に確認する。存在する場合、
Constraint 4（iprcid=1 固定）が成立しないため設計を差し戻す。

---

## 6. arm_m core_rename.def 修正

`arch/arm_m_gcc/common/core_rename.def` に `sense_lock`・`unlock_cpu` を追加し
`genrename.rb` で再生成する。これにより多重 ISR 連鎖を含む構成
（test_int2 等）の生成コード（kernel_cfg.c が `_kernel_sense_lock`/
`_kernel_unlock_cpu` を参照）が musca_b1 でリンク可能になる。

- 段階1で main ブランチでも再現確認済みの pristine 既存ギャップ（上流報告候補）。
- 検証: musca_b1-2core で test_int2 がビルド・QEMU 実行・`TTSP_RESULT: PASS` に
  到達すること（段階1では kria_arm64-1core でしか実証できなかったハーネス経路が
  主検証ターゲットで閉じる）。
- arm64 の core_rename.def が既に同エントリを持つ形式を踏襲する。

---

## 7. テスト（test_dcre2）

`test/test_dcre2.{c,cfg,h}`、musca_b1-2core、`AID_CYC(2); AID_ALM(2);`
（DEF_MPK は不要 — cyc/alm はメモリプールを使わない）：

1. acre_cyc（TNFY_HANDLER）→ sta_cyc → 発火を checkpoint で確認 → stp_cyc →
   del_cyc（停止状態の削除）
2. acre_cyc → sta_cyc → **動作中のまま del_cyc 成功**（dcre 意味論の実証）
3. 削除済み ID への sta_cyc/ref_cyc → E_NOEXS、AID 枯渇 → E_NOID
4. `msta_cyc(id, 2)` で PRC2 へ移動 → 発火が PRC2 で起きることを
   `check_point_prc`（プロセッサ別カウンタ — 段階1 Task 6 の知見）で検証
5. 非ハンドラ通知モード1ケース（TNFY_SETVAR 等）: acre_cyc で変数設定通知を
   生成し、発火後に変数値を確認（notify_handler トランポリンの実証）
6. alm 側: acre_alm → sta_alm → 発火 → 再 sta → del、E_NOEXS/E_NOID
7. カーネル変異 negative control: del_cyc の free-list 返却行を殺して
   再 acre が E_NOID になり FAIL すること（生きた経路であることの実証）

加えて test_int2（§6）と test_dcre1 の非退行、既存エラー回帰8件 +
新規（AID_CYC/AID_ALM クラス内 E_RSATR）の全パス。

---

## 8. 統治

- pristine 編集はすべて DIVERGENCE_MAP.md へ（1ファイル1行以上）。
- 上流報告候補（段階1からの引き継ぎ含む）: arm_m core_rename.def 欠落（§6 で修正）、
  TSKINICTXB コメント逆、mempool 符号比較。
- 全8構成 cfg_equivalence exit=0 維持、QEMU 7構成起動維持。
- 段階3（sem/flg/dtq/pdq/mtx/mpf）は本 spec の機構（特に §4 通知機構と
  管理領域 malloc_mpk の型 — dtq/pdq/mpf で必要）を土台に別計画。
  ISR は前提（ランタイムオブジェクト不在）が異なるため、着手時に専用 spec を書く。

## 9. 実装前確認リスト（計画の最初のタスクで現物確認する）

1. `T_NFYINFO` 型と `TNFY_*` 定数が FMP3 include/kernel.h に既存か
2. `TFN_ACRE_CYC`/`TFN_DEL_CYC`/`TFN_ACRE_ALM`/`TFN_DEL_ALM` の既存有無
3. FMP3 の CYCID/ALMID マクロ実装（2レンジ化の要否判断 — §3.5）
4. CYCCB/ALMCB の tmevtb 領域が QUEUE より大きいこと（free-list 転用の成立条件）
5. 全8プリセットで PRC1 が p_tevtcb を持つこと（§5.2 の前提）
6. dcre の check_nfyinfo/notify_handler の所在ファイルと FMP3 に無いことの確認
7. FMP3 の call_cyclic/call_alarm がハンドラ復帰後に CB を再参照する箇所と
   削除済み CB への耐性（§5.2）
8. AID_* のクラス外専用検査が cyclic/alarm オブジェクトでも共通枠組みで
   発火すること（§2.2）
