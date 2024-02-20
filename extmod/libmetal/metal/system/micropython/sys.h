/*
 * Copyright (c) 2015, Xilinx Inc. and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/*
 * @file    generic/sys.h
 * @brief   Generic system primitives for libmetal.
 */

#ifndef __METAL_SYS__H__
#error "Include metal/sys.h instead of metal/generic/sys.h"
#endif

#ifndef __METAL_GENERIC_SYS__H__
#define __METAL_GENERIC_SYS__H__

#include <limits.h>
#include <metal/config.h> // MicroPython: Added "metal/config.h" to get overridden METAL_MAX_DEVICE_REGIONS.
#include <metal/errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

// MicroPython: Removed "./@PROJECT_MACHINE@/sys.h" (and added sys_irq_enable/sys_irq_disable directly below).

#ifdef __cplusplus
extern "C" {
#endif

#ifndef METAL_MAX_DEVICE_REGIONS
#define METAL_MAX_DEVICE_REGIONS 1
#endif

/** Structure of generic libmetal runtime state. */
struct metal_state {

    /** Common (system independent) data. */
    struct metal_common_state common;
};

#ifdef METAL_INTERNAL

// MicroPython: Added these two declarations from generic/template/sys.h
void sys_irq_enable(unsigned int vector);
void sys_irq_disable(unsigned int vector);

/**
 * @brief restore interrupts to state before disable_global_interrupt()
 */
void sys_irq_restore_enable(unsigned int flags);

/**
 * @brief disable all interrupts
 */
unsigned int sys_irq_save_disable(void);

#endif /* METAL_INTERNAL */

#ifdef __cplusplus
}
#endif

#endif /* __METAL_GENERIC_SYS__H__ */
