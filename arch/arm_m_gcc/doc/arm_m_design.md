# Cortex-M（ARM-M）コア依存部 設計メモ

- 最終更新: 2026年06月20日


# 略号

- NVIC : Nested Vectored Interrupt Controller
- VTOR : Vector Table Offset Register
- MSP : Main Stack Pointer
- PSP : Process Stack Pointer
- IPI : Inter-Processor Interrupt（プロセッサ間割込み）
- HRT : 高分解能タイマ（High Resolution Timer）


# ARM-Mコア依存部の位置づけ

ARM-Mコア依存部は，カーネルのターゲット依存部のうち，ARMv8-M Mainline
（Cortex-M33等）に準拠したプロセッサコアを持つターゲットシステム（チッ
プ）に共通に使用できる部分である．FMP3として初めてのCortex-Mアーキテク
チャ依存部であり，マルチプロセッサ（SMP）に対応している．

GNU開発環境向けのARM-Mコア依存部を置くディレクトリは
arch/arm_m_gcc/common である．コアの略称は "arm_m" とする．

本コア依存部は，ARMv6-Mを意図した分岐（`__TARGET_ARCH_THUMB == 3`）を
ソース上に持つが，現在サポートしているのはARMv8-M（`__TARGET_ARCH_THUMB
>= 4`）である．以降の記述は，特記しない限りARMv8-Mを対象とする．

本メモは，アーキテクチャ「共通部」の設計を記述する．チップ・ボードに固
有な機構（プロセッサ間割込みの実体，自コア番号の取得手段，二次コアの起
動手順，ベクタテーブルの整列付与，メモリマップ等）は，各ターゲット依存部
に委ねており，本メモでは概要と参照に留める．


# 移植の経緯（pico-sdk非依存化）

本コア依存部の母体は，fmp3_pico_sdk（FMP3 3.3.x系のCortex-Mマルチコア実
装）である．これをFMP3 trunk（3.4.0）の流儀へ移植した．

移植にあたっての方針（案A）は，ビルド系をtrunk流に統一することである．
すなわち，pico-sdkに依存していたビルド（Python・CMake・pico-sdkとの密
結合）を排し，Rubyの生成スクリプト（.trb）とMakefile，configure.rbによる
自己完結したビルドに書き戻した．

母体が3.3.x系であったため，trunk 3.4.0への移植にあたっては，ディスパッ
チャ・時間イベント処理等のカーネル本体側の差分に注意する必要があった．


# 必須となる設定

## TOPPERS_ENABLE_TRUSTZONE

TrustZoneを搭載するコア（Cortex-M33等）をSecure状態で単独動作させる構成
では，本マクロの定義が必須である．例外発生時にLRに設定されるEXC_RETURN
の値の選択に用いる（arm_m.h）．

```
#if __TARGET_ARCH_THUMB >= 5 && ! defined(TOPPERS_ENABLE_TRUSTZONE)
#define EXC_RETURN              0xffffffbc
#else
#define EXC_RETURN              0xfffffffd
#endif
```

本マクロを定義しないと，EXC_RETURNがNonSecure用の値（`0xffffffbc`，Sビッ
ト=0）となる．Secure状態で動作するコアでは，これを用いた例外復帰が整合
性チェックに失敗する．本マクロを定義すると，Secure用の値（`0xfffffffd`）
となり整合する．

## __TARGET_ARCH_THUMB

ARMv8-Mを示す値（5）をターゲット依存部のMakefileで定義する
（`-D__TARGET_ARCH_THUMB=5`）．GNU開発環境ではこのマクロが自動定義され
ないため，明示が必要である．


# システム状態の管理

## CPUロック状態と割込み優先度マスク

CPUロックフラグおよび割込み優先度マスクの管理には，BASEPRIを用いる．

全割込みを禁止する機能としてPRIMASK・FAULTMASKがあるが，カーネル管理外
の割込みをサポートするため，これらはCPUロックには用いない．そのため，
BASEPRIを用いて擬似的にCPUロックフラグを実現する（core_kernel_impl.h）．

CPUロック状態の管理に，PCB内に次の変数を持つ（core_pcb.h）．

- `lock_flag` : CPUロックフラグの値を保持する変数
- `saved_iipm` : 割込み優先度マスクを保持する変数（内部表現）

CPUロックフラグがクリアされている間は，BASEPRIをモデル上の割込み優先度
マスクの値に設定する．CPUロックフラグがセットされている間は，BASEPRIを，
カーネル管理外のものを除くすべての割込み要求をマスクする値（TIPM_LOCK）
と，モデル上の割込み優先度マスクとの高い方に設定し，モデル上の割込み優先
度マスクは `saved_iipm` で保持する．

BASEPRIは値が小さいほど優先度が高く，全解除（IIPM_ENAALL）が `0` である
ため，単純な優先度比較では不十分な箇所があり，`set_basepri_max` の使用に
注意している（core_kernel_impl.h の `lock_cpu` のコメント）．

BASEPRI・PRIMASK・FAULTMASKの操作は，core_insn.h にインライン関数として
用意する（`set_basepri`/`set_basepri_max`/`get_basepri`，
`set_primask`/`clear_primask`/`get_primask`，
`set_faultmask`/`clear_faultmask`）．

## SILの全割込みロック

SIL（System Interface Layer）の全割込みロックには，PRIMASKを用いる
（core_sil.h）．`SIL_PRE_LOC`/`SIL_LOC_INT`/`SIL_UNL_INT` が PRIMASK の
退避・設定・復帰を行う．

## コンテキストの判定

実行中のコンテキストは，CONTROLレジスタのSPSEL（PSP/MSP選択）により判定
する．PSPが有効ならタスクコンテキスト，MSPが有効なら非タスクコンテキスト
とする（core_kernel_impl.h の `sense_context`）．


# 例外・割込みモデル

## NVICとEXC_RETURN

割込みはNVICで管理する．割込み番号・割込みハンドラ番号・CPU例外ハンドラ
番号は，例外発生時にIPSR（EPSR）に設定される例外番号とする（arm_m.h，
core_kernel_impl.h）．

例外発生時，LRにはEXC_RETURNが設定される．EXC_RETURNのビット（PSP使用・
FPコンテキスト等）により，復帰先のスタック（MSP/PSP）やコンテキストを
判別する（core_support.S）．

## 例外フレーム

CPU例外ハンドラへは例外フレームの先頭番地が渡される．ハードウェアが自動
で積むスタックフレームに加えて，BASEPRIの値とEXC_RETURNの情報を積んで例外
フレームを構成する（core_support.S）．主なオフセットは arm_m.h で定義する
（`P_EXCINF_OFFSET_BASEPRI`，`P_EXCINF_OFFSET_EXC_RETURN`，
`P_EXCINF_OFFSET_PC`，`P_EXCINF_OFFSET_XPSR`）．

`exc_sense_intmask` は，例外フレーム中のBASEPRI・PRIMASK・EXC_RETURNを参
照し，CPU例外発生時のシステム状態（カーネル実行中でないこと，CPUロック・
全割込みロック状態でないこと，割込み優先度マスク全解除であること，タスク
コンテキストであること）を判定する．

## Secure例外

EXCNO_SECURE（7）等のSecure関連の例外番号・許可ビットを arm_m.h に定義す
る．TrustZone搭載コアをSecure単独で動作させる構成に対応する．


# SysTickによる高分解能タイマ（HRT）

カーネルの高分解能タイマには，コア内蔵のSysTickを用いる．イベント駆動
（ワンショット）方式で，次のタイムイベント発生時刻までを設定する．

SysTickの制御レジスタ等の番地・ビットは arm_m.h に定義する
（`SYSTIC_CONTROL_STATUS` 等，`SYSTIC_TICINT` 等）．SysTickの割込み要求
の許可・禁止・要求・クリアは，core_kernel_impl.h の `disable_int`/
`enable_int`/`raise_int`/`clear_int` で，SysTick割込み番号
（`IRQNO_SYSTICK`，15）を特別扱いして実装する．


# ディスパッチャ（PendSV）

ディスパッチャはPendSV例外で実現する（core_support.S の `pendsv_handler`）．
遅延ディスパッチ要求は，ICSRのPENDSVSETビット（bit28）をセットすることで
発行する（core_kernel_impl.h の `request_dispatch_retint`）．

タスクコンテキストはPSP，非タスクコンテキストはMSPを用いる．コンテキスト
切り替えは，EXC_RETURNのビットでスタックを判別しつつ，PSP/MSPを切り替えて
行う．ディスパッチの起動には，SVC命令（`svc #0`）を用いる経路もある．

ARMv8-M（`__TARGET_ARCH_THUMB >= 5`）では，スタックリミットレジスタ
（PSPLIM/MSPLIM）を設定する．

start.S・core_support.S の各PE（プロセッサ）別スタック・テーブルの参照は，
`my_prcidx` を用いて行う（core_asm.inc の `my_pcb`/`my_exc_tbl`/
`my_istkpt`/`my_idstkpt` 等）．


# FPUのサポート

FPUの使用はターゲット依存である．`TOPPERS_FPU_CONTEXT` が定義されている場
合，タスク切替・例外の出入口で浮動小数点レジスタ（s16-s31）を保存・復帰す
る（core_support.S）．FPCCRのLazy Stacking等の設定値を arm_m.h に用意する
（`FPCCR_INIT` を `TOPPERS_FPU_NO_PRESERV`/`TOPPERS_FPU_NO_LAZYSTACKING`/
`TOPPERS_FPU_LAZYSTACKING` で選択）．既定ではソフトウェア浮動小数点
（soft-float）でビルドする．


# マルチプロセッサ抽象（共通部）

ARM-Mコア依存部は，マルチプロセッサに関わる以下の機能を「アーキ共通のイン
タフェース」として規定し，その実体は「チップ依存部」（各ターゲット依存部）
で実装する．プロセッサ数（TNUM_PRCID）が1のときは単一プロセッサとして扱い，
これらはユニプロセッサ用のダミーで構成する．

## 自コア番号の取得（get_my_prcidx）

`get_my_prcidx()` は，自コア番号（0オリジンのインデックス）を返すアーキ共
通インタフェースである．common側（core_kernel_impl.h）は本関数を呼び出すだ
けで，実体はチップ依存部で定義する．chip_kernel_impl.h は，先に
`get_my_prcidx()` を定義してから core_kernel_impl.h を include する構成と
なっている（core_kernel_impl.h が `get_my_prcidx()` に依存するため）．

Cortex-M（M-profile）には，A-profileのMPIDR相当の自コア番号レジスタがない
ため，取得手段はチップに依存する．

- RP2350: SIOのCPUIDレジスタで直接読み出す．
- SSE-200（Musca-B1）: そのようなレジスタがないため，現在のMSPが，どのコア
  の割込みスタック（istack）の範囲に入っているかで判定する．各コアは起動時
  のブートハンドシェイクで自分用のistackを設定済みであり，例外・割込みハン
  ドラもMSP（=istack）上で動作するため，呼出し回数によらず冪等に正しい値を
  返す．

## ロック（ジャイアントロック／ネイティブスピンロック）

ジャイアントロック（`acquire_glock`/`try_glock`/`release_glock`/
`initialize_glock`）およびネイティブスピンロックの操作を，アーキ共通のイン
タフェースとして規定し，実体はチップ依存部で実装する．

try系の関数（`try_glock`，`try_lock`，`try_native_spn`等）の返り値の意味は，
trunk共通の契約に従い「既に取得されていた（＝取得失敗）」が true，「取得に
成功した」が false である．この契約を誤ると `while (try_glock())` 等で無限
ループに陥るため，注意が必要である．

ロックの実体はチップにより異なる．

- RP2350: SIOのハードウェアスピンロック（32個）を用いる．
- SSE-200（Musca-B1）: ハードウェアスピンロックがないため，LDREX/STREXに
  よるソフトウェアスピンロックを用いる．

## プロセッサ間割込み（IPI）

以下のプロセッサ間割込みの発行関数を，チップ依存部で実装する．

- `request_dispatch_prc(prcid)` : ディスパッチ要求
- `request_ext_ker(prcid)` : カーネル終了要求
- `request_set_hrt_event(prcid)` : 高分解能タイマ設定要求

これらの送出に用いるハードウェア機構はチップ依存である．たとえば，SSE-200
ではMHU，RP2350ではSIO FIFOを用いる．具体的な番地・割込み番号は，各ター
ゲット依存部に閉じ込める．

## 二次コアの起動と per-PE スタック

start.S のスタートアップでは，自コア番号が0（マスタコア）でなければBSS/
DATA初期化等をスキップして `sta_ker()` へ向かう．マスタコアは初期化後に
`target_mprc_initialize()` を呼び出し（0でない場合），この中で二次コアの
起動等を行う．二次コアの起動手順はチップに依存する．

スタートアップのスタック設定前は，自コア番号の取得手段が未確立の場合がある
ため，起動時専用のアセンブラマクロ `my_prcidx_boot` を用いる（start.S）．
CPUID等で常に自コア番号が分かるチップでは，`my_prcidx_boot` は `my_prcidx`
に委譲される（core_asm.inc のデフォルト，`MY_PRCIDX_BOOT_DEFINED` 未定義
時）．SSE-200のように起動時専用の取得方法が必要なチップでは，chip_asm.inc
で `MY_PRCIDX_BOOT_DEFINED` を定義した上で `my_prcidx_boot` を上書きする．

非タスクコンテキスト用スタック（istack）はプロセッサ毎に持ち（istk_table/
istkpt_table），start.S・core_support.S が `my_prcidx` 経由で参照する．


# per-PRC ベクタテーブルの整列

ARMv8-MのVTOR（ベクタテーブルオフセットレジスタ）には，ベクタテーブルの
配置境界に制約がある．マルチプロセッサ構成では，プロセッサ毎にベクタテー
ブルを持つため，各ベクタテーブルをこの境界に整列させる必要がある．

ベクタテーブルの境界整列は，ターゲット依存部の生成スクリプト
（target_kernel.trb）で付与する（具体的な整列値はベクタエントリ数に依存し，
ターゲット依存部の領分である）．


# ベクタテーブル・ハンドラテーブルの生成

ベクタテーブル（`_kernel_vector_table`），例外/割込みハンドラテーブル
（`_kernel_exc_tbl`），割込み属性ビットパターン（`_kernel_bitpat_cfgint`）
の生成は，割込み番号の有効範囲（INHNO_VALID/INTNO_VALID/EXCNO_VALID）に依
存するため，ターゲット依存部の生成スクリプト（target_kernel.trb）で行う．

ARM-Mコア依存部の core_kernel.trb は，カーネル共通テンプレートのインクルー
ドと補助関数（ネイティブスピンロックの生成 `GenerateNativeSpn`，TSKINICTXB
の初期化情報の生成 `GenerateTskinictxb` 等）の定義のみを行う．


# offset.h の Makefile 依存関係

ARM-Mコア依存部では，core_kernel_impl.h が（CFG1_OUT以外で）offset.h を取
り込む（PCBのターゲット依存部オフセット `PCB_target_pcb` を用いるため）．

```
#ifdef TOPPERS_CFG1_OUT
#define PCB_target_pcb  0
#else
#include "offset.h"
#endif
```

この取り込みは kernel_impl.h → target_kernel_impl.h → chip_kernel_impl.h
→ core_kernel_impl.h の連鎖で，カーネル・システムサービス・アプリのほぼ全
Cオブジェクトに波及する．

他アーキ（arm64/riscv）では offset.h をアセンブラからのみ取り込むため，上
位Makefileの ASMOBJS 用の offset.timestamp 順序依存だけで足りる．しかし
ARM-MではCオブジェクトもoffset.hに依存するため，並列ビルド（make -j）時に
offset.h生成前にCオブジェクトをコンパイルしてしまい，レース不具合（offset.h
不在によるビルド失敗）が起こりうる．

これを避けるため，Makefile.core で `OFFSET_COBJS` を定義し，上位Makefile
（sample/Makefile）がこれらに offset.timestamp への順序専用依存を付与する．

```
OFFSET_COBJS = $(KERNEL_COBJS) $(SYSSVC_COBJS) $(APPL_COBJS)
```

`OFFSET_COBJS` は，OBJDIR付与後の最終リストを参照する必要があるため，遅延
展開（`=`）で定義する．非ARM-Mターゲットでは `OFFSET_COBJS` は空であり，
no-op（影響しない）．


# 変更履歴

## 2026/06/20
- ARM-Mコア依存部の設計メモ（本ファイル）を新規作成．

## 2026/06
- FMP3初のCortex-M（ARMv8-M Mainline）コア依存部 arch/arm_m_gcc を新規追加．
  fmp3_pico_sdk を母体に，FMP3 trunk の流儀（.trb/Makefile，pico-sdk非依存）
  へ移植．マルチプロセッサ（SMP）対応．対応ターゲット：musca_b1_gcc（QEMU），
  rp2350_pico2_gcc（実機 Raspberry Pi Pico2）．

以上
