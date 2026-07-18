/*
 *  TOPPERS/FMP Kernel
 *      Toyohashi Open Platform for Embedded Real-Time Systems/
 *      Flexible MultiProcessor Kernel
 *
 *  Copyright (C) 2006-2023 by Embedded and Real-Time Systems Laboratory
 *              Graduate School of Information Science, Nagoya Univ., JAPAN
 *
 *  上記著作権者は，以下の(1)～(4)の条件を満たす場合に限り，本ソフトウェ
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
 *  @(#) $Id: arm64.c 466 2026-06-03 13:25:10Z ertl-honda $
 */

#include "kernel_impl.h"

/*
 *  キャッシュ関連
 */
/*
 *  Iキャッシュの全ラインの無効化
 */
void
icache_invalidate_all(void)
{
	IC_IALLU();
	IC_IALLUIS();
	data_sync_barrier();
}

/*
 *  Dキャッシュの全ラインの無効化
 */
void
dcache_invalidate_all()
{
	uint64_t clidr;
	uint64_t ccsidr;
	uint64_t ctype;
	uint64_t level_max, level;
	uint64_t way_max, way;
	uint64_t set_max, set;
	uint64_t log2linesize;
	uint64_t bits;

	CLIDR_EL1_READ(clidr);
	level_max = (clidr & CLIDR_LOC_MASK) >> CLIDR_LOC_SHIFT;
	for (level = 0; level < level_max; level++) {
		ctype = (clidr >> (CLIDR_CTYPE_SHIFT * level)) & CLIDR_CTYPE_MASK;
		if ((ctype == CLIDR_CTYPE_DATA) || (ctype == CLIDR_CTYPE_SEPARATE)
				|| (ctype == CLIDR_CTYPE_UNIFIED)) {
			CSSELR_EL1_WRITE(level << CSSELR_LEVEL_SHIFT);
			inst_sync_barrier();
			CCSIDR_EL1_READ(ccsidr);
			way_max = ((ccsidr & CCSIDR_ASSOCIATIVITY_MASK)
					>> CCSIDR_ASSOCIATIVITY_SHIFT) + 1;
			set_max = ((ccsidr & CCSIDR_NUMSETS_MASK)
					>> CCSIDR_NUMSETS_SHIFT) + 1;
			log2linesize = ((ccsidr & CCSIDR_LINESIZE_MASK)
					>> CCSIDR_LINESIZE_SHIFT) + 4;
			data_sync_barrier();
			for (way = 0; way < way_max; way++) {
				for (set = 0; set < set_max; set++) {
					bits = 0;
					bits |= way << (__builtin_clz((uint32_t)way_max) + 1);
					bits |= set << log2linesize;
					bits |= level << DC_LEVEL_SHIFT;
					dcache_invalidate_line(bits);
				}
			}
		}
	}
	CSSELR_EL1_WRITE(0);
	data_sync_barrier();
	inst_sync_barrier();
}

/*
 *  Dキャッシュの全ラインのクリーンと無効化
 */
void
dcache_clean_and_invalidate_all()
{
	uint64_t clidr;
	uint64_t ccsidr;
	uint64_t ctype;
	uint64_t level_max, level;
	uint64_t way_max, way;
	uint64_t set_max, set;
	uint64_t log2linesize;
	uint64_t bits;

	CLIDR_EL1_READ(clidr);
	level_max = (clidr & CLIDR_LOC_MASK) >> CLIDR_LOC_SHIFT;
	for (level = 0; level < level_max; level++) {
		ctype = (clidr >> (CLIDR_CTYPE_SHIFT * level)) & CLIDR_CTYPE_MASK;
		if ((ctype == CLIDR_CTYPE_DATA) || (ctype == CLIDR_CTYPE_SEPARATE)
				|| (ctype == CLIDR_CTYPE_UNIFIED)) {
			CSSELR_EL1_WRITE(level << CSSELR_LEVEL_SHIFT);
			inst_sync_barrier();
			CCSIDR_EL1_READ(ccsidr);
			way_max = ((ccsidr & CCSIDR_ASSOCIATIVITY_MASK)
					>> CCSIDR_ASSOCIATIVITY_SHIFT) + 1;
			set_max = ((ccsidr & CCSIDR_NUMSETS_MASK)
					>> CCSIDR_NUMSETS_SHIFT) + 1;
			log2linesize = ((ccsidr & CCSIDR_LINESIZE_MASK)
					>> CCSIDR_LINESIZE_SHIFT) + 4;
			data_sync_barrier();
			for (way = 0; way < way_max; way++) {
				for (set = 0; set < set_max; set++) {
					bits = 0;
					bits |= way << (__builtin_clz((uint32_t)way_max) + 1);
					bits |= set << log2linesize;
					bits |= level << DC_LEVEL_SHIFT;
					dcache_clean_and_invalidate_line(bits);
				}
			}
		}
	}
	CSSELR_EL1_WRITE(0);
	data_sync_barrier();
	inst_sync_barrier();
}

/*
 *  Iキャッシュのフラッシュ
 */
void
icache_flush(void)
{
	icache_invalidate_all();
}

/*
 *  Dキャッシュのフラッシュ
 */
void
dcache_flush(void)
{
	uint64_t bits;

	SCTLR_EL1_READ(bits);
	if (bits & SCTLR_C_BIT) {
		dcache_clean_and_invalidate_all();
	} else {
		dcache_invalidate_all();
	}
}

/*
 *  I/Dキャッシュを両方をフラッシュ
 */
void
cache_flush(void)
{
	icache_flush();
	dcache_flush();
}

/*
 *  Iキャッシュの開始
 */
void
icache_enable(void)
{
	uint64_t bits;

	SCTLR_EL1_READ(bits);

	/* すでにONならリターン */
	if (bits & SCTLR_I_BIT) {
		return;
	}

	bits |= SCTLR_I_BIT;
	SCTLR_EL1_WRITE(bits);
	inst_sync_barrier();
}

/*
 *  Iキャッシュの停止
 */
void
icache_disable(void)
{
	uint64_t bits;

	SCTLR_EL1_READ(bits);
	bits &= ~SCTLR_I_BIT;
	SCTLR_EL1_WRITE(bits);
	inst_sync_barrier();
}

/*
 *  Dキャッシュの開始
 */
void
dcache_enable(void)
{
	uint64_t bits;

	SCTLR_EL1_READ(bits);

	/* すでにONならリターン */
	if (bits & SCTLR_C_BIT) {
		return;
	}

	bits |= SCTLR_C_BIT;
	SCTLR_EL1_WRITE(bits);
	inst_sync_barrier();
}

/*
 *  Dキャッシュの停止
 */
void
dcache_disable(void)
{
	uint64_t bits;

	SCTLR_EL1_READ(bits);
	if (bits & SCTLR_C_BIT) {
		bits &= ~SCTLR_C_BIT;
		SCTLR_EL1_WRITE(bits);
		inst_sync_barrier();
	}
}

/*
 *  I/Dキャッシュを両方を有効に
 */
void
cache_enable(void)
{
	icache_enable();
	dcache_enable();
}

/*
 *  I/Dキャッシュを両方を無効に
 */
void
cache_disable(void)
{
	icache_disable();
	dcache_disable();
}

/*
 *  TLB関連
 */
/*
 *  TLBの全エントリの無効化
 */
void
tlb_invalidate_all(void)
{
	TLBI_VMALLE1();
	data_sync_barrier();
}

/*
 *  MMU関連
 */
/*
 *  初段の変換テーブル
 */
static uintptr_t tt_pri_prc1[TT_FIRST_ENTRY_NUM] __attribute__ ((aligned(TT_FIRST_SIZE)));
#if		TNUM_PRCID >= 2
static uintptr_t tt_pri_prc2[TT_FIRST_ENTRY_NUM] __attribute__ ((aligned(TT_FIRST_SIZE)));
#endif	/* TNUM_PRCID >= 2 */
#if		TNUM_PRCID >= 3
static uintptr_t tt_pri_prc3[TT_FIRST_ENTRY_NUM] __attribute__ ((aligned(TT_FIRST_SIZE)));
#endif	/* TNUM_PRCID >= 3 */
#if		TNUM_PRCID >= 4
static uintptr_t tt_pri_prc4[TT_FIRST_ENTRY_NUM] __attribute__ ((aligned(TT_FIRST_SIZE)));
#endif	/* TNUM_PRCID >= 4 */
#if		TNUM_PRCID >= 5
static uintptr_t tt_pri_prc5[TT_FIRST_ENTRY_NUM] __attribute__ ((aligned(TT_FIRST_SIZE)));
#endif	/* TNUM_PRCID >= 5 */
#if		TNUM_PRCID >= 6
static uintptr_t tt_pri_prc6[TT_FIRST_ENTRY_NUM] __attribute__ ((aligned(TT_FIRST_SIZE)));
#endif	/* TNUM_PRCID >= 6 */
#if		TNUM_PRCID >= 7
#error "Added tt_pri_prc to arm64.c"
#endif	/* TNUM_PRCID >= 7 */

static uintptr_t *tt_pri[TNUM_PRCID] = {
	tt_pri_prc1
#if		TNUM_PRCID >= 2
	, tt_pri_prc2
#endif	/* TNUM_PRCID >= 2 */
#if		TNUM_PRCID >= 3
	, tt_pri_prc3
#endif	/* TNUM_PRCID >= 3 */
#if		TNUM_PRCID >= 4
	, tt_pri_prc4
#endif	/* TNUM_PRCID >= 4 */
#if		TNUM_PRCID >= 5
	, tt_pri_prc5
#endif	/* TNUM_PRCID >= 5 */
#if		TNUM_PRCID >= 6
	, tt_pri_prc6
#endif	/* TNUM_PRCID >= 6 */
};
#define my_tt_pri (tt_pri[get_my_prcidx()])

/*
 *  2段目以降の変換テーブル
 */
#define TT_NUM_MAX				10
static uintptr_t tt[TNUM_PRCID][TT_NUM_MAX][TT_ENTRY_NUM] __attribute__ ((aligned(TT_SIZE)));
#define my_tt (tt[get_my_prcidx()])
static int tt_num[TNUM_PRCID];
#define my_tt_num (tt_num[get_my_prcidx()])

/*
 *  メモリマップの設定を保存しておく配列
 */
#define MMAP_NUM_MAX			10
static mmap_t mmap[TNUM_PRCID][MMAP_NUM_MAX];
#define my_mmap (mmap[get_my_prcidx()])
static int mmap_num[TNUM_PRCID];
#define my_mmap_num (mmap_num[get_my_prcidx()])

/*
 *  メモリマップの初期化
 */
void
mmu_mmap_init(void)
{
	int i;

	for (i = 0; i < MMAP_NUM_MAX; i++) {
		my_mmap[i].pa = 0;
		my_mmap[i].va = 0;
		my_mmap[i].size = 0;
		my_mmap[i].attr = MEM_ATTR_SO;
		my_mmap[i].ap = MEM_AP_RW_EL1;
	#ifdef TOPPERS_TZ_S
		my_mmap[i].ns = MEM_NS_SECURE;
	#else  /* TOPPERS_TZ_S */
		my_mmap[i].ns = MEM_NS_NONSECURE;
	#endif /* TOPPERS_TZ_S */
	}
}

/*
 *  メモリマップの設定の追加
 */
void
mmu_mmap_add(const mmap_t *pmm)
{
	int insert_index;
	int i;

	assert((0 <= my_mmap_num) && (my_mmap_num < MMAP_NUM_MAX));
	assert((pmm->pa & TT_OFFSET_MASK) == 0x0);
	assert((pmm->va & TT_OFFSET_MASK) == 0x0);
	assert((pmm->size & TT_OFFSET_MASK) == 0x0);

	if (pmm->size == 0)
		return;

	/*
	 *  メモリマップの設定の挿入場所を探索
	 */
	insert_index = 0;
	while ((my_mmap[insert_index].va < pmm->va) && (insert_index < my_mmap_num)) {
		insert_index++;
	}

	assert((insert_index == 0) ||
			((my_mmap[insert_index - 1].va + my_mmap[insert_index - 1].size)
			 <= pmm->va));

	/*
	 *  メモリマップの設定の挿入
	 */
	for (i = my_mmap_num - 1; i >= insert_index; i--) {
		my_mmap[i + 1] = my_mmap[i];
	}
	my_mmap[insert_index] = *pmm;
	my_mmap_num++;

	assert(my_mmap_num < MMAP_NUM_MAX);
}

/*
 *  変換テーブルの初期化
 */
int
mmu_tt_init(int mmap_index, uintptr_t va, uintptr_t *ptt, int level)
{
	int lvl_sft;
	uintptr_t lvl_entspc_sz;	/* 現レベルの1エントリ分アドレス空間サイズ */
	uintptr_t lvl_allspc_sz;	/* 現レベルの全エントリ分アドレス空間サイズ */
	uintptr_t va_base;
	mmap_t *pmmap;
	uintptr_t desc;

	assert(level <= TT_LAST_LEVEL);

	/*
	 *  現レベルで設定するアドレス空間サイズの計算
	 */
	lvl_sft = TT_GRANULE_WIDTH + (TT_LAST_LEVEL - level) * TT_LEVEL_WIDTH;
#ifdef TOPPERS_32BIT_ABOVE_ADDR
	lvl_entspc_sz = (uintptr_t)1 << lvl_sft;
	if (level == TT_FIRST_LEVEL) {
		lvl_allspc_sz = ADDR_SPACE_SIZE;
	} else {
		lvl_allspc_sz = (uintptr_t)1 << (lvl_sft + TT_LEVEL_WIDTH);
	}
#else	/* TOPPERS_32BIT_ABOVE_ADDR */
	lvl_entspc_sz = 1 << lvl_sft;
	if (level == TT_FIRST_LEVEL) {
		lvl_allspc_sz = ADDR_SPACE_SIZE;
	} else {
		lvl_allspc_sz = 1 << (lvl_sft + TT_LEVEL_WIDTH);
	}
#endif	/* TOPPERS_32BIT_ABOVE_ADDR */

	/*
	 *  変換テーブルの各エントリを設定
	 */
	va_base = va;
	pmmap = &my_mmap[mmap_index];
	while ((va_base <= va) && (va < va_base + lvl_allspc_sz)) {
		/*
		 *  次のメモリマップの設定へ
		 */
		if (((pmmap->va + pmmap->size) <= va) && (mmap_index < my_mmap_num)) {
			mmap_index++;
			pmmap = &my_mmap[mmap_index];
			continue;
		}

		/*
		*  メモリマップの設定がないので，
		*  変換テーブルのエントリをINVALIDで埋める
		*/
		if ((mmap_index >= my_mmap_num) || (pmmap->va >= (va + lvl_entspc_sz))) {
			desc = TT_DESC_INVALID;

		/*
		*  メモリマップの設定通りに
		*  変換テーブルのエントリをBLOCK or PAGEで埋める
		*/
		}
#ifdef TOPPERS_32BIT_ABOVE_ADDR
		else if ((level >= TT_BLK_SUPPORT_LEVEL) && (pmmap->va <= va) &&
				((pmmap->va + pmmap->size) >= (va + lvl_entspc_sz))) {
#else	/* TOPPERS_32BIT_ABOVE_ADDR */
		else if ((pmmap->va <= va) &&
				((pmmap->va + pmmap->size) >= (va + lvl_entspc_sz))) {
#endif	/* TOPPERS_32BIT_ABOVE_ADDR */
			if (level == 3) {
				desc = TT_DESC_PAGE;
			} else {
				desc = TT_DESC_BLOCK;
			}
			desc |= TT_AF_BIT;
			switch (pmmap->attr) {
				case (MEM_ATTR_SO) :
					desc |= TT_SH_OSH;
					desc |= (MEM_ATTR_SO_ATTRINDX << TT_ATTRINDX_SHIFT);
					break;
				case (MEM_ATTR_DEV) :
					desc |= TT_SH_OSH;
					desc |= (MEM_ATTR_DEV_ATTRINDX << TT_ATTRINDX_SHIFT);
					break;
				case (MEM_ATTR_NML_NC) :
					desc |= TT_SH_ISH;
					desc |= (MEM_ATTR_NML_NC_ATTRINDX << TT_ATTRINDX_SHIFT);
					break;
				case (MEM_ATTR_NML_C) :
					desc |= TT_SH_ISH;
					desc |= (MEM_ATTR_NML_C_ATTRINDX << TT_ATTRINDX_SHIFT);
					break;
				default :
					desc |= TT_SH_OSH;
					desc |= (MEM_ATTR_SO_ATTRINDX << TT_ATTRINDX_SHIFT);
					break;
			}
			if (pmmap->ns == MEM_NS_NONSECURE) {
				desc = desc | TT_NS_BIT;
			}
			desc |= (pmmap->ap << 6);
			desc |= (va - pmmap->va + pmmap->pa);

		/*
		*  メモリマップの設定が細かいので，
		*  変換テーブルのエントリをTABLEにして次段へ
		*/
		} else {
			assert(my_tt_num <= TT_NUM_MAX);
			desc = TT_DESC_TABLE | (uintptr_t)my_tt[my_tt_num];
			mmap_index = mmu_tt_init(mmap_index, va, my_tt[my_tt_num++], level + 1);
			pmmap = &my_mmap[mmap_index];
		}

		/*
		 *  変換テーブルのエントリの書き込み
		 */
		*ptt++ = desc;
		va += lvl_entspc_sz;
	}

	return mmap_index;
}

/*
 *  MMUの初期化
 */
void
mmu_init(void)
{
	uint64_t bits;
#ifdef USE_ARM64_MMU_CONFIG_TABLE
	uint_t i;
#endif	/* USE_ARM64_MMU_CONFIG_TABLE */
#ifdef TOPPERS_32BIT_ABOVE_ADDR
	const uint_t	addr_space_width_table[] = {32, 36, 40, 42, 44, 48, 52};
	uint_t	pa_range, addr_space_width;
	bool_t	error = false;

	/* MMU設定が適切か確認 */
	ID_AA64MMFR0_EL1_READ(bits);
	pa_range = bits & AA64MMFR0_PARANGE_MASK;
	if( pa_range <= AA64MMFR0_PA4PB )  {
		addr_space_width = addr_space_width_table[pa_range];
	}
	else  {
        addr_space_width = 52;
	}

	if( addr_space_width < ADDR_SPACE_WIDTH )  {
		syslog(LOG_ERROR, "This system isn' support address space width %u[bit].", ADDR_SPACE_WIDTH);
		syslog(LOG_ERROR, "You must configure within %u[bit].", addr_space_width);
		assert(0);
	}

#if		TT_GRANULE_WIDTH == 12
	if( (bits & AA64MMFR0_TGRAN4_MASK) != AA64MMFR0_TGRAN4_OK )  {
		syslog(LOG_ERROR, "This system isn' support 4[KB] granule.");
		error = true;
	}
#elif	TT_GRANULE_WIDTH == 14
	if( (bits & AA64MMFR0_TGRAN16_MASK) != AA64MMFR0_TGRAN16_OK )  {
		syslog(LOG_ERROR, "This system isn' support 16[KB] granule.");
		error = true;
	}
#elif	TT_GRANULE_WIDTH == 16
	if( (bits & AA64MMFR0_TGRAN64_MASK) != AA64MMFR0_TGRAN64_OK )  {
		syslog(LOG_ERROR, "This system isn' support 64[KB] granule.");
		error = true;
	}
#else	/* TT_GRANULE_WIDTH == 16 */
	#error "TT_GRANULE_WIDTH must be 12, 14 or 16."
#endif	/* TT_GRANULE_WIDTH == 16 */

	if( error == true )  {
		syslog(LOG_ERROR, "You must configure one of the following granules.");
		if( (bits & AA64MMFR0_TGRAN4_MASK) == AA64MMFR0_TGRAN4_OK )  {
			syslog(LOG_ERROR, "   4[KB] granule");
		}
		if( (bits & AA64MMFR0_TGRAN16_MASK) == AA64MMFR0_TGRAN16_OK )  {
			syslog(LOG_ERROR, "  16[KB] granule");
		}
		if( (bits & AA64MMFR0_TGRAN64_MASK) == AA64MMFR0_TGRAN64_OK )  {
			syslog(LOG_ERROR, "  64[KB] granule");
		}
		assert(0);
	}
#endif	/* TOPPERS_32BIT_ABOVE_ADDR */

	/* パイプラインをフラッシュ */
	inst_sync_barrier();

	/* メモリマップ設定用変数を初期化 */
	my_mmap_num = 0;
	mmu_mmap_init();

	/* ターゲット依存部で定義された変換テーブルの初期化 */
#ifdef USE_ARM64_MMU_CONFIG_TABLE
	/* 静的なテーブルからメモリマップの設定を取り込む */
	for (i = 0; i < arm64_tnum_memory_area; i++) {
		mmu_mmap_add(&arm64_memory_area[i]);
	}
#else	/* USE_ARM64_MMU_CONFIG_TABLE */
	target_mmu_init();
#endif	/* USE_ARM64_MMU_CONFIG_TABLE */

	/* 変換テーブルの作成 */
	my_tt_num = 0;
	mmu_tt_init(0, 0x0, my_tt_pri, TT_FIRST_LEVEL);

	/* メモリの属性設定 */
	bits = 0;
	bits |= MAIR_ATTR_SO		<< (MAIR_ATTR_SHIFT * MEM_ATTR_SO_ATTRINDX);
	bits |= MAIR_ATTR_DEV	<< (MAIR_ATTR_SHIFT * MEM_ATTR_DEV_ATTRINDX);
	bits |= MAIR_ATTR_NML_NC	<< (MAIR_ATTR_SHIFT * MEM_ATTR_NML_NC_ATTRINDX);
	bits |= MAIR_ATTR_NML_C	<< (MAIR_ATTR_SHIFT * MEM_ATTR_NML_C_ATTRINDX);
	MAIR_EL1_WRITE(bits);

	/* TLBのインバリデート */
	tlb_invalidate_all();

	/* アドレス変換の制御設定 */
	bits = 0;
#ifdef TOPPERS_32BIT_ABOVE_ADDR
#if	ADDR_SPACE_WIDTH == 32
	bits |= TCR_IPS_4GB;
#elif ADDR_SPACE_WIDTH == 36
	bits |= TCR_IPS_64GB;
#elif ADDR_SPACE_WIDTH == 40
	bits |= TCR_IPS_1TB;
#elif ADDR_SPACE_WIDTH == 42
	bits |= TCR_IPS_4TB;
#elif ADDR_SPACE_WIDTH == 44
	bits |= TCR_IPS_16TB;
#elif ADDR_SPACE_WIDTH == 48
	bits |= TCR_IPS_256TB;
#endif	/* ADDR_SPACE_WIDTH */
#if	TT_GRANULE_WIDTH == 12
	bits |= TCR_TG0_4KB;
#elif TT_GRANULE_WIDTH == 14
	bits |= TCR_TG0_16KB;
#elif TT_GRANULE_WIDTH == 16
	bits |= TCR_TG0_64KB;
#endif /* TT_GRANULE_WIDTH == 16 */
#else	/* TOPPERS_32BIT_ABOVE_ADDR */
	bits |= TCR_IPS_4GB;
	bits |= TCR_TG0_4KB;
#endif	/* TOPPERS_32BIT_ABOVE_ADDR */
	bits |= TCR_SH0_INNER_SHAREABLE;
	bits |= TCR_ORGN0_WBWAC;
	bits |= TCR_IRGN0_WBWAC;
	bits |= (64 - ADDR_SPACE_WIDTH) << TCR_T0SZ_SHIFT;
	TCR_EL1_WRITE(bits);

	/* 変換テーブルをセットする */
	TTBR0_EL1_WRITE((uint64_t)my_tt_pri);

	/* 確実に変換テーブルをメモリに書き込み，設定を反映させる */
	data_sync_barrier();
	inst_sync_barrier();

	/* MMUを有効にする*/
	bits = 0;
	SCTLR_EL1_READ(bits);
	bits |= SCTLR_M_BIT;
	SCTLR_EL1_WRITE(bits);

	/* 確実に設定を反映させる */
	inst_sync_barrier();
}
