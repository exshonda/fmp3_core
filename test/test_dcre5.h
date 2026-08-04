/*
 *		動的生成API（acre_isr/del_isr）のテスト
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
 *  ENA_DYNISR されていない有効な割込み番号（acre_isr の E_OBJ 検査用）
 *
 *  INTNO1 は musca_b1 では (1 << 16) | (60 + 16) ＝ PRC1 の予備 NVIC IRQ60
 *  である．+1 は同じプロセッサの予備 IRQ61 で，CFG_INT もされていない．
 *  「有効範囲内だが適格 intno 表に載っていない」ケースを作るために使う．
 */
#define INTNO_UNOPTED	(INTNO1 + 1)

/*
 *  有効範囲外の割込み番号（acre_isr の E_OBJ 検査用）
 *
 *  ★dcre は範囲外を E_PAR にするが，FMP3 の acre_isr は範囲検査を持たない
 *  （FMP3 の VALID_INTNO が (prcid, intno) の2引数で呼出しコアに依存しうる
 *  ため．ISR段階の訂正A）．したがって範囲外も E_OBJ になる．
 *  値は tools/cfg_error_tests/e_par_creisr_intno_keyerror.cfg と同じ 99999．
 */
#define INTNO_BAD		99999

/*  ISR の呼出し順序を記録するログ  */
#define ISR_LOG_SIZE	16

/*  待ちループの上限（QEMU をハングさせないための保険）  */
#define SPIN_LIMIT		100000000U

/*
 *  quiesce 実証用の長い ISR の空回し回数
 *
 *  ★この値は変異 control の成立条件でもある．del_isr の quiesce ループを
 *  落とすと，del_isr が ISR 本体の完了を待たずに戻り，直後の
 *  check_assert(long_finished) が失敗する．そのためには「別コアのタスクが
 *  long_started を観測してから del_isr のジャイアントロック取得までに
 *  到達する時間」より，この空回しが十分に長い必要がある．
 *  足りない場合は増やす（減らさない）．調整したら最終値を記録すること．
 */
#define LONG_ISR_SPIN	2000000U

/*  PRC2 のタスクへの指令  */
#define CMD_NONE		0
#define CMD_HANDSHAKE	1		/*  走査中の del/acre（手順4）  */
#define CMD_FIRE_LONG	2		/*  quiesce 実証の割込み発生（手順5）  */
#define CMD_QUIT		3

#ifndef TOPPERS_MACRO_ONLY
extern void	task1(EXINF exinf);
extern void	task3(EXINF exinf);
extern void	static_isr(EXINF exinf);
extern void	dyn_isr(EXINF exinf);
extern void	long_isr(EXINF exinf);
extern void	ctx_isr(EXINF exinf);
#endif /* TOPPERS_MACRO_ONLY */
