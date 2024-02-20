/*
 * Copyright (c) 2018, Linaro Limited. and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/*
 * @file    generic/sleep.h
 * @brief   Generic sleep primitives for libmetal.
 */

#ifndef __METAL_SLEEP__H__
#error "Include metal/sleep.h instead of metal/generic/sleep.h"
#endif

#ifndef __METAL_GENERIC_SLEEP__H__
#define __METAL_GENERIC_SLEEP__H__

#include <metal/utilities.h>
#include <metal/config.h> // MicroPython

#ifdef __cplusplus
extern "C" {
#endif

// MicroPython: Must be provided by the port (in mpmetalport.h / mpmetalport.c).
int __metal_sleep_usec(unsigned int usec);

#ifdef __cplusplus
}
#endif

#endif /* __METAL_GENERIC_SLEEP__H__ */
