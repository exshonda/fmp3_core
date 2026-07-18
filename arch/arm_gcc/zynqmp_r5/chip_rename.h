/* This file is generated from chip_rename.def by genrename. */

#ifndef TOPPERS_CHIP_RENAME_H
#define TOPPERS_CHIP_RENAME_H

/*
 *  chip_kernel_impl.c
 */
#define chip_initialize				_kernel_chip_initialize
#define chip_terminate				_kernel_chip_terminate
#define chip_mprc_initialize		_kernel_chip_mprc_initialize
#define mpu_disable_allregion		_kernel_mpu_disable_allregion
#define mpu_set_region				_kernel_mpu_set_region

/*
 *  ttc_hrt.c
 */
#define target_hrt_get_current		_kernel_target_hrt_get_current
#define target_hrt_set_event		_kernel_target_hrt_set_event
#define target_hrt_clear_event		_kernel_target_hrt_clear_event
#define target_hrt_raise_event		_kernel_target_hrt_raise_event


#include "core_rename.h"

#endif /* TOPPERS_CHIP_RENAME_H */
