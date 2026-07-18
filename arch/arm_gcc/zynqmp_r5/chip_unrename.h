/* This file is generated from chip_rename.def by genrename. */

/* This file is included only when chip_rename.h has been included. */
#ifdef TOPPERS_CHIP_RENAME_H
#undef TOPPERS_CHIP_RENAME_H

/*
 *  chip_kernel_impl.c
 */
#undef chip_initialize
#undef chip_terminate
#undef chip_mprc_initialize
#undef mpu_disable_allregion
#undef mpu_set_region

/*
 *  ttc_hrt.c
 */
#undef target_hrt_get_current
#undef target_hrt_set_event
#undef target_hrt_clear_event
#undef target_hrt_raise_event


#include "core_unrename.h"

#endif /* TOPPERS_CHIP_RENAME_H */
