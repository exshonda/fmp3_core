/* This file is generated from core_rename.def by genrename. */

#ifndef TOPPERS_CORE_RENAME_H
#define TOPPERS_CORE_RENAME_H

/*
 *  kernel_cfg.c
 */
#define p_inh_table					_kernel_p_inh_table
#define p_intcfg_table				_kernel_p_intcfg_table
#define p_exc_table					_kernel_p_exc_table
#define plic_target_cidx_table		_kernel_plic_target_cidx_table

/*
 *  core_support.S
 */
#define dispatch					_kernel_dispatch
#define start_dispatch				_kernel_start_dispatch
#define exit_and_dispatch			_kernel_exit_and_dispatch
#define call_exit_kernel			_kernel_call_exit_kernel
#define dispatch_and_migrate		_kernel_dispatch_and_migrate
#define exit_and_migrate			_kernel_exit_and_migrate
#define start_r						_kernel_start_r
#define core_int_entry				_kernel_core_int_entry
#define core_exc_entry				_kernel_core_exc_entry

/*
 *  core_kernel_impl.c
 */
#define start_sync					_kernel_start_sync
#define core_initialize				_kernel_core_initialize
#define core_terminate				_kernel_core_terminate
#define xlog_sys					_kernel_xlog_sys
#define xlog_fsr					_kernel_xlog_fsr
#define default_int_handler			_kernel_default_int_handler
#define default_exc_handler			_kernel_default_exc_handler
#define giant_lock					_kernel_giant_lock

/*
 *  core_kernel_impl.h
 */
#define lock_cpu					_kernel_lock_cpu
#define unlock_cpu					_kernel_unlock_cpu
#define sense_lock					_kernel_sense_lock

/*
 *  mtimer.c
 */
#define target_hrt_initialize_global	_kernel_target_hrt_initialize_global
#define target_hrt_terminate_global	_kernel_target_hrt_terminate_global
#define target_hrt_initialize		_kernel_target_hrt_initialize
#define target_hrt_terminate		_kernel_target_hrt_terminate
#define target_hrt_handler			_kernel_target_hrt_handler

/*
 *  plic_kernel_impl.c
 */
#define plic_context_initialize		_kernel_plic_context_initialize
#define plic_global_initialize		_kernel_plic_global_initialize
#define plic_initialize_interrupt	_kernel_plic_initialize_interrupt

/*
 *  clic_kernel_impl.c
 */
#define clic_context_initialize		_kernel_clic_context_initialize
#define clic_global_initialize		_kernel_clic_global_initialize
#define clic_initialize_interrupt	_kernel_clic_initialize_interrupt


#endif /* TOPPERS_CORE_RENAME_H */
