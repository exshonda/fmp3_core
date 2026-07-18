/* This file is generated from target_rename.def by genrename. */

#ifndef TOPPERS_TARGET_RENAME_H
#define TOPPERS_TARGET_RENAME_H

/*
 *  target_kernel_impl.c
 */
#define target_mprc_initialize		_kernel_target_mprc_initialize
#define target_initialize			_kernel_target_initialize
#define target_exit					_kernel_target_exit

/*
 *  target_timer.c
 */
#define target_hrt_initialize		_kernel_target_hrt_initialize
#define target_hrt_terminate		_kernel_target_hrt_terminate
#define hrt_get_current_body		_kernel_hrt_get_current_body
#define hrt_set_event_body			_kernel_hrt_set_event_body
#define hrt_raise_event_body		_kernel_hrt_raise_event_body
#define hrt_clear_event_body		_kernel_hrt_clear_event_body


#include "chip_rename.h"

#endif /* TOPPERS_TARGET_RENAME_H */
