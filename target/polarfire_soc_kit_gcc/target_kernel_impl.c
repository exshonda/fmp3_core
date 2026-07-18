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
 *  $Id: target_kernel_impl.c 335 2023-04-18 10:50:40Z ertl-honda $
 */

/*
 *    カーネルのターゲット依存部（Plarfire SoC Kit用）
 */

#include "kernel_impl.h"
#include <sil.h>
#include "riscv.h"

/*
 *  カーネル動作時のメモリマップと関連する定義(ToDo)
 */

/*
 *  システムログの低レベル出力のための初期化
 */
#ifndef TOPPERS_OMIT_TECS

/*
 *  セルタイプtPutLogSIOPort内に実装されている関数を直接呼び出す．
 */
extern void tPutLogSIOPort_initialize(void);

#else /* TOPPERS_OMIT_TECS */

extern void sio_initialize(EXINF exinf);
extern void target_fput_initialize(void);

#endif /* TOPPERS_OMIT_TECS */

/*
 *  entry point (start.S)
 */
extern void start(void);

/*
 *  ハードウェアの初期化
 */
void
hardware_init_hook(void)
{

}

/*
 *  Master processor initialization before str_ker().
 */
void
target_mprc_initialize(void)
{
    chip_mprc_initialize();
}

/*
 *  ターゲット依存の初期化
 */
void
target_initialize(PCB *p_my_pcb)
{    
    /*
     *  チップ依存の初期化
     */
    chip_initialize(p_my_pcb);

    /*
     *  SIOを初期化
     */
#ifdef USE_UART0    
    /* クロックの有効化 */
    sil_wrw_mem(SYSREG_SUBBLK_CLOCK_CR, 
                sil_rew_mem(SYSREG_SUBBLK_CLOCK_CR)
                | SYSREG_SUBBLK_CLOCK_CR_UART0);
    
    /* リセットの解除 */
    sil_wrw_mem(SYSREG_SOFT_RESET_CR, 
                sil_rew_mem(SYSREG_SOFT_RESET_CR)
                & ~SYSREG_SOFT_RESET_CR_UART0);
#endif /* USE_UART0 */

#ifdef USE_UART1 
    /* クロックの有効化 */
    sil_wrw_mem(SYSREG_SUBBLK_CLOCK_CR, 
                sil_rew_mem(SYSREG_SUBBLK_CLOCK_CR)
                | SYSREG_SUBBLK_CLOCK_CR_UART1);
    
    /* リセットの解除 */
    sil_wrw_mem(SYSREG_SOFT_RESET_CR, 
                sil_rew_mem(SYSREG_SOFT_RESET_CR)
                & ~SYSREG_SOFT_RESET_CR_UART1);
#endif /* USE_UART1 */    
    
    sio_initialize(0);
    target_fput_initialize();
}

/*
 *  デフォルトのsoftware_term_hook（weak定義）
 */
__attribute__((weak))
void software_term_hook(void)
{
}

/*
 *  ターゲット依存の終了処理
 */
void
target_exit(void)
{
    /*
     *  software_term_hookの呼出し
     */
    software_term_hook();

    /*
     *  チップ依存の終了処理
     */
    chip_terminate();

    while (true) ;
}


/*
 *  システムログの低レベル出力（本来は別のファイルにすべき）
 */
#include "target_syssvc.h"
#include "target_serial.h"

/*
 *  低レベル出力用のSIOポート管理ブロック
 */
static SIOPCB  *p_siopcb_target_fput;

/*
 *  SIOポートの初期化
 */
void
target_fput_initialize(void)
{
    p_siopcb_target_fput = sio_opn_por(SIOPID_FPUT, 0);
}

/*
 *  SIOポートへのポーリング出力
 */
Inline void
polafire_soc_kit_uart_fput(char c)
{
    /*
     *  送信できるまでポーリング
     */
    while (!(sio_snd_chr(p_siopcb_target_fput, c))) {
        sil_dly_nse(100);
    }
}

/*
 *  SIOポートへの文字出力
 */
void
target_fput_log(char c)
{
    if (c == '\n') {
        polafire_soc_kit_uart_fput('\r');
    }
    polafire_soc_kit_uart_fput(c);
}

/*
 *  _sbrk（newlib のヒープ確保）
 *
 *  newlib の malloc/printf 系（_sbrk_r 経由）がヒープを要求するため，自己完結
 *  の静的ヒープを用いた最小限の _sbrk を提供する．SDK の newlib_stubs.c は
 *  _write 等が FMP3 のシリアル出力と競合するため取り込まず，本実装を用いる．
 */
#include <sys/types.h>

#ifndef TARGET_HEAP_SIZE
#define TARGET_HEAP_SIZE  (64 * 1024)
#endif /* TARGET_HEAP_SIZE */

static char target_heap[TARGET_HEAP_SIZE];

caddr_t
_sbrk(int incr)
{
    static char *heap_end = target_heap;
    char        *prev_heap_end = heap_end;

    if ((heap_end + incr) > (target_heap + TARGET_HEAP_SIZE)
            || (heap_end + incr) < target_heap) {
        return (caddr_t) -1;            /* ヒープ不足 */
    }
    heap_end += incr;
    return (caddr_t) prev_heap_end;
}
