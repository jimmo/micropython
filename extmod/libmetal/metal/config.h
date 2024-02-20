/*
 * Copyright (c) 2015, Xilinx Inc. and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/*
 * @file    config.h
 * @brief   Generated configuration settings for libmetal.
 */

#ifndef __METAL_CONFIG__H__
#define __METAL_CONFIG__H__

#ifdef __cplusplus
extern "C" {
#endif

// Get port-specific config.
#include "mpmetalport.h"

/** Library major version number. */
#define METAL_VER_MAJOR     1

/** Library minor version number. */
#define METAL_VER_MINOR     5

/** Library patch level. */
#define METAL_VER_PATCH     0

/** Library version string. */
#define METAL_VER           "1.5.0"

#if METAL_HAVE_STDATOMIC_H
#define HAVE_STDATOMIC_H
#endif

#if METAL_HAVE_FUTEX_H
#define HAVE_FUTEX_H
#endif

#ifdef __cplusplus
}
#endif

#endif /* __METAL_CONFIG__H__ */
