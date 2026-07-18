# ESP32-P4 チップ依存部 設計メモ（移植知見・落とし穴）
- 最終更新: 2026年06月29日

> **本書の位置づけ**: 本書は設計・調査の記録であり，利用手順書ではない．利用者向けの
> 入口は `chip_user.md`（`arch/riscv_gcc/esp32p4/`）および `target_user.md`
> （`target/m5stamp_esp32p4_gcc/`）を参照．本書は上記「最終更新: 2026年06月29日」時点の
> 記録であり，現在の実装と異なる場合がある．

# 位置づけ

本メモは，TOPPERS/FMP3 を ESP32-P4 の HP RISC-V コア（2コア，RV32IMAFC）へ移植する
過程で判明した **ESP32-P4 固有の事項・落とし穴・対策** をまとめたものである．
利用方法は chip_user.md，CLIC 共通設計は doc/clic_design.md / clic_memo.md を参照．

ESP32-P4 は RISC-V 標準と異なる挙動・未実装 CSR・read-back 不安定レジスタ等を持ち，
仕様書（TRM）に明記されない HW 既定値に依存する箇所がある．以下は実機で確認した
知見である．

# 1. ビルド／統合（方式 a）

- FMP3 を configure.rb/make で静的ライブラリ libfmp3.a にまとめ，ESP-IDF アプリの
  main コンポーネントがリンクする．
- ABI は **ilp32f**（IDF と一致）．mcmodel=medany．
- `.data`/`.bss` は **IDF 起動が初期化**するため start.S の自前初期化を
  `TOPPERS_OMIT_BSS_INIT` / `TOPPERS_OMIT_DATA_INIT` で抑止する．
  - **落とし穴**: この結果，C の static/global は **0 初期化が保証されない場合がある**
    （IDF の .bss クリアより前に触れる初期化経路など）．BSS 非初期化前提で，
    起動初期に使うグローバル（例: storm breaker 状態）は **明示ゼロ化** する．
- 全 text を内部 RAM（IRAM）へ集める（objcopy で .text→.iram1.fmptext）．
  flash MMU 窓との overlap 回避と，asm の jal(±1MB) を同一 IRAM 内に収めるため．

# 2. CLIC（割込みコントローラ）

ESP32-P4 は PLIC を持たず **CLIC モード固定**（mtvec.MODE=3）．非ベクタ（SHV=0）+
ソフトディスパッチで運用する．

## 2.1 未実装・不正な CSR（重要）

実機で **不正命令例外**になる CSR があり，仕様の素朴な実装が動かない．
- `mil` は **mintstatus CSR `0x346`** の [31:24] で読む．**`0xFB1` は不正**．
- **`mintthresh`（CSR `0x347`）は存在せず** csrw で不正命令例外．
  割込み優先度マスク（しきい値）は **メモリマップド `CLIC_INT_THRESH`(0x2080_0008)** のみ．
- CLIC のモード/NLBITS 設定は ROM 既定値に依存（明示設定は不要だった）．

## 2.2 clicintattr の MODE ビット保持（実機ハマり）

`CLIC_INT_CTRL(i)` の byte2 = clicintattr に **MODE(bit7:6)** があり，ROM 既定で
`0xC0`（MODE=M-mode）．**全ワード書込みで attr を 0 に潰すと M-mode へ配信されない**
（実機で IP は立つが割込みが入らない）．
- **対策**: 割込み許可/レベル設定は **read-modify-write** で attr を保持し，
  ctl（レベル）と IE のみ変更する（chip_initialize の mtimer/msip 設定参照）．

## 2.3 CLIC_INT_THRESH の read-back 不安定 → IPM ソフトシャドウ

`CLIC_INT_THRESH` は **書いた値をそのまま読み戻せない**（同一命令の連続読みで
ライブ状態を返す）．HW 読戻しに依存すると t_get_ipm が誤った IPM を返し，
ディスパッチ可否判定を誤らせる（task 文脈で 0x7F を返して mtrans5 を E_CTX で
落としていた）．
- **対策**: IPM の真値を **ソフトウェアシャドウ `clic_ipm_shadow[]`** で管理し，
  t_get_ipm はシャドウを返す（clic_kernel_impl.c．HW 非依存の防御的設計）．

## 2.4 NLBITS とレベル

NLBITS=3．`CLIC_INT_CTRL` の CTL[31:24] の上位 3bit がレベル，残りが優先度．
割込みレベル = `CTL >> (8-NLBITS) = CTL >> 5`．確実配信したい内部割込みは
CTL=0xE0（レベル7）とする．

## 2.5 割込み線の割付

- 内部: **msip = 線3**，**mtimer = 線7**（いずれも CLINT 由来）．
- 外部（INTERRUPT MATRIX 由来）は **+16 オフセット**で線16〜47．
  線16 は USB-Serial/JTAG が使用 → アプリ用は線17以降．
- `CLIC_TNUM_INTNO=48` は線の **本数**（有効線 0..47）．**最大線番号は 47**．
  - **落とし穴(off-by-one)**: `TMAX_INTNO` を本数（48）に等値すると VALID_INTNO が
    範囲外を有効と誤判定し intcfg_table を OOB 参照する（TTSP3 が検出）．
    `TMAX_INTNO = CLIC_TNUM_INTNO-1`．初期化ループも `< CLIC_TNUM_INTNO`．

# 3. CLINT（IPI／タイマ）

## 3.1 コアローカル

ESP32-P4 の CLINT は **コアローカル**．各コアから見て
- 自コア: `CLINT_BASE`(0x2000_0000)
- 他コア(peer): `+CLINT_CORE_STRIDE`(0x0001_0000)
- **落とし穴**: msip を相手コアに送るには **peer アドレス** を使う．自コア基準で
  相手の msip を立てようとすると相手がクリアできず msip 割込み無限ループになる．
- mtimecmp も自コア基準（`CLINT_BASE+0x4000`）で常に自プロセッサのものを指す
  （pid 引数は使わない）．

## 3.2 mtime の有効化

ESP32-P4 の mtime は **`mtimectl` の `MTIME_EN`(bit0) で明示有効化が必要**
（PolarFire は常時稼働なので common の mtimer.c は有効化しない）．
これが無いと mtime が進まず周期処理・時間計測が止まる．chip_initialize で設定．

## 3.3 64bit mtimecmp の安全な書込み（RV32）

RV32 では mtimecmp を上位/下位 2 ワードで書く．**上位を最大値にして一致抑止 →
下位 → 上位** の順で書く（spurious 割込み回避）．
- **落とし穴**: cmp_u/cmp_l を計算しながら下位32bit切詰の値を上位に書くと，
  上位に下位の値が入って一致せず **タイマ割込みが発火しない**（mtimer.h 参照）．

## 3.4 msip のメモリ順序（lost-wakeup 根治）

RISC-V の弱メモリ順序では，p_schedtsk 等の更新 store と msip アクセスが他コアから
逆順に見えうる．すると msip で起こされたコアが古い p_schedtsk（NULL 等）を読み，
実行可能タスクがあるのに再びアイドルへ落ちて二度と起きない（**lost wakeup**）．
mtrans2 で数千回叩かれ決定論的にデッドロックしていた．
- **対策**: `raise_msip` に **release フェンス**（先行 store を msip より前に順序付け），
  `clear_msip` に **acquire フェンス**（msip クリア後の p_schedtsk 読出しを順序付け）．

## 3.5 送出側 msip 合体（coalesce）

`raise_msip` で対象 msip を読み，**既に pending(=1) なら冗長な set を省く**
（FreeRTOS の pending-yield 相当）．pending な msip のハンドラは無条件に
dispatch_handler を呼び p_schedtsk を再評価するため要求は失われない．storm 時の
送出側コスト・コア間バス競合を下げる．
- lost-wakeup 防止のため発行前フェンスを **rw,rw**（先行 store を msip **読出し**
  より前に順序付け）とする．pending を介したコヒーレンス順序＋受信側 acquire で，
  合体時も対象コアは最新 p_schedtsk を見てディスパッチする．

# 4. dispatch-IPI storm livelock と breaker（本移植の核心対策）

## 4.1 現象・真因

CLIC + msip 構成で，他コアの `sus_tsk`/`rsm_tsk` 連打が割込み対象コアへ dispatch IPI
(msip) を高頻度で送ると，割込み対象コアが割込み入口/出口処理に飽和し，割込まれた
タスクが割込みの合間に **1命令も retire できない位相共振 livelock** に陥る
（実機 test_mtrans2 で実証）．**ESP32-P4 の割込み1回コストの高さに起因**し，
PolarFire 等の低コスト構成では発生しない（よって本対策はチップ依存部に置く）．

## 4.2 対策＝受信側 escalating backoff breaker（irc_begin_ipi）

`msi_handler` 入口（clear_msip の前）の関数フック `irc_begin_ipi`
（chip_kernel_impl.c）で，連続入口の mcycle 間隔から storm を検出し，**連続数に
比例した有界 backoff** で受信側の割込み受理位相を他コアの送出 cadence からずらす．
- **なぜ escalating（連続数比例）か**: 固定 backoff は timing 敏感で脆く，**デバッグ
  計装の数十 cyc が消えただけで効かなくなる Heisenbug** を実測した．連続数比例なら
  任意の共振点で backoff が伸びて必ず前進窓に入り，前進で storm が止み cnt=0 に戻る
  **self-tuning**．COUNT_CAP で最悪レイテンシを bound．
- **なぜ clear の前か**: clear 前に遅延すると backoff 中に他コアが立てる msip は既存
  pending へ合体し，受信側の割込み回数自体が減る．
- **安全性**: giant lock 非保持．スケジューラ状態も msip マスク論理も変えない．
  msip は level-pending のまま保持され復帰後に必ず再評価＝lost-wakeup 無し．

## 4.3 受信側 breaker は必須（実証）

送出側合体（3.5）を入れた後に **breaker を無効化して mtrans2 を回すと livelock が
再発**した（5回中1回ハング）．合体は IPI 数を減らす効率化だが，受信側の割込みレート
自体は HW 合体と同窓のため大きく減らず，**位相共振 livelock は受信側 backoff でしか
断てない**．送出側合体（根本側の負荷軽減）と受信側 breaker（共振の遮断）の
**二段構えを維持** する．

# 5. トラップ機構（mil の管理）

CLIC は割込み入口で in-service level(mil) を上げ，mret で mcause.mpil から復元する．
カーネルの mil 復帰は **チップ依存の割込み出口フック `irc_end_int`（chip_support.S）**
に局所化し，全割込み出口で `mcause.mpil=0` を強制して後続 mret を一律 mil=0 へ戻す．
共通部 core_support.S は CLIC 非依存（素の mret）に保つ．詳細は doc/clic_design.md．
- 本移植の preemption の門は **threshold（CLIC_INT_THRESH）**で，mil は実質常に 0 に
  保つ不変量．ネスト復帰で外側ハンドラも mil=0 になるが threshold が保護を担う．

# 6. その他の知見・落とし穴

- **BSS 非初期化**（1 参照）: 起動初期に使うグローバルは明示ゼロ化．
- **Heisenbug**（4.2）: 位相共振は timing 敏感で，計装を入れると消える/挙動が変わる．
  デバッグ出力で「直る」場合は timing 依存を疑う．固定パラメータより self-tuning．
- **mcycle**: CSR `0xB00`（下位32bit）．360MHz．storm 検出の時間基準に使用．
- **CLIC mcause**: 例外時 mcause 上位に mpp/mpie/mpil 等が同居する．例外番号(exccode)
  抽出は下位12bit マスク（`slli/srli 20`，RV32）．chip_asm.inc の core_get_exccode_asm．

# 7. 検証メモ

- 回帰は ESP32-P4 実機（target/m5stamp_esp32p4_gcc/tools/fmp_loader/run_fmp_test.sh）で FMP3 test/ を実行．
  SMP の canary は **mtrans2/raster2**（storm/lost-wakeup を露呈する）．intermittent
  なので mtrans2 は複数回回すこと．
- 適合性は本物の TTSP3（/home/honda/TOPPERS/ttsp3）で実行（off-by-one を検出した）．
- 非CLIC 側（PolarFire）の回帰は QEMU `microchip-icicle-kit` および実機（FlashPro5/
  JTAG）で sample1 4コアSMP．共通部変更時は両系統で確認する．

以上
