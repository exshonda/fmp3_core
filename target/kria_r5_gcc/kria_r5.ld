/*
 *		リンカスクリプト（ZynqMP R5用）
 *
 *  メモリ配置（QEMU/DDR用）
 *    .vector : 0x00000000（SCTLR.V=0で例外ベクタとして参照される）
 *    .text等 : 0x7F000000（DDRの低位2GB内）
 *
 *  $Id$
 */

OUTPUT_FORMAT("elf32-littlearm", "elf32-bigarm", "elf32-littlearm")
OUTPUT_ARCH(arm)
ENTRY(start_r5);

MEMORY {
	VEC (xr)  : ORIGIN = 0x00000000, LENGTH = 4K
	RAM (xrw) : ORIGIN = 0x7F000000, LENGTH = 16M
}

SECTIONS {
	.vector : {
		KEEP(*(.vector))
	} > VEC

	.text : {
		__start_text = .;
		*(.text)
		*(.text.*)
		*(.glue_7t)
		*(.glue_7)
		__end_kernel = .;
	} > RAM
	_etext = .;
	PROVIDE(etext = .);

	.rodata ALIGN(4) : {
		__start_rodata = .;
		*(.rodata)
		*(.rodata.*)
		__end_rodata = .;
	} > RAM
	.ARM.exidx ALIGN(4) : {
		__exidx_start = .;
		*(.ARM.exidx* .gnu.linkonce.armexidx.*)
		__exidx_end = .;
	} > RAM

	.data : ALIGN(4) {
		__start_data = .;
		*(.data)
		*(.data.*)
		__end_data = .;
	} > RAM
	__start_idata = LOADADDR(.data);
	. = ALIGN(4);
	_edata = .;
	PROVIDE(edata = .);

	.bss ALIGN(4) (NOLOAD) : {
		__start_bss = .;
		*(.bss)
		*(.bss.*)
		*(.kernel_data_*)
		*(.stack*)
		*(COMMON)
		__end_bss = .;
	} > RAM
	. = ALIGN(4);
	_end = .;
	PROVIDE(end = .);
	.comment 0 : { *(.comment) }

	/*
	 *  DWARF debug sections.
	 *  Symbols in the DWARF debugging sections are relative to
	 *  the beginning of the section so we begin them at 0.
	 */

	/* DWARF 1 */
	.debug			0 : { *(.debug) }
	.line			0 : { *(.line) }

	/* GNU DWARF 1 extensions */
	.debug_srcinfo	0 : { *(.debug_srcinfo) }
	.debug_sfnames	0 : { *(.debug_sfnames) }

	/* DWARF 1.1 and DWARF 2 */
	.debug_aranges	0 : { *(.debug_aranges) }
	.debug_pubnames	0 : { *(.debug_pubnames) }

	/* DWARF 2 */
	.debug_info		0 : { *(.debug_info) }
	.debug_abbrev	0 : { *(.debug_abbrev) }
	.debug_line		0 : { *(.debug_line) }
	.debug_frame	0 : { *(.debug_frame) }
	.debug_str		0 : { *(.debug_str) }
	.debug_loc		0 : { *(.debug_loc) }
	.debug_macinfo	0 : { *(.debug_macinfo) }

	/* SGI/MIPS DWARF 2 extensions */
	.debug_weaknames 0 : { *(.debug_weaknames) }
	.debug_funcnames 0 : { *(.debug_funcnames) }
	.debug_typenames 0 : { *(.debug_typenames) }
	.debug_varnames  0 : { *(.debug_varnames) }
}
