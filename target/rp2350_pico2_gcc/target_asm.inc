/*
 *  アセンブリ言語用マクロ定義のターゲット依存部（RP2350 用）
 */

#ifndef TOPPERS_TARGET_ASM_INC
#define TOPPERS_TARGET_ASM_INC

/*
 *  チップ依存部で共通な定義
 */
#include "chip_asm.inc"

/*
 *  セミホスティング（SWD）でデバッグ・テストする場合の idle 処理．
 *
 *  RP2350 の Cortex-M33 は wfi でコアクロックがゲートされ，デバッグ
 *  アクセスポート(AP)へのアクセスが WAIT でストールする（openocd が
 *  "Too long SWD WAIT" を出して以後コアを操作できなくなる）．SWD 経由
 *  でセミホスティング出力を取得するビルドでは，idle で wfi を発行せず
 *  ビジーループにして AP を常時アクセス可能に保つ．割込み（タイマ
 *  ティック等）は basepri を全許可にしているので受け付けられる．
 *
 *  実運用（UART）ビルドでは本マクロを定義しないため，通常どおり
 *  core_support.S 側の wfi を用いる idle が使われる．
 */
#ifdef TOPPERS_USE_SEMIHOSTING
#define TOPPERS_CUSTOM_IDLE

	.macro	toppers_asm_custom_idle
	my_pcb r0, r1
	/* アイドル処理用スタックへ切り替え */
	mov    r1, #0
	msr    psplim, r1
	ldr    sp, [r0, #PCB_idstkpt]
	ldr    r1, [r0, #PCB_idstktop]
	msr    psplim, r1
	/* CPU ロック解除状態へ */
	mov    r1, #0
	str    r1, [r0, #PCB_lock_flag]
	ldr    r0, =#IIPM_ENAALL
	msr    basepri, r0
1:
	/* wfi を発行せず，AP を常時アクセス可能に保つためのビジーループ */
	dsb
	b      1b
	.endm
#endif /* TOPPERS_USE_SEMIHOSTING */

#endif /* TOPPERS_TARGET_ASM_INC */
