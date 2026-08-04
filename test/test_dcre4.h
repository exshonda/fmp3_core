/*
 *		動的生成API（acre_dtq/del_dtq・acre_pdq/del_pdq・acre_mpf/del_mpf）
 *		のテスト
 */

#include <kernel.h>
#include "target_test.h"

#define HIGH_PRIORITY	9
#define MID_PRIORITY	10
#define LOW_PRIORITY	11

#ifndef STACK_SIZE
#define	STACK_SIZE		4096
#endif /* STACK_SIZE */

/*
 *  カーネルメモリプールのサイズ
 *
 *  ★この値は変異controlの成立条件でもある．kernel/startup.cのプールは
 *  bump allocatorで，free_mempoolはcountが0になったときにだけbrkを
 *  先頭へ戻す（段階3b 訂正G）．したがって
 *
 *    ・正常時：acre_mpf/del_mpfを1周するとcountが0に戻りbrkがリセット
 *              されるので，MPF_CYCLES回まわしても消費は1周分（272B前後）
 *              で頭打ちになる．
 *    ・del_mpfのTA_MEMALLOC解放を落とすと：1周ごとにブロック領域
 *              （ROUND_MPF_T(64)*4 = 256B）が返らずbrkが単調に進み，
 *              MPK_SIZEを7〜8周で使い切ってacre_mpfがE_NOMEMになる．
 *
 *  MPK_SIZE = 2048 は「1周分（272B前後）は十分入るが，MPF_CYCLES(=16)周
 *  ぶんの累積（4KB超）は入らない」ように選んである．
 */
#ifndef MPK_SIZE
#define MPK_SIZE		2048
#endif /* MPK_SIZE */

/*  動的生成する固定長メモリプールの諸元  */
#define MPF_BLKCNT		4
#define MPF_BLKSZ		64
#define MPF_CYCLES		16

/*  ユーザ供給の管理領域を持つデータキューの容量  */
#define USER_DTQCNT		2

/*  他プロセッサのタスクが待ちに入るのを待つ時間（単位: マイクロ秒）  */
#ifndef TEST_TIME_PROC
#define TEST_TIME_PROC	200000U		/* 200ms */
#endif /* TEST_TIME_PROC */

#ifndef TOPPERS_MACRO_ONLY
extern void	task1(EXINF exinf);
extern void	task2(EXINF exinf);
extern void	task3(EXINF exinf);
#endif /* TOPPERS_MACRO_ONLY */
