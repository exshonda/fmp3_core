/*
 *  TOPPERS/FMP Kernel
 *      Toyohashi Open Platform for Embedded Real-Time Systems/
 *      Flexible MultiProcessor Kernel
 * 
 *  Copyright (C) 2000-2003 by Embedded and Real-Time Systems Laboratory
 *                              Toyohashi Univ. of Technology, JAPAN
 *  Copyright (C) 2006-2022 by Embedded and Real-Time Systems Laboratory
 *              Graduate School of Information Science, Nagoya Univ., JAPAN
 * 
 *  上記著作権者は，以下の(1)〜(4)の条件を満たす場合に限り，本ソフトウェ
 *  ア（本ソフトウェアを改変したものを含む．以下同じ）を使用・複製・改
 *  変・再配布（以下，利用と呼ぶ）することを無償で許諾する．
 *  (1) 本ソフトウェアをソースコードの形で利用する場合には，上記の著作
 *      権表示，この利用条件および下記の無保証規定が，そのままの形でソー
 *      スコード中に含まれていること．
 *  (2) 本ソフトウェアを，ライブラリ形式など，他のソフトウェア開発に使
 *      用できる形で再配布する場合には，再配布に伴うドキュメント（利用
 *      者マニュアルなど）に，上記の著作権表示，この利用条件および下記
 *      の無保証規定を掲載すること．
 *  (3) 本ソフトウェアを，機器に組み込むなど，他のソフトウェア開発に使
 *      用できない形で再配布する場合には，次のいずれかの条件を満たすこ
 *      と．
 *    (a) 再配布に伴うドキュメント（利用者マニュアルなど）に，上記の著
 *        作権表示，この利用条件および下記の無保証規定を掲載すること．
 *    (b) 再配布の形態を，別に定める方法によって，TOPPERSプロジェクトに
 *        報告すること．
 *  (4) 本ソフトウェアの利用により直接的または間接的に生じるいかなる損
 *      害からも，上記著作権者およびTOPPERSプロジェクトを免責すること．
 *      また，本ソフトウェアのユーザまたはエンドユーザからのいかなる理
 *      由に基づく請求からも，上記著作権者およびTOPPERSプロジェクトを
 *      免責すること．
 * 
 *  本ソフトウェアは，無保証で提供されているものである．上記著作権者お
 *  よびTOPPERSプロジェクトは，本ソフトウェアに関して，特定の使用目的
 *  に対する適合性も含めて，いかなる保証も行わない．また，本ソフトウェ
 *  アの利用により直接的または間接的に生じたいかなる損害に関しても，そ
 *  の責任を負わない．
 * 
 *  $Id: core_kernel_impl.h 541 2026-06-30 13:07:32Z ertl-honda $
 */

/*
 *		kernel_impl.hのコア依存部（ARM用）
 *
 *  このヘッダファイルは，target_kernel_impl.h（または，そこからインク
 *  ルードされるファイル）のみからインクルードされる．他のファイルから
 *  直接インクルードしてはならない．
 */

#ifndef TOPPERS_CORE_KERNEL_IMPL_H
#define TOPPERS_CORE_KERNEL_IMPL_H

#include "arm.h"

/*
 *  ターゲット依存のタスク属性（エラーチェック用）
 */
#ifdef USE_ARM_FPU_SELECTIVE
#define TARGET_TSKATR		(TA_FPU)
#endif /* USE_ARM_FPU_SELECTIVE */

/*
 *  エラーチェック方法の指定
 */
#define CHECK_STKSZ_ALIGN	8		/* スタックサイズのアライン単位 */
#define CHECK_INTPTR_ALIGN	4		/* intptr_t型の変数のアライン単位 */
#define CHECK_INTPTR_NONNULL		/* intptr_t型の変数の非NULLチェック */
#ifndef  __thumb__
#define CHECK_FUNC_ALIGN	4		/* 関数のアライン単位 */
#endif /* __thumb__ */
#define CHECK_FUNC_NONNULL			/* 関数の非NULLチェック */
#define CHECK_STACK_ALIGN	8		/* スタック領域のアライン単位 */
#define CHECK_STACK_NONNULL			/* スタック領域の非NULLチェック */
#define CHECK_MPF_ALIGN		4		/* 固定長メモリプール領域のアライン単位 */
#define CHECK_MPF_NONNULL			/* 固定長メモリプール領域の非NULLチェック */
#define CHECK_MPK_ALIGN		4		/* カーネルメモリプール領域のアライン単位 */
#define CHECK_MPK_NONNULL			/* カーネルメモリプール領域の非NULL  */
									/*							チェック */
#define CHECK_MB_ALIGN		4		/* 管理領域のアライン単位 */

#ifndef TOPPERS_MACRO_ONLY

/*
 *  プロセッサ管理ブロックのターゲット依存部の定義
 *
 *  ARM依存部では，タスクコンテキストと非タスクコンテキストの両方をスー
 *  パバイザモードで動作させるため，実行中のコンテキストをプロセッサモー
 *  ドで判断することができない．そのため，割込みハンドラ／CPU例外ハン
 *  ドラのネスト段数（これを，例外ネストカウントと呼ぶ）を管理し，例外
 *  ネストカウントが0の時にタスクコンテキスト，0より大きい場合に非タス
 *  クコンテキストであると判断する．
 */
typedef struct target_processor_control_block {
	uint32_t	excpt_nest_count;	/* 例外ネストカウント */
	STK_T		*istkpt;			/* 非タスクコンテキスト用スタックの初期値 */
	STK_T		*idstkpt;			/* アイドル処理用スタックの初期値 */
	const FP	*p_inh_tbl;			/* 割込みハンドラテーブルへのポインタ */
	const FP	*p_exc_tbl;			/* CPU例外ハンドラテーブルへのポインタ */
} TPCB;

#ifdef USE_THREAD_ID_PCB
/*
 *  スレッドIDレジスタから自プロセッサのPCBへのポインタを取り出す
 */
Inline PCB *
core_get_my_pcb(void)
{
	uint32_t reg;

	CP15_READ_TPIDRPRW(reg);
	return((PCB *) reg);
}

#define get_my_pcb()	core_get_my_pcb()

#endif /* USE_THREAD_ID_PCB */

/*
 *  レディキューサーチのためのビットマップサーチ関数
 *
 *  CLZ命令は最上位ビットからサーチするため，最上位ビットを最高優先度に
 *  対応させる．
 */
#define	OMIT_BITMAP_SEARCH
#define	PRIMAP_BIT(pri)		(0x8000U >> (pri))

Inline uint_t
bitmap_search(uint16_t bitmap)
{
	return((uint_t)(count_leading_zero((uint32_t) bitmap) - 16));
}

/*
 *  コンテキストの参照
 *
 *  sense_contextを，インライン関数ではなくマクロ定義としているのは，
 *  この時点ではPCBが定義されていないためである．
 */
#define sense_context(p_pcb) \
				((p_pcb)->target_pcb.excpt_nest_count > 0U)

#endif /* TOPPERS_MACRO_ONLY */

/*
 *  TOPPERS標準割込み処理モデルの実現
 *
 *  ARMコア依存部では，割込みの扱いに関して，次の2つの方法をサポートす
 *  る．
 *
 *  (1) カーネルを単体で使用する場合やSafeGのノンセキュアモードで使用す
 *  る場合：IRQをカーネル管理の割込み，FIQをカーネル管理外の割込みとす
 *  る．デフォルトでは，この方法が使用される．
 *
 *  (2) SafeGのセキュアモードで使用する場合：FIQをカーネル管理の割込み
 *  とし，カーネルの動作中はIRQを常にマスクする．この方法を使用する場合
 *  には，TOPPERS_SAFEG_SECUREをマクロ定義する．
 * 
 *  TOPPERS標準割込み処理モデルの中で，割込み優先度マスクと割込み要求禁
 *  止フラグに関しては，割込みコントローラによって実現方法が異なるため，
 *  ARMコア依存部では扱わない．
 */

/*
 *  FIQとIRQの両方を禁止するCPSRのビットパターン
 */
#ifndef TOPPERS_MACRO_ONLY
#define CPSR_FIQ_IRQ_BIT	(CPSR_FIQ_BIT|CPSR_IRQ_BIT)
#else /* TOPPERS_MACRO_ONLY */
#define CPSR_FIQ_IRQ_BIT	(CPSR_FIQ_BIT AOR CPSR_IRQ_BIT)
#endif /* TOPPERS_MACRO_ONLY */

/*
 *  CPUロック・割込みロックでない状態でのCPSRのビットパターン
 */
#ifndef TOPPERS_SAFEG_SECURE
#define CPSR_UNLOCK			UINT_C(0x00)
#else /* TOPPERS_SAFEG_SECURE */
#define CPSR_UNLOCK			CPSR_IRQ_BIT
#endif /* TOPPERS_SAFEG_SECURE */

/*
 *  CPUロック状態でのCPSRのビットパターン
 */
#ifndef TOPPERS_SAFEG_SECURE
#define CPSR_CPULOCK		CPSR_IRQ_BIT
#else /* TOPPERS_SAFEG_SECURE */
#define CPSR_CPULOCK		CPSR_FIQ_IRQ_BIT
#endif /* TOPPERS_SAFEG_SECURE */

/*
 *  割込みロック状態でのCPSRのビットパターン
 */
#define CPSR_INTLOCK		CPSR_FIQ_IRQ_BIT

#ifndef TOPPERS_MACRO_ONLY

/*
 *  CPUロック状態への遷移
 */
Inline void
lock_cpu(void)
{
#ifndef TOPPERS_SAFEG_SECURE
	disable_irq();
#else /* TOPPERS_SAFEG_SECURE */
	disable_fiq();
#endif /* TOPPERS_SAFEG_SECURE */

	/*
	 *  メモリ参照が，この関数を超えて最適化されることを抑止
	 */
	ARM_MEMORY_CHANGED;
}

/*
 *  CPUロック状態への移行（ディスパッチできる状態）
 */
#define lock_cpu_dsp()		lock_cpu()

/*
 *  CPUロック状態の解除
 */
Inline void
unlock_cpu(void)
{
	/*
	 *  メモリ参照が，この関数を超えて最適化されることを抑止
	 */
	ARM_MEMORY_CHANGED;

#ifndef TOPPERS_SAFEG_SECURE
	enable_irq();
#else /* TOPPERS_SAFEG_SECURE */
	enable_fiq();
#endif /* TOPPERS_SAFEG_SECURE */
}

/*
 *  CPUロック状態の解除（ディスパッチできる状態）
 */
#define unlock_cpu_dsp()	unlock_cpu()

/*
 *  CPUロック状態の参照
 */
Inline bool_t
sense_lock(void)
{
#ifndef TOPPERS_SAFEG_SECURE
	return((current_cpsr() & CPSR_IRQ_BIT) != 0U);
#else /* TOPPERS_SAFEG_SECURE */
	return((current_cpsr() & CPSR_FIQ_BIT) != 0U);
#endif /* TOPPERS_SAFEG_SECURE */
}

/*
 *  割込みを受け付けるための遅延処理
 */
Inline void
delay_for_interrupt(void)
{
}

/*
 *  非タスクコンテキスト用のスタック初期値
 */
#define TOPPERS_ISTKPT(istk, istksz)	((STK_T *)((char *)(istk) + (istksz)))

/*
 *  タスクコンテキストブロックの定義
 */
typedef struct task_context_block {
	void	*sp;		/* スタックポインタ */
	FP		pc;			/* 実行再開番地 */
} TSKCTXB;

/*
 *  タスクディスパッチャ
 */

/*
 *  最高優先順位タスクへのディスパッチ（core_support.S）
 *
 *  dispatchは，タスクコンテキストから呼び出されたサービスコール処理か
 *  ら呼び出すべきもので，タスクコンテキスト・CPUロック状態・ディスパッ
 *  チ許可状態・（モデル上の）割込み優先度マスク全解除状態で呼び出さな
 *  ければならない．
 */
extern void dispatch(void);

/*
 *  非タスクコンテキストからのディスパッチ要求
 */
#define request_dispatch_retint()

/*
 *  ディスパッチャの動作開始（core_support.S）
 *
 *  start_dispatchは，カーネル起動時に呼び出すべきもので，すべての割込
 *  みを禁止した状態（割込みロック状態と同等の状態）で呼び出さなければ
 *  ならない．
 */
extern void start_dispatch(void) NoReturn;

/*
 *  現在のコンテキストを捨ててディスパッチ（core_support.S）
 *
 *  exit_and_dispatchは，ext_tskから呼び出すべきもので，タスクコンテキ
 *  スト・CPUロック状態・ディスパッチ許可状態・（モデル上の）割込み優先
 *  度マスク全解除状態で呼び出さなければならない．
 */
extern void exit_and_dispatch(void) NoReturn;

/*
 *  カーネルの終了処理の呼出し（core_support.S）
 *
 *  call_exit_kernelは，カーネルの終了時に呼び出すべきもので，非タスク
 *  コンテキストに切り換えて，カーネルの終了処理（exit_kernel）を呼び出
 *  す．
 */
extern void call_exit_kernel(PCB *p_my_pcb) NoReturn;
     
/*
 *  自タスクのマイグレーション（core_support.S）
 */
extern void dispatch_and_migrate(PCB *p_my_pcb, TCB *p_selftsk);

/*
 *  現在のコンテキストを捨てマイグレーション（core_support.S）
 */
extern void exit_and_migrate(PCB *p_my_pcb, TCB *p_selftsk);

/*
 *  メモリバリア操作
 */
Inline void
memory_barrier(void)
{
	data_memory_barrier();
}

/*
 *  タスクコンテキストの初期化
 *
 *  タスクが休止状態から実行できる状態に遷移する時に呼ばれる．この時点
 *  でスタック領域を使ってはならない．
 *
 *  activate_contextを，インライン関数ではなくマクロ定義としているのは，
 *  この時点ではTCBが定義されていないためである．
 */
extern void start_r(void);

#define activate_context(p_tcb)											\
{																		\
	(p_tcb)->tskctxb.sp = (void *)((char *)((p_tcb)->p_tinib->stk)		\
										+ (p_tcb)->p_tinib->stksz);		\
	(p_tcb)->tskctxb.pc = (FP) start_r;									\
}

/*
 *  ディスパッチハンドラを呼ばない場合はコードから除外する
 */
#ifdef USE_BYPASS_IPI_DISPATCH_HANDER
#define OMIT_DISPATCH_HANDLER
#endif /* USE_BYPASS_IPI_DISPATCH_HANDER */

/*
 *  アイドル処理用のスタック値アクセステーブル（kernel_cfg.c）
 */
extern STK_T *const idstkpt_table[TNUM_PRCID];

/*
 *  標準の割込み管理機能の初期化処理を用いない
 */
#define OMIT_INITIALIZE_INTERRUPT

/*
 *  割込みハンドラアクセステーブル（kernel_cfg.c）
 */
extern const FP* const p_inh_table[TNUM_PRCID];

/*
 *  割込み要求ライン設定アクセステーブル（kernel_cfg.c）
 * 
 *  割込み要求ラインに対して割込み属性が設定されていれば1，設定されてい
 *  なければ0を保持するテーブル．
 */
#ifdef USE_INTCFG_TABLE
extern const uint8_t* const p_intcfg_table[TNUM_PRCID];
#endif /* USE_INTCFG_TABLE */

/*
 *  標準の例外管理機能の初期化処理を用いない
 */
#define OMIT_INITIALIZE_EXCEPTION

/*
 *  CPU例外ハンドラアクセステーブル（kernel_cfg.c）
 */
extern const FP* const p_exc_table[TNUM_PRCID];

/*
 *  CPU例外ハンドラの初期化
 */
Inline void
initialize_exception(PCB *p_my_pcb)
{
}

/*
 *  CPU例外の発生した時のコンテキストの参照
 *
 *  CPU例外の発生した時のコンテキストが，タスクコンテキストの時にfalse，
 *  そうでない時にtrueを返す．
 */
Inline bool_t
exc_sense_context(void *p_excinf)
{
	return(((T_EXCINF *)(p_excinf))->nest_count != 0U);
}

/*
 *  CPU例外の発生した時の割込み優先度マスクの参照
 *
 *  この関数は，CPU例外がタスクコンテキストで発生した場合にのみ呼び出
 *  される．そのため，CPU例外が非タスクコンテキストで発生した場合には，
 *  正しい値を返す必要がない．
 */
Inline PRI
exc_get_intpri(void *p_excinf)
{
	return((PRI)(((T_EXCINF *)(p_excinf))->intpri));
}

/*
 *  CPUロック状態または割込みロック状態かの参照
 */
Inline bool_t
exc_sense_lock(void *p_excinf)
{
#ifndef TOPPERS_SAFEG_SECURE
	return(((((T_EXCINF *)(p_excinf))->cpsr) & CPSR_INT_MASK) != 0U);
#else /* TOPPERS_SAFEG_SECURE */
	return(((((T_EXCINF *)(p_excinf))->cpsr) & CPSR_FIQ_BIT) != 0U);
#endif /* TOPPERS_SAFEG_SECURE */
}

/*
 *  CPU例外の発生した時のコンテキストと割込みのマスク状態の参照
 *
 *  CPU例外の発生した時のシステム状態が，カーネル実行中でなく，タスクコ
 *  ンテキストであり，全割込みロック状態でなく，CPUロック状態でなく，割
 *  込み優先度マスク全解除状態である時にtrue，そうでない時にfalseを返す
 *  （CPU例外がカーネル管理外の割込み処理中で発生した場合にもfalseを返
 *  す）．
 *
 */
Inline bool_t
exc_sense_intmask(void *p_excinf)
{
	return(!exc_sense_context(p_excinf)
				&& exc_get_intpri(p_excinf) == TIPM_ENAALL
				&& !exc_sense_lock(p_excinf));
}

#endif /* TOPPERS_MACRO_ONLY */

/*
 *  MMU関連の操作（VMSA）
 */
#ifdef USE_ARM_MMU

/*
 *  変換テーブルベースレジスタ（TTBR）の設定値
 */
#if __TARGET_ARCH_ARM < 7
#define TTBR_CONFIG		(CP15_TTBR_RGN_CACHEABLE|CP15_TTBR_RGN_SHAREABLE \
												|CP15_TTBR_RGN_WBACK)
#else /* __TARGET_ARCH_ARM < 7 */
#define TTBR_CONFIG		(CP15_TTBR_RGN_SHAREABLE|CP15_TTBR_RGN_WBWA \
												|CP15_TTBR_IRGN_WBWA)
#endif

/*
 *  MMUの初期化
 */
#ifndef TOPPERS_MACRO_ONLY

extern void arm_mmu_initialize(void);

/*
 *  MMUの設定情報のデータ型
 */
typedef struct arm_mmu_config {
	uint32_t	vaddr;			/* 仮想アドレス */
	uint32_t	paddr;			/* 物理アドレス */
	uint32_t	size;			/* サイズ */
	uint32_t	attr;			/* セクション属性 */
} ARM_MMU_CONFIG;

/*
 *  MMUの設定情報の数（メモリエリアの数）（target_kernel_impl.c）
 */
extern const uint_t arm_tnum_memory_area;

/*
 *  MMUの設定情報（メモリエリアの情報）（target_kernel_impl.c）
 */
extern ARM_MMU_CONFIG arm_memory_area[];


#endif /* TOPPERS_MACRO_ONLY */
#endif /* USE_ARM_MMU */

#ifndef TOPPERS_MACRO_ONLY

/*
 *  コア依存の初期化
 */
extern void core_initialize(PCB *p_my_pcb);

/*
 *  コア依存の終了処理
 */
extern void core_terminate(void);

/*
 *  CPU例外の発生状況のログ出力
 */
#ifndef OMIT_XLOG_SYS
extern void xlog_sys(void *p_excinf);
extern void xlog_fsr(uint32_t fsr, uint32_t far);
#endif /* OMIT_XLOG_SYS */

/*
 *  未定義の割込みが入った場合の処理
 */
extern void default_int_handler(void);

/*
 *  未定義の例外が入った場合の処理
 */
extern void default_exc_handler(void *p_excinf, EXCNO excno);

/*
 *  ロックに関する定義と操作
 */

/*
 *  キャッシュラインのサイズ
 *
 *  1つのキャッシュラインに1つのロック変数のみを配置するために使用する．
 *  キャッシュラインのサイズはコアによって異なるため，チップ依存部で定
 *  義すべきである．32または64であることが多いため，ここでは，大きい側
 *  の64で定義する．
 */
#ifndef ARM_CACHELINE_SIZE
#define ARM_CACHELINE_SIZE	64		/* キャッシュラインのサイズ */
#endif /* ARM_CACHELINE_SIZE */

/*
 *  ロックの型
 */
typedef volatile uint32_t __attribute__((aligned(ARM_CACHELINE_SIZE)))
							LOCK[ARM_CACHELINE_SIZE / sizeof(uint32_t)];

/*
 *  ロックの初期化
 */
Inline void
initialize_lock(LOCK *p_lock)
{
	(*p_lock)[0] = 0U;
}

/*
 *  ロックの取得
 *
 *  ロック取得失敗時に，スピンを繰り返すのではなく，WFE命令によりイベ
 *  ント待ちとすることが推奨されている．
 *
 *  WFE命令によるイベント待ちの際，割込み応答性を低下させないため，割
 *  込みを許可した状態で，イベント待ちとするべきである．WFE命令自身は，
 *  割込みの許可／禁止を行わないため，割込みを明示的に許可／禁止する．
 *
 *  その際，割込みが許可された直後（WFE命令を実行する前）に割込みが入
 *  り，その後，割込みハンドラからのリターン後にWFE命令によりイベント
 *  待ちになり，イベントを逃す可能性が考えられる．
 *
 *  しかしながら，割込みハンドラからのリターン（正確には例外一般からの
 *  リターン）を行うと，イベントレジスタがセットされる．WFE命令実行時
 *  にイベントレジスタがセットされていると，クリアしてWFE命令が即座に
 *  終了するため，問題ない．
 */
Inline void
acquire_lock(LOCK *p_lock)
{
	while (test_and_set_uint32(&(*p_lock)[0])) {
		unlock_cpu();
#ifndef TOPPERS_OMIT_USE_WFE
		data_sync_barrier();
		arm_wait_for_event();
#else /* TOPPERS_OMIT_USE_WFE */
		delay_for_interrupt();
#endif /* TOPPERS_OMIT_USE_WFE */
		lock_cpu();
	}
	/* ロック取得成功 */
	data_memory_barrier();
	ARM_MEMORY_CHANGED;
}

/*
 *  ロックの取得の試行
 */
Inline bool_t
try_lock(LOCK *p_lock)
{
	if (test_and_set_uint32(&(*p_lock)[0])) {
		return(true);
	}
	/* ロック取得成功 */
	data_memory_barrier();
	ARM_MEMORY_CHANGED;
	return(false);
}

/*
 *  ロックの解放
 */
Inline void
release_lock(LOCK *p_lock)
{
	ARM_MEMORY_CHANGED;
	data_memory_barrier();
	(*p_lock)[0] = 0U;
#ifndef TOPPERS_OMIT_USE_WFE
	data_sync_barrier();
	arm_send_event();
#endif /* TOPPERS_OMIT_USE_WFE */
}

/*
 *  ロック状態の参照
 */
Inline bool_t
refer_lock(LOCK *p_lock)
{
	return((*p_lock)[0] != 0U);
}

/*
 *  ジャイアントロックに関する定義と操作
 */

/*
 *  ジャイアントロック用のロック変数
 */
extern LOCK giant_lock;

/*
 *  ロックの初期化
 */
#define initialize_glock()		initialize_lock(&giant_lock)

/*
 *  ロックの取得
 */
#define acquire_glock()		acquire_lock(&giant_lock)

/*
 *  ロックの取得の試行（ポーリング）
 */
#define try_glock()			try_lock(&giant_lock)

/*
 *  ロックの解放
 */
#define release_glock()		release_lock(&giant_lock)

/*
 *  ネイティブスピンロックに関する定義と操作
 *
 *  この時点では，SPNINIBが定義されていないため，ネイティブスピンロッ
 *  クに対する操作はマクロで実現している．
 */

/*
 *  スピンロックの初期化
 */
#define initialize_native_spn(p_spninib) \
				initialize_lock((LOCK *)((p_spninib)->lock))

/*
 *  スピンロックの取得
 */
#define lock_native_spn(p_spninib) \
				acquire_lock((LOCK *)((p_spninib)->lock))

/*
 *  スピンロックの取得の試行
 */
#define try_native_spn(p_spninib) \
				try_lock((LOCK *)((p_spninib)->lock))

/*
 *  スピンロックの返却
 */
#define unlock_native_spn(p_spninib) \
				release_lock((LOCK *)((p_spninib)->lock))

/*
 *  スピンロック状態の参照
 */
#define refer_native_spn(p_spninib) \
				refer_lock((LOCK *)((p_spninib)->lock))

/*
 *  エミュレーションされたスピンロックに関する定義
 */
#define delay_for_emulate_spn()

#endif /* TOPPERS_MACRO_ONLY */

/*
 *  整合性検査のための定義
 *
 *  保存されているスタックポインタが，4バイト境界にアラインし，スタッ
 *  ク上を指しているかをチェックする．
 *
 *  VALID_TSKCTXBを，インライン関数ではなくマクロ定義としているのは，
 *  この時点ではTCBが定義されていないためである．
 */
#define VALID_TSKCTXB(p_tskctxb, p_tcb)								\
			((((uintptr_t)((p_tskctxb)->sp) & 0x03U) == 0U)			\
				&& on_sstack((p_tskctxb)->sp, 0, (p_tcb)->p_tinib))

/*
 *  追加コンテキスト保存/解放フック（コプロセッサ等を持たないため空）
 *    save_context: mig_tsk(移行時)，release_context: task_terminate(終了時)に呼ばれる．
 */
#define save_context(p_tcb)		((void)(p_tcb))
#define release_context(p_tcb)		((void)(p_tcb))

#endif /* TOPPERS_CORE_KERNEL_IMPL_H */
