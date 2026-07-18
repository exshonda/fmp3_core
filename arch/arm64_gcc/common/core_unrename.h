/* This file is generated from core_rename.def by genrename. */

/* This file is included only when core_rename.h has been included. */
#ifdef TOPPERS_CORE_RENAME_H
#undef TOPPERS_CORE_RENAME_H

/*
 *  arm64.c
 */
#undef icache_invalidate_all
#undef icache_flush
#undef dcache_flush
#undef cache_flush
#undef icache_enable
#undef icache_disable
#undef dcache_enable
#undef dcache_disable
#undef cache_enable
#undef cache_disable
#undef tlb_invalidate_all
#undef mmu_mmap_init
#undef mmu_mmap_add
#undef mmu_tt_init
#undef mmu_init

/*
 *  core_support.S
 */
#undef vector_table
#undef dispatch
#undef dispatch_and_migrate
#undef exit_and_migrate
#undef start_dispatch
#undef exit_and_dispatch
#undef call_exit_kernel
#undef start_r
#undef cur_sp0_sync_handler
#undef cur_sp0_irq_handler
#undef cur_sp0_fiq_handler
#undef cur_sp0_serr_handler
#undef cur_spx_irq_handler
#undef cur_spx_sync_handler
#undef cur_spx_fiq_handler
#undef cur_spx_serr_handler
#undef l64_sync_handler
#undef l64_irq_handler
#undef l64_fiq_handler
#undef l64_serr_handler
#undef l32_sync_handler
#undef l32_irq_handler
#undef l32_fiq_handler
#undef l32_serr_handler
#undef dcache_invalidate_all
#undef dcache_clean_and_invalidate_all

/*
 *  core_kernel_impl.c
 */
#undef core_mprc_initialize
#undef core_initialize
#undef core_terminate
#undef initialize_exception
#undef xlog_sys
#undef default_exc_handler
#undef default_int_handler
#undef arm_fpu_initialize

/*
 *  core_kernel_impl.h
 */
#undef lock_cpu
#undef unlock_cpu
#undef sense_lock

/*
 *  kernel_cfg.c
 */
#undef p_inh_table
#undef p_intcfg_table
#undef p_exc_table
#undef idstkpt_table

/*
 *  gic_kernel_impl.c
 */
#undef gic_init
#undef gicc_stop
#undef gicd_disable_int
#undef gicd_enable_int
#undef gicd_clear_pending
#undef gicd_set_pending
#undef gicd_probe_pending
#undef gicd_config
#undef gicd_set_priority
#undef gicd_set_target
#undef gicd_initialize
#undef gicd_terminate
#undef initialize_interrupt

/*
 *  gic_support.S
 */
#undef irc_begin_int
#undef irc_end_int
#undef irc_get_intpri
#undef irc_begin_exc
#undef irc_end_exc

/*
 *  psci_support.S
 */
#undef psci_smc_cpuon


#endif /* TOPPERS_CORE_RENAME_H */
