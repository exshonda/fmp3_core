/* This file is generated from core_rename.def by genrename. */

/* This file is included only when core_rename.h has been included. */
#ifdef TOPPERS_CORE_RENAME_H
#undef TOPPERS_CORE_RENAME_H

/*
 *  kernel_cfg.c
 */
#undef p_inh_table
#undef p_intcfg_table
#undef p_exc_table
#undef plic_target_cidx_table

/*
 *  core_support.S
 */
#undef dispatch
#undef start_dispatch
#undef exit_and_dispatch
#undef call_exit_kernel
#undef dispatch_and_migrate
#undef exit_and_migrate
#undef start_r
#undef core_int_entry
#undef core_exc_entry

/*
 *  core_kernel_impl.c
 */
#undef start_sync
#undef core_initialize
#undef core_terminate
#undef xlog_sys
#undef xlog_fsr
#undef default_int_handler
#undef default_exc_handler
#undef giant_lock

/*
 *  core_kernel_impl.h
 */
#undef lock_cpu
#undef unlock_cpu
#undef sense_lock

/*
 *  mtimer.c
 */
#undef target_hrt_initialize_global
#undef target_hrt_terminate_global
#undef target_hrt_initialize
#undef target_hrt_terminate
#undef target_hrt_handler

/*
 *  plic_kernel_impl.c
 */
#undef plic_context_initialize
#undef plic_global_initialize
#undef plic_initialize_interrupt

/*
 *  clic_kernel_impl.c
 */
#undef clic_context_initialize
#undef clic_global_initialize
#undef clic_initialize_interrupt


#endif /* TOPPERS_CORE_RENAME_H */
