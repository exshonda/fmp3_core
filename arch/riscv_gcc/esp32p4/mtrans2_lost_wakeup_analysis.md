# mtrans2 デッドロック解析資料

※ファイル名は調査初期の仮説（lost-wakeup）に由来する．**確定した真因は
dispatch-IPI storm による livelock** であり，lost-wakeup ではない（§1で棄却済み）．

最終更新: 2026-06-28 / 対象: TOPPERS/FMP3 (Release 3.4.0) を ESP32-P4 デュアル RISC-V HP コアに
SMP 移植したもの．FMP3 カーネルテスト `test_mtrans2` のみが決定論的にハングする問題の解析資料．

> **本書の位置づけ**: 本書は設計・調査の記録であり，利用手順書ではない．利用者向けの
> 入口は `chip_user.md`（`arch/riscv_gcc/esp32p4/`）および `target_user.md`
> （`target/m5stamp_esp32p4_gcc/`）を参照．本書は上記「最終更新: 2026-06-28」時点の
> 記録であり，現在の実装と異なる場合がある（利用者向けの要点は
> `chip_user.md`「割込み処理に関する規定」にも反映済み）．

---

## 1. 真因・根治（確定，2026-06-28，commit 7ac1dd8．このコミットハッシュは git リポジトリ esp32_p4 側のもの）

**真因**: dispatch-IPI(msip) storm による位相共振 livelock．
PRC2 の sus_tsk/rsm_tsk 連打が PRC1 へ msip を ≈983K/s 送り（実測），PRC1 が割込み入口/出口処理
（FP 含む文脈退避・CLIC mret 等）に飽和．TASK1 が割込みの合間に 1 命令も retire できず，
storm を止める唯一の `task2_flag=false` に到達できない自己保持ループ（livelock）．

**実測カウンタ**（2 時点 8s 間隔の差分レート，無摂動ハング直後）:
- `g_raise[PRC1]` ≈ 983,000/s（PRC2 が PRC1 宛 raise_msip）
- `g_msih[PRC1]` = `g_clear[PRC1]` ≈ 655,000/s（msi_handler 入口 = clear_msip 回数）
- `g_ipstuck[PRC1]` ≈ 0（clear 後 CLIC 線3 IP 残存なし）
- `count_run` = 1000 / `count_sus` = 0 / `task2_flag` = 1 が 8s 不変

**実測で棄却した仮説**:
- coupling 不全: ipstuck ≈ 0 → CLINT クリアは CLIC 線3 IP を正しく落とす
- lost-wakeup/アイドル: PRC1 は一度もアイドルに入らない（idle obs の liveness カウンタ = 0）
- CLIC マスク窓: mil=0 / THRESH=0 / CLIC_INT_CTRL(3) IE=1，CTL=0xff → level7 > mil0 かつ > THRESH0
- 可視性順序バグ: release/acquire fence 追加後も解消せず

~~PolarFire(RV64GC) では mtrans2 が完走 → 割込み1回コストが低く飽和しない → P4 固有問題．~~

**★2026-07-19 訂正: 「P4 固有問題」は誤りだった．同じ livelock が全ターゲットで発現する．**

複数ターゲットの実機で対照実験（同一ボード・同一セッションで `TEST_DELAY_TIME_NSE` のみを
変えた比較）を行った結果，本 livelock は **アーキテクチャにも割込みコントローラにも依存しない**
ことが確認された．

| ターゲット | 割込みコントローラ | 10U | 300U | 1000U |
|---|---|---|---|---|
| zybo_z7_gcc (Cortex-A9) | GIC-400 (GICv2) | DONE | — | — |
| kria_arm64_gcc (A53) | GIC-400 (GICv2) | HANG 3/3 | DONE 3/3 | — |
| stm32mp257f_dk (A35) | GIC-400 (GICv2) | HANG 3/3 | DONE 3/3 | — |
| imx8mm_evk (A53) | **GICv3** | HANG 2/2 | DONE 3/3 | — |
| polarfire_soc_kit (U54) | **PLIC+CLINT (RISC-V)** | — | **HANG 3/3** | **DONE 3/3** |

実測した signature も一致する（PolarFire, PRC_NUM=2, 300U, 7点サンプリング）:
- 被害コア(PRC1): 6/7 が IPI・割込み経路（`msi_handler` ×3 / `core_int_entry_*` ×3），
  残り 1 点が `dispatcher_2`（＝戻れてはいるが即座に引き戻される）
- 加害コア(PRC2): 7/7 がテストループ（`task2` / `make_non_runnable` / `sus_tsk` / `sil_dly_nse1`）
- `CLINT MSIP[PRC1]` が **7/7 で 1**（処理中に常に次の IPI が pending）

ARM64 側（stm32mp257, 10U）でも同型: 被害コア 8/8 が割込み入口・出口
（`cur_spx_irq_handler`/`irq_handler_*`/`irc_begin_int`/`irc_end_int`），
加害コア 8/8 がテストループ．

**観測上の注意**: JTAG で halt すると storm が止まり，被害コアは `dispatcher_2` に落ち着く．
halt 保持したまま観測すると「アイドルで固着し IPI が配送されない」ように見え，別現象と
誤認する（実際に imx8mm で一度誤認した）．attach/detach を繰り返す方式か RAM カウンタを使うこと．

**現状の対処と残課題**:
- 各ターゲットは `target_test.h` の `TEST_DELAY_TIME_NSE` 引き上げで回避している
  （必要値: zybo=既定10Uで可 / kria・stm32mp257・imx8mm=300U / polarfire=1000U）．
  **これは閾値依存の回避策であり根治ではない．** 必要値がターゲット・コア数・ボードで
  変わること自体がその証拠（polarfire は 4 コア時 300U で完走した記録があるが，
  Discovery Kit・2 コアでは 300U で HANG する）．
- **根治は本書 §1 の backoff breaker の共通化**（現状 `USE_RISCV_DIRECT_TRAP` ガード内のみ）．
- **未修正の順序問題**: `update_schedtsk_dsp()` の `memory_barrier()` は `p_schedtsk` ストアの
  *前* にあり，ストアと IPI 発行の *間* にバリアが無い（§4.1）．esp32p4 では lost wakeup の
  実害が観測され sender release fence を追加したが，**同じ穴は polarfire にも ARM64 にも
  残っている**（ARM64 は GICD_SGIR への device write が先行する通常メモリのストアと
  順序付けられる保証がない）．リリースタグ(3.4.0)への適用は影響が大きいため trunk 側で要検討．

**根治**: `arch/riscv_gcc/common/msi_ipi.c`（`USE_RISCV_DIRECT_TRAP` ガード）の msi_handler 入口に
**escalating 有界 backoff breaker** を追加．
- storm 検出: 連続 msi_handler 入口の mcycle 間隔が閾値未満
- backoff: clear_msip の前に実施，連続数に比例（上限 cap で最悪レイテンシ bound）して受信位相を
  raise cadence からずらし，victim task の前進を保証
- escalating（self-tuning）にした理由: 固定 backoff は timing 敏感で脆い（Heisenbug 実測）
- msip は level-pending 保持 → backoff 中に lost-wakeup なし
- 他ターゲットへの影響なし（`USE_RISCV_DIRECT_TRAP` ガード内のみ変更）

**補完策**（storm 強度を下げる．根治は backoff breaker）:
- `raise_msip` での msip pending 中の重複送出省略（msip は level-pending 保持のため新 rsm_tsk の
  起床は backoff breaker がカバー）
- `raise_msip` sender release fence / `clear_msip` receiver acquire fence（潜在的な順序バグとして温存）

**検証**: 実機・計装なしで mtrans2 5/5 DONE．回帰（mtskman3/raster1/2/mtrans3/4/5/mstress1/task1/sem1）
全 PASS/DONE．退行ゼロ．

---

## 2. ハードウェア / カーネル構成

### 2.1 SoC
- **ESP32-P4**: デュアル RISC-V HP コア（RV32IMAFC + U, 360MHz）+ LP コア（未使用）．
- mhartid: core0=0, core1=1．
- 割込み: **CLIC モード固定**（`mtvec.MODE=3` ハードワイヤ）．PLIC は無い．
- コア間/タイマ: **CLINT**（msip / mtime / mtimecmp）．CLINT はコアローカル（JTAG からの他コア読みは不可）．

### 2.2 FMP3 SMP モデル
- `prcid = mhartid + 1`（hart0=PRC1=master, hart1=PRC2）．`get_my_prcidx() = mhartid`（0 始まり）．
- **giant lock**: FMP3 はカーネル全体をスピンロックで保護．
- **コア間ディスパッチ要求**: 対象コアの msip（machine software interrupt）を立てて起床させる．

### 2.3 CLIC の要点（標準 + ESP32-P4 固有）
- **mil**（mintstatus[31:24], CSR 0x346）= 現在の in-service 割込みレベル．mil を下げるのは mret のみ
  （mret が mcause.mpil(mcause[23:16]) から mil を復元）．
- 優先度マスク = メモリマップド `CLIC_INT_THRESH`(0x2080_0008)（`mintthresh` CSR は P4 に無い）．
- 内部線: msip=3，mtimer=7．`CLIC_INT_CTRL(i)` = 0x2080_1000 + i*4（byte0=IP, byte1=IE, byte3=CTL，
  level=CTL>>5，NLBITS=3）．
- **ESP32-P4 固有**: `CLIC_INT_THRESH` の read-back が不安定 → IPM ソフトシャドウで対処済み
  （mtrans5 の修正．mtrans2 とは別問題）．
- **BSS 未初期化**: `TOPPERS_OMIT_BSS_INIT` のため，計測カウンタ等のグローバル変数は個別にゼロ初期化必須．

---

## 3. テスト `test_mtrans2.c` の構造

「過渡的な状態のテスト(2)」．TASK1=CLS_PRC1（PRC1専用，起動済み），TASK2/TASK3=CLS_PRC2（PRC2専用），
MTX1=TA_CEILING，NO_LOOP=1000．

```c
/* TASK1 (PRC1, メイン) */
test(A): task2_flag=true; act_tsk(TASK2);
         for(i<1000){ get_tst(TSK_SELF,&st); count TTS_RUN/TTS_SUS; delay_count(rand); }
         task2_flag=false; syslog(...);   /* ← この syslog が出ない = test(A) で停止 */
test(B)〜test(D): ... syslog("Test finished."); check_finish_PRC1(0);

/* TASK2 (PRC2) */
while(task2_flag){ sus_tsk(TASK1); rsm_tsk(TASK1); delay_count(rand); }
```

**storm の発生機構**: TASK2(PRC2) が sus_tsk/rsm_tsk を高速ループ → rsm_tsk が `raise_msip(PRC1)` を
≈983K/s 送出 → PRC1 が msi_handler 入口/出口で飽和 → TASK1 が `task2_flag=false` に到達できず
→ TASK2 が storm を継続する自己保持ループ．

`mtskman3`/`raster2` が PASS するのは，「単一タスクのコアが何千回もアイドル↔他コア起床を繰り返す」
パターンを踏まないため．

---

## 4. 関連カーネルコード

### 4.1 ディスパッチ要求 — `kernel/task.h: update_schedtsk_dsp()`
```c
update_schedtsk_dsp(PCB *p_my_pcb, PCB *p_pcb, TCB *p_new_schedtsk)
{
    TCB *p_prev_schedtsk;
    if (p_pcb->dspflg) {
        p_prev_schedtsk = p_pcb->p_schedtsk;
        memory_barrier();                 /* p_schedtsk ストアの前 */
        p_pcb->p_schedtsk = p_new_schedtsk;
        if ((p_pcb != p_my_pcb) && (p_prev_schedtsk != p_pcb->p_schedtsk)) {
            request_dispatch_prc(p_pcb->prcid);   /* = raise_msip(対象コア) */
        }
    }
}
```
`memory_barrier()` は p_schedtsk ストアの前にあり，ストア → raise_msip 間にバリアが無い
（raise_msip への sender release fence 追加の根拠）．

### 4.2 IPI 発行/クリア — `arch/riscv_gcc/esp32p4/clint_ipi.h`
```c
raise_msip(ID prcid) {
    sil_get_pid(&self);
    addr = (prcid == self) ? MSIP_SELF_ADDR : MSIP_PEER_ADDR;
    __asm__ volatile ("fence rw, w" ::: "memory");   /* sender release */
    sil_swrw_mem(addr, 1U);
    (void)sil_rew_mem(addr);
}
clear_msip(ID prcid) {          /* 常に自コア(CLINT_BASE)を 0 に．msi_handler 入口で呼ぶ */
    sil_swrw_mem(MSIP_SELF_ADDR, 0U);
    (void)sil_rew_mem(MSIP_SELF_ADDR);
    __asm__ volatile ("fence r, rw" ::: "memory");   /* receiver acquire */
}
```

### 4.3 msip ハンドラ — `arch/riscv_gcc/common/msi_ipi.c: msi_handler()`
```c
void msi_handler(void) {
    PCB *p_my_pcb = get_my_pcb();
    /* ★escalating backoff breaker（USE_RISCV_DIRECT_TRAP ガード，clear_msip の前） */
    clear_msip(p_my_pcb->prcid);
    dispatch_handler();            /* 常に呼ぶ．全 ready を再評価しディスパッチ */
    if (ext_ker_req_flg_table[...]) ext_ker_handler();
    if (set_hrt_event_req_flg_table[...]) { ...; set_hrt_event_handler(); }
}
```
dispatch_handler は msip の発生要因によらず毎回全 ready を再評価する設計のため，
backoff 中に新たな rsm_tsk が来ても次の handler 呼び出しで吸収される．

---

## 5. 再現方法

```sh
# 環境（IDF_TOOLS_PATH は導入時と使用時で同じ値にすること．未設定時の既定は ~/.espressif．
#  詳細は target/m5stamp_esp32p4_gcc/target_user.md「開発環境の準備」参照）
export IDF_TOOLS_PATH=$HOME/tools IDF_PATH=$HOME/tools/esp-idf
. $HOME/tools/esp-idf/export.sh

# runner（PRC_NUM=2 でビルド→flash→capture→判定）
cd <fmp3>/target/m5stamp_esp32p4_gcc/tools/fmp_loader
CAPT=600 ./run_fmp_test.sh 2 test_mtrans2     # CAPT=600 推奨（走行中ほぼ無出力）

# 比較（PASS する SMP テスト）
CAPT=150 ./run_fmp_test.sh 2 test_mtskman3 test_raster2
```
- ビルドは `rm -rf fmp3_build` 後に実施（runner が自動実行）．
- JTAG halt は livelock を解くため，ハング状態の観測には RAM カウンタを使うこと．

---

## 6. 関連ファイル

チップ/ターゲット依存部:
- `arch/riscv_gcc/esp32p4/clint_ipi.h` — raise_msip/clear_msip（fence，msip coalescing）
- `arch/riscv_gcc/common/clic_kernel_impl.{c,h}` — CLIC 操作 + IPM ソフトシャドウ（ESP32-P4 quirk）
- `arch/riscv_gcc/esp32p4/chip_kernel_impl.h` — t_set_ipm/t_get_ipm（threshold/シャドウ経由，MSIE/MTIE 連動）
- `arch/riscv_gcc/esp32p4/chip_support.S`, `esp32p4.h` — トラップ入口，アドレス定義
- `patches/common_core_support_clic.patch` — 共通 core_support.S への CLIC mil 管理（mret 化，mcause マスク）
  （別リポジトリ esp32_p4 側のファイル）

FMP3 共通/カーネル（参照ツリー，原則変更しない）:
- `kernel/task.h` — update_schedtsk_dsp
- `arch/riscv_gcc/common/msi_ipi.c`, `msi_ipi.h` — msi_handler（backoff breaker 追加）/ request_dispatch_prc
- `arch/riscv_gcc/common/core_support.S` — ディスパッチ/割込み入口・出口・アイドル（CLIC 経路は USE_RISCV_DIRECT_TRAP）
- `arch/riscv_gcc/polarfire_soc/` — 参考 RISC-V SMP（PLIC+CLINT）実装．比較対象．

---

## 7. branch 3.4 での storm 回避コード要否の再検証（2026-07-20）

本書 §1 の storm 回避（backoff breaker / msip coalescing）が **branch 3.4（`^/branches/3.4`）＋
現行 ESP-IDF v5.5.4 でも必要か**を実機で再検証した（m5stamp_esp32p4, PRC_NUM=2, TEST_DELAY_TIME_NSE=10U）。
各対策を実験用 `-D` で無効化して test_mtrans2 を反復:

| 構成（10U） | 結果 |
|---|---|
| backoff breaker ON + coalescing ON（現状） | DONE |
| **backoff breaker OFF**（`CLIC_STORM_BACKOFF_UNIT=0`） + coalescing ON | **6/6 DONE** |
| **backoff breaker OFF + coalescing OFF**（`NO_MSIP_COALESCE`） | **8/8 DONE**（全 boot 確認） |

※ fence（release/acquire, `raise_msip`/`clear_msip`）は無効化していない（lost-wakeup の正しさ修正で storm とは別件）。

**結論**: **現行 branch 3.4 では，backoff breaker も coalescing も無しで mtrans2 は 10U を安定して通る**
（両 OFF で 8/8）。よって §1 の storm 回避は **現状 mtrans2 を通すためには不要**．

**§1（決定論的ハング）との食い違いの解釈**（未確定・向こう＝esp32_p4 repo で要調査）:
- §1 は esp32_p4 開発リポジトリでの観測．branch 3.4 はマージ/整理で dispatch/IPI/CLIC 経路が
  異なり livelock 感受性が消えた可能性（要差分特定）．
- livelock は timing 敏感（Heisenbug）．現行 toolchain の timing では発火しないだけの可能性も
  完全には否定できない（が両 OFF で 8/8 は強い）．
- 一般解析（storm 周期 < dispatch 完了遅延 で livelock）と arm 3ボード実測は
  `test/mtrans2_livelock_analysis.md` にまとめ済み．他ボードは TEST_DELAY_TIME_NSE 引き上げで回避＝
  esp32p4 だけカーネル側 fix を持つのは非対称．

**方針**: fence は残す（正しさ）。backoff breaker（重い escalating 版）は現状“効いていない”＝
削除/簡素化候補．削除前に「branch 3.4 が livelock しない理由（esp32_p4 repo との差分）」を1点特定して
timing による偶然マスクでないことを確認する。**残りの調査・コード整理は esp32_p4 リポジトリ側で進める。**
