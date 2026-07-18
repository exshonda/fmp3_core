/* This file is generated from core_rename.def by genrename. */

#ifndef TOPPERS_CORE_RENAME_H
#define TOPPERS_CORE_RENAME_H

/*
 *  arm64.c
 */
#define icache_invalidate_all		_kernel_icache_invalidate_all
#define icache_flush				_kernel_icache_flush
#define dcache_flush				_kernel_dcache_flush
#define cache_flush					_kernel_cache_flush
#define icache_enable				_kernel_icache_enable
#define icache_disable				_kernel_icache_disable
#define dcache_enable				_kernel_dcache_enable
#define dcache_disable				_kernel_dcache_disable
#define cache_enable				_kernel_cache_enable
#define cache_disable				_kernel_cache_disable
#define tlb_invalidate_all			_kernel_tlb_invalidate_all
#define mmu_mmap_init				_kernel_mmu_mmap_init
#define mmu_mmap_add				_kernel_mmu_mmap_add
#define mmu_tt_init					_kernel_mmu_tt_init
#define mmu_init					_kernel_mmu_init

/*
 *  core_support.S
 */
#define vector_table				_kernel_vector_table
#define dispatch					_kernel_dispatch
#define dispatch_and_migrate		_kernel_dispatch_and_migrate
#define exit_and_migrate			_kernel_exit_and_migrate
#define start_dispatch				_kernel_start_dispatch
#define exit_and_dispatch			_kernel_exit_and_dispatch
#define call_exit_kernel			_kernel_call_exit_kernel
#define start_r						_kernel_start_r
#define cur_sp0_sync_handler		_kernel_cur_sp0_sync_handler
#define cur_sp0_irq_handler			_kernel_cur_sp0_irq_handler
#define cur_sp0_fiq_handler			_kernel_cur_sp0_fiq_handler
#define cur_sp0_serr_handler		_kernel_cur_sp0_serr_handler
#define cur_spx_irq_handler			_kernel_cur_spx_irq_handler
#define cur_spx_sync_handler		_kernel_cur_spx_sync_handler
#define cur_spx_fiq_handler			_kernel_cur_spx_fiq_handler
#define cur_spx_serr_handler		_kernel_cur_spx_serr_handler
#define l64_sync_handler			_kernel_l64_sync_handler
#define l64_irq_handler				_kernel_l64_irq_handler
#define l64_fiq_handler				_kernel_l64_fiq_handler
#define l64_serr_handler			_kernel_l64_serr_handler
#define l32_sync_handler			_kernel_l32_sync_handler
#define l32_irq_handler				_kernel_l32_irq_handler
#define l32_fiq_handler				_kernel_l32_fiq_handler
#define l32_serr_handler			_kernel_l32_serr_handler
#define dcache_invalidate_all		_kernel_dcache_invalidate_all
#define dcache_clean_and_invalidate_all	_kernel_dcache_clean_and_invalidate_all

/*
 *  core_kernel_impl.c
 */
#define core_mprc_initialize		_kernel_core_mprc_initialize
#define core_initialize				_kernel_core_initialize
#define core_terminate				_kernel_core_terminate
#define initialize_exception		_kernel_initialize_exception
#define xlog_sys					_kernel_xlog_sys
#define default_exc_handler			_kernel_default_exc_handler
#define default_int_handler			_kernel_default_int_handler
#define arm_fpu_initialize			_kernel_arm_fpu_initialize

/*
 *  core_kernel_impl.h
 */
#define lock_cpu					_kernel_lock_cpu
#define unlock_cpu					_kernel_unlock_cpu
#define sense_lock					_kernel_sense_lock

/*
 *  kernel_cfg.c
 */
#define p_inh_table					_kernel_p_inh_table
#define p_intcfg_table				_kernel_p_intcfg_table
#define p_exc_table					_kernel_p_exc_table
#define idstkpt_table				_kernel_idstkpt_table

/*
 *  gic_kernel_impl.c
 */
#define gic_init					_kernel_gic_init
#define gicc_stop					_kernel_gicc_stop
#define gicd_disable_int			_kernel_gicd_disable_int
#define gicd_enable_int				_kernel_gicd_enable_int
#define gicd_clear_pending			_kernel_gicd_clear_pending
#define gicd_set_pending			_kernel_gicd_set_pending
#define gicd_probe_pending			_kernel_gicd_probe_pending
#define gicd_config					_kernel_gicd_config
#define gicd_set_priority			_kernel_gicd_set_priority
#define gicd_set_target				_kernel_gicd_set_target
#define gicd_initialize				_kernel_gicd_initialize
#define gicd_terminate				_kernel_gicd_terminate
#define initialize_interrupt		_kernel_initialize_interrupt

/*
 *  gic_support.S
 */
#define irc_begin_int				_kernel_irc_begin_int
#define irc_end_int					_kernel_irc_end_int
#define irc_get_intpri				_kernel_irc_get_intpri
#define irc_begin_exc				_kernel_irc_begin_exc
#define irc_end_exc					_kernel_irc_end_exc

/*
 *  psci_support.S
 */
#define psci_smc_cpuon				_kernel_psci_smc_cpuon


#endif /* TOPPERS_CORE_RENAME_H */
