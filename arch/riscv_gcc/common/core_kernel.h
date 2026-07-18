/*
 *  TOPPERS/FMP Kernel
 *      Toyohashi Open Platform for Embedded Real-Time Systems/
 *      Flexible MultiProcessor Kernel
 * 
 *  Copyright (C) 2024 by Embedded and Real-Time Systems Laboratory
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
 *  $Id: core_kernel.h 335 2023-04-18 10:50:40Z ertl-honda $
 */

/*
 *    kernel.hのコア依存部（RISC-V用）
 *
 *  このヘッダファイルは，target_kernel.h（または，そこからインクルード
 *  されるファイル）のみからインクルードされる．他のファイルから直接イ
 *  ンクルードしてはならない．
 */

#ifndef TOPPERS_CORE_KERNEL_H
#define TOPPERS_CORE_KERNEL_H

/*
 *  スタックの型
 *
 *  RISC-Vでは，スタックを16バイト境界に配置する必要がある．
 *  __int128 の可否は ISA 系統ではなく汎用レジスタのワード幅(TImode の有無)で
 *  決まる(rv64/AArch64 等の64bitターゲットは可, rv32/RV32E 等の32bitは不可)．
 *  GCC が __int128 を持つ場合に定義する __SIZEOF_INT128__ で直接判定する．
 *  ・__int128 あり : __int128 で16バイト幅・境界を確保(従来通り)．
 *  ・__int128 なし : 16バイト幅・16バイト境界の構造体 toppers_stk_t で代替する
 *                   (型の実体 typedef は下の TOPPERS_MACRO_ONLY ブロック内)．
 */
#ifdef __SIZEOF_INT128__
#define TOPPERS_STK_T    __int128
#else /* __SIZEOF_INT128__ */
#define TOPPERS_STK_T    toppers_stk_t
#endif /* __SIZEOF_INT128__ */

/*
 *  CPU例外ハンドラ番号の数
 */
#ifndef TNUM_EXCNO
#define TNUM_EXCNO    16
#endif /* TNUM_EXCNO */

/*
 *  CPU例外ハンドラ番号の定義
 */
#define EXCNO_MISALIGNED_FETCH  UINT_C(0)
#define EXCNO_FAULT_FETCH       UINT_C(1)
#define EXCNO_IINST             UINT_C(2)
#define EXCNO_BREAKPOINT        UINT_C(3)
#define EXCNO_MISALIGNED_LOAD   UINT_C(4)
#define EXCNO_FAULT_LOAD        UINT_C(5)
#define EXCNO_MISALIGNED_STORE  UINT_C(6)
#define EXCNO_FAULT_STORE       UINT_C(7)
#define EXCNO_USER_ECALL        UINT_C(8)
#define EXCNO_SUPERVISOR_ECALL  UINT_C(9)
#define EXCNO_HYPERVISOR_ECALL  UINT_C(10)
#define EXCNO_MACHINE_ECALL     UINT_C(11)
#define EXCNO_FETCH_PAGE_FAULT  UINT_C(12)
#define EXCNO_LOAD_PAGE_FAULT   UINT_C(13)
#define EXCNO_RESERVED_14       UINT_C(14)
#define EXCNO_STORE_PAGE_FAULT  UINT_C(15)

#ifndef TOPPERS_MACRO_ONLY

/*
 *  スタックの型の実体（__int128 非対応ターゲット: rv32/RV32E 等）．
 *  16バイト幅かつ16バイト境界の型が必要(スタック領域確保単位)．要素サイズ<
 *  アラインメントだと配列に使えないため，サイズ・アラインメントとも16の構造体
 *  で代替する．本宣言は C 専用のため TOPPERS_MACRO_ONLY ブロック内に置き，
 *  アセンブラからは自動的に除外する(個別の __ASSEMBLER__ ガードは不要)．
 */
#ifndef __SIZEOF_INT128__
typedef struct { unsigned int _stkunit[4]; } __attribute__((aligned(16))) toppers_stk_t;
#endif /* __SIZEOF_INT128__ */

#if __riscv_flen == 64
#define FREG_T  double
#elif __riscv_flen == 32
#define FREG_T  float
#endif /* __riscv_flen == 64 */

/*
 *  CPU例外の情報を記憶しているメモリ領域の構造
 *
 *  割込み優先度マスクは，CPU例外がタスクコンテキストで発生した場合に
 *  のみ有効である．非タスクコンテキストで発生した場合には，正しい値と
 *  ならない場合がある．
 */
typedef struct t_excinf {
    int32_t  intpri;    /* 割込み優先度マスク */
    uint32_t exncnt;    /* 例外ネストカウント */
    ulong_t ra;
    ulong_t tp;    
    ulong_t t0;
    ulong_t t1;    
    ulong_t t2;
    ulong_t a0;
    ulong_t a1;
    ulong_t a2;
    ulong_t a3;
    ulong_t a4;
    ulong_t a5;
    ulong_t pc;
    ulong_t mstatus;
#ifndef __riscv_32e    
    ulong_t t3;
    ulong_t t4;
    ulong_t t5;
    ulong_t t6;
    ulong_t a6;
    ulong_t a7;
#endif /* !__riscv_32e */
#ifdef __riscv_flen
    FREG_T ft0;
    FREG_T ft1;
    FREG_T ft2;
    FREG_T ft3;
    FREG_T ft4;
    FREG_T ft5;
    FREG_T ft6;
    FREG_T ft7;     
    FREG_T fa0;
    FREG_T fa1;
    FREG_T fa2;
    FREG_T fa3;
    FREG_T fa4;
    FREG_T fa5;
    FREG_T fa6;
    FREG_T fa7;
    FREG_T ft8;
    FREG_T ft9;
    FREG_T ft10;
    FREG_T ft11 ;          
#endif /* __riscv_flen */        
} T_EXCINF;

#endif /* TOPPERS_MACRO_ONLY */
#endif /* TOPPERS_CORE_KERNEL_H */
