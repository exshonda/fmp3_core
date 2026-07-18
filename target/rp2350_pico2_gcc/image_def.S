/*
 *  RP2350 bootROM 用 IMAGE_DEF（最小ブロック，20バイト）
 *
 *  RP2350 の bootROM は，フラッシュ先頭 4KB から PICOBIN ブロックを探索し，
 *  IMAGE_TYPE アイテムを持つ実行イメージを起動する．本ブロックはベクタ表
 *  エントリを持たない最小構成であり，bootROM はイメージ先頭（0x10000000）を
 *  ベクタ表とみなして SP=vector[0] / PC=vector[1] で起動する．
 *
 *  形式は pico-sdk picobin.h に基づく：
 *    START      = 0xffffded3
 *    IMAGE_TYPE item  : 0x42, size=1, 0x1021 (EXE|Secure|ARM|RP2350)
 *    LAST item        : 0xff, (size-16)/4, 0
 *    link             : 0（単一ブロックの自己ループ）
 *    END        = 0xab123579
 */

	.section .picobin_block, "a"
	.p2align 2
	.global image_def_block
image_def_block:
embedded_block:
	.word	0xffffded3	/* PICOBIN_BLOCK_MARKER_START */

	/* IMAGE_TYPE item */
	.byte	0x42		/* PICOBIN_BLOCK_ITEM_1BS_IMAGE_TYPE */
	.byte	0x1		/* size: 1 word */
	.hword	0x1021		/* IMAGE_TYPE_EXE | EXE_SECURITY_S | EXE_CPU_ARM | EXE_CHIP_RP2350 */

	/* LAST item */
	.byte	0xff		/* PICOBIN_BLOCK_ITEM_2BS_LAST */
	.hword	(embedded_block_end - embedded_block - 16) / 4
	.byte	0

	/* single-block loop (points to self) */
	.word	0

	.word	0xab123579	/* PICOBIN_BLOCK_MARKER_END */
embedded_block_end:
